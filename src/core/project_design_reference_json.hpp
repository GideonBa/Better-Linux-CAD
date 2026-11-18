#pragma once
#include "blcad/core/project.hpp"
#include "blcad/core/project_design_reference.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <set>
#include <type_traits>

namespace blcad::detail {
inline nlohmann::json design_occurrence_json(const ProjectDesignOccurrence& occurrence) {
  nlohmann::json path = nlohmann::json::array();
  for (const auto& id : occurrence.path)
    path.push_back(id.value());
  return {{"path", std::move(path)},
          {"component", occurrence.component.value()},
          {"part", occurrence.part.value()}};
}
inline nlohmann::json design_references_to_json(const Project& project) {
  using J = nlohmann::json;
  J result = J::array();
  for (const auto& ref : project.design_references()) {
    J geometry = std::visit(
        [](const auto& value) -> J {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, ProjectDesignSemanticSource>)
            return {{"kind", "semantic"}, {"reference", value.reference}};
          else
            return {{"kind", "path"}, {"path", value.path.value()}};
        },
        ref.source_geometry());
    J target = std::visit(
        [](const auto& value) -> J {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, ConstructionPointId>)
            return {{"kind", "point"}, {"id", value.value()}};
          else if constexpr (std::is_same_v<T, ConstructionLineId>)
            return {{"kind", "line"}, {"id", value.value()}};
          else if constexpr (std::is_same_v<T, ConstructionPlaneId>)
            return {{"kind", "plane"}, {"id", value.value()}};
          else
            return {{"kind", "path"}, {"id", value.path.value()}, {"sketch", value.sketch.value()}};
        },
        ref.target());
    result.push_back({{"version", 1},
                      {"id", ref.id().value()},
                      {"name", ref.name()},
                      {"source", design_occurrence_json(ref.source())},
                      {"source_state", ref.source_state().value()},
                      {"geometry", std::move(geometry)},
                      {"driver", design_occurrence_json(ref.driver())},
                      {"target", std::move(target)}});
  }
  return result;
}
inline Result<std::size_t> load_design_references(const nlohmann::json& values, Project& project) {
  using J = nlohmann::json;
  auto fail = [] {
    return Result<std::size_t>::failure(Error::validation(
        "project.design_references", "Invalid strict directed design reference record"));
  };
  auto exact = [](const J& value, const std::set<std::string>& keys) {
    if (!value.is_object() || value.size() != keys.size())
      return false;
    for (const auto& key : keys)
      if (!value.contains(key))
        return false;
    return true;
  };
  auto occurrence_valid = [&](const J& value) {
    return exact(value, {"path", "component", "part"}) && value.at("path").is_array() &&
           value.at("component").is_string() && value.at("part").is_string() &&
           std::all_of(value.at("path").begin(), value.at("path").end(),
                       [](const J& id) { return id.is_string(); });
  };
  auto occurrence = [](const J& value) {
    std::vector<SubassemblyInstanceId> path;
    for (const auto& id : value.at("path"))
      path.emplace_back(id.get<std::string>());
    return ProjectDesignOccurrence{std::move(path),
                                   ComponentInstanceId(value.at("component").get<std::string>()),
                                   DocumentId(value.at("part").get<std::string>())};
  };
  if (!values.is_array() || values.empty())
    return fail();
  for (const auto& value : values) {
    if (!exact(value, {"version", "id", "name", "source", "source_state", "geometry", "driver",
                       "target"}) ||
        !value.at("version").is_number_integer() || value.at("version") != 1 ||
        !occurrence_valid(value.at("source")) || !occurrence_valid(value.at("driver")))
      return fail();
    const auto& geometry = value.at("geometry");
    const auto& target = value.at("target");
    if (!geometry.is_object() || !geometry.contains("kind") || !target.is_object() ||
        !target.contains("kind"))
      return fail();
    ProjectDesignSource source = ProjectDesignSemanticSource{};
    if (geometry.at("kind") == "semantic" && exact(geometry, {"kind", "reference"}))
      source = ProjectDesignSemanticSource{geometry.at("reference").get<std::string>()};
    else if (geometry.at("kind") == "path" && exact(geometry, {"kind", "path"}))
      source = ProjectDesignPathSource{PathCurveId(geometry.at("path").get<std::string>())};
    else
      return fail();
    ProjectDesignTarget destination = ConstructionPointId{};
    if (target.at("kind") == "point" && exact(target, {"kind", "id"}))
      destination = ConstructionPointId(target.at("id").get<std::string>());
    else if (target.at("kind") == "line" && exact(target, {"kind", "id"}))
      destination = ConstructionLineId(target.at("id").get<std::string>());
    else if (target.at("kind") == "plane" && exact(target, {"kind", "id"}))
      destination = ConstructionPlaneId(target.at("id").get<std::string>());
    else if (target.at("kind") == "path" && exact(target, {"kind", "id", "sketch"}))
      destination = ProjectDerivedPathTarget{PathCurveId(target.at("id").get<std::string>()),
                                             Sketch3DId(target.at("sketch").get<std::string>())};
    else
      return fail();
    const ProjectDesignReferenceId id(value.at("id").get<std::string>());
    if (project.find_design_reference(id))
      return fail();
    auto ref = ProjectDesignReference::create(
        id, value.at("name").get<std::string>(), occurrence(value.at("source")),
        ModelStateId(value.at("source_state").get<std::string>()), std::move(source),
        occurrence(value.at("driver")), std::move(destination));
    if (!ref)
      return Result<std::size_t>::failure(ref.error());
    auto added = project.set_design_reference(std::move(ref.value()));
    if (!added)
      return added;
  }
  return Result<std::size_t>::success(values.size());
}
} // namespace blcad::detail
