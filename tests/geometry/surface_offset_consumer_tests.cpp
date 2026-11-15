#include "../support/geometry_volume_oracle.hpp"
#include "../support/surface_offset_fixture.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/surface_adapter.hpp"
#include "blcad/geometry/surface_offset_adapter.hpp"
#include "imported_body_test_fixture.hpp"
#include "native_path_curve.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <gp_Ax2.hxx>
#include <numbers>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::surface_offset;
namespace {
std::string brep(const GeometryShape& shape) {
  std::ostringstream text;
  BRepTools::Write(*OcctShapeView::get(shape), text);
  return text.str();
}
double volume(const GeometryShape& shape) {
  return blcad::test::integrated_solid_volume(shape).volume_mm3;
}
void execute(PartDocument& p, ShapeCache& cache, StepImportTopologyContext* context = nullptr) {
  const auto result = GeometryRecomputeExecutor{context}.execute_document(p, cache);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
}
void physical(const GeometryShape& shape, double expected) {
  CHECK(volume(shape) == Catch::Approx(expected).margin(1e-5));
  CHECK(blcad::test::classify_solid_point(shape, {0, 0, 5}) ==
        blcad::test::SolidPointLocation::Outside);
  CHECK(blcad::test::classify_solid_point(shape, {2, 0, 7.5}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(shape, {3, 0, 5}) ==
        blcad::test::SolidPointLocation::Inside);
}
} // namespace
TEST_CASE(
    "G03 offset skin supports associative Sketch, Hole, finishing and physical STEP reopening",
    "[geometry][surface-offset-consumer][parity-b171]") {
  auto p = f::thickened();
  auto cache = f::cache();
  execute(p, cache);
  const auto faces =
      f::checked(ResultFaceSupportGeometry::selectable_faces(p, cache, BodyId("thick.body")));
  REQUIRE(faces.size() == 2);
  const auto upper = std::max_element(faces.begin(), faces.end(), [](const auto& a, const auto& b) {
    return a.anchor.z < b.anchor.z;
  });
  REQUIRE(upper->anchor.z == Catch::Approx(6.5));
  REQUIRE(p.add_result_face_workplane(f::checked(
      ResultFaceWorkplane::create(DatumPlaneId("skin.plane"), "Offset skin", upper->reference))));
  f::length(p, "bore.diameter", 1);
  const auto support = f::checked(PlaneReference::create_datum_plane(
      PartFeatureInputRole::HoleSupportPlane, DatumPlaneId("skin.plane")));
  REQUIRE(p.add_hole_feature(f::checked(HoleFeature::create(
      FeatureId("bore"), "Bore", HoleKind::Plain, BodyId("thick.body"), support,
      {{HolePlacementId("center"), Point2{0, 0}}}, ParameterId("bore.diameter")))));
  execute(p, cache);
  CHECK(volume(*cache.find_body_shape(BodyId("thick.body"))) ==
        Catch::Approx((48 - std::numbers::pi / 4) * 3).margin(1e-6));
  REQUIRE(p.set_parameter_value(ParameterId("distance"), f::checked(Quantity::length_mm(4))));
  execute(p, cache);
  const auto* workplane = p.find_result_face_workplane(DatumPlaneId("skin.plane"));
  REQUIRE(workplane);
  const auto frame = f::checked(ResultFaceSupportGeometry::resolve(p, *workplane, cache));
  CHECK(frame.origin.z == Catch::Approx(7));
  CHECK(volume(*cache.find_body_shape(BodyId("thick.body"))) ==
        Catch::Approx((48 - std::numbers::pi / 4) * 4).margin(1e-6));
  const auto edges = f::checked(FinishingSelectionBuilder::build(p, cache, BodyId("thick.body"),
                                                                 PartFeatureInputRole::FilletEdge));
  const auto corner = std::find_if(edges.begin(), edges.end(), [](const auto& edge) {
    return std::abs(std::abs(edge.anchor.x) - 4) < 1e-6 &&
           std::abs(std::abs(edge.anchor.y) - 3) < 1e-6 && std::abs(edge.anchor.z - 5) < 1e-6;
  });
  REQUIRE(corner != edges.end());
  f::length(p, "round.radius", .2);
  REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create(
      FeatureId("round"), "Round", BodyId("thick.body"),
      {std::get<EdgeReference>(corner->reference)}, ParameterId("round.radius")))));
  execute(p, cache);
  f::length(p, "boss.diameter", .5);
  f::length(p, "boss.height", 1);
  auto sketch = f::checked(
      Sketch::create(SketchId("skin.sketch"), "Skin Sketch", DatumPlaneId("skin.plane")));
  REQUIRE(sketch.add_profile(
      f::checked(CircleProfile::create(ProfileId("boss"), ParameterId("boss.diameter"), {2, 0}))));
  REQUIRE(p.add_sketch(sketch));
  auto boss = f::checked(Feature::create_additive_extrude(FeatureId("boss"), "Boss", sketch.id(),
                                                          ParameterId("boss.height")));
  const auto context = f::checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::Join, BodyId("thick.body"), std::nullopt));
  REQUIRE(p.add_feature(f::checked(boss.with_body_result_context(context))));
  execute(p, cache);
  const double expected =
      4 * (48 - std::numbers::pi / 4 - .04 * (1 - std::numbers::pi / 4)) + .0625 * std::numbers::pi;
  physical(*cache.find_body_shape(BodyId("thick.body")), expected);
  const auto bytes = f::saved(p);
  auto reopened = f::checked(deserialize_part_document_from_json(bytes));
  auto fresh = f::cache();
  execute(reopened, fresh);
  CHECK(f::saved(reopened) == bytes);
  physical(*fresh.find_body_shape(BodyId("thick.body")), expected);
  imported_body_test::Fixture files;
  const auto output = files.root / "thickened.step";
  REQUIRE(StepExporter{}.write_step(*fresh.find_body_shape(BodyId("thick.body")), output.string()));
  const auto source = f::checked(StepImportSource::create(
      StepImportSourceId("roundtrip"), "Roundtrip", output.filename().string(),
      f::checked(step_import_sha256(imported_body_test::bytes(output))),
      StepImportMode::Reference));
  const auto imported = f::checked(StepImportReader{}.read(source, files.root));
  const auto shapes = f::checked(StepImportReader{}.preview_shapes(imported));
  REQUIRE(shapes.size() == 1);
  physical(shapes.front(), expected);
}
TEST_CASE("G03 placed imported cylinder selections use the actual predecessor in all Body modes",
          "[geometry][surface-offset-import][parity-b171]") {
  imported_body_test::Fixture files;
  files.write(
      BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(30, -5, 12), gp_Dir(0, 0, 1)), 10, 20).Shape());
  const auto supplier = files.source_bytes();
  for (const auto mode : {FeatureBodyOperationMode::NewBody, FeatureBodyOperationMode::Join,
                          FeatureBodyOperationMode::Cut, FeatureBodyOperationMode::Intersect}) {
    CAPTURE(mode);
    auto p = files.editable();
    const BodyId owner("body.1");
    REQUIRE(p.add_body_transform(f::checked(BodyTransform::create_translate(
        BodyTransformId("placement"), owner, BodyTransformCoordinateSpace::World, std::nullopt,
        {2, 3, 4}, false, false))));
    f::length(p, "distance", 3);
    f::length(p, "tolerance", 1e-6);
    if (mode == FeatureBodyOperationMode::NewBody)
      REQUIRE(p.add_body(f::checked(Body::create(BodyId("thick.body"), "Thick", BodyKind::Solid))));
    const auto& catalog = p.imported_topology_catalogs().front();
    const auto wall =
        std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
          return entry.descriptor.kind == ImportedTopologyKind::Face &&
                 entry.descriptor.geometry == ImportedGeometryClass::Cylinder;
        });
    REQUIRE(wall != catalog.entries().end());
    const auto imported_face = f::checked(ImportedTopologyReference::create(
        catalog.id(), wall->id, ImportedTopologyCapability::Face));
    const auto reference = f::checked(SurfaceReference::create_faces(
        owner, {f::checked(FaceReference::create_imported(PartFeatureInputRole::SurfaceSourceFace,
                                                          imported_face))}));
    const auto context = f::context(mode, owner);
    const SurfaceOffsetParameters parameters{
        ParameterId("distance"), ParameterId("tolerance"), SurfaceOffsetSide::Symmetric,
        SurfaceOffsetApproximation::ExactAnalytic, std::nullopt};
    const auto feature = f::checked(SurfaceOffsetFeature::create_thicken(
        FeatureId("thick"), "Thick", reference, parameters, context));
    REQUIRE(p.add_surface_feature(feature));
    StepImportTopologyContext imports(files.root);
    auto cache = f::cache();
    execute(p, cache, &imports);
    const auto output = context.effective_produced_body();
    const auto* shape = cache.find_body_shape(output);
    REQUIRE(shape);
    if (mode == FeatureBodyOperationMode::NewBody) {
      const auto offered = f::checked(FinishingSelectionBuilder::build(
          p, cache, output, PartFeatureInputRole::ShellRemovalFace, &imports));
      REQUIRE(offered.size() == 2);
      std::vector<double> radii;
      for (const auto& target : offered) {
        const auto& face = std::get<FaceReference>(target.reference);
        CHECK(std::holds_alternative<ResultFaceReference>(face.source()));
        BRepAdaptor_Surface native(TopoDS::Face(*OcctShapeView::get(target.shape)));
        REQUIRE(native.GetType() == GeomAbs_Cylinder);
        radii.push_back(native.Cylinder().Radius());
      }
      CHECK(*std::min_element(radii.begin(), radii.end()) == Catch::Approx(8.5));
      CHECK(*std::max_element(radii.begin(), radii.end()) == Catch::Approx(11.5));
    }
    const double expected = (mode == FeatureBodyOperationMode::NewBody ? 1200
                             : mode == FeatureBodyOperationMode::Join  ? 2645
                             : mode == FeatureBodyOperationMode::Cut   ? 1445
                                                                       : 555) *
                            std::numbers::pi;
    CHECK(volume(*shape) == Catch::Approx(expected).margin(1e-4));
    const auto bounds = f::checked(BodyTransformAdapter{}.bounds(*shape));
    CHECK(bounds.minimum.z == Catch::Approx(16).margin(1e-6));
    CHECK(bounds.maximum.z == Catch::Approx(36).margin(1e-6));
    const auto valid = brep(*shape);
    const auto source_before = brep(*cache.find_feature_shape(FeatureId("import.base")));
    execute(p, cache, &imports);
    CHECK(brep(*cache.find_body_shape(output)) == valid);
    CHECK(files.source_bytes() == supplier);
    const auto bytes = f::saved(p);
    auto reopened = f::checked(deserialize_part_document_from_json(bytes));
    auto fresh = f::cache();
    StepImportTopologyContext reimports(files.root);
    execute(reopened, fresh, &reimports);
    CHECK(volume(*fresh.find_body_shape(output)) == Catch::Approx(expected).margin(1e-4));
    CHECK(f::saved(reopened) == bytes);
    auto impossible = parameters;
    impossible.side = SurfaceOffsetSide::OppositeNormal;
    REQUIRE(p.update_surface_feature(f::checked(SurfaceOffsetFeature::create_thicken(
        feature.id(), "Impossible", reference, impossible, context))));
    REQUIRE(p.set_parameter_value(ParameterId("distance"), f::checked(Quantity::length_mm(11))));
    const auto failed_intent = f::saved(p);
    CHECK_FALSE(GeometryRecomputeExecutor{&imports}.execute_document(p, cache));
    CHECK(f::saved(p) == failed_intent);
    CHECK(brep(*cache.find_body_shape(output)) == valid);
    CHECK(brep(*cache.find_feature_shape(FeatureId("import.base"))) == source_before);
    files.write_bytes(supplier + "\n/* changed supplier */\n");
    CHECK_FALSE(GeometryRecomputeExecutor{&imports}.execute_document(reopened, fresh));
    files.write_bytes(supplier);
    CHECK(files.source_bytes() == supplier);
  }
}

