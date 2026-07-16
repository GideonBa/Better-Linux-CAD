#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_offset_project.hpp"
#include "blcad/core/sketch_transform_pattern.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("Projected sketch references store construction and semantic sources", "[core][sketch]") {
  auto vertex =
      SemanticVertexReference::create(FeatureId("feature.base"), SemanticVertex::TopFrontRight);
  REQUIRE(vertex);
  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);

  auto point_from_construction = ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("ref.point.mid"), ConstructionPointId("point.mid"));
  REQUIRE(point_from_construction);
  CHECK(point_from_construction.value().id().value() == "ref.point.mid");
  CHECK(point_from_construction.value().source() == ProjectedSketchPointSource::ConstructionPoint);
  CHECK(point_from_construction.value().construction_point().value() == "point.mid");
  CHECK(point_from_construction.value().referenced_node_id() == "point.mid");

  auto point_from_vertex = ProjectedSketchPoint::create_from_semantic_vertex(
      SketchEntityId("ref.vertex.top_front_right"), vertex.value());
  REQUIRE(point_from_vertex);
  CHECK(point_from_vertex.value().source() == ProjectedSketchPointSource::SemanticVertex);
  REQUIRE(point_from_vertex.value().semantic_vertex().has_value());
  CHECK(point_from_vertex.value().referenced_node_id() == "feature.base.vertex.top_front_right");

  auto line_from_construction = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("ref.line.axis"), ConstructionLineId("line.axis"));
  REQUIRE(line_from_construction);
  CHECK(line_from_construction.value().source() == ProjectedSketchLineSource::ConstructionLine);
  CHECK(line_from_construction.value().construction_line().value() == "line.axis");
  CHECK(line_from_construction.value().referenced_node_id() == "line.axis");

  auto line_from_edge = ProjectedSketchLine::create_from_semantic_edge(
      SketchEntityId("ref.edge.top_front"), edge.value());
  REQUIRE(line_from_edge);
  CHECK(line_from_edge.value().source() == ProjectedSketchLineSource::SemanticEdge);
  REQUIRE(line_from_edge.value().semantic_edge().has_value());
  CHECK(line_from_edge.value().referenced_node_id() == "feature.base.edge.top_front");
}

TEST_CASE("Sketch stores projected reference entities with unique sketch ids", "[core][sketch]") {
  auto sketch = Sketch::create(SketchId("sketch.top"), "Sketch_Top", DatumPlaneId("wp.top"));
  REQUIRE(sketch);

  auto point = ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("ref.shared"), ConstructionPointId("point.mid"));
  REQUIRE(point);
  REQUIRE(sketch.value().add_reference(point.value()));
  CHECK(sketch.value().projected_points().size() == 1);
  CHECK(sketch.value().find_projected_point(SketchEntityId("ref.shared")) != nullptr);

  auto duplicate_line = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("ref.shared"), ConstructionLineId("line.axis"));
  REQUIRE(duplicate_line);
  const auto added = sketch.value().add_reference(duplicate_line.value());
  REQUIRE(added.has_error());
  CHECK(added.error().message() == "sketch reference id must be unique within sketch");
}

namespace {
Sketch make_offset_project_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.offset-project"), "Offset Project",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  return std::move(sketch.value());
}

void add_offset_project_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId(id), start, end).value()));
}
} // namespace

TEST_CASE("Block 117 offsets an ordered line chain with deterministic miter joints",
          "[core][sketch-offset-project]") {
  auto sketch = make_offset_project_sketch();
  add_offset_project_line(sketch, "line.a", {0.0, 0.0}, {10.0, 0.0});
  add_offset_project_line(sketch, "line.b", {10.0, 0.0}, {10.0, 8.0});

  auto result = SketchOffsetProjectService::offset_lines(
      sketch, {SketchEntityId("line.a"), SketchEntityId("line.b")}, 2.0);
  REQUIRE(result);
  REQUIRE(result.value().created_entities.size() == 2U);
  const auto* first = result.value().sketch.find_line_segment(result.value().created_entities[0]);
  const auto* second = result.value().sketch.find_line_segment(result.value().created_entities[1]);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first->start().x == Catch::Approx(0.0));
  CHECK(first->start().y == Catch::Approx(2.0));
  CHECK(first->end().x == Catch::Approx(8.0));
  CHECK(first->end().y == Catch::Approx(2.0));
  CHECK(second->start().x == Catch::Approx(8.0));
  CHECK(second->start().y == Catch::Approx(2.0));
  CHECK(second->end().x == Catch::Approx(8.0));
  CHECK(second->end().y == Catch::Approx(8.0));
}

