#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

bool contains_node(const std::vector<std::string>& nodes, const char* node) {
  return std::find(nodes.begin(), nodes.end(), std::string(node)) != nodes.end();
}

PartDocument make_base_document() {
  auto document =
      PartDocument::create(DocumentId("part.projected_invalidations"), "Projected invalidations");
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

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "BaseExtrude", SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

} // namespace

TEST_CASE("Projected semantic references invalidate sketches and dependent features",
          "[core][dependency]") {
  PartDocument document = make_base_document();

  auto top_face = SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(top_face);
  auto top_workplane = DerivedWorkplane::create_on_feature_face(DatumPlaneId("workplane.top"),
                                                                "Top", top_face.value());
  REQUIRE(top_workplane);
  REQUIRE(document.add_derived_workplane(top_workplane.value()));

  auto sketch =
      Sketch::create(SketchId("sketch.top_refs"), "TopRefs", DatumPlaneId("workplane.top"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.helper"), Point2{0.0, 0.0}, Point2{1.0, 0.0})
          .value()));

  auto vertex =
      SemanticVertexReference::create(FeatureId("feature.base"), SemanticVertex::TopFrontRight);
  REQUIRE(vertex);
  auto projected_vertex = ProjectedSketchPoint::create_from_semantic_vertex(
      SketchEntityId("ref.vertex.top_front_right"), vertex.value());
  REQUIRE(projected_vertex);
  REQUIRE(sketch.value().add_reference(projected_vertex.value()));

  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto projected_edge = ProjectedSketchLine::create_from_semantic_edge(
      SketchEntityId("ref.edge.top_front"), edge.value());
  REQUIRE(projected_edge);
  REQUIRE(sketch.value().add_reference(projected_edge.value()));

  REQUIRE(document.add_sketch(sketch.value()));

  auto marker =
      Feature::create_additive_extrude(FeatureId("feature.top_marker"), "TopMarker",
                                       SketchId("sketch.top_refs"), ParameterId("part.depth"));
  REQUIRE(marker);
  REQUIRE(document.add_feature(marker.value()));

  CHECK(document.dependency_graph().has_dependency("feature.base", "workplane.top"));
  CHECK(document.dependency_graph().has_dependency("feature.base",
                                                   "feature.base.vertex.top_front_right"));
  CHECK(document.dependency_graph().has_dependency("feature.base.vertex.top_front_right",
                                                   "sketch.top_refs"));
  CHECK(document.dependency_graph().has_dependency("feature.base", "feature.base.edge.top_front"));
  CHECK(
      document.dependency_graph().has_dependency("feature.base.edge.top_front", "sketch.top_refs"));
  CHECK(document.dependency_graph().has_dependency("sketch.top_refs", "feature.top_marker"));

  auto affected = document.mark_parameter_changed(ParameterId("part.depth"));
  REQUIRE(affected);
  CHECK(contains_node(affected.value(), "feature.base"));
  CHECK(contains_node(affected.value(), "feature.base.vertex.top_front_right"));
  CHECK(contains_node(affected.value(), "feature.base.edge.top_front"));
  CHECK(contains_node(affected.value(), "sketch.top_refs"));
  CHECK(contains_node(affected.value(), "feature.top_marker"));
}

TEST_CASE("Projected construction references invalidate sketches through generated construction "
          "relations",
          "[core][dependency]") {
  PartDocument document = make_base_document();

  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto relation = ConstructionRelation::create_point_on_generated_edge(
      ConstructionRelationId("relation.point.top_front_mid"),
      ConstructionPointId("point.top_front_mid"), edge.value());
  REQUIRE(relation);
  auto point = ConstructionPoint::create_on_generated_edge(
      ConstructionPointId("point.top_front_mid"), "TopFrontMid", relation.value());
  REQUIRE(point);
  REQUIRE(document.add_construction_point(point.value()));

  auto sketch = Sketch::create(SketchId("sketch.projected_point"), "ProjectedPoint",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto projected = ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("ref.point.top_front_mid"), ConstructionPointId("point.top_front_mid"));
  REQUIRE(projected);
  REQUIRE(sketch.value().add_reference(projected.value()));
  REQUIRE(document.add_sketch(sketch.value()));

  CHECK(document.dependency_graph().has_dependency("feature.base", "point.top_front_mid"));
  CHECK(
      document.dependency_graph().has_dependency("point.top_front_mid", "sketch.projected_point"));

  auto affected = document.mark_parameter_changed(ParameterId("part.depth"));
  REQUIRE(affected);
  CHECK(contains_node(affected.value(), "point.top_front_mid"));
  CHECK(contains_node(affected.value(), "sketch.projected_point"));
}
