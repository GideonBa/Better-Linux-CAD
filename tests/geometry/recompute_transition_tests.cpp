#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace {
ShapeCache baseline(const PartDocument& p) {
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("verified.transition")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  REQUIRE(cache.validate_complete_geometry_result(p));
  return cache;
}
double volume(const ShapeCache& cache, const char* body = "body.base") {
  const auto* shape = cache.find_body_shape(BodyId(body));
  REQUIRE(shape);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void second(PartDocument& p) {
  REQUIRE(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("other.depth"), "Other depth", f::checked(Quantity::length_mm(3))))));
  auto feature = f::checked(Feature::create_additive_extrude(
      FeatureId("other"), "Other", SketchId("profile"), ParameterId("other.depth")));
  REQUIRE(p.add_feature(f::checked(feature.with_body_result_context(f::new_body(p, "other")))));
}
} // namespace

TEST_CASE("Verified transition derives clean replacement changes and retains independent producers",
          "[geometry][verified-transition][preview-cache]") {
  auto before = f::native_base();
  second(before);
  const auto source = f::checked(serialize_part_document_to_json(before));
  auto cache = baseline(before);
  const auto* old_base = OcctShapeView::get(*cache.find_feature_shape(FeatureId("base")));
  const auto* old_other = OcctShapeView::get(*cache.find_feature_shape(FeatureId("other")));
  auto after = before;
  REQUIRE(after.set_parameter_value(ParameterId("depth"), f::checked(Quantity::length_mm(12))));
  after.mark_all_clean();
  const auto computed = GeometryRecomputeExecutor{}.execute_transition(before, after, cache);
  REQUIRE(computed);
  CHECK(computed.value().executed_feature_count == 1);
  REQUIRE(cache.validate_complete_geometry_result(after));
  CHECK_FALSE(cache.validate_complete_geometry_result(before));
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("base"))) != old_base);
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("other"))) == old_other);
  CHECK(volume(cache) == Catch::Approx(2400));
  CHECK(volume(cache, "body.other") == Catch::Approx(600));
  CHECK_FALSE(cache.has_final_shape());
  CHECK(f::checked(serialize_part_document_to_json(before)) == source);
  const auto changed_base = *cache.find_feature_shape(FeatureId("base"));
  auto unused = after;
  REQUIRE(unused.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(.4))));
  unused.mark_all_clean();
  REQUIRE(GeometryRecomputeExecutor{}.execute_transition(after, unused, cache));
  REQUIRE(cache.validate_complete_geometry_result(unused));
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("base"))) ==
        OcctShapeView::get(changed_base));
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("other"))) == old_other);
  auto appended = unused;
  REQUIRE(appended.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("move"), BodyId("body.base"), BodyTransformCoordinateSpace::World, {},
      {2, 0, 0}, false, false))));
  const auto moved = GeometryRecomputeExecutor{}.execute_transition(unused, appended, cache);
  REQUIRE(moved);
  CHECK(moved.value().executed_feature_count == 1);
  REQUIRE(cache.validate_complete_geometry_result(appended));
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("base"))) ==
        OcctShapeView::get(changed_base));
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("other"))) == old_other);
  const auto bounds =
      f::checked(BodyTransformAdapter{}.bounds(*cache.find_body_shape(BodyId("body.base"))));
  CHECK(bounds.minimum.x == Catch::Approx(-8).margin(1e-6));
}

