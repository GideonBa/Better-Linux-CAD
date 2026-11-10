#include "../support/thread_feature_fixture.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <TopoDS_Shape.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::thread_fixture;
using f::checked;

TEST_CASE("Invalid external Thread support cannot replace any previously computed Body",
          "[geometry][parity-b155][thread-boundaries]") {
  const auto original = f::pair_part();
  auto cache = checked(ShapeCache::create(ShapeCacheId("thread.atomic")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(original, cache));
  const auto base = *cache.find_body_shape(BodyId("body.base"));
  const auto shaft = *cache.find_body_shape(BodyId("body.shaft"));
  auto part = original;
  const auto support = blcad::test::history_fixture::face(PartFeatureInputRole::ThreadSupportFace);
  REQUIRE(part.add_thread_feature(
      checked(ThreadFeature::create(FeatureId("invalid.thread"), "Invalid planar support",
                                    BodyId("body.base"), support, f::definition()))));
  const auto intent = checked(serialize_part_document_to_json(part));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(OcctShapeView::get(base)->IsSame(
      *OcctShapeView::get(*cache.find_body_shape(BodyId("body.base")))));
  CHECK(OcctShapeView::get(shaft)->IsSame(
      *OcctShapeView::get(*cache.find_body_shape(BodyId("body.shaft")))));
  CHECK(checked(serialize_part_document_to_json(part)) == intent);
}

TEST_CASE("An outward blind Threaded Hole cannot silently drop metadata or replace cached Bodies",
          "[geometry][parity-b155][thread-boundaries]") {
  const auto original = f::pair_part();
  auto cache = checked(ShapeCache::create(ShapeCacheId("thread.hole.atomic")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(original, cache));
  const auto base = *cache.find_body_shape(BodyId("body.base"));
  auto part = original;
  const auto hole = f::internal();
  REQUIRE(part.add_hole_feature(checked(HoleFeature::create(
      hole.id(), hole.name(), hole.kind(), hole.target_body(), hole.support(), hole.placements(),
      hole.diameter(), hole.extent(), HoleDirection::SupportNormal, hole.tip(),
      hole.manufacturing(), hole.thread()))));
  const auto intent = checked(serialize_part_document_to_json(part));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(OcctShapeView::get(base)->IsSame(
      *OcctShapeView::get(*cache.find_body_shape(BodyId("body.base")))));
  CHECK(checked(serialize_part_document_to_json(part)) == intent);
}
