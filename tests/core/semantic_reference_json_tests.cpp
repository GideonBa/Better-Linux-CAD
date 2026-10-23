#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_document_with_semantic_reference_points() {
  auto document =
      PartDocument::create(DocumentId("part.semantic_reference_json"), "SemanticReferenceJson");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 100.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "BaseExtrude", SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  auto vertex_ref =
      SemanticVertexReference::create(FeatureId("feature.base"), SemanticVertex::TopFrontRight);
  REQUIRE(vertex_ref);
  auto vertex_relation = ConstructionRelation::create_point_on_generated_vertex(
      ConstructionRelationId("relation.point_vertex"), ConstructionPointId("point.vertex"),
      vertex_ref.value());
  REQUIRE(vertex_relation);
  auto vertex_point = ConstructionPoint::create_on_generated_vertex(
      ConstructionPointId("point.vertex"), "VertexPoint", vertex_relation.value());
  REQUIRE(vertex_point);
  REQUIRE(document.value().add_construction_point(vertex_point.value()));

  auto edge_ref = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge_ref);
  auto edge_relation = ConstructionRelation::create_point_on_generated_edge(
      ConstructionRelationId("relation.point_edge"), ConstructionPointId("point.edge_mid"),
      edge_ref.value());
  REQUIRE(edge_relation);
  auto edge_point = ConstructionPoint::create_on_generated_edge(
      ConstructionPointId("point.edge_mid"), "EdgeMidpoint", edge_relation.value());
  REQUIRE(edge_point);
  REQUIRE(document.value().add_construction_point(edge_point.value()));

  return document.value();
}

} // namespace

TEST_CASE("PartDocument JSON round-trips relation-driven generated reference points",
          "[core][json]") {
  auto document = make_document_with_semantic_reference_points();

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("on_generated_vertex") != std::string::npos);
  CHECK(serialized.value().find("point_on_generated_edge") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().construction_point_count() == 2);

  const ConstructionPoint* vertex =
      restored.value().find_construction_point(ConstructionPointId("point.vertex"));
  REQUIRE(vertex != nullptr);
  CHECK(vertex->kind() == ConstructionPointKind::OnGeneratedVertex);
  REQUIRE(vertex->relation().has_value());
  CHECK(vertex->relation().value().generated_vertex().has_value());

  const ConstructionPoint* edge_mid =
      restored.value().find_construction_point(ConstructionPointId("point.edge_mid"));
  REQUIRE(edge_mid != nullptr);
  CHECK(edge_mid->kind() == ConstructionPointKind::OnGeneratedEdge);
  REQUIRE(edge_mid->relation().has_value());
  CHECK(edge_mid->relation().value().generated_edge().has_value());
}
namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
} // namespace

TEST_CASE("Generated point updates reject missing semantic sources without mutation",
          "[core][construction-generated]") {
  auto part = make_document_with_semantic_reference_points();
  const auto before = checked(serialize_part_document_to_json(part));
  const auto edges = part.dependency_graph().dependencies();
  auto relation = checked(ConstructionRelation::create_point_on_generated_vertex(
      ConstructionRelationId("relation.point_vertex"), ConstructionPointId("point.vertex"),
      checked(SemanticVertexReference::create(FeatureId("missing"), SemanticVertex::TopBackLeft))));
  CHECK_FALSE(part.update_construction_point(checked(ConstructionPoint::create_on_generated_vertex(
      ConstructionPointId("point.vertex"), "Invalid vertex", relation))));
  auto edge_relation = checked(ConstructionRelation::create_point_on_generated_edge(
      ConstructionRelationId("missing.edge.relation"), ConstructionPointId("missing.edge"),
      checked(SemanticEdgeReference::create(FeatureId("missing"), SemanticEdge::TopFront))));
  CHECK_FALSE(part.add_construction_point(checked(ConstructionPoint::create_on_generated_edge(
      ConstructionPointId("missing.edge"), "Invalid edge", edge_relation))));
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK(part.dependency_graph().dependencies() == edges);
  auto invalid_json = before;
  const std::string source = "\"source_feature\": \"feature.base\"";
  const auto position = invalid_json.find(source);
  REQUIRE(position != std::string::npos);
  invalid_json.replace(position, source.size(), "\"source_feature\": \"missing\"");
  CHECK_FALSE(deserialize_part_document_from_json(invalid_json));
}

TEST_CASE("JSON retains generated point order across later feature and plane dependencies",
          "[core][json][construction-generated]") {
  auto part = make_document_with_semantic_reference_points();
  auto plane_relation = checked(ConstructionRelation::create_plane_parallel_to_plane_through_point(
      ConstructionRelationId("later.plane.relation"), DatumPlaneId("datum.xy"),
      ConstructionPointId("point.edge_mid")));
  REQUIRE(
      part.add_construction_plane(checked(ConstructionPlane::create_parallel_to_plane_through_point(
          ConstructionPlaneId("later.plane"), "Later plane", plane_relation))));
  auto sketch =
      checked(Sketch::create(SketchId("later.sketch"), "Later", DatumPlaneId("later.plane")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("later.profile"), ParameterId("part.width"), ParameterId("part.height")))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  REQUIRE(part.add_feature(checked(
      Feature::create_additive_extrude(FeatureId("later.feature"), "Later feature",
                                       SketchId("later.sketch"), ParameterId("part.depth")))));
  auto relation = checked(ConstructionRelation::create_point_on_generated_vertex(
      ConstructionRelationId("relation.point_vertex"), ConstructionPointId("point.vertex"),
      checked(SemanticVertexReference::create(FeatureId("later.feature"),
                                              SemanticVertex::TopFrontRight))));
  REQUIRE(part.update_construction_point(checked(ConstructionPoint::create_on_generated_vertex(
      ConstructionPointId("point.vertex"), "Rebound vertex", relation))));
  auto line_relation = checked(ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("line.relation"), ConstructionPointId("point.vertex"),
      ConstructionPointId("point.edge_mid")));
  REQUIRE(part.add_construction_line(checked(ConstructionLine::create_through_two_points(
      ConstructionLineId("line"), "Join", line_relation))));
  const auto serialized = checked(serialize_part_document_to_json(part));
  auto loaded = checked(deserialize_part_document_from_json(serialized));
  CHECK(checked(serialize_part_document_to_json(loaded)) == serialized);
  CHECK(loaded.construction_points()[0].id() == ConstructionPointId("point.vertex"));
  CHECK(loaded.construction_points()[1].id() == ConstructionPointId("point.edge_mid"));
  CHECK(loaded.dependency_graph().has_dependency("later.feature", "point.vertex"));
  CHECK(loaded.dependency_graph().has_dependency("point.edge_mid", "later.plane"));
  // Rebinding the source point back to its own derived feature is cyclic.
  auto cyclic_relation = checked(ConstructionRelation::create_point_on_generated_edge(
      ConstructionRelationId("relation.point_edge"), ConstructionPointId("point.edge_mid"),
      checked(SemanticEdgeReference::create(FeatureId("later.feature"), SemanticEdge::TopFront))));
  CHECK_FALSE(part.update_construction_point(checked(ConstructionPoint::create_on_generated_edge(
      ConstructionPointId("point.edge_mid"), "Cycle", cyclic_relation))));
  CHECK(checked(serialize_part_document_to_json(part)) == serialized);
}
