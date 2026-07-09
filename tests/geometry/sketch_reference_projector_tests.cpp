#include "blcad/geometry/sketch_reference_projector.hpp"

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
  auto document = PartDocument::create(DocumentId("part.projected_refs"), "ProjectedRefs");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 100.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));

  auto feature = Feature::create_additive_extrude(FeatureId("feature.base"), "BaseExtrude",
                                                  SketchId("sketch.base"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  auto top_face = SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(top_face);
  auto top_workplane = DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("workplane.top"), "TopWorkplane", top_face.value());
  REQUIRE(top_workplane);
  REQUIRE(document.value().add_derived_workplane(top_workplane.value()));

  return document.value();
}

void add_generated_top_front_midpoint_and_axis(PartDocument& document) {
  auto edge_ref = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge_ref);

  auto point_relation = ConstructionRelation::create_point_on_generated_edge(
      ConstructionRelationId("relation.point.top_front_mid"), ConstructionPointId("point.top_front_mid"),
      edge_ref.value());
  REQUIRE(point_relation);
  auto point = ConstructionPoint::create_on_generated_edge(ConstructionPointId("point.top_front_mid"),
                                                           "TopFrontMid", point_relation.value());
  REQUIRE(point);
  REQUIRE(document.add_construction_point(point.value()));

  auto line_relation = ConstructionRelation::create_line_parallel_to_generated_edge_through_point(
      ConstructionRelationId("relation.line.top_front_axis"), edge_ref.value(),
      ConstructionPointId("point.top_front_mid"));
  REQUIRE(line_relation);
  auto line = ConstructionLine::create_parallel_to_generated_edge_through_point(
      ConstructionLineId("line.top_front_axis"), "TopFrontAxis", line_relation.value());
  REQUIRE(line);
  REQUIRE(document.add_construction_line(line.value()));
}

Sketch make_top_reference_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.top_refs"), "TopRefs", DatumPlaneId("workplane.top"));
  REQUIRE(sketch);
  return sketch.value();
}

} // namespace

TEST_CASE("SketchReferenceProjector projects semantic generated refs into sketch space",
          "[geometry][sketch-reference]") {
  PartDocument document = make_rectangular_additive_document();
  Sketch sketch = make_top_reference_sketch();

  auto vertex_ref = SemanticVertexReference::create(FeatureId("feature.base"),
                                                    SemanticVertex::TopFrontRight);
  REQUIRE(vertex_ref);
  auto projected_vertex = ProjectedSketchPoint::create_from_semantic_vertex(
      SketchEntityId("ref.vertex.top_front_right"), vertex_ref.value());
  REQUIRE(projected_vertex);

  auto edge_ref = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge_ref);
  auto projected_edge =
      ProjectedSketchLine::create_from_semantic_edge(SketchEntityId("ref.edge.top_front"), edge_ref.value());
  REQUIRE(projected_edge);

  const SketchReferenceProjector projector;
  auto resolved_point = projector.resolve_point(document, sketch, projected_vertex.value());
  REQUIRE(resolved_point);
  CHECK(resolved_point.value().position.x == Catch::Approx(50.0));
  CHECK(resolved_point.value().position.y == Catch::Approx(30.0));

  auto resolved_line = projector.resolve_line(document, sketch, projected_edge.value());
  REQUIRE(resolved_line);
  CHECK(resolved_line.value().point.x == Catch::Approx(-50.0));
  CHECK(resolved_line.value().point.y == Catch::Approx(30.0));
  CHECK(resolved_line.value().direction.x == Catch::Approx(1.0));
  CHECK(resolved_line.value().direction.y == Catch::Approx(0.0));
}

TEST_CASE("SketchReferenceProjector projects construction references into sketch space",
          "[geometry][sketch-reference]") {
  PartDocument document = make_rectangular_additive_document();
  add_generated_top_front_midpoint_and_axis(document);
  Sketch sketch = make_top_reference_sketch();

  auto projected_point = ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("ref.point.top_front_mid"), ConstructionPointId("point.top_front_mid"));
  REQUIRE(projected_point);
  auto projected_line = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("ref.line.top_front_axis"), ConstructionLineId("line.top_front_axis"));
  REQUIRE(projected_line);

  const SketchReferenceProjector projector;
  auto resolved_point = projector.resolve_point(document, sketch, projected_point.value());
  REQUIRE(resolved_point);
  CHECK(resolved_point.value().position.x == Catch::Approx(0.0));
  CHECK(resolved_point.value().position.y == Catch::Approx(30.0));

  auto resolved_line = projector.resolve_line(document, sketch, projected_line.value());
  REQUIRE(resolved_line);
  CHECK(resolved_line.value().point.x == Catch::Approx(0.0));
  CHECK(resolved_line.value().point.y == Catch::Approx(30.0));
  CHECK(resolved_line.value().direction.x == Catch::Approx(1.0));
  CHECK(resolved_line.value().direction.y == Catch::Approx(0.0));
}

TEST_CASE("SketchReferenceProjector rejects out-of-plane projected points",
          "[geometry][sketch-reference]") {
  PartDocument document = make_rectangular_additive_document();
  Sketch sketch = make_top_reference_sketch();

  auto bottom_vertex = SemanticVertexReference::create(FeatureId("feature.base"),
                                                       SemanticVertex::BottomFrontRight);
  REQUIRE(bottom_vertex);
  auto projected_vertex = ProjectedSketchPoint::create_from_semantic_vertex(
      SketchEntityId("ref.vertex.bottom_front_right"), bottom_vertex.value());
  REQUIRE(projected_vertex);

  const SketchReferenceProjector projector;
  auto resolved = projector.resolve_point(document, sketch, projected_vertex.value());
  REQUIRE(resolved.has_error());
  CHECK(resolved.error().message() == "projected sketch reference must lie on the sketch workplane");
}
