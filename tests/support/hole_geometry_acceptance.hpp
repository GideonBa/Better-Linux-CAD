#pragma once

#include "blcad/core/hole_feature.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <BRepClass3d_SolidClassifier.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>

namespace blcad::test {
// The expected material volume and section points come from the uncut analytic
// fixture, independently of the Hole tool and its Boolean history.
inline void verify_hole_on_generated_body(PartDocument part, BodyId body, Point3 support_origin,
                                          Vector3 x, Vector3 y, Vector3 normal, double uncut_volume,
                                          int extent_kind) {
  const auto checked = []<typename T>(Result<T> result) {
    INFO((result ? "ok" : result.error().message()));
    REQUIRE(result);
    return std::move(result.value());
  };
  REQUIRE(part.add_parameter(checked(Parameter::create_length(
      ParameterId("hole.diameter"), "Hole diameter", checked(Quantity::length_mm(2))))));
  REQUIRE(part.add_parameter(checked(Parameter::create_length(
      ParameterId("hole.depth"), "Hole depth", checked(Quantity::length_mm(3))))));
  REQUIRE(part.add_parameter(checked(Parameter::create_angle(ParameterId("hole.tip"), "Hole tip",
                                                             checked(Quantity::angle_deg(90))))));
  REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId("hole.support"), "Hole support", support_origin, x, y, normal))));
  const auto support = checked(PlaneReference::create_construction_plane(
      PartFeatureInputRole::HoleSupportPlane, ConstructionPlaneId("hole.support")));
  const HoleExtent extent = extent_kind == 0 ? HoleExtent{HoleThroughAllExtent{}}
                            : extent_kind == 1
                                ? HoleExtent{HoleToNextExtent{}}
                                : HoleExtent{HoleBlindExtent{ParameterId("hole.depth")}};
  const HoleTip tip =
      extent_kind == 3 ? HoleTip{HoleConicalTip{ParameterId("hole.tip")}} : HoleTip{HoleFlatTip{}};
  REQUIRE(part.add_hole_feature(checked(
      HoleFeature::create(FeatureId("hole.acceptance"), "Hole", HoleKind::Plain, body, support,
                          {{HolePlacementId("center"), Point2{}}}, ParameterId("hole.diameter"),
                          extent, HoleDirection::OppositeSupportNormal, tip))));
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("hole.generated")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.validate_complete_geometry_result(part));
  const double removed = std::numbers::pi * (extent_kind < 2    ? 20.
                                             : extent_kind == 2 ? 3.
                                                                : 3. + 1. / 3.);
  const auto* shape = cache.find_body_shape(body);
  REQUIRE(shape);
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
        Catch::Approx(uncut_volume - removed).margin(1e-5));
  const auto material = [&](double depth, double radial) {
    const Point3 point{support_origin.x - depth * normal.x + radial * x.x,
                       support_origin.y - depth * normal.y + radial * x.y,
                       support_origin.z - depth * normal.z + radial * x.z};
    BRepClass3d_SolidClassifier classifier(*geometry::OcctShapeView::get(*shape),
                                           gp_Pnt(point.x, point.y, point.z), 1e-7);
    return classifier.State() == TopAbs_IN;
  };
  CHECK_FALSE(material(1, 0));
  CHECK(material(1, 1.1));
  CHECK(material(3.5, 0) == (extent_kind == 2));
  CHECK(material(4.1, 0) == (extent_kind >= 2));
}
} // namespace blcad::test