TEST_CASE("G03 nonlocal offset collision rejects below every local curvature limit",
          "[geometry][surface-offset-collision][parity-b171]") {
  const auto section = [](double z) {
    const auto native = f::checked(blcad::geometry::detail::native_rational_bezier(
        {{-.1, 0, z}, {-10, -10, z}, {10, -10, z}, {.1, 0, z}}, {1, 1, 1, 1}, "near-mouth"));
    return std::vector<SweepPathSegment>{
        {SweepPathSegmentKind::DerivedNative, {-.1, 0, z}, {}, {.1, 0, z}, native}};
  };
  const auto source = f::checked(
      SurfaceAdapter{}.make_boundary_surface(FeatureId("near-mouth"), {section(0), section(5)}));
  const auto before = brep(source);
  double maximum_curvature = 0;
  for (int i = 0; i <= 256; ++i) {
    const double t = i / 256.0, s = 1 - t;
    const double dx = 3 * (-9.9 * s * s + 40 * s * t - 9.9 * t * t);
    const double dy = 3 * (-10 * s * s + 10 * t * t);
    const double ddx = 6 * (29.9 * s - 29.9 * t), ddy = 60;
    const double curvature = std::abs(dx * ddy - dy * ddx) / std::pow(dx * dx + dy * dy, 1.5);
    maximum_curvature = std::max(maximum_curvature, curvature);
  }
  CHECK(maximum_curvature < 1);
  const auto small =
      SurfaceOffsetAdapter{}.construct(FeatureId("clear"), source, SurfaceFeatureKind::Thicken, .03,
                                       SurfaceOffsetSide::OppositeNormal, 1e-6,
                                       SurfaceOffsetApproximation::VerifiedApproximation, 1e-4);
  INFO((small ? "ok" : small.error().message()));
  REQUIRE(small);
  const auto colliding =
      SurfaceOffsetAdapter{}.construct(FeatureId("colliding"), source, SurfaceFeatureKind::Thicken,
                                       .3, SurfaceOffsetSide::OppositeNormal, 1e-6,
                                       SurfaceOffsetApproximation::VerifiedApproximation, 1e-4);
  REQUIRE_FALSE(colliding);
  const auto message = colliding.error().message();
  INFO(message);
  CHECK((message.find("collid") != std::string::npos ||
         message.find("self-intersect") != std::string::npos ||
         message.find("invalid topology") != std::string::npos));
  CHECK(brep(source) == before);
}

