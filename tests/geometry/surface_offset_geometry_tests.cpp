#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/surface_adapter.hpp"
#include "blcad/geometry/surface_boundary_query.hpp"
#include "blcad/geometry/surface_offset_adapter.hpp"
#include "blcad/geometry/surface_selection_geometry.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <gp_Lin.hxx>
#include <numbers>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace {
void length(PartDocument& p, const char* id, double value) {
  (void)f::checked(p.add_parameter(f::checked(
      Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
}
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("surface.offset")));
}
std::string brep(const GeometryShape& shape) {
  std::ostringstream text;
  BRepTools::Write(*OcctShapeView::get(shape), text);
  return text.str();
}
double volume(const GeometryShape& shape) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  REQUIRE(BRepCheck_Analyzer(*native, true).IsValid());
  GProp_GProps properties;
  BRepGProp::VolumeProperties(*native, properties);
  return properties.Mass();
}
SurfaceReference plane() {
  return f::checked(SurfaceReference::create_semantic_face(
      f::checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))));
}
SurfaceReference cylinder(PartDocument& p) {
  length(p, "diameter", 20);
  auto s = f::checked(Sketch::create(SketchId("circle"), "Circle", DatumPlaneId("datum.xy")));
  (void)f::checked(s.add_profile(
      f::checked(CircleProfile::create(ProfileId("outline"), ParameterId("diameter")))));
  (void)f::checked(p.add_sketch(s));
  (void)f::checked(
      p.add_body(f::checked(Body::create(BodyId("cylinder"), "Cylinder", BodyKind::Solid))));
  auto feature = f::checked(Feature::create_additive_extrude(FeatureId("cylinder"), "Cylinder",
                                                             s.id(), ParameterId("depth")));
  (void)f::checked(p.add_feature(
      f::checked(feature.with_body_result_context(f::checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::NewBody, {}, BodyId("cylinder")))))));
  std::vector<FaceReference> walls;
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    const auto role = std::string("extrude/circle/outline") +
                      (quadrant == 0 ? "/wall" : "/wall/quadrant/" + std::to_string(quadrant));
    walls.push_back(f::checked(FaceReference::create_result(
        PartFeatureInputRole::SurfaceSourceFace,
        f::checked(ResultFaceReference::create(FeatureId("cylinder"), BodyId("cylinder"),
                                               {{FeatureId("cylinder"), role}})))));
  }
  return f::checked(SurfaceReference::create_faces(BodyId("cylinder"), walls));
}
void check_report(const SurfaceConstructionResult& result, double distance) {
  CHECK(result.verification.requested_mm == distance);
  CHECK(result.verification.minimum_mm == Catch::Approx(distance).margin(1e-6));
  CHECK(result.verification.maximum_mm == Catch::Approx(distance).margin(1e-6));
  CHECK(result.verification.maximum_error_mm <= result.verification.permitted_error_mm);
  CHECK(result.verification.stations >= 300);
  CHECK(result.verification.before_body_operation);
}
} // namespace
TEST_CASE("G03 plane offsets and symmetric skins have independent physical positions and volume",
          "[geometry][surface-offset-geometry][parity-b171]") {
  auto p = f::part();
  auto c = cache();
  GeometryRecomputeExecutor execute;
  REQUIRE(execute.execute_document(p, c));
  const auto source =
      f::checked(SurfaceSelectionGeometry::resolve(p, c, plane(), FeatureId("select"), 1e-6));
  const auto original = brep(source);
  for (const auto side : {SurfaceOffsetSide::AlongNormal, SurfaceOffsetSide::OppositeNormal,
                          SurfaceOffsetSide::Symmetric})
    for (const bool thick : {false, true}) {
      CAPTURE(side, thick);
      auto result = f::checked(SurfaceOffsetAdapter{}.construct(
          FeatureId("offset"), source,
          thick ? SurfaceFeatureKind::Thicken : SurfaceFeatureKind::OffsetSurface, 3, side, 1e-6,
          SurfaceOffsetApproximation::ExactAnalytic));
      check_report(result, 3);
      if (thick)
        CHECK(volume(result.shape) == Catch::Approx(8 * 6 * 3).margin(1e-6));
      else
        CHECK(RectangleExtrusionAdapter{}.summarize(result.shape).solid_count == 0);
      std::vector<double> levels;
      for (TopExp_Explorer it(*OcctShapeView::get(result.shape), TopAbs_FACE); it.More();
           it.Next()) {
        BRepAdaptor_Surface face(TopoDS::Face(it.Current()));
        if (face.GetType() == GeomAbs_Plane && std::abs(face.Plane().Axis().Direction().Z()) > .99)
          levels.push_back(face.Plane().Location().Z());
      }
      REQUIRE(!levels.empty());
      const double low = side == SurfaceOffsetSide::Symmetric
                             ? 3.5
                             : (side == SurfaceOffsetSide::OppositeNormal ? 2 : 5);
      const double high = side == SurfaceOffsetSide::Symmetric
                              ? 6.5
                              : (side == SurfaceOffsetSide::OppositeNormal ? 5 : 8);
      if (thick || side == SurfaceOffsetSide::Symmetric) {
        CHECK(*std::min_element(levels.begin(), levels.end()) == Catch::Approx(low));
        CHECK(*std::max_element(levels.begin(), levels.end()) == Catch::Approx(high));
      } else
        CHECK(levels.front() == Catch::Approx(side == SurfaceOffsetSide::OppositeNormal ? 2 : 8));
      CHECK(brep(source) == original);
    }
}
TEST_CASE("G03 cylinders retain analytic radii, positive volume and reject inward collapse",
          "[geometry][surface-offset-geometry][parity-b171]") {
  auto p = f::part();
  REQUIRE(p.set_parameter_value(ParameterId("depth"), f::checked(Quantity::length_mm(20))));
  const auto reference = cylinder(p);
  auto c = cache();
  GeometryRecomputeExecutor execute;
  REQUIRE(execute.execute_document(p, c));
  const auto owner_before = brep(*c.find_body_shape(BodyId("cylinder")));
  const auto source =
      f::checked(SurfaceSelectionGeometry::resolve(p, c, reference, FeatureId("select"), 1e-6));
  CHECK(brep(*c.find_body_shape(BodyId("cylinder"))) == owner_before);
  const auto original = brep(source);
  for (const auto side : {SurfaceOffsetSide::AlongNormal, SurfaceOffsetSide::OppositeNormal,
                          SurfaceOffsetSide::Symmetric}) {
    auto result = f::checked(
        SurfaceOffsetAdapter{}.construct(FeatureId("thicken"), source, SurfaceFeatureKind::Thicken,
                                         3, side, 1e-6, SurfaceOffsetApproximation::ExactAnalytic));
    check_report(result, 3);
    const double inner = side == SurfaceOffsetSide::Symmetric
                             ? 8.5
                             : (side == SurfaceOffsetSide::OppositeNormal ? 7 : 10);
    const double outer = side == SurfaceOffsetSide::Symmetric
                             ? 11.5
                             : (side == SurfaceOffsetSide::OppositeNormal ? 10 : 13);
    CHECK(volume(result.shape) ==
          Catch::Approx(std::numbers::pi * (outer * outer - inner * inner) * 20).margin(1e-5));
    std::vector<double> radii;
    for (TopExp_Explorer it(*OcctShapeView::get(result.shape), TopAbs_FACE); it.More(); it.Next()) {
      BRepAdaptor_Surface face(TopoDS::Face(it.Current()));
      if (face.GetType() == GeomAbs_Cylinder)
        radii.push_back(face.Cylinder().Radius());
    }
    REQUIRE(radii.size() == 8);
    CHECK(*std::min_element(radii.begin(), radii.end()) == Catch::Approx(inner));
    CHECK(*std::max_element(radii.begin(), radii.end()) == Catch::Approx(outer));
    CHECK(brep(source) == original);
  }
  for (const double distance : {10., 10.01, 11.}) {
    auto result = SurfaceOffsetAdapter{}.construct(
        FeatureId("impossible"), source, SurfaceFeatureKind::Thicken, distance,
        SurfaceOffsetSide::OppositeNormal, 1e-6, SurfaceOffsetApproximation::ExactAnalytic);
    CHECK_FALSE(result);
    CHECK(brep(source) == original);
  }
}
TEST_CASE("G03 Thicken reuses all Body modes and preserves exact JSON and tool verification",
          "[geometry][surface-offset-geometry][surface-offset-history][parity-b171]") {
  for (const auto mode : {FeatureBodyOperationMode::NewBody, FeatureBodyOperationMode::Join,
                          FeatureBodyOperationMode::Cut, FeatureBodyOperationMode::Intersect}) {
    CAPTURE(mode);
    auto p = f::part();
    length(p, "tolerance", 1e-6);
    REQUIRE(p.set_parameter_value(ParameterId("distance"), f::checked(Quantity::length_mm(3))));
    if (mode == FeatureBodyOperationMode::NewBody)
      REQUIRE(p.add_body(f::checked(Body::create(BodyId("thick.body"), "Thick", BodyKind::Solid))));
    auto context = f::checked(FeatureBodyResultContext::create(
        mode,
        mode == FeatureBodyOperationMode::NewBody ? std::nullopt
                                                  : std::optional(BodyId("base.body")),
        mode == FeatureBodyOperationMode::NewBody ? std::optional(BodyId("thick.body"))
                                                  : std::nullopt));
    auto feature = f::checked(SurfaceOffsetFeature::create_thicken(
        FeatureId("thick"), "Thicken", plane(),
        {ParameterId("distance"), ParameterId("tolerance"), SurfaceOffsetSide::Symmetric,
         SurfaceOffsetApproximation::ExactAnalytic, std::nullopt},
        context));
    REQUIRE(p.add_surface_feature(feature));
    auto c = cache();
    GeometryRecomputeExecutor execute;
    const auto result = execute.execute_document(p, c);
    INFO((result ? "ok" : result.error().message()));
    REQUIRE(result);
    const auto* shape = c.find_body_shape(context.effective_produced_body());
    REQUIRE(shape);
    const double expected = mode == FeatureBodyOperationMode::NewBody ? 144
                            : mode == FeatureBodyOperationMode::Join  ? 312
                            : mode == FeatureBodyOperationMode::Cut   ? 168
                                                                      : 72;
    CHECK(volume(*shape) == Catch::Approx(expected).margin(1e-6));
    const auto report = SurfaceOffsetAdapter::verification(*shape);
    REQUIRE(report);
    CHECK(report->criterion == SurfaceDistanceCriterion::WallThickness);
    CHECK(report->before_body_operation);
    CHECK(report->requested_mm == 3);
    const auto bytes = f::checked(serialize_part_document_to_json(p));
    auto reopened = f::checked(deserialize_part_document_from_json(bytes));
    auto fresh = cache();
    REQUIRE(execute.execute_document(reopened, fresh));
    CHECK(volume(*fresh.find_body_shape(context.effective_produced_body())) ==
          Catch::Approx(expected).margin(1e-6));
    CHECK(f::checked(serialize_part_document_to_json(reopened)) == bytes);
  }
}
TEST_CASE("G03 perforated source retains its inner opening and rejects disconnected selections",
          "[geometry][surface-offset-geometry][surface-offset-holes][parity-b171]") {
  auto p = f::part();
  length(p, "hole.diameter", 2);
  auto sketch =
      f::checked(Sketch::create(SketchId("bore.sketch"), "Bore", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      f::checked(CircleProfile::create(ProfileId("bore"), ParameterId("hole.diameter")))));
  REQUIRE(p.add_sketch(sketch));
  auto cut = f::checked(Feature::create_subtractive_extrude(FeatureId("bore"), "Bore", sketch.id(),
                                                            FeatureId("base.feature")));
  const auto cut_context = f::checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::Cut, BodyId("base.body"), std::nullopt));
  REQUIRE(p.add_feature(f::checked(cut.with_body_result_context(cut_context))));
  auto c = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, c));
  auto reference = f::checked(SurfaceReference::create_faces(
      BodyId("base.body"), {f::checked(FaceReference::create_result(
                               PartFeatureInputRole::SurfaceSourceFace,
                               f::checked(ResultFaceReference::create(
                                   FeatureId("bore"), BodyId("base.body"),
                                   {{FeatureId("base.feature"), "extrude/upper-cap"}}))))}));
  const auto source =
      f::checked(SurfaceSelectionGeometry::resolve(p, c, reference, FeatureId("select"), 1e-6));
  const auto original = brep(source);
  for (const auto side : {SurfaceOffsetSide::AlongNormal, SurfaceOffsetSide::OppositeNormal,
                          SurfaceOffsetSide::Symmetric}) {
    const auto result = f::checked(
        SurfaceOffsetAdapter{}.construct(FeatureId("thick"), source, SurfaceFeatureKind::Thicken, 3,
                                         side, 1e-6, SurfaceOffsetApproximation::ExactAnalytic));
    CHECK(volume(result.shape) == Catch::Approx((48 - std::numbers::pi) * 3).margin(1e-6));
    check_report(result, 3);
    CHECK(brep(source) == original);
    CHECK(f::checked(SurfaceBoundaryQuery::inspect({result.shape}, 1e-6, .01)).free_edges.empty());
  }
  auto disconnected = f::checked(SurfaceReference::create_faces(
      BodyId("base.body"), {f::checked(FaceReference::create_result(
                                PartFeatureInputRole::SurfaceSourceFace,
                                f::checked(ResultFaceReference::create(
                                    FeatureId("bore"), BodyId("base.body"),
                                    {{FeatureId("base.feature"), "extrude/upper-cap"}})))),
                            f::checked(FaceReference::create_result(
                                PartFeatureInputRole::SurfaceSourceFace,
                                f::checked(ResultFaceReference::create(
                                    FeatureId("bore"), BodyId("base.body"),
                                    {{FeatureId("base.feature"), "extrude/lower-cap"}}))))}));
  CHECK_FALSE(SurfaceSelectionGeometry::resolve(p, c, disconnected, FeatureId("bad"), 1e-6));
  CHECK(brep(source) == original);
}
TEST_CASE("Surface gap reports distinguish whole-edge maximum from a nearby endpoint",
          "[geometry][surface-boundary-query][parity-b172]") {
  const auto patch = [](const char* id, double low, double high, bool right) {
    const std::vector<Point3> points =
        right ? std::vector<Point3>{{low, 0, 0}, {2, 0, 0}, {2, 2, 0}, {high, 2, 0}}
              : std::vector<Point3>{{-2, 0, 0}, {0, 0, 0}, {0, 2, 0}, {-2, 2, 0}};
    std::vector<SweepPathSegment> loop;
    for (std::size_t i = 0; i < points.size(); ++i)
      loop.push_back({SweepPathSegmentKind::Line, points[i], {}, points[(i + 1) % points.size()]});
    return f::checked(SurfaceAdapter{}.make_fill_surface(FeatureId(id), {loop}));
  };
  const auto left = patch("left", 0, 0, false), right = patch("right", .0005, .0005, true);
  for (const double tolerance : {.001, .0001}) {
    const auto report = f::checked(SurfaceBoundaryQuery::inspect({left, right}, tolerance, .01));
    CHECK(report.free_edges.size() == 8);
    CHECK(report.non_manifold_edges == 0);
    REQUIRE(report.gaps.size() == 1);
    CHECK(report.gaps.front().minimum_mm == Catch::Approx(.0005).margin(1e-8));
    CHECK(report.gaps.front().maximum_mm == Catch::Approx(.0005).margin(1e-8));
    CHECK(report.gaps.front().within_tolerance == (tolerance == .001));
  }
  const auto diverging = patch("diverging", .0005, .5, true);
  const auto report = f::checked(SurfaceBoundaryQuery::inspect({left, diverging}, .001, 1));
  REQUIRE(!report.gaps.empty());
  CHECK(std::any_of(report.gaps.begin(), report.gaps.end(), [](const auto& gap) {
    return gap.minimum_mm < .001 && gap.maximum_mm > .49 && !gap.within_tolerance;
  }));
}

