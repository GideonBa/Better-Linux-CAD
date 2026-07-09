#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

TEST_CASE("Projected sketch references roundtrip through part document JSON", "[core][json][sketch]") {
  auto document = PartDocument::create(DocumentId("part.projected_refs"), "Projected reference part");
  REQUIRE(document);

  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));

  auto sketch = Sketch::create(SketchId("sketch.top"), "Sketch_Top", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto vertex = SemanticVertexReference::create(FeatureId("feature.base"), SemanticVertex::TopFrontRight);
  REQUIRE(vertex);
  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);

  auto projected_point = ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("ref.point.mid"), ConstructionPointId("point.mid"));
  REQUIRE(projected_point);
  REQUIRE(sketch.value().add_reference(projected_point.value()));

  auto projected_vertex = ProjectedSketchPoint::create_from_semantic_vertex(
      SketchEntityId("ref.vertex.top_front_right"), vertex.value());
  REQUIRE(projected_vertex);
  REQUIRE(sketch.value().add_reference(projected_vertex.value()));

  auto projected_line = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("ref.line.axis"), ConstructionLineId("line.axis"));
  REQUIRE(projected_line);
  REQUIRE(sketch.value().add_reference(projected_line.value()));

  auto projected_edge =
      ProjectedSketchLine::create_from_semantic_edge(SketchEntityId("ref.edge.top_front"), edge.value());
  REQUIRE(projected_edge);
  REQUIRE(sketch.value().add_reference(projected_edge.value()));

  REQUIRE(document.value().add_sketch(sketch.value()));

  auto serialized = serialize_part_document_to_json(document.value());
  REQUIRE(serialized);
  CHECK(serialized.value().find("projected_points") != std::string::npos);
  CHECK(serialized.value().find("semantic_vertex") != std::string::npos);
  CHECK(serialized.value().find("projected_lines") != std::string::npos);
  CHECK(serialized.value().find("semantic_edge") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const Sketch* restored_sketch = restored.value().find_sketch(SketchId("sketch.top"));
  REQUIRE(restored_sketch != nullptr);
  REQUIRE(restored_sketch->projected_points().size() == 2);
  REQUIRE(restored_sketch->projected_lines().size() == 2);
  CHECK(restored_sketch->projected_points()[0].construction_point().value() == "point.mid");
  REQUIRE(restored_sketch->projected_points()[1].semantic_vertex().has_value());
  CHECK(restored_sketch->projected_points()[1].referenced_node_id() ==
        "feature.base.vertex.top_front_right");
  CHECK(restored_sketch->projected_lines()[0].construction_line().value() == "line.axis");
  REQUIRE(restored_sketch->projected_lines()[1].semantic_edge().has_value());
  CHECK(restored_sketch->projected_lines()[1].referenced_node_id() == "feature.base.edge.top_front");
}
