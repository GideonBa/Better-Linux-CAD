#include "../support/curve_surface_milestone_oracle.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/surface_offset_fixture.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include <TopoDS_Shape.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::curve_surface_milestone;
namespace s = blcad::test::surface_offset;
namespace {
void execute(PartDocument& part, ShapeCache& cache) {
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
}
double volume(const GeometryShape& shape) {
  return blcad::test::integrated_solid_volume(shape).volume_mm3;
}
void verify(const GeometryShape& shape) {
  const auto r = f::physical(shape);
  CAPTURE(r.boundary_error_mm, r.normal_error_rad, r.curvature_error_per_mm, r.blend_error_mm);
  CHECK(r.valid);
  CHECK(r.material);
  CHECK(r.solids == 2);
  CHECK(r.stations == 32);
  CHECK(r.boundary_error_mm < 1e-5);
  CHECK(r.normal_error_rad < 1e-4);
  CHECK(r.curvature_error_per_mm < 1e-4);
  CHECK(r.blend_error_mm < 1e-5);
}
} // namespace
TEST_CASE("Connected curve Surface milestone checks independent differentials volumes and "
          "finishing contacts",
          "[geometry][curve-surface-milestone][parity-b174]") {
  auto part = f::connected_fixture();
  auto cache = s::cache();
  execute(part, cache);
  verify(*cache.find_body_shape(BodyId("pattern.body")));
  CHECK(f::section_error(*cache.find_feature_shape(FeatureId("main"))) < 1e-5);
  const double pi = std::numbers::pi, r = .2, R = 1;
  const double removed = pi * ((2 - pi / 2) * R * r * r + (pi / 2 - 5. / 3) * r * r * r);
  CHECK(f::rim_removed_volume(*cache.find_feature_shape(FeatureId("pattern")),
                              *cache.find_feature_shape(FeatureId("finish"))) ==
        Catch::Approx(removed).margin(1e-6));
  CHECK(volume(*cache.find_feature_shape(FeatureId("thick"))) ==
        Catch::Approx((21 * std::acos(.8) + 3) * 7).margin(1e-5));
  CHECK(volume(*cache.find_feature_shape(FeatureId("milestone.bore"))) ==
        Catch::Approx((21 * std::acos(.8) + 3) * 7 - .04 * pi).margin(1e-5));
  CHECK(volume(*cache.find_feature_shape(FeatureId("duct"))) ==
        Catch::Approx(20 * pi).margin(1e-6));
  const auto original = s::saved(part);
  const auto last = *cache.find_body_shape(BodyId("pattern.body"));
  const auto last_native = *OcctShapeView::get(last);
  auto bad = part;
  f::edit_connected_section_and_guide(bad, .8, false);
  const auto rejected = GeometryRecomputeExecutor{}.execute_document(bad, cache);
  REQUIRE_FALSE(rejected);
  CHECK(OcctShapeView::get(*cache.find_body_shape(BodyId("pattern.body")))->IsSame(last_native));
  CHECK(s::saved(part) == original);
  f::edit_connected_section_and_guide(part, .2);
  execute(part, cache);
  CHECK(f::section_error(*cache.find_feature_shape(FeatureId("main")), .2) < 1e-5);
  verify(*cache.find_body_shape(BodyId("pattern.body")));
  REQUIRE(part.set_parameter_value(ParameterId("window.height"), Quantity::length_mm(6).value()));
  execute(part, cache);
  verify(*cache.find_body_shape(BodyId("pattern.body")));
  CHECK(volume(*cache.find_feature_shape(FeatureId("milestone.bore"))) ==
        Catch::Approx((21 * std::acos(.8) + 3) * 9 - .04 * pi).margin(1e-5));
  auto fresh = s::cache();
  auto reopened = deserialize_part_document_from_json(s::saved(part)).value();
  execute(reopened, fresh);
  verify(*fresh.find_body_shape(BodyId("pattern.body")));
  CHECK(volume(*fresh.find_body_shape(BodyId("pattern.body"))) ==
        Catch::Approx(volume(*cache.find_body_shape(BodyId("pattern.body")))).margin(1e-5));
}
