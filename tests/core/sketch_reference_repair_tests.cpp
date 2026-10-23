#include "../support/reference_repair_fixture.hpp"
#include "blcad/core/part_document_json.hpp"

using namespace blcad;
using blcad::test::reference_checked;
namespace {
std::string snapshot(const PartDocument& part) {
  return reference_checked(serialize_part_document_to_json(part));
}
} // namespace
TEST_CASE("Projected reference replacement preserves identity constraints and dependency consumers",
          "[core][sketch-reference-repair]") {
  auto part = test::reference_repair_fixture(false, false, true);
  auto sketch = *part.find_sketch(SketchId("sketch.refs"));
  REQUIRE(sketch.add_reference(
      reference_checked(ReferenceGeneratedLine::create_from_projected_point_constraints(
          SketchEntityId("generated"), SketchConstraintId("generated.start"),
          SketchConstraintId("generated.end")))));
  REQUIRE(sketch.add_constraint(
      reference_checked(SketchConstraint::create_coincident_to_projected_point(
          SketchConstraintId("generated.start"),
          reference_checked(
              SketchReferenceTarget::create_line_segment_start(SketchEntityId("generated"))),
          reference_checked(
              SketchReferenceTarget::create_projected_point(SketchEntityId("point")))))));
  REQUIRE(sketch.add_constraint(
      reference_checked(SketchConstraint::create_coincident_to_projected_point(
          SketchConstraintId("generated.end"),
          reference_checked(
              SketchReferenceTarget::create_line_segment_end(SketchEntityId("generated"))),
          reference_checked(
              SketchReferenceTarget::create_projected_point(SketchEntityId("point")))))));
  REQUIRE(
      sketch.replace_reference(reference_checked(ProjectedSketchPoint::create_from_semantic_vertex(
          SketchEntityId("point"),
          reference_checked(SemanticVertexReference::create(FeatureId("feature.new"),
                                                            SemanticVertex::BottomBackLeft))))));
  REQUIRE(sketch.replace_reference(reference_checked(ProjectedSketchLine::create_from_semantic_edge(
      SketchEntityId("edge"), reference_checked(SemanticEdgeReference::create(
                                  FeatureId("feature.new"), SemanticEdge::BottomBack))))));
  REQUIRE(part.update_sketch(sketch));
  CHECK(part.sketches().back().id() == SketchId("sketch.refs"));
  CHECK(sketch.constraints().size() == 4);
  CHECK(sketch.reference_generated_lines().front().start_constraint() ==
        SketchConstraintId("generated.start"));
  CHECK(sketch.reference_generated_lines().front().end_constraint() ==
        SketchConstraintId("generated.end"));
  CHECK(sketch.find_constraint(SketchConstraintId("coincident"))->reference_target().entity() ==
        SketchEntityId("point"));
  CHECK(sketch.find_constraint(SketchConstraintId("parallel"))->reference_target().entity() ==
        SketchEntityId("edge"));
  const auto& graph = part.dependency_graph();
  CHECK(graph.has_dependency("feature.new.vertex.bottom_back_left", "sketch.refs"));
  CHECK(graph.has_dependency("feature.new.edge.bottom_back", "sketch.refs"));
  CHECK_FALSE(graph.has_dependency("feature.old.vertex.top_front_right", "sketch.refs"));
  CHECK_FALSE(graph.has_dependency("feature.old.edge.top_front", "sketch.refs"));
  CHECK(graph.has_dependency("sketch.refs", "feature.consumer"));
  CHECK(snapshot(reference_checked(deserialize_part_document_from_json(snapshot(part)))) ==
        snapshot(part));
}
TEST_CASE(
    "Projected replacement rejects absent wrong-kind missing-source and cyclic targets atomically",
    "[core][sketch-reference-repair]") {
  auto part = test::reference_repair_fixture(false, false, true);
  auto sketch = *part.find_sketch(SketchId("sketch.refs"));
  const auto before = snapshot(part);
  for (const std::string id : {"absent", "line", "edge"}) {
    CAPTURE(id);
    CHECK_FALSE(sketch.replace_reference(
        reference_checked(ProjectedSketchPoint::create_from_construction_point(
            SketchEntityId(id), ConstructionPointId("point.fallback")))));
  }
  CHECK_FALSE(
      sketch.replace_reference(reference_checked(ProjectedSketchLine::create_from_construction_line(
          SketchEntityId("point"), ConstructionLineId("line.fallback")))));
  for (const std::string source : {"feature.absent", "feature.consumer"}) {
    auto candidate = sketch;
    REQUIRE(candidate.replace_reference(
        reference_checked(ProjectedSketchPoint::create_from_semantic_vertex(
            SketchEntityId("point"), reference_checked(SemanticVertexReference::create(
                                         FeatureId(source), SemanticVertex::TopFrontRight))))));
    auto changed = part.update_sketch(candidate);
    REQUIRE_FALSE(changed);
    CHECK(changed.error().category() ==
          (source == "feature.consumer" ? ErrorCategory::Dependency : ErrorCategory::Validation));
    CHECK_FALSE(part.dependency_graph().has_cycle());
    CHECK(snapshot(part) == before);
  }
}
TEST_CASE(
    "Projected geometry can explicitly fall back to construction intent without losing consumers",
    "[core][sketch-reference-repair]") {
  auto part = test::reference_repair_fixture();
  auto sketch = *part.find_sketch(SketchId("sketch.refs"));
  REQUIRE(sketch.replace_reference(
      reference_checked(ProjectedSketchPoint::create_from_construction_point(
          SketchEntityId("point"), ConstructionPointId("point.fallback")))));
  REQUIRE(
      sketch.replace_reference(reference_checked(ProjectedSketchLine::create_from_construction_line(
          SketchEntityId("edge"), ConstructionLineId("line.fallback")))));
  REQUIRE(part.update_sketch(sketch));
  CHECK(part.dependency_graph().has_dependency("point.fallback", "sketch.refs"));
  CHECK(part.dependency_graph().has_dependency("line.fallback", "sketch.refs"));
  const auto restored = reference_checked(deserialize_part_document_from_json(snapshot(part)));
  const auto* result = restored.find_sketch(sketch.id());
  REQUIRE(result);
  CHECK(result->projected_points().front().source() ==
        ProjectedSketchPointSource::ConstructionPoint);
  CHECK(result->projected_lines().front().source() == ProjectedSketchLineSource::ConstructionLine);
  CHECK(result->constraints().size() == 2);
}
