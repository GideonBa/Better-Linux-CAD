#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_offset_project.hpp"

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