TEST_CASE("Block 117 rejects disconnected and open-loop offset selections",
          "[core][sketch-offset-project]") {
  auto sketch = make_offset_project_sketch();
  add_offset_project_line(sketch, "line.a", {0.0, 0.0}, {4.0, 0.0});
  add_offset_project_line(sketch, "line.b", {8.0, 0.0}, {8.0, 4.0});
  CHECK_FALSE(SketchOffsetProjectService::offset_lines(
      sketch, {SketchEntityId("line.a"), SketchEntityId("line.b")}, 1.0));

  auto connected = make_offset_project_sketch();
  add_offset_project_line(connected, "line.a", {0.0, 0.0}, {4.0, 0.0});
  add_offset_project_line(connected, "line.b", {4.0, 0.0}, {4.0, 4.0});
  CHECK_FALSE(SketchOffsetProjectService::offset_lines(
      connected, {SketchEntityId("line.a"), SketchEntityId("line.b")}, 1.0, true));
}

TEST_CASE("Block 117 keeps construction projection associative until explicit break-link",
          "[core][sketch-offset-project]") {
  auto sketch = make_offset_project_sketch();
  auto projected = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("axis.projected"), ConstructionLineId("axis.x"));
  REQUIRE(projected);

  auto included = SketchOffsetProjectService::project_line(sketch, projected.value());
  REQUIRE(included);
  REQUIRE(included.value().sketch.find_projected_line(SketchEntityId("axis.projected")) != nullptr);
  CHECK(included.value().sketch.find_line_segment(SketchEntityId("axis.projected")) == nullptr);

  auto detached = SketchOffsetProjectService::break_link_line(
      included.value().sketch, SketchEntityId("axis.projected"), {-5.0, 0.0}, {5.0, 0.0});
  REQUIRE(detached);
  CHECK(detached.value().sketch.find_projected_line(SketchEntityId("axis.projected")) == nullptr);
  const auto* line = detached.value().sketch.find_line_segment(SketchEntityId("axis.projected"));
  REQUIRE(line != nullptr);
  CHECK(line->start().x == Catch::Approx(-5.0));
  CHECK(line->end().x == Catch::Approx(5.0));
}

TEST_CASE("Block 118 transforms preserve ids and copy mirror create ordinary curves",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_offset_project_sketch();
  add_offset_project_line(sketch, "line.transform", {1.0, 1.0}, {3.0, 1.0});
  auto moved = SketchTransformPatternService::move(
      sketch, {SketchEntityId("line.transform")}, {2.0, 3.0});
  REQUIRE(moved);
  const auto* moved_line = moved.value().sketch.find_line_segment(SketchEntityId("line.transform"));
  REQUIRE(moved_line != nullptr);
  CHECK(moved_line->start().x == Catch::Approx(3.0));
  CHECK(moved_line->start().y == Catch::Approx(4.0));

  auto copied = SketchTransformPatternService::copy(
      sketch, {SketchEntityId("line.transform")}, {0.0, 5.0});
  REQUIRE(copied);
  CHECK(copied.value().created_entities.front().value() == "line.transform.copy.1");
  auto mirrored = SketchTransformPatternService::mirror(
      sketch, {SketchEntityId("line.transform")}, {0.0, 0.0}, {0.0, 1.0});
  REQUIRE(mirrored);
  const auto* mirrored_line = mirrored.value().sketch.find_line_segment(
      mirrored.value().created_entities.front());
  REQUIRE(mirrored_line != nullptr);
  CHECK(mirrored_line->start().x == Catch::Approx(-1.0));
}

