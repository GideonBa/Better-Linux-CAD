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

PartDocument make_document_with_base_feature() {
  auto document = PartDocument::create(DocumentId("part.chained_relations"), "ChainedRelations");
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

  auto feature = Feature::create_additive_extrude(FeatureId("feature.base"), "BaseExtrude",
                                                  SketchId("sketch.base"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

void add_reference_points(PartDocument& document) {
  auto origin = ConstructionPoint::create_explicit(ConstructionPointId("point.origin"), "Origin",
                                                   Point3{0.0, 0.0, 0.0});
  REQUIRE(origin);
  REQUIRE(document.add_construction_point(origin.value()));

  auto x = ConstructionPoint::create_explicit(ConstructionPointId("point.x"), "X",
                                              Point3{10.0, 0.0, 0.0});
  REQUIRE(x);
  REQUIRE(document.add_construction_point(x.value()));

  auto y = ConstructionPoint::create_explicit(ConstructionPointId("point.y"), "Y",
                                              Point3{0.0, 10.0, 0.0});
  REQUIRE(y);
  REQUIRE(document.add_construction_point(y.value()));

  auto z = ConstructionPoint::create_explicit(ConstructionPointId("point.z"), "Z",
                                              Point3{0.0, 0.0, 10.0});
  REQUIRE(z);
  REQUIRE(document.add_construction_point(z.value()));
}

} // namespace

TEST_CASE("Semantic generated edge and vertex references are stable model intent", "[core][construction]") {
  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  CHECK(edge.value().source_feature().value() == "feature.base");
  CHECK(edge.value().edge() == SemanticEdge::TopFront);
  CHECK(edge.value().node_id() == "feature.base.edge.top_front");

  auto vertex = SemanticVertexReference::create(FeatureId("feature.base"),
                                                SemanticVertex::BottomBackLeft);
  REQUIRE(vertex);
  CHECK(vertex.value().source_feature().value() == "feature.base");
  CHECK(vertex.value().vertex() == SemanticVertex::BottomBackLeft);
  CHECK(vertex.value().node_id() == "feature.base.vertex.bottom_back_left");

  auto invalid_edge = SemanticEdgeReference::create(FeatureId(), SemanticEdge::TopFront);
  REQUIRE(invalid_edge.has_error());
  CHECK(invalid_edge.error().message() == "semantic edge source feature id must not be empty");
}

TEST_CASE("ConstructionRelation stores chained relation definitions", "[core][construction]") {
  auto point_on_plane = ConstructionRelation::create_point_on_plane(
      ConstructionRelationId("relation.point_on_xy"), ConstructionPointId("point.origin"),
      DatumPlaneId("datum.xy"));
  REQUIRE(point_on_plane);
  CHECK(point_on_plane.value().type() == ConstructionRelationType::PointOnPlane);
  CHECK(point_on_plane.value().source_plane().value() == "datum.xy");

  auto line_on_plane = ConstructionRelation::create_line_on_plane(
      ConstructionRelationId("relation.line_on_xy"), ConstructionLineId("line.base"),
      DatumPlaneId("datum.xy"));
  REQUIRE(line_on_plane);
  CHECK(line_on_plane.value().type() == ConstructionRelationType::LineOnPlane);

  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto generated_edge_relation = ConstructionRelation::create_line_parallel_to_generated_edge_through_point(
      ConstructionRelationId("relation.parallel_to_edge"), edge.value(),
      ConstructionPointId("point.origin"));
  REQUIRE(generated_edge_relation);
  CHECK(generated_edge_relation.value().generated_edge().has_value());
  CHECK(generated_edge_relation.value().generated_edge().value().edge() == SemanticEdge::TopFront);
}

TEST_CASE("PartDocument stores chained construction relation dependencies", "[core][construction]") {
  PartDocument document = make_document_with_base_feature();
  add_reference_points(document);

  auto base_relation = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.line_base"), ConstructionPointId("point.origin"),
      ConstructionPointId("point.x"));
  REQUIRE(base_relation);
  auto base_line = ConstructionLine::create_through_two_points(ConstructionLineId("line.base"),
                                                               "BaseLine", base_relation.value());
  REQUIRE(base_line);
  REQUIRE(document.add_construction_line(base_line.value()));

  auto parallel_relation = ConstructionRelation::create_line_parallel_to_line_through_point(
      ConstructionRelationId("relation.line_parallel"), ConstructionLineId("line.base"),
      ConstructionPointId("point.y"));
  REQUIRE(parallel_relation);
  auto parallel_line = ConstructionLine::create_parallel_to_line_through_point(
      ConstructionLineId("line.parallel"), "ParallelLine", parallel_relation.value());
  REQUIRE(parallel_line);
  REQUIRE(document.add_construction_line(parallel_line.value()));

  auto plane_relation = ConstructionRelation::create_plane_parallel_to_plane_through_point(
      ConstructionRelationId("relation.plane_parallel"), DatumPlaneId("datum.xy"),
      ConstructionPointId("point.z"));
  REQUIRE(plane_relation);
  auto parallel_plane = ConstructionPlane::create_parallel_to_plane_through_point(
      ConstructionPlaneId("construction_plane.parallel_xy"), "ParallelXY", plane_relation.value());
  REQUIRE(parallel_plane);
  REQUIRE(document.add_construction_plane(parallel_plane.value()));

  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto generated_edge_relation = ConstructionRelation::create_line_parallel_to_generated_edge_through_point(
      ConstructionRelationId("relation.edge_parallel"), edge.value(), ConstructionPointId("point.z"));
  REQUIRE(generated_edge_relation);
  auto edge_line = ConstructionLine::create_parallel_to_generated_edge_through_point(
      ConstructionLineId("line.edge_parallel"), "GeneratedEdgeParallel", generated_edge_relation.value());
  REQUIRE(edge_line);
  REQUIRE(document.add_construction_line(edge_line.value()));

  CHECK(document.dependency_graph().has_dependency("line.base", "line.parallel"));
  CHECK(document.dependency_graph().has_dependency("point.y", "line.parallel"));
  CHECK(document.dependency_graph().has_dependency("datum.xy", "construction_plane.parallel_xy"));
  CHECK(document.dependency_graph().has_dependency("point.z", "construction_plane.parallel_xy"));
  CHECK(document.dependency_graph().has_dependency("feature.base", "line.edge_parallel"));
  CHECK(document.dependency_graph().has_dependency("point.z", "line.edge_parallel"));
}

TEST_CASE("PartDocument JSON round-trips chained relations and semantic generated refs", "[core][json]") {
  PartDocument document = make_document_with_base_feature();
  add_reference_points(document);

  auto base_relation = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.line_base"), ConstructionPointId("point.origin"),
      ConstructionPointId("point.x"));
  REQUIRE(base_relation);
  auto base_line = ConstructionLine::create_through_two_points(ConstructionLineId("line.base"),
                                                               "BaseLine", base_relation.value());
  REQUIRE(base_line);
  REQUIRE(document.add_construction_line(base_line.value()));

  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto edge_relation = ConstructionRelation::create_line_parallel_to_generated_edge_through_point(
      ConstructionRelationId("relation.edge_parallel"), edge.value(), ConstructionPointId("point.z"));
  REQUIRE(edge_relation);
  auto edge_line = ConstructionLine::create_parallel_to_generated_edge_through_point(
      ConstructionLineId("line.edge_parallel"), "GeneratedEdgeParallel", edge_relation.value());
  REQUIRE(edge_line);
  REQUIRE(document.add_construction_line(edge_line.value()));

  auto plane_relation = ConstructionRelation::create_plane_parallel_to_plane_through_point(
      ConstructionRelationId("relation.plane_parallel"), DatumPlaneId("datum.xy"),
      ConstructionPointId("point.z"));
  REQUIRE(plane_relation);
  auto parallel_plane = ConstructionPlane::create_parallel_to_plane_through_point(
      ConstructionPlaneId("construction_plane.parallel_xy"), "ParallelXY", plane_relation.value());
  REQUIRE(parallel_plane);
  REQUIRE(document.add_construction_plane(parallel_plane.value()));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("generated_edge") != std::string::npos);
  CHECK(serialized.value().find("line_parallel_to_generated_edge_through_point") != std::string::npos);
  CHECK(serialized.value().find("plane_parallel_to_plane_through_point") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().construction_line_count() == 2);
  CHECK(restored.value().construction_plane_count() == 1);

  const ConstructionLine* restored_edge_line =
      restored.value().find_construction_line(ConstructionLineId("line.edge_parallel"));
  REQUIRE(restored_edge_line != nullptr);
  REQUIRE(restored_edge_line->relation().has_value());
  CHECK(restored_edge_line->relation().value().generated_edge().has_value());
  CHECK(restored_edge_line->relation().value().generated_edge().value().edge() == SemanticEdge::TopFront);
}
