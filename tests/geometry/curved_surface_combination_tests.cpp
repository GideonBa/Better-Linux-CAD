#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/surface_boundary_query.hpp"
#include "imported_body_test_fixture.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::curved_surface;
namespace {
void execute(PartDocument& p, ShapeCache& cache) {
  const auto result = GeometryRecomputeExecutor{}.execute_document(p, cache);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
}
double expected(double height = 4, double length = 3) {
  return (20 * std::acos(.8) + length) * (height + length) - .04 * std::numbers::pi;
}
void physical(const GeometryShape& shape, double volume) {
  CHECK(blcad::test::integrated_solid_volume(shape).volume_mm3 ==
        Catch::Approx(volume).margin(1e-5));
  CHECK(blcad::test::classify_solid_point(shape, {10, 0, 10}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(shape, {7.1, 7.2, 11.5}) ==
        blcad::test::SolidPointLocation::Outside);
  CHECK(blcad::test::classify_solid_point(shape, {7.1, 7.2, 10}) ==
        blcad::test::SolidPointLocation::Inside);
}
} // namespace
TEST_CASE("G04 seam trim, curved and tangent extension, Stitch and Thicken carry a bore through "
          "edits and STEP",
          "[geometry][curved-surface-combination][parity-b172]") {
  auto p = f::combination();
  auto cache = blcad::test::surface_offset::cache();
  execute(p, cache);
  CHECK(blcad::test::integrated_solid_volume(*cache.find_body_shape(BodyId("thick.body")))
            .volume_mm3 == Catch::Approx(expected() + .04 * std::numbers::pi).margin(1e-5));
  const auto boundaries = f::checked(
      SurfaceBoundaryQuery::inspect({*cache.find_body_shape(BodyId("quilt.body"))}, 1e-6, .01));
  CHECK(boundaries.non_manifold_edges == 0);
  CHECK(!boundaries.free_edges.empty());
  const auto faces =
      f::checked(ResultFaceSupportGeometry::selectable_faces(p, cache, BodyId("thick.body")));
  REQUIRE(faces.size() == 2);
  const auto outer = std::max_element(faces.begin(), faces.end(), [](const auto& a, const auto& b) {
    return .8 * a.anchor.x + .6 * a.anchor.y < .8 * b.anchor.x + .6 * b.anchor.y;
  });
  REQUIRE(p.add_result_face_workplane(f::checked(
      ResultFaceWorkplane::create(DatumPlaneId("quilt.skin"), "Quilt skin", outer->reference))));
  f::length(p, "bore.diameter", .4);
  const auto support = f::checked(PlaneReference::create_datum_plane(
      PartFeatureInputRole::HoleSupportPlane, DatumPlaneId("quilt.skin")));
  REQUIRE(p.add_hole_feature(f::checked(HoleFeature::create(
      FeatureId("bore"), "Bore", HoleKind::Plain, BodyId("thick.body"), support,
      {{HolePlacementId("center"), Point2{0, 0}}}, ParameterId("bore.diameter")))));
  execute(p, cache);
  physical(*cache.find_body_shape(BodyId("thick.body")), expected());
  const auto before = blcad::test::surface_offset::saved(p);
  REQUIRE(p.set_parameter_value(ParameterId("window.height"), f::checked(Quantity::length_mm(6))));
  execute(p, cache);
  physical(*cache.find_body_shape(BodyId("thick.body")), expected(6));
  const auto changed = blcad::test::surface_offset::saved(p);
  auto reopened = f::checked(deserialize_part_document_from_json(changed));
  auto fresh = blcad::test::surface_offset::cache();
  execute(reopened, fresh);
  CHECK(blcad::test::surface_offset::saved(reopened) == changed);
  physical(*fresh.find_body_shape(BodyId("thick.body")), expected(6));
  auto undo = f::checked(deserialize_part_document_from_json(before));
  auto undo_cache = blcad::test::surface_offset::cache();
  execute(undo, undo_cache);
  physical(*undo_cache.find_body_shape(BodyId("thick.body")), expected());
  imported_body_test::Fixture files;
  const auto output = files.root / "curved-quilt.step";
  REQUIRE(StepExporter{}.write_step(*fresh.find_body_shape(BodyId("thick.body")), output.string()));
  const auto source = f::checked(
      StepImportSource::create(StepImportSourceId("g04"), "G04", output.filename().string(),
                               f::checked(step_import_sha256(imported_body_test::bytes(output))),
                               StepImportMode::Reference));
  const auto imported = f::checked(StepImportReader{}.read(source, files.root));
  const auto shapes = f::checked(StepImportReader{}.preview_shapes(imported));
  REQUIRE(shapes.size() == 1);
  physical(shapes.front(), expected(6));
}

TEST_CASE(
    "Historical quilts modify their source Solid and survive contour edits and exact reopening",
    "[geometry][surface-body-snapshot][curved-surface-combination][parity-b173]") {
  for (const auto mode : {FeatureBodyOperationMode::Join, FeatureBodyOperationMode::Cut,
                          FeatureBodyOperationMode::Intersect}) {
    auto p = f::combination();
    auto cache = blcad::test::surface_offset::cache();
    execute(p, cache);
    const auto source = *cache.find_feature_shape(FeatureId("source.feature"));
    const auto modifier = f::checked(SurfaceOffsetFeature::create_thicken(
        FeatureId("modify"), "Modify", f::body("quilt.body"),
        {ParameterId("wall"),
         ParameterId("tolerance"),
         SurfaceOffsetSide::Symmetric,
         SurfaceOffsetApproximation::ExactAnalytic,
         {}},
        f::checked(FeatureBodyResultContext::create(mode, BodyId("source.body"), {}))));
    REQUIRE(p.add_surface_feature(modifier));
    execute(p, cache);
    const double outside = 20 * std::acos(.8) + 3;
    const double inside =
        9.75 * std::acos(.8) + .5 * (3 * std::sqrt(91.) + 100 * std::asin(.3)) - 28.5;
    const auto expected = [&](double height) {
      const double base = 2000 * std::numbers::pi;
      return mode == FeatureBodyOperationMode::Join  ? base + (outside - inside) * height
             : mode == FeatureBodyOperationMode::Cut ? base - inside * height
                                                     : inside * height;
    };
    const auto check = [&](const ShapeCache& c, double height) {
      CHECK(blcad::test::integrated_solid_volume(*c.find_body_shape(BodyId("source.body")))
                .volume_mm3 == Catch::Approx(expected(height)).margin(5e-5));
      CHECK(c.find_body_result(BodyId("source.body"))->source_feature_id == FeatureId("modify"));
      CHECK(blcad::test::integrated_solid_volume(*c.find_feature_shape(FeatureId("source.feature")))
                .volume_mm3 == Catch::Approx(2000 * std::numbers::pi).margin(1e-6));
      CHECK(blcad::test::integrated_solid_volume(source).volume_mm3 ==
            Catch::Approx(2000 * std::numbers::pi).margin(1e-6));
    };
    check(cache, 7);
    REQUIRE(
        p.set_parameter_value(ParameterId("window.height"), f::checked(Quantity::length_mm(6))));
    execute(p, cache);
    check(cache, 9);
    auto reopened =
        f::checked(deserialize_part_document_from_json(blcad::test::surface_offset::saved(p)));
    auto fresh = blcad::test::surface_offset::cache();
    execute(reopened, fresh);
    check(fresh, 9);
  }
}