TEST_CASE("G03 curved quilt opening survives signed and symmetric thickness across a native seam",
          "[geometry][surface-offset-curved-hole][parity-b171]") {
  Handle(Geom_Surface) cylinder =
      new Geom_CylindricalSurface(gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 10);
  BRepBuilderAPI_Sewing joined(1e-7);
  joined.Add(BRepBuilderAPI_MakeFace(cylinder, -.6, 0, 0, 20, 1e-7).Face());
  joined.Add(BRepBuilderAPI_MakeFace(cylinder, 0, .6, 0, 20, 1e-7).Face());
  joined.Perform();
  BRepAlgoAPI_Cut window(joined.SewedShape(),
                         BRepPrimAPI_MakeBox(gp_Pnt(8, -1, 8), 4, 2, 4).Shape());
  REQUIRE(window.IsDone());
  BRepBuilderAPI_Sewing quilt(1e-7);
  for (TopExp_Explorer it(window.Shape(), TopAbs_FACE); it.More(); it.Next())
    quilt.Add(it.Current());
  quilt.Perform();
  imported_body_test::Fixture files;
  files.write(quilt.SewedShape());
  const auto supplier = files.source_bytes();
  auto p = files.editable();
  REQUIRE(p.bodies().size() == 1);
  REQUIRE(p.bodies().front().kind() == BodyKind::Surface);
  f::length(p, "distance", 3);
  f::length(p, "tolerance", 1e-6);
  REQUIRE(p.add_body(f::checked(Body::create(BodyId("thick.body"), "Thick", BodyKind::Solid))));
  const auto& catalog = p.imported_topology_catalogs().front();
  std::vector<FaceReference> faces;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face) {
      REQUIRE(entry.descriptor.geometry == ImportedGeometryClass::Cylinder);
      faces.push_back(f::checked(FaceReference::create_imported(
          PartFeatureInputRole::SurfaceSourceFace,
          f::checked(ImportedTopologyReference::create(catalog.id(), entry.id,
                                                       ImportedTopologyCapability::Face)))));
    }
  REQUIRE(faces.size() == 2);
  const auto source = f::checked(SurfaceReference::create_faces(BodyId("body.1"), faces));
  bool first = true;
  StepImportTopologyContext imports(files.root);
  auto cache = f::cache();
  for (const auto side : {SurfaceOffsetSide::AlongNormal, SurfaceOffsetSide::OppositeNormal,
                          SurfaceOffsetSide::Symmetric}) {
    const SurfaceOffsetParameters parameters{ParameterId("distance"), ParameterId("tolerance"),
                                             side, SurfaceOffsetApproximation::ExactAnalytic,
                                             std::nullopt};
    const auto feature = f::checked(SurfaceOffsetFeature::create_thicken(
        FeatureId("thick"), "Quilt thickness", source, parameters, f::context()));
    if (first)
      REQUIRE(p.add_surface_feature(feature));
    else
      REQUIRE(p.update_surface_feature(feature));
    first = false;
    execute(p, cache, &imports);
    const auto* shape = cache.find_body_shape(BodyId("thick.body"));
    REQUIRE(shape);
    const double inner = side == SurfaceOffsetSide::AlongNormal      ? 10
                         : side == SurfaceOffsetSide::OppositeNormal ? 7
                                                                     : 8.5;
    const double outer = side == SurfaceOffsetSide::AlongNormal      ? 13
                         : side == SurfaceOffsetSide::OppositeNormal ? 10
                                                                     : 11.5;
    const double expected =
        (1.2 * 20 - 2 * std::asin(.1) * 4) * (outer * outer - inner * inner) / 2;
    CHECK(volume(*shape) == Catch::Approx(expected).margin(1e-4));
    const double radius = (inner + outer) / 2;
    CHECK(blcad::test::classify_solid_point(*shape, {radius, 0, 10}) ==
          blcad::test::SolidPointLocation::Outside);
    CHECK(blcad::test::classify_solid_point(*shape,
                                            {radius * std::cos(.3), radius * std::sin(.3), 10}) ==
          blcad::test::SolidPointLocation::Inside);
    const auto report = SurfaceOffsetAdapter::verification(*shape);
    REQUIRE(report);
    CHECK(report->maximum_error_mm <= 1e-6);
    CHECK(files.source_bytes() == supplier);
  }
}
