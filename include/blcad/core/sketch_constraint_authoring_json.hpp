#pragma once

#include "blcad/core/sketch_constraint_authoring.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

[[nodiscard]] inline Result<std::string>
serialize_sketch_constraint_catalog_to_json(const SketchConstraintCatalog& catalog) {
  using json = nlohmann::json;
  json root{{"schema", "blcad.sketch_constraints.mvp8"},
            {"version", 1},
            {"document", catalog.document().value()},
            {"sketch", catalog.sketch().value()},
            {"constraints", json::array()}};
  for (const auto& constraint : catalog.constraints()) {
    json targets = json::array();
    for (const auto& target : constraint.targets())
      targets.push_back(json{{"kind", std::string(to_string(target.kind()))},
                             {"id", target.id()}});
    root["constraints"].push_back(
        json{{"id", constraint.id().value()},
             {"kind", std::string(to_string(constraint.kind()))},
             {"source", std::string(to_string(constraint.source()))},
             {"targets", std::move(targets)}});
  }
  return Result<std::string>::success(root.dump(2));
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

[[nodiscard]] inline Result<SketchConstraintCatalog>
deserialize_sketch_constraint_catalog_from_json(std::string_view content) {
  using json = nlohmann::json;
  try {
    const json root = json::parse(content.begin(), content.end());
    if (!root.is_object() ||
        root.value("schema", std::string{}) != "blcad.sketch_constraints.mvp8" ||
        root.value("version", 0) != 1 || !root.contains("document") ||
        !root.at("document").is_string() || !root.contains("sketch") ||
        !root.at("sketch").is_string() || !root.contains("constraints") ||
        !root.at("constraints").is_array())
      return Result<SketchConstraintCatalog>::failure(Error::validation(
          "sketch_constraint_json", "unsupported or malformed Sketch constraint catalog"));

    std::vector<SketchConstraintIntent> constraints;
    for (const auto& value : root.at("constraints")) {
      if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
          !value.contains("kind") || !value.at("kind").is_string() ||
          !value.contains("source") || !value.at("source").is_string() ||
          !value.contains("targets") || !value.at("targets").is_array())
        return Result<SketchConstraintCatalog>::failure(Error::validation(
            "sketch_constraint_json", "Sketch constraint record has invalid fields"));

      const auto kind = sketch_constraint_intent_kind_from_string(
          value.at("kind").get<std::string>());
      if (!kind)
        return Result<SketchConstraintCatalog>::failure(Error::validation(
            value.at("id").get<std::string>(), "unsupported Sketch constraint kind"));
      const std::string source_name = value.at("source").get<std::string>();
      std::optional<SketchConstraintIntentSource> source;
      if (source_name == "manual") source = SketchConstraintIntentSource::Manual;
      if (source_name == "automatic") source = SketchConstraintIntentSource::Automatic;
      if (!source)
        return Result<SketchConstraintCatalog>::failure(Error::validation(
            value.at("id").get<std::string>(), "unsupported Sketch constraint source"));

      std::vector<SketchConstraintIntentTarget> targets;
      for (const auto& target : value.at("targets")) {
        if (!target.is_object() || !target.contains("kind") ||
            !target.at("kind").is_string() || !target.contains("id") ||
            !target.at("id").is_string())
          return Result<SketchConstraintCatalog>::failure(Error::validation(
              value.at("id").get<std::string>(), "Sketch constraint target has invalid fields"));
        const std::string target_kind = target.at("kind").get<std::string>();
        Result<SketchConstraintIntentTarget> parsed =
            target_kind == "point"
                ? SketchConstraintIntentTarget::point(
                      SketchPointId(target.at("id").get<std::string>()))
                : target_kind == "entity"
                      ? SketchConstraintIntentTarget::entity(
                            target.at("id").get<std::string>())
                      : Result<SketchConstraintIntentTarget>::failure(Error::validation(
                            value.at("id").get<std::string>(),
                            "unsupported Sketch constraint target kind"));
        if (parsed.has_error())
          return Result<SketchConstraintCatalog>::failure(parsed.error());
        targets.push_back(std::move(parsed.value()));
      }

      auto constraint = SketchConstraintIntent::create(
          SketchConstraintId(value.at("id").get<std::string>()), *kind,
          std::move(targets), *source);
      if (constraint.has_error())
        return Result<SketchConstraintCatalog>::failure(constraint.error());
      constraints.push_back(std::move(constraint.value()));
    }

    return SketchConstraintCatalog::create(
        DocumentId(root.at("document").get<std::string>()),
        SketchId(root.at("sketch").get<std::string>()), std::move(constraints));
  } catch (const std::exception& exception) {
    return Result<SketchConstraintCatalog>::failure(
        Error::validation("sketch_constraint_json", exception.what()));
  }
}

} // namespace blcad
