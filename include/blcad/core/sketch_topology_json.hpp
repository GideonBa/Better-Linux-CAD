#pragma once

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_topology.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

struct SketchTopologyJsonMigrationResult {
  SketchTopology topology;
  SketchTopologyMigrationReport report;
};

[[nodiscard]] inline Result<std::string>
serialize_sketch_topology_to_json(const SketchTopology& topology) {
  using json = nlohmann::json;
  json root{{"schema", "blcad.sketch_topology.mvp8"},
            {"version", 1},
            {"sketch", topology.sketch().value()},
            {"points", json::array()},
            {"entities", json::array()},
            {"dependencies", json::array()}};

  for (const auto& point : topology.points())
    root["points"].push_back(
        {{"id", point.id().value()},
         {"position", {{"x", point.position().x}, {"y", point.position().y}}},
         {"construction", point.construction()},
         {"reference", point.reference()}});

  for (const auto& entity : topology.entities()) {
    json points = json::array();
    for (const auto& point : entity.points()) points.push_back(point.value());
    json dependencies = json::array();
    for (const auto& dependency : entity.entity_dependencies())
      dependencies.push_back(dependency);
    root["entities"].push_back(
        {{"id", entity.id()},
         {"kind", std::string(to_string(entity.kind()))},
         {"points", std::move(points)},
         {"entity_dependencies", std::move(dependencies)},
         {"construction", entity.construction()},
         {"reference", entity.reference()}});
  }

  for (const auto& dependency : topology.dependencies())
    root["dependencies"].push_back({{"consumer", dependency.consumer_id},
                                     {"source_entity", dependency.source_entity_id},
                                     {"role", dependency.role}});

  return Result<std::string>::success(root.dump(2));
}

[[nodiscard]] inline Result<SketchTopology>
deserialize_sketch_topology_from_json(std::string_view content) {
  using json = nlohmann::json;
  try {
    const json root = json::parse(content);
    if (!root.is_object() || root.value("schema", std::string{}) != "blcad.sketch_topology.mvp8" ||
        root.value("version", 0) != 1 || !root.contains("sketch") ||
        !root.at("sketch").is_string() || !root.contains("points") ||
        !root.at("points").is_array() || !root.contains("entities") ||
        !root.at("entities").is_array() || !root.contains("dependencies") ||
        !root.at("dependencies").is_array())
      return Result<SketchTopology>::failure(Error::validation(
          "sketch_topology_json", "unsupported or malformed Sketch topology JSON schema"));

    std::vector<SketchTopologyPoint> points;
    for (const auto& value : root.at("points")) {
      if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
          !value.contains("position") || !value.at("position").is_object() ||
          !value.at("position").contains("x") || !value.at("position").contains("y") ||
          !value.contains("construction") || !value.at("construction").is_boolean() ||
          !value.contains("reference") || !value.at("reference").is_boolean())
        return Result<SketchTopology>::failure(Error::validation(
            "sketch_topology_json", "malformed Sketch topology point record"));
      auto point = SketchTopologyPoint::create(
          SketchPointId(value.at("id").get<std::string>()),
          Point2{value.at("position").at("x").get<double>(),
                 value.at("position").at("y").get<double>()},
          {.construction = value.at("construction").get<bool>(),
           .reference = value.at("reference").get<bool>()});
      if (point.has_error()) return Result<SketchTopology>::failure(point.error());
      points.push_back(std::move(point.value()));
    }

    std::vector<SketchTopologyEntity> entities;
    for (const auto& value : root.at("entities")) {
      if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
          !value.contains("kind") || !value.at("kind").is_string() ||
          !value.contains("points") || !value.at("points").is_array() ||
          !value.contains("entity_dependencies") || !value.at("entity_dependencies").is_array() ||
          !value.contains("construction") || !value.at("construction").is_boolean() ||
          !value.contains("reference") || !value.at("reference").is_boolean())
        return Result<SketchTopology>::failure(Error::validation(
            "sketch_topology_json", "malformed Sketch topology entity record"));
      const auto kind = sketch_topology_entity_kind_from_string(
          value.at("kind").get<std::string>());
      if (!kind)
        return Result<SketchTopology>::failure(Error::validation(
            value.at("id").get<std::string>(), "unsupported Sketch topology entity kind"));
      std::vector<SketchPointId> point_ids;
      for (const auto& point : value.at("points")) {
        if (!point.is_string())
          return Result<SketchTopology>::failure(Error::validation(
              value.at("id").get<std::string>(), "Sketch topology point id must be a string"));
        point_ids.emplace_back(point.get<std::string>());
      }
      std::vector<std::string> entity_dependencies;
      for (const auto& dependency : value.at("entity_dependencies")) {
        if (!dependency.is_string())
          return Result<SketchTopology>::failure(Error::validation(
              value.at("id").get<std::string>(), "Sketch topology dependency must be a string"));
        entity_dependencies.push_back(dependency.get<std::string>());
      }
      auto entity = SketchTopologyEntity::create(
          value.at("id").get<std::string>(), *kind, std::move(point_ids),
          std::move(entity_dependencies),
          {.construction = value.at("construction").get<bool>(),
           .reference = value.at("reference").get<bool>()});
      if (entity.has_error()) return Result<SketchTopology>::failure(entity.error());
      entities.push_back(std::move(entity.value()));
    }

    std::vector<SketchTopologyDependency> dependencies;
    for (const auto& value : root.at("dependencies")) {
      if (!value.is_object() || !value.contains("consumer") ||
          !value.at("consumer").is_string() || !value.contains("source_entity") ||
          !value.at("source_entity").is_string() || !value.contains("role") ||
          !value.at("role").is_string())
        return Result<SketchTopology>::failure(Error::validation(
            "sketch_topology_json", "malformed Sketch topology dependency record"));
      dependencies.push_back({value.at("consumer").get<std::string>(),
                              value.at("source_entity").get<std::string>(),
                              value.at("role").get<std::string>()});
    }

    return SketchTopology::create(SketchId(root.at("sketch").get<std::string>()),
                                  std::move(points), std::move(entities),
                                  std::move(dependencies));
  } catch (const std::exception& exception) {
    return Result<SketchTopology>::failure(Error::validation(
        "sketch_topology_json", std::string("failed to parse Sketch topology JSON: ") +
                                    exception.what()));
  }
}

[[nodiscard]] inline Result<SketchTopologyJsonMigrationResult>
migrate_legacy_part_document_sketch_json(std::string_view legacy_part_document_json,
                                         SketchId sketch_id) {
  auto document = deserialize_part_document_from_json(legacy_part_document_json);
  if (document.has_error())
    return Result<SketchTopologyJsonMigrationResult>::failure(document.error());
  const Sketch* sketch = document.value().find_sketch(sketch_id);
  if (sketch == nullptr)
    return Result<SketchTopologyJsonMigrationResult>::failure(Error::validation(
        sketch_id.empty() ? "sketch_topology_json" : sketch_id.value(),
        "legacy Part document does not contain the requested Sketch"));
  SketchTopologyMigrationReport report;
  auto topology = SketchTopology::migrate_legacy(*sketch, &report);
  if (topology.has_error())
    return Result<SketchTopologyJsonMigrationResult>::failure(topology.error());
  return Result<SketchTopologyJsonMigrationResult>::success(
      {std::move(topology.value()), std::move(report)});
}

} // namespace blcad
