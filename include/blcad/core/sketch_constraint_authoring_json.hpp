#pragma once

#include "blcad/core/sketch_constraint_authoring.hpp"

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

using SketchConstraintJson = nlohmann::json;

[[nodiscard]] inline SketchConstraintJson
sketch_constraint_intent_to_json(const SketchConstraintIntent& constraint) {
  SketchConstraintJson targets = SketchConstraintJson::array();
  for (const auto& target : constraint.targets())
    targets.push_back(SketchConstraintJson{{"kind", std::string(to_string(target.kind()))},
                                           {"id", target.id()}});
  return SketchConstraintJson{{"id", constraint.id().value()},
                              {"kind", std::string(to_string(constraint.kind()))},
                              {"source", std::string(to_string(constraint.source()))},
                              {"targets", std::move(targets)}};
}

[[nodiscard]] inline std::optional<SketchSolverConstraintKind>
sketch_constraint_intent_kind_from_string(std::string_view value) noexcept {
  if (value == "coincident") return SketchSolverConstraintKind::Coincident;
  if (value == "fixed") return SketchSolverConstraintKind::Fixed;
  if (value == "horizontal") return SketchSolverConstraintKind::Horizontal;
  if (value == "vertical") return SketchSolverConstraintKind::Vertical;
  if (value == "parallel") return SketchSolverConstraintKind::Parallel;
  if (value == "perpendicular") return SketchSolverConstraintKind::Perpendicular;
  if (value == "collinear") return SketchSolverConstraintKind::Collinear;
  if (value == "equal") return SketchSolverConstraintKind::Equal;
  if (value == "tangent") return SketchSolverConstraintKind::Tangent;
  if (value == "concentric") return SketchSolverConstraintKind::Concentric;
  if (value == "midpoint") return SketchSolverConstraintKind::Midpoint;
  if (value == "symmetric") return SketchSolverConstraintKind::Symmetric;
  if (value == "point_on_object") return SketchSolverConstraintKind::PointOnObject;
  return std::nullopt;
}

[[nodiscard]] inline Result<SketchConstraintIntent>
sketch_constraint_intent_from_json(const SketchConstraintJson& value) {
  if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
      !value.contains("kind") || !value.at("kind").is_string() ||
      !value.contains("source") || !value.at("source").is_string() ||
      !value.contains("targets") || !value.at("targets").is_array())
    return Result<SketchConstraintIntent>::failure(Error::validation(
        "sketch_constraint_json", "Sketch constraint record has invalid fields"));

  const auto kind = sketch_constraint_intent_kind_from_string(
      value.at("kind").get<std::string>());
  if (!kind)
    return Result<SketchConstraintIntent>::failure(Error::validation(
        value.at("id").get<std::string>(), "unsupported Sketch constraint kind"));
  const std::string source_name = value.at("source").get<std::string>();
  std::optional<SketchConstraintIntentSource> source;
  if (source_name == "manual") source = SketchConstraintIntentSource::Manual;
  if (source_name == "automatic") source = SketchConstraintIntentSource::Automatic;
  if (!source)
    return Result<SketchConstraintIntent>::failure(Error::validation(
        value.at("id").get<std::string>(), "unsupported Sketch constraint source"));

  std::vector<SketchConstraintIntentTarget> targets;
  for (const auto& target : value.at("targets")) {
    if (!target.is_object() || !target.contains("kind") ||
        !target.at("kind").is_string() || !target.contains("id") ||
        !target.at("id").is_string())
      return Result<SketchConstraintIntent>::failure(Error::validation(
          value.at("id").get<std::string>(), "Sketch constraint target has invalid fields"));
    const std::string target_kind = target.at("kind").get<std::string>();
    Result<SketchConstraintIntentTarget> parsed =
        target_kind == "point"
            ? SketchConstraintIntentTarget::point(
                  SketchPointId(target.at("id").get<std::string>()))
            : target_kind == "entity"
                  ? SketchConstraintIntentTarget::entity(target.at("id").get<std::string>())
                  : Result<SketchConstraintIntentTarget>::failure(Error::validation(
                        value.at("id").get<std::string>(),
                        "unsupported Sketch constraint target kind"));
    if (parsed.has_error())
      return Result<SketchConstraintIntent>::failure(parsed.error());
    targets.push_back(std::move(parsed.value()));
  }

  return SketchConstraintIntent::create(
      SketchConstraintId(value.at("id").get<std::string>()), *kind,
      std::move(targets), *source);
}

} // namespace detail

