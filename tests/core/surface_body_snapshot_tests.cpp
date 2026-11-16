#include "../support/curved_surface_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
namespace f = blcad::test::curved_surface;
namespace {
SurfaceOffsetFeature modifier(FeatureBodyOperationMode mode) {
  return f::checked(SurfaceOffsetFeature::create_thicken(
      FeatureId("modify"), "Modify", f::body("quilt.body"),
      {ParameterId("wall"),
       ParameterId("tolerance"),
       SurfaceOffsetSide::Symmetric,
       SurfaceOffsetApproximation::ExactAnalytic,
       {}},
      f::checked(FeatureBodyResultContext::create(mode, BodyId("source.body"), {}))));
}
} // namespace
TEST_CASE(
    "Derived Surface quilts can modify their historical Solid source without a feedback cycle",
    "[core][surface-body-snapshot][parity-b173]") {
  for (const auto mode : {FeatureBodyOperationMode::Join, FeatureBodyOperationMode::Cut,
                          FeatureBodyOperationMode::Intersect}) {
    auto p = f::combination();
    REQUIRE(p.add_surface_feature(modifier(mode)));
    const auto& graph = p.dependency_graph();
    CHECK_FALSE(graph.has_cycle());
    CHECK(graph.has_dependency("source.feature", "left"));
    CHECK_FALSE(graph.has_dependency("body:source.body", "left"));
    CHECK(graph.has_dependency("modify", "body:source.body"));
    CHECK(graph.has_dependency("source.feature", "modify"));
    const auto bytes = blcad::test::surface_offset::saved(p);
    auto reopened = f::checked(deserialize_part_document_from_json(bytes));
    CHECK(blcad::test::surface_offset::saved(reopened) == bytes);
    CHECK_FALSE(reopened.dependency_graph().has_cycle());
    REQUIRE(p.add_body(f::checked(Body::create(BodyId("later.body"), "Later", BodyKind::Surface))));
    auto later = f::checked(TrimSurfaceFeature::create(
        FeatureId("later"), "Later", f::quadrant(),
        f::checked(TrimmingReference::create_profile_region(f::checked(
            ProfileRegionReference::create(SketchId("window.sketch"), ProfileId("window"))))),
        BodyId("later.body")));
    REQUIRE(p.add_surface_feature(later));
    CHECK(p.dependency_graph().has_dependency("body:source.body", "later"));
    REQUIRE(p.update_surface_feature(modifier(mode)));
    CHECK(p.dependency_graph().has_dependency("body:source.body", "later"));

    auto changed =
        p.set_parameter_value(ParameterId("window.height"), f::checked(Quantity::length_mm(6)));
    REQUIRE(changed);
    CHECK(std::find(changed.value().begin(), changed.value().end(), "modify") !=
          changed.value().end());
    CHECK(std::find(changed.value().begin(), changed.value().end(), "later") !=
          changed.value().end());
    const auto before = blcad::test::surface_offset::saved(p);
    const auto original = *p.find_surface_feature(FeatureId("modify"));
    const auto& existing = std::get<SurfaceOffsetFeature>(original);
    auto future = f::checked(SurfaceOffsetFeature::create_thicken(
        existing.id(), existing.name(), f::body("later.body"), existing.parameters(),
        existing.body_result_context()));
    CHECK_FALSE(p.update_surface_feature(future));
    CHECK(blcad::test::surface_offset::saved(p) == before);

    auto cycle = f::checked(TrimSurfaceFeature::create(
        FeatureId("left"), "Cycle", f::body("later.body"), later.trimming(), BodyId("left.body")));
    CHECK_FALSE(p.update_surface_feature(cycle));
    CHECK(blcad::test::surface_offset::saved(p) == before);
  }
}
