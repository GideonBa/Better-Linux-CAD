#pragma once

#include "blcad/core/sketch_dimension_authoring.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {
namespace detail {

using SketchDimensionJson = nlohmann::json;

[[nodiscard]] inline std::optional<SketchDimensionKind>
sketch_dimension_kind_from_string(std::string_view value) noexcept {
  if (value == "horizontal_distance") return SketchDimensionKind::HorizontalDistance;
  if (value == "vertical_distance") return SketchDimensionKind::VerticalDistance;
  if (value == "aligned_distance") return SketchDimensionKind::AlignedDistance;
  if (value == "point_to_point_distance") return SketchDimensionKind::PointToPointDistance;
  if (value == "length") return SketchDimensionKind::Length;
  if (value == "radius") return SketchDimensionKind::Radius;
  if (value == "diameter") return SketchDimensionKind::Diameter;
  if (value == "angle") return SketchDimensionKind::Angle;
  if (value == "arc_length") return SketchDimensionKind::ArcLength;
  return std::nullopt;
}

[[nodiscard]] inline SketchDimensionJson
sketch_dimension_intent_to_json(const SketchDimensionIntent& dimension) {
  SketchDimensionJson targets = SketchDimensionJson::array();
  for (const auto& target : dimension.targets())
    targets.push_back(SketchDimensionJson{{"kind", std::string(to_string(target.kind()))},
                                          {"id", target.id()}});
  SketchDimensionJson value{{"id", dimension.id().value()},
                            {"kind", std::string(to_string(dimension.kind()))},
                            {"mode", std::string(to_string(dimension.mode()))},
                            {"targets", std::move(targets)}};
  if (dimension.parameter()) value["parameter"] = dimension.parameter()->value();
  return value;
}

[[nodiscard]] inline Result<SketchDimensionIntent>
sketch_dimension_intent_from_json(const SketchDimensionJson& value) {
  if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
      !value.contains("kind") || !value.at("kind").is_string() ||
      !value.contains("mode") || !value.at("mode").is_string() ||
      !value.contains("targets") || !value.at("targets").is_array())
    return Result<SketchDimensionIntent>::failure(Error::validation(
        "sketch_dimension_json", "Sketch dimension record has invalid fields"));
  const std::string id = value.at("id").get<std::string>();
  const auto kind = sketch_dimension_kind_from_string(value.at("kind").get<std::string>());
  if (!kind)
    return Result<SketchDimensionIntent>::failure(
        Error::validation(id, "unsupported Sketch dimension kind"));
  const std::string mode_name = value.at("mode").get<std::string>();
  std::optional<SketchDimensionMode> mode;
  if (mode_name == "driving") mode = SketchDimensionMode::Driving;
  if (mode_name == "reference") mode = SketchDimensionMode::Reference;
  if (!mode)
    return Result<SketchDimensionIntent>::failure(
        Error::validation(id, "unsupported Sketch dimension mode"));

  std::vector<SketchConstraintIntentTarget> targets;
  for (const auto& target : value.at("targets")) {
    if (!target.is_object() || !target.contains("kind") ||
        !target.at("kind").is_string() || !target.contains("id") ||
        !target.at("id").is_string())
      return Result<SketchDimensionIntent>::failure(
          Error::validation(id, "Sketch dimension target has invalid fields"));
    const std::string target_kind = target.at("kind").get<std::string>();
    Result<SketchConstraintIntentTarget> parsed =
        target_kind == "point"
            ? SketchConstraintIntentTarget::point(
                  SketchPointId(target.at("id").get<std::string>()))
            : target_kind == "entity"
                  ? SketchConstraintIntentTarget::entity(
                        target.at("id").get<std::string>())
                  : Result<SketchConstraintIntentTarget>::failure(
                        Error::validation(id, "unsupported Sketch dimension target kind"));
    if (parsed.has_error())
      return Result<SketchDimensionIntent>::failure(parsed.error());
    targets.push_back(std::move(parsed.value()));
  }

  std::optional<ParameterId> parameter;
  if (value.contains("parameter")) {
    if (!value.at("parameter").is_string())
      return Result<SketchDimensionIntent>::failure(
          Error::validation(id, "Sketch dimension parameter field must be a string"));
    parameter = ParameterId(value.at("parameter").get<std::string>());
  }
  return SketchDimensionIntent::create(SketchDimensionId(id), *kind, *mode,
                                       std::move(targets), std::move(parameter));
}

} // namespace detail

