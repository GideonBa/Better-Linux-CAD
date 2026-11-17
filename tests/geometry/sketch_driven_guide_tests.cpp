#include "../support/rib_web_fixture.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sketch_driven_guides.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace r = blcad::test::rib_fixture;
TEST_CASE("Rib guides retain physical thickness sides and authored distance units",
          "[geometry][sketch-driven-guides][parity-b184]") {
  for (auto side : {RibThicknessSide::Symmetric, RibThicknessSide::First, RibThicknessSide::Second,
                    RibThicknessSide::Asymmetric}) {
    auto p = r::fixture();
    auto rib = r::intent();
    rib.side = side;
    if (side == RibThicknessSide::Asymmetric)
      rib.second_thickness = ParameterId("rib.second");
    r::add(p, rib);
    auto c = f::checked(ShapeCache::create(ShapeCacheId("guides")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(p, 1, c));
    auto guides = f::checked(
        SketchDrivenGuideQuery::guides(p, p.sketch_driven_features().front(), c, nullptr, 1));
    REQUIRE(guides.size() >= 2);
    const auto thickness = std::find_if(guides.begin(), guides.end(),
                                        [](const auto& g) { return g.id == "thickness"; });
    REQUIRE(thickness != guides.end());
    CHECK(thickness->binding == ParameterId("rib.t"));
    CHECK(thickness->value == 1);
    CHECK(thickness->origin == Point3{0, 0, 0});
    CHECK(thickness->axis == Vector3{0, side == RibThicknessSide::Second ? -1. : 1., 0});
    CHECK(thickness->model_units_per_value == (side == RibThicknessSide::Symmetric ? .5 : 1));
    const auto distance = std::find_if(guides.begin(), guides.end(),
                                       [](const auto& g) { return g.id == "distance"; });
    REQUIRE(distance != guides.end());
    CHECK(distance->value == 12);
    CHECK(distance->axis == Vector3{0, 0, 1});
  }
}
TEST_CASE("Planar depth guides follow raise and recess material normals without kernel types",
          "[geometry][sketch-driven-guides][parity-b184]") {
  for (auto operation : {EmbossOperation::Raise, EmbossOperation::Recess}) {
    auto p = f::native_base();
    EmbossIntent e{
        f::checked(
            ProfileRegionReference::create(SketchId("profile"), ProfileId("profile.profile"))),
        {f::checked(FaceReference::create_planar(
            PartFeatureInputRole::SurfaceSourceFace,
            f::checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))))},
        operation,
        ParameterId("radius"),
        EmbossMapping::Planar,
        {},
        0,
        std::nullopt};
    auto feature = f::checked(SketchDrivenFeature::create(
        FeatureId("label"), "Label", BodyId("body.base"), SketchDrivenDefinition{e}));
    f::accept(p.add_sketch_driven_feature(feature));
    auto c = f::checked(ShapeCache::create(ShapeCacheId("guides")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(p, 1, c));
    auto guides = f::checked(SketchDrivenGuideQuery::guides(p, feature, c, nullptr, 1));
    REQUIRE(guides.size() == 1);
    CHECK(guides.front().origin == Point3{0, 0, 8});
    CHECK(guides.front().axis == Vector3{0, 0, operation == EmbossOperation::Raise ? 1. : -1.});
    CHECK(guides.front().binding == ParameterId("radius"));
    const auto faces =
        f::checked(SketchDrivenGuideQuery::faces(p, c, BodyId("body.base"), nullptr, 1));
    CHECK(faces.size() == 6);
    for (const auto& face : faces) {
      CHECK(face.mapping == EmbossMapping::Planar);
      CHECK(length(face.normal) == Catch::Approx(1));
    }
  }
}
