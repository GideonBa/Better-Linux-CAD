#pragma once
#include "blcad/core/part_document_json.hpp"
#include "repair_open_fixture.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace blcad::test {
// Six missing inputs: two derived faces, two semantic projections and two
// construction projections, including constraints and a generated-line consumer.
inline std::string missing_reference_fixture_json() {
  auto part = repair_open_fixture();
  auto sketch = *part.find_sketch(SketchId("sketch.refs"));
  REQUIRE(
      sketch.add_reference(reference_checked(ProjectedSketchPoint::create_from_construction_point(
          SketchEntityId("point.construction"), ConstructionPointId("point.fallback")))));
  REQUIRE(sketch.add_reference(reference_checked(ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("line.construction"), ConstructionLineId("line.fallback")))));
  REQUIRE(sketch.add_reference(
      reference_checked(ReferenceGeneratedLine::create_from_projected_point_constraints(
          SketchEntityId("generated"), SketchConstraintId("generated.start"),
          SketchConstraintId("generated.end")))));
  for (const auto& [suffix, point] : {std::pair{"start", "point"}, {"end", "point.construction"}}) {
    auto target =
        std::string(suffix) == "start"
            ? SketchReferenceTarget::create_line_segment_start(SketchEntityId("generated"))
            : SketchReferenceTarget::create_line_segment_end(SketchEntityId("generated"));
    REQUIRE(sketch.add_constraint(
        reference_checked(SketchConstraint::create_coincident_to_projected_point(
            SketchConstraintId(std::string("generated.") + suffix), reference_checked(target),
            reference_checked(
                SketchReferenceTarget::create_projected_point(SketchEntityId(point)))))));
  }
  REQUIRE(part.update_sketch(sketch));
  auto root = nlohmann::json::parse(reference_checked(serialize_part_document_to_json(part)));
  auto& features = root["features"];
  features.erase(
      std::remove_if(features.begin(), features.end(),
                     [](const auto& record) { return record.at("id") == "feature.old"; }),
      features.end());
  root["construction_points"] = nlohmann::json::array();
  root["construction_lines"] = nlohmann::json::array();
  return root.dump(2);
}

inline Result<std::size_t> repair_missing_projected(PartDocument& part) {
  for (const std::string id : {"point", "point.construction", "edge", "line.construction"}) {
    auto sketch = *part.find_sketch(SketchId("sketch.refs"));
    auto replaced =
        sketch.find_projected_point(SketchEntityId(id))
            ? sketch.replace_reference(
                  reference_checked(ProjectedSketchPoint::create_from_semantic_vertex(
                      SketchEntityId(id), reference_checked(SemanticVertexReference::create(
                                              FeatureId("feature.new"),
                                              id == "point" ? SemanticVertex::TopFrontRight
                                                            : SemanticVertex::BottomBackLeft)))))
            : sketch.replace_reference(
                  reference_checked(ProjectedSketchLine::create_from_semantic_edge(
                      SketchEntityId(id), reference_checked(SemanticEdgeReference::create(
                                              FeatureId("feature.new"), SemanticEdge::TopFront)))));
    if (!replaced)
      return replaced;
    auto changed = part.update_sketch(sketch);
    if (!changed)
      return changed;
  }
  return Result<std::size_t>::success(4);
}
inline Result<std::size_t> repair_all_missing_references(PartDocument& part) {
  auto projected = repair_missing_projected(part);
  if (!projected)
    return projected;
  auto planes = repair_open_planes(part);
  if (!planes)
    return planes;
  return Result<std::size_t>::success(6);
}
} // namespace blcad::test
