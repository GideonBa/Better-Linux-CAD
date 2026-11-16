#include "../support/geometry_volume_oracle.hpp"
#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sweep_region_query.hpp"
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
PartDocument tube(FeatureBodyOperationMode mode, bool material = true) {
  auto part = sf::straight_sweep();
  for (auto [id, value] : {std::pair{"outer.diameter", 4.}, {"inner.diameter", 2.}})
    (void)f::checked(part.add_parameter(f::checked(
        Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
  auto profile = f::checked(
      Sketch::create(SketchId("sweep.profile"), "Hollow section", DatumPlaneId("datum.xy")));
  (void)f::checked(profile.add_profile(
      f::checked(CircleProfile::create(ProfileId("outer"), ParameterId("outer.diameter")))));
  (void)f::checked(profile.add_profile(
      f::checked(CircleProfile::create(ProfileId("inner"), ParameterId("inner.diameter")))));
  (void)f::checked(part.update_sketch(profile));
  const auto region = f::checked(ProfileRegionReference::create(
      profile.id(), ProfileId("outer"), PartFeatureInputRole::SweepProfile));
  const auto old = *part.find_sweep_feature(FeatureId("driven.sweep"));
  const auto context =
      mode == FeatureBodyOperationMode::NewBody
          ? old.body_result_context()
          : f::checked(FeatureBodyResultContext::create(mode, BodyId("base.body"), {}));
  const auto create = mode == FeatureBodyOperationMode::Cut ? SweepFeature::create_sweep_cut
                                                            : SweepFeature::create_sweep;
  (void)f::checked(part.update_sweep_feature(f::checked(create(
      old.id(), old.name(),
      f::checked(SweepProfileReference::create_closed_region(
          region, material ? SweepRegionPolicy::MaterialRegion : SweepRegionPolicy::OuterBoundary)),
      old.path(), context, {}, {}, {}, {}))));
  if (mode != FeatureBodyOperationMode::NewBody)
    (void)f::checked(part.remove_body(BodyId("driven.body")));
  return part;
}
bool material_at(const GeometryShape& shape, Point3 p) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  BRepClass3d_SolidClassifier classifier(*native, gp_Pnt(p.x, p.y, p.z), 1e-7);
  REQUIRE(classifier.State() != TopAbs_ON);
  return classifier.State() == TopAbs_IN;
}
} // namespace
TEST_CASE("Material-region Sweep preserves an actual hollow section in every Body operation",
          "[geometry][parity-b167][semantic-sweep-intent][material-region-sweep]") {
  for (auto mode : {FeatureBodyOperationMode::NewBody, FeatureBodyOperationMode::Join,
                    FeatureBodyOperationMode::Cut, FeatureBodyOperationMode::Intersect}) {
    const auto part = tube(mode);
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("tube")));
    const auto run = GeometryRecomputeExecutor{}.execute_document(part, cache);
    INFO((run ? "Computed" : run.error().message()));
    REQUIRE(run);
    const auto body =
        mode == FeatureBodyOperationMode::NewBody ? BodyId("driven.body") : BodyId("base.body");
    const auto* result = cache.find_body_shape(body);
    REQUIRE(result);
    const auto summary = RectangleExtrusionAdapter{}.summarize(*result);
    const double pi = std::numbers::pi;
    const double volume = mode == FeatureBodyOperationMode::NewBody ? 30 * pi
                          : mode == FeatureBodyOperationMode::Join  ? 240 + 15 * pi
                          : mode == FeatureBodyOperationMode::Cut   ? 240 - 15 * pi
                                                                    : 15 * pi;
    CHECK(summary.volume_mm3 == Catch::Approx(volume).margin(1e-6));
    REQUIRE(OcctShapeView::get(*result));
    CHECK(BRepCheck_Analyzer(*OcctShapeView::get(*result)).IsValid());
    CHECK(material_at(*result, {0, 0, 2}) ==
          (mode == FeatureBodyOperationMode::Join || mode == FeatureBodyOperationMode::Cut));
    CHECK(material_at(*result, {1.5, 0, 2}) == (mode != FeatureBodyOperationMode::Cut));
    CHECK(material_at(*result, {0, 0, 7}) == false);
    CHECK(material_at(*result, {1.5, 0, 7}) ==
          (mode == FeatureBodyOperationMode::NewBody || mode == FeatureBodyOperationMode::Join));
    const auto saved = f::checked(serialize_part_document_to_json(part));
    const auto reopened = f::checked(deserialize_part_document_from_json(saved));
    auto fresh = f::checked(ShapeCache::create(ShapeCacheId("fresh.tube")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(reopened, fresh));
    REQUIRE(fresh.find_body_shape(body));
    CHECK(RectangleExtrusionAdapter{}.summarize(*fresh.find_body_shape(body)).volume_mm3 ==
          Catch::Approx(volume).margin(1e-6));
    CHECK(f::checked(serialize_part_document_to_json(reopened)) == saved);
  }
}
TEST_CASE("Legacy outer-boundary Sweep does not silently acquire new holes",
          "[geometry][parity-b167][semantic-sweep-intent][material-region-sweep]") {
  const auto part = tube(FeatureBodyOperationMode::NewBody, false);
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("legacy.tube")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.find_body_shape(BodyId("driven.body")));
  const auto& shape = *cache.find_body_shape(BodyId("driven.body"));
  CHECK(RectangleExtrusionAdapter{}.summarize(shape).volume_mm3 ==
        Catch::Approx(40 * std::numbers::pi).margin(1e-6));
  CHECK(material_at(shape, {0, 0, 7}));
}
TEST_CASE("A touching inner profile rejects the candidate without filling the accepted void",
          "[geometry][parity-b167][semantic-sweep-intent][material-region-sweep]") {
  auto part = tube(FeatureBodyOperationMode::NewBody);
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("invalid.tube")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto accepted = part;
  REQUIRE(
      part.set_parameter_value(ParameterId("inner.diameter"), f::checked(Quantity::length_mm(4))));
  const auto source = f::checked(serialize_part_document_to_json(part));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  CHECK(cache.validate_complete_geometry_result(accepted));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
  CHECK(f::checked(serialize_part_document_to_json(part)) == source);
  REQUIRE(cache.find_body_shape(BodyId("driven.body")));
  CHECK_FALSE(material_at(*cache.find_body_shape(BodyId("driven.body")), {0, 0, 7}));
}
TEST_CASE("Exact rational inner contours and separate holes retain their material sections",
          "[geometry][parity-b167][semantic-sweep-intent][material-region-sweep]") {
  for (bool rational : {false, true}) {
    auto part = tube(FeatureBodyOperationMode::NewBody);
    auto sketch = f::checked(
        Sketch::create(SketchId("sweep.profile"), "Exact voids", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(
        f::checked(CircleProfile::create(ProfileId("outer"), ParameterId("outer.diameter")))));
    double expected = 30 * std::numbers::pi;
    if (rational) {
      const double c = std::sqrt(.5), w = (1 + 2 * c) / 3, h = 2 * c / (1 + 2 * c);
      std::vector<SketchEntityId> ids;
      for (int i = 0; i < 4; ++i) {
        const auto rotate = [&](Point2 p) {
          for (int n = 0; n < i; ++n)
            p = {-p.y, p.x};
          return p;
        };
        const auto id = SketchEntityId("rational." + std::to_string(i));
        REQUIRE(sketch.add_entity(f::checked(SplineSegment::create_cubic_bezier(
            id, rotate({1, 0}), rotate({1, h}), rotate({h, 1}), rotate({0, 1}), {1, w, w, 1}))));
        ids.push_back(id);
      }
      REQUIRE(sketch.add_profile(
          f::checked(ArcClosedProfile::create(ProfileId("rational.hole"), ids))));
    } else {
      REQUIRE(part.add_parameter(f::checked(Parameter::create_length(
          ParameterId("small.diameter"), "Small", f::checked(Quantity::length_mm(.7))))));
      for (double x : {-.8, .8})
        REQUIRE(sketch.add_profile(f::checked(CircleProfile::create(
            ProfileId(x < 0 ? "left" : "right"), ParameterId("small.diameter"), {x, 0}))));
      expected = 37.55 * std::numbers::pi;
    }
    REQUIRE(part.update_sketch(sketch));
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("exact.holes")));
    const auto run = GeometryRecomputeExecutor{}.execute_document(part, cache);
    INFO((run ? "Computed" : run.error().message()));
    REQUIRE(run);
    const auto* shape = cache.find_body_shape(BodyId("driven.body"));
    REQUIRE(shape);
    CHECK(RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
          Catch::Approx(expected).margin(2e-6));
    if (rational)
      for (int i = 0; i < 16; ++i) {
        const double a = 2 * std::numbers::pi * i / 16;
        CHECK_FALSE(material_at(*shape, {.9 * std::cos(a), .9 * std::sin(a), 7}));
        CHECK(material_at(*shape, {1.1 * std::cos(a), 1.1 * std::sin(a), 7}));
      }
    else {
      CHECK_FALSE(material_at(*shape, {-.8, 0, 7}));
      CHECK_FALSE(material_at(*shape, {.8, 0, 7}));
      CHECK(material_at(*shape, {0, 0, 7}));
    }
  }
}
TEST_CASE("Crossing and overlapping void boundaries fail instead of disappearing",
          "[geometry][parity-b167][semantic-sweep-intent][material-region-sweep]") {
  for (bool crosses_outer : {false, true}) {
    auto part = tube(FeatureBodyOperationMode::NewBody);
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("crossing.holes")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    const auto accepted = part;
    auto sketch =
        f::checked(Sketch::create(SketchId("sweep.profile"), "Crossing", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(
        f::checked(CircleProfile::create(ProfileId("outer"), ParameterId("outer.diameter")))));
    REQUIRE(sketch.add_profile(f::checked(CircleProfile::create(
        ProfileId("inner"), ParameterId("inner.diameter"), {crosses_outer ? 1.5 : -.25, 0}))));
    if (!crosses_outer)
      REQUIRE(sketch.add_profile(f::checked(
          CircleProfile::create(ProfileId("other"), ParameterId("inner.diameter"), {.25, 0}))));
    REQUIRE(part.update_sketch(sketch));
    const auto run = GeometryRecomputeExecutor{}.execute_document(part, cache);
    REQUIRE_FALSE(run);
    CHECK(run.error().message().find("touch or intersect") != std::string::npos);
    CHECK(cache.validate_complete_geometry_result(accepted));
  }
}
namespace {
PartDocument closed_tube(PathOrientationRule mode, double twist, bool rectangle = false) {
  auto part = tube(FeatureBodyOperationMode::NewBody);
  (void)f::checked(part.add_construction_plane(f::checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("circle.section"), "Circle normal",
                                         {20, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}))));
  auto section = f::checked(
      Sketch::create(SketchId("sweep.profile"), "Cyclic section", DatumPlaneId("circle.section")));
  if (rectangle) {
    (void)f::checked(part.add_parameter(f::checked(Parameter::create_length(
        ParameterId("short.side"), "Short side", f::checked(Quantity::length_mm(.4))))));
    (void)f::checked(section.add_profile(f::checked(RectangleProfile::create(
        ProfileId("outer"), ParameterId("profile.side"), ParameterId("short.side")))));
  } else
    for (auto id : {"outer", "inner"})
      (void)f::checked(section.add_profile(f::checked(
          CircleProfile::create(ProfileId(id), ParameterId(std::string(id) + ".diameter")))));
  (void)f::checked(part.update_sketch(section));
  auto spine = f::checked(
      Sketch::create(SketchId("circle.spine"), "Circle spine", DatumPlaneId("datum.xy")));
  std::vector<PathSegmentReference> segments;
  const auto at = [](double angle) { return Point2{20 * std::cos(angle), 20 * std::sin(angle)}; };
  for (int i = 0; i < 4; ++i) {
    const auto id = SketchEntityId("quarter." + std::to_string(i));
    (void)f::checked(spine.add_entity(f::checked(ArcSegment::create_three_point(
        id, at(i * std::numbers::pi / 2), at((i + .5) * std::numbers::pi / 2),
        at((i + 1) * std::numbers::pi / 2)))));
    segments.push_back(
        f::checked(PathSegmentReference::create_planar(spine.id(), id, PlanarPathCurveKind::Arc)));
  }
  (void)f::checked(part.add_sketch(spine));
  (void)f::checked(part.add_path_curve(f::checked(
      PathCurve::create(PathCurveId("closed.path"), "Closed circle", segments, PathClosure::Closed,
                        PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                        f::checked(PathExecutionPolicy::curve_preserving(1e-5))))));
  (void)f::checked(
      part.set_parameter_value(ParameterId("angle"), f::checked(Quantity::angle_deg(twist))));
  const auto old = *part.find_sweep_feature(FeatureId("driven.sweep"));
  (void)f::checked(part.update_sweep_feature(f::checked(SweepFeature::create_sweep(
      old.id(), old.name(), old.profile(), PathCurveId("closed.path"), old.body_result_context(),
      mode,
      mode == PathOrientationRule::FixedUpVector ? std::optional<Vector3>{{0, 0, 1}} : std::nullopt,
      ParameterId("angle")))));
  return part;
}
Point3 torus_point(double fraction, double radial, double z) {
  const double a = 2 * std::numbers::pi * fraction;
  return {(20 + radial) * std::cos(a), (20 + radial) * std::sin(a), z};
}
} // namespace
TEST_CASE("Closed hollow Sweep preserves toroidal sections and cyclic frames under full-turn roll",
          "[geometry][parity-b167][semantic-sweep-intent][closed-sweep]") {
  for (auto mode : {PathOrientationRule::ProfileNormal, PathOrientationRule::MinimumTwist,
                    PathOrientationRule::FixedUpVector})
    for (double twist : {0., 360., -360.}) {
      CAPTURE(mode, twist);
      const auto part = closed_tube(mode, twist);
      auto cache = f::checked(ShapeCache::create(ShapeCacheId("closed.tube")));
      const auto run = GeometryRecomputeExecutor{}.execute_document(part, cache);
      INFO((run ? "Computed" : run.error().message()));
      REQUIRE(run);
      const auto* shape = cache.find_body_shape(BodyId("driven.body"));
      REQUIRE(shape);
      const auto summary = RectangleExtrusionAdapter{}.summarize(*shape);
      CHECK(summary.solid_count == 1);
      CHECK(summary.volume_mm3 ==
            Catch::Approx(120 * std::numbers::pi * std::numbers::pi).margin(2e-4));
      for (double fraction : {0., .0001, .125, .25, .5, .75, .9999}) {
        CHECK_FALSE(material_at(*shape, torus_point(fraction, 0, 0)));
        CHECK(material_at(*shape, torus_point(fraction, 1.5, 0)));
        CHECK_FALSE(material_at(*shape, torus_point(fraction, 2.1, 0)));
        CHECK(blcad::test::distance_to_faces(*shape, torus_point(fraction, 1, 0)) < 2e-5);
        CHECK(blcad::test::distance_to_faces(*shape, torus_point(fraction, 0, 2)) < 2e-5);
      }
    }
}
TEST_CASE("Closed rectangular Sweep exposes actual roll and seam drift through physical boundaries",
          "[geometry][parity-b167][semantic-sweep-intent][closed-sweep]") {
  for (auto mode : {PathOrientationRule::ProfileNormal, PathOrientationRule::MinimumTwist,
                    PathOrientationRule::FixedUpVector})
    for (double twist : {0., 360., -360., 720.}) {
      const auto part = closed_tube(mode, twist, true);
      auto cache = f::checked(ShapeCache::create(ShapeCacheId("closed.rectangle")));
      const auto run = GeometryRecomputeExecutor{}.execute_document(part, cache);
      INFO((run ? "Computed" : run.error().message()));
      REQUIRE(run);
      const auto* shape = cache.find_body_shape(BodyId("driven.body"));
      REQUIRE(shape);
      CHECK(RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
            Catch::Approx(12.8 * std::numbers::pi).margin(2e-4));
      for (double fraction : {0., .0001, .0625, .1875, .375, .625, .875, .9999})
        for (const auto p : std::array<Point2, 6>{
                 {{-.4, -.2}, {.4, -.2}, {.4, .2}, {-.4, .2}, {.4, 0}, {0, .2}}}) {
          const double a = twist * std::numbers::pi / 180 * fraction;
          CHECK(blcad::test::distance_to_faces(
                    *shape, torus_point(fraction, p.x * std::cos(a) + p.y * std::sin(a),
                                        -p.x * std::sin(a) + p.y * std::cos(a))) < 2e-5);
        }
    }
}
TEST_CASE(
    "Closed Sweep rejects fractional roll and spindle folds without replacing accepted geometry",
    "[geometry][parity-b167][semantic-sweep-intent][closed-sweep]") {
  for (bool fractional : {false, true}) {
    auto part = closed_tube(PathOrientationRule::MinimumTwist, 0);
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("closed.invalid")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    const auto accepted = part;
    if (fractional)
      REQUIRE(part.set_parameter_value(ParameterId("angle"), f::checked(Quantity::angle_deg(90))));
    else
      REQUIRE(part.set_parameter_value(ParameterId("outer.diameter"),
                                       f::checked(Quantity::length_mm(44))));
    const auto source = f::checked(serialize_part_document_to_json(part));
    const auto run = GeometryRecomputeExecutor{}.execute_document(part, cache);
    REQUIRE_FALSE(run);
    CHECK(run.error().message().find(fractional ? "integer" : "self-intersect") !=
          std::string::npos);
    CHECK(cache.validate_complete_geometry_result(accepted));
    CHECK_FALSE(cache.validate_complete_geometry_result(part));
    CHECK(f::checked(serialize_part_document_to_json(part)) == source);
    const auto reopened = f::checked(
        deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(accepted))));
    auto fresh = f::checked(ShapeCache::create(ShapeCacheId("closed.reopen")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(reopened, fresh));
    REQUIRE(fresh.find_body_shape(BodyId("driven.body")));
    CHECK(RectangleExtrusionAdapter{}
              .summarize(*fresh.find_body_shape(BodyId("driven.body")))
              .volume_mm3 == Catch::Approx(120 * std::numbers::pi * std::numbers::pi).margin(2e-4));
  }
}
TEST_CASE("Closed Sweep rejects competing guides and a noncyclic fixed-up capability",
          "[geometry][parity-b167][semantic-sweep-intent][closed-sweep]") {
  for (bool guide : {false, true}) {
    auto part = closed_tube(PathOrientationRule::MinimumTwist, 0);
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("closed.controls")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    const auto accepted = part;
    const auto old = *part.find_sweep_feature(FeatureId("driven.sweep"));
    REQUIRE(part.update_sweep_feature(f::checked(SweepFeature::create_sweep(
        old.id(), old.name(), old.profile(), old.path(), old.body_result_context(),
        guide ? PathOrientationRule::MinimumTwist : PathOrientationRule::FixedUpVector,
        guide ? std::nullopt : std::optional<Vector3>{{1, 0, 0}}, old.twist_parameter(),
        guide ? std::optional<PathCurveId>{PathCurveId("driven.path")} : std::nullopt))));
    const auto run = GeometryRecomputeExecutor{}.execute_document(part, cache);
    REQUIRE_FALSE(run);
    CHECK(run.error().message().find(guide ? "authored guide" : "parallel to the circle axis") !=
          std::string::npos);
    CHECK(cache.validate_complete_geometry_result(accepted));
  }
}
TEST_CASE("Closed Sweep does not coalesce authored near-circles into an undeclared exact circle",
          "[geometry][parity-b167][semantic-sweep-intent][closed-sweep]") {
  auto part = closed_tube(PathOrientationRule::MinimumTwist, 0);
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("near.circle")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto accepted = part;
  auto spine =
      f::checked(Sketch::create(SketchId("circle.spine"), "Near circle", DatumPlaneId("datum.xy")));
  const auto at = [](double angle, double radius = 20.) {
    return Point2{radius * std::cos(angle), radius * std::sin(angle)};
  };
  for (int i = 0; i < 4; ++i)
    REQUIRE(spine.add_entity(f::checked(ArcSegment::create_three_point(
        SketchEntityId("quarter." + std::to_string(i)), at(i * std::numbers::pi / 2),
        at((i + .5) * std::numbers::pi / 2, i == 0 ? 20.000001 : 20.),
        at((i + 1) * std::numbers::pi / 2)))));
  REQUIRE(part.update_sketch(spine));
  const auto run = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(run);
  CHECK(run.error().message().find("same circle") != std::string::npos);
  CHECK(cache.validate_complete_geometry_result(accepted));
}

TEST_CASE("Sweep material inspection reports native holes and rejects touching contours",
          "[geometry][material-region-sweep][parity-b173]") {
  auto p = tube(FeatureBodyOperationMode::NewBody);
  auto holes = inspect_sweep_material_holes(p, SketchId("sweep.profile"), ProfileId("outer"));
  REQUIRE(holes);
  REQUIRE(holes.value().size() == 1);
  CHECK(holes.value().front() == ProfileId("inner"));
  f::checked(
      p.set_parameter_value(ParameterId("inner.diameter"), f::checked(Quantity::length_mm(4))));
  auto touching = inspect_sweep_material_holes(p, SketchId("sweep.profile"), ProfileId("outer"));
  REQUIRE_FALSE(touching);
  CHECK(touching.error().message().find("touch or intersect") != std::string::npos);
  CHECK_FALSE(inspect_sweep_material_holes(p, SketchId("missing"), ProfileId("outer")));
}
