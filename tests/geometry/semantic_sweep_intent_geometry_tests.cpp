#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;

TEST_CASE("Unimplemented material-region intent preserves the certified outer-boundary result",
          "[geometry][parity-b167][semantic-sweep-intent]") {
  auto part = sf::straight_sweep();
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("material.intent")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto accepted = part;
  const auto old = *part.find_sweep_feature(FeatureId("driven.sweep"));
  const auto region = std::get<ProfileRegionReference>(old.profile().source());
  const auto profile = f::checked(
      SweepProfileReference::create_closed_region(region, SweepRegionPolicy::MaterialRegion));
  REQUIRE(part.update_sweep_feature(f::checked(SweepFeature::create_sweep(
      old.id(), old.name(), profile, old.path(), old.body_result_context()))));
  const auto source = f::checked(serialize_part_document_to_json(part));
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("multi-wire") != std::string::npos);
  CHECK(f::checked(serialize_part_document_to_json(part)) == source);
  CHECK(cache.validate_complete_geometry_result(accepted));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
  REQUIRE(cache.find_body_shape(BodyId("driven.body")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("driven.body")))
            .volume_mm3 == Catch::Approx(6.4).margin(1e-6));
}

TEST_CASE("Unimplemented semantic paths cannot publish a partial full or planned result",
          "[geometry][parity-b167][semantic-sweep-intent]") {
  auto part = f::part();
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("semantic.intent")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto accepted = part;
  const auto edge = f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::PathSegment, f::checked(SemanticEdgeReference::create(
                                             FeatureId("base.feature"), SemanticEdge::TopFront))));
  const auto segment = f::checked(PathSegmentReference::create_feature_edge(edge));
  REQUIRE(part.add_path_curve(f::checked(
      PathCurve::create(PathCurveId("semantic.path"), "Semantic path", {segment}, PathClosure::Open,
                        PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                        f::checked(PathExecutionPolicy::curve_preserving(1e-6))))));
  const auto source = f::checked(serialize_part_document_to_json(part));
  const auto plan = f::checked(part.create_recompute_plan());
  const auto planned = GeometryRecomputeExecutor{}.execute_plan(part, plan, cache);
  REQUIRE_FALSE(planned);
  CHECK(planned.error().message().find("capability") != std::string::npos);
  const auto full = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(full);
  CHECK(full.error().message().find("capability") != std::string::npos);
  CHECK(f::checked(serialize_part_document_to_json(part)) == source);
  CHECK(cache.validate_complete_geometry_result(accepted));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
  REQUIRE(cache.find_body_shape(BodyId("base.body")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("base.body")))
            .volume_mm3 == Catch::Approx(240).margin(1e-6));
}
