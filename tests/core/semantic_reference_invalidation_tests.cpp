#include "blcad/core/part_document.hpp"

#include <algorithm>
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

bool contains_node(const std::vector<std::string>& nodes, const char* node) {
  return std::find(nodes.begin(), nodes.end(), std::string(node)) != nodes.end();
}

PartDocument make_document_with_generated_edge_line() {
  auto document = PartDocument::create(DocumentId("part.semantic_invalidation"), "SemanticInvalidation");
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

  auto point = ConstructionPoint::create_explicit(ConstructionPointId("point.offset"), "Offset",
                                                  Point3{0.0, 0.0, 20.0});
  REQUIRE(point);
  REQUIRE(document.value().add_construction_point(point.value()));

  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto relation = ConstructionRelation::create_line_parallel_to_generated_edge_through_point(
      ConstructionRelationId("relation.edge_parallel"), edge.value(), ConstructionPointId("point.offset"));
  REQUIRE(relation);

  auto line = ConstructionLine::create_parallel_to_generated_edge_through_point(
      ConstructionLineId("line.edge_parallel"), "GeneratedEdgeParallel", relation.value());
  REQUIRE(line);
  REQUIRE(document.value().add_construction_line(line.value()));

  return document.value();
}

} // namespace

TEST_CASE("Generated-edge construction references follow source feature invalidation",
          "[core][invalidation][construction]") {
  PartDocument document = make_document_with_generated_edge_line();
  CHECK(document.dependency_graph().has_dependency("feature.base", "line.edge_parallel"));

  auto updated_depth = Quantity::length_mm(20.0, "part.depth");
  REQUIRE(updated_depth);
  auto affected = document.set_parameter_value(ParameterId("part.depth"), updated_depth.value());
  REQUIRE(affected);

  CHECK(contains_node(affected.value(), "part.depth"));
  CHECK(contains_node(affected.value(), "feature.base"));
  CHECK(contains_node(affected.value(), "line.edge_parallel"));
}
