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

  auto point = ConstructionPoint::create_explicit(ConstructionPointId("point.offset"), "OffsetPoint",
                                                  Point3{5.0, 6.0, 7.0});
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
  const auto resolved = resolver.resolve(document.value(), DatumPlaneId("construction_plane.parallel_xy"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin.x == Catch::Approx(5.0));
  CHECK(resolved.value().origin.y == Catch::Approx(6.0));
  CHECK(resolved.value().origin.z == Catch::Approx(7.0));
  CHECK(resolved.value().x_axis.x == Catch::Approx(1.0));
  CHECK(resolved.value().y_axis.y == Catch::Approx(1.0));
  CHECK(resolved.value().normal.z == Catch::Approx(1.0));
}