[[nodiscard]] inline Result<std::string>
serialize_sketch_dimension_catalogs_to_json(
    DocumentId document, std::vector<SketchDimensionCatalog> catalogs) {
  using json = nlohmann::json;
  if (document.empty())
    return Result<std::string>::failure(Error::validation(
        "sketch_dimension_json", "dimension sidecar requires a document id"));
  std::sort(catalogs.begin(), catalogs.end(), [](const auto& left, const auto& right) {
    return left.sketch().value() < right.sketch().value();
  });
  for (std::size_t index = 0U; index < catalogs.size(); ++index) {
    if (catalogs[index].document() != document)
      return Result<std::string>::failure(Error::validation(
          catalogs[index].sketch().value(), "dimension catalog belongs to another document"));
    if (index > 0U && catalogs[index - 1U].sketch() == catalogs[index].sketch())
      return Result<std::string>::failure(Error::validation(
          catalogs[index].sketch().value(), "dimension sidecar Sketch catalogs must be unique"));
  }

  json root{{"schema", "blcad.sketch_dimensions.mvp8"},
            {"version", 1},
            {"document", document.value()},
            {"catalogs", json::array()}};
  for (const auto& catalog : catalogs) {
    json dimensions = json::array();
    for (const auto& dimension : catalog.dimensions())
      dimensions.push_back(detail::sketch_dimension_intent_to_json(dimension));
    root["catalogs"].push_back(
        json{{"sketch", catalog.sketch().value()},
             {"dimensions", std::move(dimensions)}});
  }
  return Result<std::string>::success(root.dump(2));
}

[[nodiscard]] inline Result<std::vector<SketchDimensionCatalog>>
deserialize_sketch_dimension_catalogs_from_json(std::string_view content) {
  using json = nlohmann::json;
  try {
    const json root = json::parse(content.begin(), content.end());
    if (!root.is_object() ||
        root.value("schema", std::string{}) != "blcad.sketch_dimensions.mvp8" ||
        root.value("version", 0) != 1 || !root.contains("document") ||
        !root.at("document").is_string() || !root.contains("catalogs") ||
        !root.at("catalogs").is_array())
      return Result<std::vector<SketchDimensionCatalog>>::failure(Error::validation(
          "sketch_dimension_json", "unsupported or malformed Sketch dimension sidecar"));

    const DocumentId document(root.at("document").get<std::string>());
    std::vector<SketchDimensionCatalog> catalogs;
    for (const auto& catalog_json : root.at("catalogs")) {
      if (!catalog_json.is_object() || !catalog_json.contains("sketch") ||
          !catalog_json.at("sketch").is_string() || !catalog_json.contains("dimensions") ||
          !catalog_json.at("dimensions").is_array())
        return Result<std::vector<SketchDimensionCatalog>>::failure(Error::validation(
            "sketch_dimension_json", "Sketch dimension catalog has invalid fields"));
      std::vector<SketchDimensionIntent> dimensions;
      for (const auto& value : catalog_json.at("dimensions")) {
        auto dimension = detail::sketch_dimension_intent_from_json(value);
        if (dimension.has_error())
          return Result<std::vector<SketchDimensionCatalog>>::failure(dimension.error());
        dimensions.push_back(std::move(dimension.value()));
      }
      auto catalog = SketchDimensionCatalog::create(
          document, SketchId(catalog_json.at("sketch").get<std::string>()),
          std::move(dimensions));
      if (catalog.has_error())
        return Result<std::vector<SketchDimensionCatalog>>::failure(catalog.error());
      catalogs.push_back(std::move(catalog.value()));
    }
    std::sort(catalogs.begin(), catalogs.end(), [](const auto& left, const auto& right) {
      return left.sketch().value() < right.sketch().value();
    });
    for (std::size_t index = 1U; index < catalogs.size(); ++index)
      if (catalogs[index - 1U].sketch() == catalogs[index].sketch())
        return Result<std::vector<SketchDimensionCatalog>>::failure(Error::validation(
            catalogs[index].sketch().value(), "duplicate Sketch dimension catalog"));
    return Result<std::vector<SketchDimensionCatalog>>::success(std::move(catalogs));
  } catch (const std::exception& exception) {
    return Result<std::vector<SketchDimensionCatalog>>::failure(
        Error::validation("sketch_dimension_json", exception.what()));
  }
}

[[nodiscard]] inline Result<std::string>
serialize_sketch_dimension_catalog_to_json(const SketchDimensionCatalog& catalog) {
  return serialize_sketch_dimension_catalogs_to_json(catalog.document(), {catalog});
}

[[nodiscard]] inline Result<SketchDimensionCatalog>
deserialize_sketch_dimension_catalog_from_json(std::string_view content) {
  auto catalogs = deserialize_sketch_dimension_catalogs_from_json(content);
  if (catalogs.has_error())
    return Result<SketchDimensionCatalog>::failure(catalogs.error());
  if (catalogs.value().size() != 1U)
    return Result<SketchDimensionCatalog>::failure(Error::validation(
        "sketch_dimension_json", "single-catalog reader requires exactly one Sketch catalog"));
  return Result<SketchDimensionCatalog>::success(std::move(catalogs.value().front()));
}

} // namespace blcad