TEST_CASE("Verified transition owns plan derivation and arbitrary subsets cannot certify intent",
          "[geometry][verified-transition][physical-untrusted-origin]") {
  const auto before = f::native_base();
  auto cache = baseline(before);
  const auto certified_baseline = cache;
  auto after = before;
  REQUIRE(after.set_parameter_value(ParameterId("depth"), f::checked(Quantity::length_mm(12))));
  after.mark_all_clean();
  const auto omitted =
      f::checked(RecomputePlan::from_graph_and_changed_nodes(after.dependency_graph(), {"radius"}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(after, omitted, cache));
  CHECK(volume(cache) == Catch::Approx(1600));
  CHECK_FALSE(cache.validate_complete_geometry_result(after));
  CHECK_FALSE(cache.validate_complete_geometry_result(before));
  const auto subset_shape = *cache.find_feature_shape(FeatureId("base"));
  const auto rejected = GeometryRecomputeExecutor{}.execute_transition(before, after, cache);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().category() == ErrorCategory::Dependency);
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("base"))) ==
        OcctShapeView::get(subset_shape));
  CHECK(volume(cache) == Catch::Approx(1600));
  CHECK_FALSE(cache.validate_complete_geometry_result(after));
  // The untrusted subset cannot launder its output. Starting from the untouched
  // certificate, the owned transition derives the real omitted input itself.
  cache = certified_baseline;
  REQUIRE(cache.validate_complete_geometry_result(before));
  REQUIRE(GeometryRecomputeExecutor{}.execute_transition(before, after, cache));
  REQUIRE(cache.validate_complete_geometry_result(after));
  CHECK(volume(cache) == Catch::Approx(2400));
  auto incomplete = baseline(before);
  REQUIRE(incomplete.remove_feature_shape(FeatureId("base")));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_transition(before, after, incomplete));
  CHECK_FALSE(incomplete.validate_complete_geometry_result(after));
  CHECK(incomplete.find_feature_shape(FeatureId("base")) == nullptr);
  auto prefix = f::checked(ShapeCache::create(ShapeCacheId("prefix")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(before, 1, prefix));
  const auto prefix_shape = *prefix.find_feature_shape(FeatureId("base"));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_transition(before, after, prefix));
  CHECK_FALSE(prefix.validate_complete_geometry_result(after));
  CHECK(OcctShapeView::get(*prefix.find_feature_shape(FeatureId("base"))) ==
        OcctShapeView::get(prefix_shape));
  auto malformed = baseline(before);
  const auto foreign = f::checked(ShapeCache::create(ShapeCacheId("empty")));
  malformed = foreign;
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_transition(before, after, malformed));
  CHECK(malformed.feature_shape_count() == 0);
  auto public_write = baseline(before);
  const auto shape = *public_write.find_feature_shape(FeatureId("base"));
  REQUIRE(public_write.store_feature_shape(FeatureId("base"), shape));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_transition(before, after, public_write));
  CHECK(OcctShapeView::get(*public_write.find_feature_shape(FeatureId("base"))) ==
        OcctShapeView::get(shape));
}

TEST_CASE(
    "Verified transition rejects stale baselines and does not reuse erased executable lifetime",
    "[geometry][verified-transition][physical-untrusted-origin]") {
  const auto before = f::native_base();
  auto cache = baseline(before);
  const auto original_shape = *cache.find_feature_shape(FeatureId("base"));
  auto stale_before = before;
  REQUIRE(
      stale_before.set_parameter_value(ParameterId("depth"), f::checked(Quantity::length_mm(9))));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_transition(stale_before, stale_before, cache));
  REQUIRE(cache.validate_complete_geometry_result(before));
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("base"))) ==
        OcctShapeView::get(original_shape));
  auto replacement = before;
  const auto feature = *replacement.find_feature(FeatureId("base"));
  REQUIRE(replacement.remove_feature(feature.id()));
  REQUIRE(replacement.add_feature(feature));
  REQUIRE(f::checked(serialize_part_document_to_json(replacement)) ==
          f::checked(serialize_part_document_to_json(before)));
  REQUIRE(GeometryRecomputeExecutor{}.execute_transition(before, replacement, cache));
  REQUIRE(cache.validate_complete_geometry_result(replacement));
  CHECK_FALSE(cache.validate_complete_geometry_result(before));
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("base"))) !=
        OcctShapeView::get(original_shape));
  CHECK(volume(cache) == Catch::Approx(1600));
  const auto accepted_shape = *cache.find_feature_shape(FeatureId("base"));
  auto failed = replacement;
  REQUIRE(failed.add_parameter(f::checked(Parameter::create_length(
      ParameterId("failed.radius"), "Impossible radius", f::checked(Quantity::length_mm(80))))));
  REQUIRE(failed.add_fillet_feature(f::checked(FilletFeature::create(
      FeatureId("impossible"), "Impossible", BodyId("body.base"),
      {f::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("failed.radius")))));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_transition(replacement, failed, cache));
  REQUIRE(cache.validate_complete_geometry_result(replacement));
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("base"))) ==
        OcctShapeView::get(accepted_shape));
}
