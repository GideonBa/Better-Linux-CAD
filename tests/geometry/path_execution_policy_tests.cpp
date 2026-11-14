#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;

TEST_CASE("Unavailable curve-preserving execution retains the previously certified result",
          "[geometry][parity-b166][path-execution-policy]") {
  auto part = sf::straight_sweep();
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("policy")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto previous = part;
  const auto count = cache.feature_shape_count();
  REQUIRE(cache.find_body_shape(BodyId("driven.body")));
  const auto volume = RectangleExtrusionAdapter{}
                          .summarize(*cache.find_body_shape(BodyId("driven.body")))
                          .volume_mm3;
  CHECK(volume == Catch::Approx(6.4).margin(1e-6));
  REQUIRE(part.set_path_execution_policy(PathCurveId("driven.path"),
                                         f::checked(PathExecutionPolicy::curve_preserving(1e-4))));
  const auto before = f::checked(serialize_part_document_to_json(part));
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("Curve-preserving") != std::string::npos);
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
  CHECK(cache.feature_shape_count() == count);
  REQUIRE(cache.find_body_shape(BodyId("driven.body")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("driven.body")))
            .volume_mm3 == Catch::Approx(volume).margin(1e-12));
  CHECK(cache.validate_complete_geometry_result(previous));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
}
