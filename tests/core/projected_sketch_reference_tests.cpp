#include "blcad/core/sketch.hpp"

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
