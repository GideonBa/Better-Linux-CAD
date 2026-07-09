#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/construction_point_resolver.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_rectangular_additive_document() {
  auto document = PartDocument::create(DocumentId("part.semantic_refs"), "SemanticRefs");
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

void add_line_points(PartDocument& document) {
  auto origin = ConstructionPoint::create_explicit(ConstructionPointId("point.origin"), "Origin",
                                                   Point3{0.0, 0.0, 0.0});
  REQUIRE(origin);
  REQUIRE(document.add_construction_point(origin.value()));

  auto x = ConstructionPoint::create_explicit(ConstructionPointId("point.x"), "X",
                                              Point3{10.0, 0.0, 0.0});
  REQUIRE(x);
  REQUIRE(document.add_construction_point(x.value()));

  auto offset = ConstructionPoint::create_explicit(ConstructionPointId("point.offset"), "Offset",
                                                   Point3{0.0, 0.0, 20.0});
  REQUIRE(offset);
  REQUIRE(document.add_construction_point(offset.value()));
}

} // namespace

TEST_CASE("SemanticReferenceEvaluator resolves rectangular additive extrude edges and vertices",
          "[geometry][semantic-reference]") {
  PartDocument document = make_rectangular_additive_document();
  const SemanticReferenceEvaluator evaluator;

  auto edge_ref = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge_ref);
  auto edge = evaluator.resolve_edge(document, edge_ref.value());
  REQUIRE(edge);

  CHECK(edge.value().start.x == Catch::Approx(-50.0));
  CHECK(edge.value().start.y == Catch::Approx(30.0));
  CHECK(edge.value().start.z == Catch::Approx(10.0));
  CHECK(edge.value().end.x == Catch::Approx(50.0));
  CHECK(edge.value().end.y == Catch::Approx(30.0));
  CHECK(edge.value().end.z == Catch::Approx(10.0));

  auto vertex_ref = SemanticVertexReference::create(FeatureId("feature.base"),
                                                    SemanticVertex::BottomBackLeft);
  REQUIRE(vertex_ref);
  auto vertex = evaluator.resolve_vertex(document, vertex_ref.value());
  REQUIRE(vertex);

  CHECK(vertex.value().position.x == Catch::Approx(-50.0));
  CHECK(vertex.value().position.y == Catch::Approx(-30.0));
  CHECK(vertex.value().position.z == Catch::Approx(0.0));
}

TEST_CASE("ConstructionPointResolver resolves generated vertex points and generated edge midpoints",
          "[geometry][construction-point]") {
  PartDocument document = make_rectangular_additive_document();

  auto vertex_ref = SemanticVertexReference::create(FeatureId("feature.base"),
                                                    SemanticVertex::TopFrontRight);
  REQUIRE(vertex_ref);
  auto vertex_relation = ConstructionRelation::create_point_on_generated_vertex(
      ConstructionRelationId("relation.point_on_vertex"), ConstructionPointId("point.vertex"),
      vertex_ref.value());
  REQUIRE(vertex_relation);
  auto vertex_point = ConstructionPoint::create_on_generated_vertex(
      ConstructionPointId("point.vertex"), "VertexPoint", vertex_relation.value());
  REQUIRE(vertex_point);
  REQUIRE(document.add_construction_point(vertex_point.value()));

  auto edge_ref = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge_ref);
  auto edge_relation = ConstructionRelation::create_point_on_generated_edge(
      ConstructionRelationId("relation.point_on_edge"), ConstructionPointId("point.edge_mid"),
      edge_ref.value());
  REQUIRE(edge_relation);
  auto edge_point = ConstructionPoint::create_on_generated_edge(
      ConstructionPointId("point.edge_mid"), "EdgeMidpoint", edge_relation.value());
  REQUIRE(edge_point);
  REQUIRE(document.add_construction_point(edge_point.value()));

  const ConstructionPointResolver resolver;
  auto resolved_vertex = resolver.resolve(document, ConstructionPointId("point.vertex"));
  REQUIRE(resolved_vertex);
  CHECK(resolved_vertex.value().position.x == Catch::Approx(50.0));
  CHECK(resolved_vertex.value().position.y == Catch::Approx(30.0));
  CHECK(resolved_vertex.value().position.z == Catch::Approx(10.0));

  auto resolved_edge = resolver.resolve(document, ConstructionPointId("point.edge_mid"));
  REQUIRE(resolved_edge);
  CHECK(resolved_edge.value().position.x == Catch::Approx(0.0));
  CHECK(resolved_edge.value().position.y == Catch::Approx(30.0));
  CHECK(resolved_edge.value().position.z == Catch::Approx(10.0));
}

TEST_CASE("ConstructionLineResolver resolves deterministic chained construction lines",
          "[geometry][construction-line]") {
  PartDocument document = make_rectangular_additive_document();
  add_line_points(document);

  auto base_relation = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.base"), ConstructionPointId("point.origin"),
      ConstructionPointId("point.x"));
  REQUIRE(base_relation);
  auto base_line = ConstructionLine::create_through_two_points(ConstructionLineId("line.base"),
                                                               "BaseLine", base_relation.value());
  REQUIRE(base_line);
  REQUIRE(document.add_construction_line(base_line.value()));

  auto parallel_relation = ConstructionRelation::create_line_parallel_to_line_through_point(
      ConstructionRelationId("relation.parallel"), ConstructionLineId("line.base"),
      ConstructionPointId("point.offset"));
  REQUIRE(parallel_relation);
  auto parallel_line = ConstructionLine::create_parallel_to_line_through_point(
      ConstructionLineId("line.parallel"), "ParallelLine", parallel_relation.value());
  REQUIRE(parallel_line);
  REQUIRE(document.add_construction_line(parallel_line.value()));

  auto edge_ref = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge_ref);
  auto edge_relation = ConstructionRelation::create_line_parallel_to_generated_edge_through_point(
      ConstructionRelationId("relation.edge_parallel"), edge_ref.value(),
      ConstructionPointId("point.offset"));
  REQUIRE(edge_relation);
  auto edge_line = ConstructionLine::create_parallel_to_generated_edge_through_point(
      ConstructionLineId("line.edge_parallel"), "EdgeParallel", edge_relation.value());
  REQUIRE(edge_line);
  REQUIRE(document.add_construction_line(edge_line.value()));

  const ConstructionLineResolver resolver;
  auto resolved_base = resolver.resolve(document, ConstructionLineId("line.base"));
  REQUIRE(resolved_base);
  CHECK(resolved_base.value().point.x == Catch::Approx(0.0));
  CHECK(resolved_base.value().direction.x == Catch::Approx(1.0));
  CHECK(resolved_base.value().direction.y == Catch::Approx(0.0));
  CHECK(resolved_base.value().direction.z == Catch::Approx(0.0));

  auto resolved_parallel = resolver.resolve(document, ConstructionLineId("line.parallel"));
  REQUIRE(resolved_parallel);
  CHECK(resolved_parallel.value().point.z == Catch::Approx(20.0));
  CHECK(resolved_parallel.value().direction.x == Catch::Approx(1.0));

  auto resolved_edge_parallel = resolver.resolve(document, ConstructionLineId("line.edge_parallel"));
  REQUIRE(resolved_edge_parallel);
  CHECK(resolved_edge_parallel.value().point.z == Catch::Approx(20.0));
  CHECK(resolved_edge_parallel.value().direction.x == Catch::Approx(1.0));
  CHECK(resolved_edge_parallel.value().direction.y == Catch::Approx(0.0));
  CHECK(resolved_edge_parallel.value().direction.z == Catch::Approx(0.0));
}