[[nodiscard]] inline Result<std::string>
serialize_sketch_constraint_catalogs_to_json(
    DocumentId document, std::vector<SketchConstraintCatalog> catalogs) {
  using json = nlohmann::json;
  if (document.empty())
    return Result<std::string>::failure(Error::validation(
        "sketch_constraint_json", "constraint sidecar requires a document id"));
  std::sort(catalogs.begin(), catalogs.end(), [](const auto& left, const auto& right) {
    return left.sketch().value() < right.sketch().value();
  });
  for (std::size_t index = 0U; index < catalogs.size(); ++index) {
    if (catalogs[index].document() != document)
      return Result<std::string>::failure(Error::validation(
          catalogs[index].sketch().value(), "constraint catalog belongs to another document"));
    if (index > 0U && catalogs[index - 1U].sketch() == catalogs[index].sketch())
      return Result<std::string>::failure(Error::validation(
          catalogs[index].sketch().value(), "constraint sidecar Sketch catalogs must be unique"));
  }

  json root{{"schema", "blcad.sketch_constraints.mvp8"},
            {"version", 1},
            {"document", document.value()},
            {"catalogs", json::array()}};
  for (const auto& catalog : catalogs) {
    json constraints = json::array();
    for (const auto& constraint : catalog.constraints())
      constraints.push_back(detail::sketch_constraint_intent_to_json(constraint));
    root["catalogs"].push_back(
        json{{"sketch", catalog.sketch().value()},
             {"constraints", std::move(constraints)}});
  }
  return Result<std::string>::success(root.dump(2));
}

[[nodiscard]] inline Result<std::vector<SketchConstraintCatalog>>
deserialize_sketch_constraint_catalogs_from_json(std::string_view content) {
  using json = nlohmann::json;
  try {
    const json root = json::parse(content.begin(), content.end());
    if (!root.is_object() ||
        root.value("schema", std::string{}) != "blcad.sketch_constraints.mvp8" ||
        root.value("version", 0) != 1 || !root.contains("document") ||
        !root.at("document").is_string() || !root.contains("catalogs") ||
        !root.at("catalogs").is_array())
      return Result<std::vector<SketchConstraintCatalog>>::failure(Error::validation(
          "sketch_constraint_json", "unsupported or malformed Sketch constraint sidecar"));

    const DocumentId document(root.at("document").get<std::string>());
    std::vector<SketchConstraintCatalog> catalogs;
    for (const auto& catalog_json : root.at("catalogs")) {
      if (!catalog_json.is_object() || !catalog_json.contains("sketch") ||
          !catalog_json.at("sketch").is_string() || !catalog_json.contains("constraints") ||
          !catalog_json.at("constraints").is_array())
        return Result<std::vector<SketchConstraintCatalog>>::failure(Error::validation(
            "sketch_constraint_json", "Sketch constraint catalog has invalid fields"));
      std::vector<SketchConstraintIntent> constraints;
      for (const auto& value : catalog_json.at("constraints")) {
        auto constraint = detail::sketch_constraint_intent_from_json(value);
        if (constraint.has_error())
          return Result<std::vector<SketchConstraintCatalog>>::failure(constraint.error());
        constraints.push_back(std::move(constraint.value()));
      }
      auto catalog = SketchConstraintCatalog::create(
          document, SketchId(catalog_json.at("sketch").get<std::string>()),
          std::move(constraints));
      if (catalog.has_error())
        return Result<std::vector<SketchConstraintCatalog>>::failure(catalog.error());
      catalogs.push_back(std::move(catalog.value()));
    }
    std::sort(catalogs.begin(), catalogs.end(), [](const auto& left, const auto& right) {
      return left.sketch().value() < right.sketch().value();
    });
    for (std::size_t index = 1U; index < catalogs.size(); ++index)
      if (catalogs[index - 1U].sketch() == catalogs[index].sketch())
        return Result<std::vector<SketchConstraintCatalog>>::failure(Error::validation(
            catalogs[index].sketch().value(), "duplicate Sketch constraint catalog"));
    return Result<std::vector<SketchConstraintCatalog>>::success(std::move(catalogs));
  } catch (const std::exception& exception) {
    return Result<std::vector<SketchConstraintCatalog>>::failure(
        Error::validation("sketch_constraint_json", exception.what()));
  }
}

[[nodiscard]] inline Result<std::string>
serialize_sketch_constraint_catalog_to_json(const SketchConstraintCatalog& catalog) {
  return serialize_sketch_constraint_catalogs_to_json(catalog.document(), {catalog});
}

[[nodiscard]] inline Result<SketchConstraintCatalog>
deserialize_sketch_constraint_catalog_from_json(std::string_view content) {
  auto catalogs = deserialize_sketch_constraint_catalogs_from_json(content);
  if (catalogs.has_error())
    return Result<SketchConstraintCatalog>::failure(catalogs.error());
  if (catalogs.value().size() != 1U)
    return Result<SketchConstraintCatalog>::failure(Error::validation(
        "sketch_constraint_json", "single-catalog reader requires exactly one Sketch catalog"));
  return Result<SketchConstraintCatalog>::success(std::move(catalogs.value().front()));
}

} // namespace blcad
