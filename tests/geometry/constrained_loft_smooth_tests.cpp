#include "../support/constrained_loft_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Surface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::constrained_loft;
namespace lf = blcad::test::loft_refinement;
namespace sf = blcad::test::spatial_fixture;
namespace {
void length(PartDocument& p, const std::string& id, double value) {
  (void)lf::checked(p.add_parameter(lf::checked(
      Parameter::create_length(ParameterId(id), id, lf::checked(Quantity::length_mm(value))))));
}
void circle(PartDocument& p, const std::string& id, double z, double radius) {
  const std::string plane = id + ".plane";
  (void)lf::checked(p.add_construction_plane(lf::checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId(plane), plane, {0, 0, z}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  length(p, id + ".diameter", 2 * radius);
  auto sketch = lf::checked(Sketch::create(SketchId(id), id, DatumPlaneId(plane)));
  REQUIRE(sketch.add_profile(
      lf::checked(CircleProfile::create(ProfileId("outline"), ParameterId(id + ".diameter")))));
  (void)lf::checked(p.add_sketch(sketch));
}
void cylinder(PartDocument& p, const std::string& id, double z, double radius, double height) {
  circle(p, id + ".profile", z, radius);
  length(p, id + ".height", height);
  (void)lf::checked(p.add_body(lf::checked(Body::create(BodyId(id), id, BodyKind::Solid))));
  auto feature = lf::checked(Feature::create_additive_extrude(
      FeatureId(id), id, SketchId(id + ".profile"), ParameterId(id + ".height")));
  (void)lf::checked(p.add_feature(lf::checked(feature.with_body_result_context(f::context(id)))));
}
PartDocument smooth(FeatureBodyOperationMode operation = FeatureBodyOperationMode::NewBody) {
  auto part = lf::checked(PartDocument::create(DocumentId("smooth.transition"), "G2 transition"));
  (void)lf::checked(part.add_datum_plane(lf::checked(DatumPlane::xy())));
  length(part, "tolerance", 1e-5);
  cylinder(part, "root", -5, 4, 5);
  cylinder(part, "tip", 20, 8, 5);
  cylinder(part, "target", 0, 6, 20);
  std::vector<ProfileSectionReference> sections;
  for (int s = 0; s < 3; ++s) {
    const std::string id = "section." + std::to_string(s);
    circle(part, id, 10 * s, 4 + 2 * s);
    sections.push_back(f::section(id));
  }
  std::vector<PathCurveId> rails;
  LoftConstraintIntent intent{ParameterId("tolerance"), 1e-5, 1e-5, {}, {}, {}};
  for (int g = 0; g < 4; ++g) {
    const std::string id = "rail." + std::to_string(g);
    auto sketch = lf::checked(Sketch3D::create(Sketch3DId(id + ".sketch3d"), id));
    std::vector<SketchPointReference3D> refs;
    const double angle = g * std::numbers::pi / 2;
    for (int i = 0; i < 6; ++i) {
      const std::string name = "p" + std::to_string(i);
      const double radius = i < 3 ? 4 : 8;
      REQUIRE(sketch.add_point(
          sf::point(name, {radius * std::cos(angle), radius * std::sin(angle), 4.0 * i})));
      refs.push_back(lf::checked(SketchPointReference3D::create_local_point(SketchEntityId(name))));
    }
    REQUIRE(sketch.add_spline(lf::checked(
        SketchSpline3D::create(SketchEntityId("curve"), SketchSpline3DRepresentation::ControlPoints,
                               refs, 5, SketchSpline3DContinuity::Curvature))));
    (void)lf::checked(part.add_sketch_3d(sketch));
    const auto segment = lf::checked(PathSegmentReference::create_sketch_3d(
        sketch.id(), SketchEntityId("curve"), Sketch3DPathCurveKind::Spline));
    (void)lf::checked(part.add_path_curve(lf::checked(PathCurve::create(
        PathCurveId(id), id, {segment}, PathClosure::Open, PathOrientationRule::MinimumTwist, {},
        {}, 1e-7, lf::checked(PathExecutionPolicy::curve_preserving(1e-7))))));
    rails.emplace_back(id);
    intent.guides.push_back({PathCurveId(id),
                             false,
                             {{sections[0].identity(), {}, g * .25, 0},
                              {sections[1].identity(), {}, g * .25, .5},
                              {sections[2].identity(), {}, g * .25, 1}}});
  }
  for (const bool first : {true, false}) {
    const std::string producer = first ? "root" : "tip";
    for (int g = 0; g < 4; ++g) {
      const std::string role =
          "extrude/circle/outline/wall" + (g ? "/quadrant/" + std::to_string(g) : "");
      const auto face = lf::checked(ResultFaceReference::create(
          FeatureId(producer), BodyId(producer), {{FeatureId(producer), role}}));
      const auto reference = lf::checked(
          FaceReference::create_result(PartFeatureInputRole::LoftBoundarySupport, face));
      intent.patch_boundaries.push_back({{(first ? sections.front() : sections.back()).identity(),
                                          reference,
                                          {},
                                          LoftBoundaryContinuity::G2,
                                          false,
                                          1},
                                         rails[g],
                                         rails[(g + 1) % 4]});
    }
  }
  (void)lf::checked(
      part.add_body(lf::checked(Body::create(BodyId("main"), "Main", BodyKind::Solid))));
  const auto context = operation == FeatureBodyOperationMode::NewBody
                           ? f::context("main")
                           : lf::checked(FeatureBodyResultContext::create(
                                 operation, BodyId("target"), BodyId("main")));
  const auto feature = lf::checked(
      operation == FeatureBodyOperationMode::Cut
          ? LoftFeature::create_loft_cut(FeatureId("main"), "G2 cut", sections, context, {}, rails)
          : LoftFeature::create_loft(FeatureId("main"), "G2 transition", sections, context, {},
                                     rails));
  (void)lf::checked(part.add_loft_feature(lf::checked(feature.with_constraints(intent))));
  return part;
}
// Independent smoothstep polynomial: R(t)=4+4*(10t^3-15t^4+6t^5).
// Integrate its square algebraically, without consulting OCCT or construction poles.
double integral(double t) {
  const std::array<double, 6> r{4, 0, 0, 40, -60, 24};
  double result = 0;
  for (int i = 0; i < 6; ++i)
    for (int j = 0; j < 6; ++j)
      result += r[i] * r[j] * std::pow(t, i + j + 1) / double(i + j + 1);
  return 20 * std::numbers::pi * result;
}
void sections(const GeometryShape& shape) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  for (const double t : {.125, .25, .375, .5, .625, .75, .875}) {
    const double radius = 4 + 4 * (10 * std::pow(t, 3) - 15 * std::pow(t, 4) + 6 * std::pow(t, 5));
    for (int i = 0; i < 16; ++i) {
      const double angle = 2 * std::numbers::pi * i / 16;
      const gp_Pnt point(radius * std::cos(angle), radius * std::sin(angle), 20 * t);
      CHECK(blcad::test::distance_to_faces(shape, {point.X(), point.Y(), point.Z()}) <= 1e-5);
    }
  }
}
void curvature(const GeometryShape& shape) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  int sides = 0;
  for (TopExp_Explorer e(*native, TopAbs_FACE); e.More(); e.Next()) {
    const auto face = TopoDS::Face(e.Current());
    if (BRepAdaptor_Surface(face).GetType() != GeomAbs_BSplineSurface)
      continue;
    ++sides;
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    const auto surface = BRep_Tool::Surface(face);
    for (const bool first : {true, false})
      for (int i = 1; i < 8; ++i) {
        gp_Pnt p;
        gp_Vec du, dv, duu, dvv, duv;
        surface->D2(std::lerp(u0, u1, i / 8.0), first ? v0 : v1, p, du, dv, duu, dvv, duv);
        auto normal = du.Crossed(dv);
        normal.Normalize();
        if (face.Orientation() == TopAbs_REVERSED)
          normal.Reverse();
        const double radius = first ? 4 : 8;
        CHECK(p.Z() == Catch::Approx(first ? 0 : 20).margin(1e-8));
        CHECK(normal.Dot(gp_Vec(p.X() / radius, p.Y() / radius, 0)) >= 1 - 1e-9);
        CHECK(duu.Dot(normal) / du.SquareMagnitude() == Catch::Approx(-1 / radius).margin(1e-7));
        CHECK(std::abs(dvv.Dot(normal) / dv.SquareMagnitude()) <= 1e-7);
        CHECK(std::abs(duv.Dot(normal)) <= 1e-7);
      }
  }
  CHECK(sides == 4);
}

} // namespace
TEST_CASE("G02 smooth circular G2 transition has independent volume across all Body operations",
          "[geometry][constrained-loft-feature][constrained-loft-smooth][parity-b169]") {
  const double target = 720 * std::numbers::pi, tool = integral(1),
               overlap = integral(.5) + 360 * std::numbers::pi;
  for (const auto operation :
       {FeatureBodyOperationMode::NewBody, FeatureBodyOperationMode::Join,
        FeatureBodyOperationMode::Cut, FeatureBodyOperationMode::Intersect}) {
    INFO(to_string(operation));
    auto part = smooth(operation);
    auto cache = f::cache();
    const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
    INFO((result ? std::string("ok") : result.error().message()));
    REQUIRE(result);
    const double expected = operation == FeatureBodyOperationMode::NewBody ? tool
                            : operation == FeatureBodyOperationMode::Join  ? target + tool - overlap
                            : operation == FeatureBodyOperationMode::Cut   ? target - overlap
                                                                           : overlap;
    CHECK(f::volume(cache) == Catch::Approx(expected).margin(2e-4));
    const auto bytes = lf::checked(serialize_part_document_to_json(part));
    auto reopened = lf::checked(deserialize_part_document_from_json(bytes));
    auto fresh = f::cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(reopened, fresh));
    CHECK(f::volume(fresh) == Catch::Approx(expected).margin(2e-4));
    CHECK(lf::checked(serialize_part_document_to_json(reopened)) == bytes);
    if (operation == FeatureBodyOperationMode::NewBody) {
      sections(*cache.find_body_shape(BodyId("main")));
      curvature(*cache.find_body_shape(BodyId("main")));
    }
  }
}
TEST_CASE("Verified G2 transition retains physical sections and volume after STEP reimport",
          "[geometry][constrained-loft-feature][constrained-loft-smooth][parity-b169]") {
  auto part = smooth();
  auto cache = f::cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  imported_body_test::Fixture files;
  const auto output = files.root / "g2-transition.step";
  REQUIRE(StepExporter{}.write_step(*cache.find_body_shape(BodyId("main")), output.string()));
  const auto source = lf::checked(StepImportSource::create(
      StepImportSourceId("g2.transition"), "G2 transition", output.filename().string(),
      lf::checked(step_import_sha256(imported_body_test::bytes(output))),
      StepImportMode::Reference));
  const auto imported = lf::checked(StepImportReader{}.read(source, files.root));
  const auto shapes = lf::checked(StepImportReader{}.preview_shapes(imported));
  REQUIRE(shapes.size() == 1);
  CHECK(RectangleExtrusionAdapter{}.summarize(shapes[0]).volume_mm3 ==
        Catch::Approx(integral(1)).margin(2e-4));
  sections(shapes[0]);
  curvature(shapes[0]);
}
