#include "../support/missing_reference_fixture.hpp"
#include "../support/reference_repair_fixture.hpp"
#include "blcad/core/part_document_json.hpp"

using namespace blcad;
using blcad::test::reference_checked;
namespace {
std::string snapshot(const PartDocument& part) {
  return reference_checked(serialize_part_document_to_json(part));
}
} // namespace
TEST_CASE("Explicit recovery retains missing typed sources and preserves canonical authored intent",
          "[core][missing-reference-recovery]") {
  const auto input = test::missing_reference_fixture_json();
  REQUIRE_FALSE(deserialize_part_document_from_json(input));
  auto part = reference_checked(
      deserialize_part_document_from_json(input, PartDocumentReadMode::ReferenceRepair));
  CHECK(snapshot(part) == input);
  CHECK_FALSE(part.validate_reference_sources());
  CHECK_FALSE(part.find_feature(FeatureId("feature.old")));
  CHECK_FALSE(part.dependency_graph().has_node("feature.old"));
  CHECK_FALSE(part.dependency_graph().has_node("point.fallback"));
  CHECK_FALSE(part.dependency_graph().has_node("line.fallback"));
  REQUIRE(part.find_sketch(SketchId("sketch.refs")));
  CHECK(part.find_sketch(SketchId("sketch.refs"))->constraints().size() == 4);
  CHECK(part.find_sketch(SketchId("sketch.refs"))->reference_generated_lines().size() == 1);
  CHECK(snapshot(reference_checked(deserialize_part_document_from_json(
            snapshot(part), PartDocumentReadMode::ReferenceRepair))) == input);
  REQUIRE(test::repair_missing_projected(part));
  CHECK_FALSE(part.validate_reference_sources());
  REQUIRE(test::repair_open_planes(part));
  CHECK(part.validate_reference_sources());
  CHECK(snapshot(reference_checked(deserialize_part_document_from_json(snapshot(part)))) ==
        snapshot(part));
  CHECK(part.dependency_graph().has_dependency("feature.new", "plane"));
  CHECK(
      part.dependency_graph().has_dependency("feature.new.vertex.top_front_right", "sketch.refs"));
  CHECK(part.dependency_graph().has_dependency("feature.new.vertex.bottom_back_left",
                                               "sketch.refs.reference_generated_line.generated"));
  CHECK(part.sketches().back().id() == SketchId("face.two"));
}

TEST_CASE("Intermediate recovery updates cannot introduce new missing targets or change lost roles",
          "[core][missing-reference-recovery]") {
  auto part = reference_checked(deserialize_part_document_from_json(
      test::missing_reference_fixture_json(), PartDocumentReadMode::ReferenceRepair));
  const auto before = snapshot(part);
  for (const std::string source : {"feature.absent", "feature.old"}) {
    auto sketch = *part.find_sketch(SketchId("sketch.refs"));
    REQUIRE(sketch.replace_reference(
        reference_checked(ProjectedSketchPoint::create_from_semantic_vertex(
            SketchEntityId("point"), reference_checked(SemanticVertexReference::create(
                                         FeatureId(source), SemanticVertex::BottomBackLeft))))));
    CHECK_FALSE(part.update_sketch(sketch));
    CHECK(snapshot(part) == before);
  }
  auto sketch = *part.find_sketch(SketchId("sketch.refs"));
  REQUIRE(
      sketch.add_reference(reference_checked(ProjectedSketchPoint::create_from_construction_point(
          SketchEntityId("new.missing"), ConstructionPointId("point.fallback")))));
  CHECK_FALSE(part.update_sketch(sketch));
  CHECK(snapshot(part) == before);
  // Once a consumer is repaired, its old missing source cannot be reintroduced.
  REQUIRE(test::repair_missing_projected(part));
  const auto repaired = snapshot(part);
  sketch = *part.find_sketch(SketchId("sketch.refs"));
  REQUIRE(
      sketch.replace_reference(reference_checked(ProjectedSketchLine::create_from_construction_line(
          SketchEntityId("edge"), ConstructionLineId("line.fallback")))));
  CHECK_FALSE(part.update_sketch(sketch));
  CHECK(snapshot(part) == repaired);
}

TEST_CASE("Recovery rejects unrelated invalid intent wrong-kind sources and cycles",
          "[core][missing-reference-recovery]") {
  const auto input = nlohmann::json::parse(test::missing_reference_fixture_json());
  for (const std::string invalid :
       {"parameter", "workplane", "wrong-kind", "cycle", "role", "duplicate"}) {
    CAPTURE(invalid);
    auto root = input;
    if (invalid == "parameter")
      root["parameters"].erase(root["parameters"].begin());
    else if (invalid == "workplane")
      root["sketches"][0]["workplane"] = "plane.absent";
    else if (invalid == "wrong-kind")
      root["derived_workplanes"][0]["source_feature"] = "width";
    else if (invalid == "cycle")
      root["derived_workplanes"][0]["source_feature"] = "face.solid.one";
    else if (invalid == "role")
      root["derived_workplanes"][0]["face"] = "unsupported";
    else
      root["derived_workplanes"].push_back(root["derived_workplanes"][0]);
    CHECK_FALSE(
        deserialize_part_document_from_json(root.dump(), PartDocumentReadMode::ReferenceRepair));
  }
}
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