TEST_CASE("Block 118 rectangular and circular patterns expose explicit associative intent",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_offset_project_sketch();
  add_offset_project_line(sketch, "line.seed", {1.0, 0.0}, {2.0, 0.0});
  auto rectangular = SketchTransformPatternService::rectangular_pattern(
      sketch, {SketchEntityId("line.seed")}, {4.0, 0.0}, 3U, {0.0, 5.0}, 2U,
      SketchPatternMode::Associative, "pattern.grid");
  REQUIRE(rectangular);
  CHECK(rectangular.value().created_entities.size() == 5U);
  REQUIRE(rectangular.value().pattern_intent.has_value());
  CHECK(rectangular.value().pattern_intent->id == "pattern.grid");

  auto circular = SketchTransformPatternService::circular_pattern(
      sketch, {SketchEntityId("line.seed")}, {0.0, 0.0}, 4U,
      3.14159265358979323846 * 1.5, SketchPatternMode::Exploded);
  REQUIRE(circular);
  CHECK(circular.value().created_entities.size() == 3U);
  CHECK_FALSE(circular.value().pattern_intent.has_value());
}

TEST_CASE("Block 118 rejects empty selections and degenerate transform parameters",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_offset_project_sketch();
  add_offset_project_line(sketch, "line.a", {0.0, 0.0}, {1.0, 0.0});
  CHECK_FALSE(SketchTransformPatternService::move(sketch, {}, {1.0, 0.0}));
  CHECK_FALSE(SketchTransformPatternService::scale(
      sketch, {SketchEntityId("line.a")}, {0.0, 0.0}, 0.0));
  CHECK_FALSE(SketchTransformPatternService::mirror(
      sketch, {SketchEntityId("line.a")}, {0.0, 0.0}, {0.0, 0.0}));
}

TEST_CASE("Block 118 move preserves referencing constraints and dimensions in place",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_offset_project_sketch();
  add_offset_project_line(sketch, "line.a", {0.0, 0.0}, {10.0, 0.0});
  REQUIRE(sketch.add_constraint(
      SketchGeometricConstraint::create_horizontal(
          SketchConstraintId("horizontal.a"),
          SketchReferenceTarget::create_line_segment(SketchEntityId("line.a")).value())
          .value()));
  REQUIRE(sketch.add_dimension(
      SketchDrivingDimension::create_point_to_point_distance(
          SketchDimensionId("length.a"),
          SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.a")).value(),
          SketchReferenceTarget::create_line_segment_end(SketchEntityId("line.a")).value(),
          ParameterId("length"))
          .value()));

  auto moved = SketchTransformPatternService::move(sketch, {SketchEntityId("line.a")}, {5.0, 5.0});
  REQUIRE(moved);
  CHECK(moved.value().sketch.geometric_constraints().size() == 1U);
  CHECK(moved.value().sketch.driving_dimensions().size() == 1U);
  CHECK(moved.value().uncopied_references.empty());
  const auto* line = moved.value().sketch.find_line_segment(SketchEntityId("line.a"));
  REQUIRE(line != nullptr);
  CHECK(line->start() == Point2{5.0, 5.0});
  CHECK(line->end() == Point2{15.0, 5.0});
}

TEST_CASE("Block 118 copy and patterns report constraints that are not duplicated",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_offset_project_sketch();
  add_offset_project_line(sketch, "line.a", {0.0, 0.0}, {10.0, 0.0});
  REQUIRE(sketch.add_constraint(
      SketchGeometricConstraint::create_horizontal(
          SketchConstraintId("horizontal.a"),
          SketchReferenceTarget::create_line_segment(SketchEntityId("line.a")).value())
          .value()));

  auto copied = SketchTransformPatternService::copy(sketch, {SketchEntityId("line.a")}, {0.0, 4.0});
  REQUIRE(copied);
  REQUIRE(copied.value().uncopied_references.size() == 1U);
  CHECK(copied.value().uncopied_references.front() == "horizontal.a");

  auto pattern = SketchTransformPatternService::rectangular_pattern(
      sketch, {SketchEntityId("line.a")}, {0.0, 5.0}, 1U, {0.0, 5.0}, 3U,
      SketchPatternMode::Exploded);
  REQUIRE(pattern);
  CHECK(pattern.value().uncopied_references ==
        std::vector<std::string>{"horizontal.a"});
}