TEST_CASE("G03 nonplanar C2 patch satisfies independent symmetric normal sections and tube volume",
          "[geometry][surface-offset-geometry][surface-offset-curved][parity-b171]") {
  const std::vector<SweepPathSegment> a = {{SweepPathSegmentKind::Line, {0, 0, 0}, {}, {10, 0, 0}}};
  const std::vector<SweepPathSegment> b = {
      {SweepPathSegmentKind::Line, {0, 10, 0}, {}, {10, 10, 2}}};
  const auto source =
      f::checked(SurfaceAdapter{}.make_boundary_surface(FeatureId("saddle"), {a, b}));
  const auto original = brep(source);
  CHECK_FALSE(SurfaceOffsetAdapter{}.construct(
      FeatureId("exact"), source, SurfaceFeatureKind::Thicken, .2, SurfaceOffsetSide::Symmetric,
      1e-6, SurfaceOffsetApproximation::ExactAnalytic));
  const auto result = f::checked(SurfaceOffsetAdapter{}.construct(
      FeatureId("curved"), source, SurfaceFeatureKind::Thicken, .2, SurfaceOffsetSide::Symmetric,
      1e-6, SurfaceOffsetApproximation::VerifiedApproximation, 1e-4));
  check_report(result, .2);
  CHECK(result.verification.approximate);
  // z=.02*x*y: analytic first/second fundamental forms give
  // dA=sqrt(1+.0004*(x*x+y*y)); K=-.0004/(1+.0004*(x*x+y*y))^2.
  // Symmetric tube volume = integral(.2*dA + .2^3/12*K*dA).
  double integral = 0;
  constexpr int subdivisions = 100;
  for (int i = 0; i <= subdivisions; ++i)
    for (int j = 0; j <= subdivisions; ++j) {
      const double x = i * .1, y = j * .1, q = 1 + .0004 * (x * x + y * y);
      const double weight = (i == 0 || i == subdivisions ? 1
                             : i % 2                     ? 4
                                                         : 2) *
                            (j == 0 || j == subdivisions ? 1
                             : j % 2                     ? 4
                                                         : 2);
      integral += weight * (.2 * std::sqrt(q) - (.2 * .2 * .2 / 12) * .0004 / std::pow(q, 1.5));
    }
  integral *= .01 / 9;
  GProp_GProps properties;
  BRepGProp::VolumeProperties(*OcctShapeView::get(result.shape), properties, 1e-10, true);
  CHECK(properties.Mass() == Catch::Approx(integral).margin(1e-5));
  IntCurvesFace_ShapeIntersector sections;
  sections.Load(*OcctShapeView::get(result.shape), 1e-7);
  for (int x = 2; x <= 8; x += 2)
    for (int y = 2; y <= 8; y += 2) {
      const gp_Pnt center(x, y, .02 * x * y);
      const gp_Dir normal(-.02 * y, -.02 * x, 1);
      sections.Perform(gp_Lin(center, normal), -.15, .15);
      REQUIRE(sections.IsDone());
      REQUIRE(sections.NbPnt() == 2);
      std::vector<double> hits = {sections.WParameter(1), sections.WParameter(2)};
      std::sort(hits.begin(), hits.end());
      CHECK(hits[0] == Catch::Approx(-.1).margin(1e-5));
      CHECK(hits[1] == Catch::Approx(.1).margin(1e-5));
    }
  CHECK_FALSE(SurfaceOffsetAdapter{}.construct(
      FeatureId("collapse"), source, SurfaceFeatureKind::Thicken, 200, SurfaceOffsetSide::Symmetric,
      1e-6, SurfaceOffsetApproximation::VerifiedApproximation, 1e-4));
  CHECK(brep(source) == original);
}
