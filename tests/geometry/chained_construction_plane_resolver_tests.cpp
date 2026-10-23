#include "blcad/geometry/workplane_resolver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

TEST_CASE("WorkplaneResolver resolves construction planes parallel to planes through points",
          "[geometry][workplane]") {
  auto document = PartDocument::create(DocumentId("part.parallel_plane"), "ParallelPlane");
  REQUIRE(document);

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto point = ConstructionPoint::create_explicit(ConstructionPointId("point.offset"),
                                                  "OffsetPoint", Point3{5.0, 6.0, 7.0});
  REQUIRE(point);
  REQUIRE(document.value().add_construction_point(point.value()));

  auto relation = ConstructionRelation::create_plane_parallel_to_plane_through_point(
      ConstructionRelationId("relation.parallel_xy"), DatumPlaneId("datum.xy"),
      ConstructionPointId("point.offset"));
  REQUIRE(relation);

  auto plane = ConstructionPlane::create_parallel_to_plane_through_point(
      ConstructionPlaneId("construction_plane.parallel_xy"), "ParallelXY", relation.value());
  REQUIRE(plane);
  REQUIRE(document.value().add_construction_plane(plane.value()));

  const WorkplaneResolver resolver;
  const auto resolved =
      resolver.resolve(document.value(), DatumPlaneId("construction_plane.parallel_xy"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin.x == Catch::Approx(5.0));
  CHECK(resolved.value().origin.y == Catch::Approx(6.0));
  CHECK(resolved.value().origin.z == Catch::Approx(7.0));
  CHECK(resolved.value().x_axis.x == Catch::Approx(1.0));
  CHECK(resolved.value().y_axis.y == Catch::Approx(1.0));
  CHECK(resolved.value().normal.z == Catch::Approx(1.0));
}

namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
PartDocument generated_points() {
  auto part = checked(PartDocument::create(DocumentId("part.generated"), "Generated points"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::yz())));
  for (const auto& [id, value] :
       {std::pair{"width", 100.0}, std::pair{"height", 60.0}, std::pair{"depth", 10.0}})
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  auto sketch = checked(Sketch::create(SketchId("base"), "Base", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("profile"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  REQUIRE(part.add_feature(checked(Feature::create_additive_extrude(
      FeatureId("extrude"), "Extrude", SketchId("base"), ParameterId("depth")))));
  for (const auto& [id, vertex] :
       {std::pair{"a", SemanticVertex::TopBackLeft}, std::pair{"b", SemanticVertex::TopBackRight},
        std::pair{"c", SemanticVertex::TopFrontLeft}}) {
    auto relation = checked(ConstructionRelation::create_point_on_generated_vertex(
        ConstructionRelationId(std::string(id) + ".relation"), ConstructionPointId(id),
        checked(SemanticVertexReference::create(FeatureId("extrude"), vertex))));
    REQUIRE(part.add_construction_point(checked(
        ConstructionPoint::create_on_generated_vertex(ConstructionPointId(id), id, relation))));
  }
  auto relation = checked(ConstructionRelation::create_point_on_generated_edge(
      ConstructionRelationId("mid.relation"), ConstructionPointId("mid"),
      checked(SemanticEdgeReference::create(FeatureId("extrude"), SemanticEdge::TopFront))));
  REQUIRE(part.add_construction_point(checked(ConstructionPoint::create_on_generated_edge(
      ConstructionPointId("mid"), "Midpoint", relation))));
  return part;
}
} // namespace

TEST_CASE("Construction planes use evaluated generated points and follow upstream depth edits",
          "[geometry][construction-generated]") {
  auto part = generated_points();
  auto through = checked(ConstructionRelation::create_plane_through_three_points(
      ConstructionRelationId("three.relation"), ConstructionPointId("a"), ConstructionPointId("b"),
      ConstructionPointId("c")));
  REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_through_three_points(
      ConstructionPlaneId("three"), "Three points", through))));
  auto parallel = checked(ConstructionRelation::create_plane_parallel_to_plane_through_point(
      ConstructionRelationId("parallel.relation"), DatumPlaneId("datum.yz"),
      ConstructionPointId("mid")));
  REQUIRE(
      part.add_construction_plane(checked(ConstructionPlane::create_parallel_to_plane_through_point(
          ConstructionPlaneId("parallel"), "Parallel", parallel))));
  const WorkplaneResolver resolver;
  auto frame = checked(resolver.resolve(part, DatumPlaneId("three")));
  CHECK(frame.origin == Point3{-50, -30, 10});
  CHECK(frame.normal == Vector3{0, 0, 1});
  auto aligned = checked(resolver.resolve(part, DatumPlaneId("parallel")));
  CHECK(aligned.origin == Point3{0, 30, 10});
  CHECK(aligned.normal == Vector3{1, 0, 0});
  REQUIRE(part.set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(18))));
  CHECK(checked(resolver.resolve(part, DatumPlaneId("three"))).origin == Point3{-50, -30, 18});
  CHECK(checked(resolver.resolve(part, DatumPlaneId("parallel"))).origin == Point3{0, 30, 18});
  // The stored placeholders are unchanged; evaluation is a Geometry authority.
  CHECK(part.find_construction_point(ConstructionPointId("a"))->position() == Point3{});
}

TEST_CASE("Construction plane evaluation rejects coincident generated and explicit point positions",
          "[geometry][construction-generated]") {
  auto part = generated_points();
  REQUIRE(part.add_construction_point(checked(ConstructionPoint::create_explicit(
      ConstructionPointId("copy"), "Same as a", {-50, -30, 10}))));
  auto relation = checked(ConstructionRelation::create_plane_through_three_points(
      ConstructionRelationId("degenerate.relation"), ConstructionPointId("a"),
      ConstructionPointId("copy"), ConstructionPointId("b")));
  REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_through_three_points(
      ConstructionPlaneId("degenerate"), "Degenerate", relation))));
  const auto frame = WorkplaneResolver{}.resolve(part, DatumPlaneId("degenerate"));
  REQUIRE_FALSE(frame);
  CHECK(frame.error().message() == "plane through three points requires non-collinear points");
}
