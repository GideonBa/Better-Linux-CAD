#include "../../src/geometry/surface_join_verification.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/hole_feature_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "../support/surface_offset_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace {
FaceReference face(SemanticFace side,
                   PartFeatureInputRole role = PartFeatureInputRole::FilletSupportFace) {
  return f::checked(FaceReference::create_planar(
      role, f::checked(SemanticFaceReference::create(FeatureId("base"), side))));
}
FaceBlendIntent blend(FilletContinuity c) {
  return {{"first", "Top", {face(SemanticFace::Top)}},
          {"second", "Front", {face(SemanticFace::Front)}},
          c,
          c == FilletContinuity::Tangent ? FaceBlendSizeKind::Radius
                                         : FaceBlendSizeKind::ContactSetback,
          ParameterId("radius"),
          {}};
}
FullRoundIntent round(FilletContinuity c) {
  return {{"first", "Front", {face(SemanticFace::Front)}},
          {"second", "Back", {face(SemanticFace::Back)}},
          {"middle", "Top", {face(SemanticFace::Top, PartFeatureInputRole::FilletMiddleFace)}},
          c,
          {}};
}
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("faces.cache")));
}
void execute(PartDocument& p, ShapeCache& c, StepImportTopologyContext* context = nullptr) {
  auto result = GeometryRecomputeExecutor(context).execute_document(p, c);
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
}
double volume(const ShapeCache& c, BodyId body = BodyId("body.base")) {
  auto shape = c.find_body_shape(body);
  REQUIRE(shape);
  return blcad::test::integrated_solid_volume(*shape).volume_mm3;
}
} // namespace
TEST_CASE("Face Fillet measures native G1 and G2 contacts and independent removed volumes",
          "[geometry][face-fillet][parity-b178]") {
  for (auto continuity : {FilletContinuity::Tangent, FilletContinuity::Curvature}) {
    auto p = f::native_base();
    auto c = cache();
    execute(p, c);
    double before = volume(c);
    REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_face_blend(
        FeatureId("finish"), "Face blend", BodyId("body.base"), blend(continuity)))));
    execute(p, c);
    const double removed =
        continuity == FilletContinuity::Tangent ? 20 * (1 - std::numbers::pi / 4) : 2.;
    CHECK(volume(c) == Catch::Approx(before - removed).margin(2e-5));
    const auto text = f::checked(serialize_part_document_to_json(p));
    auto reopened = f::checked(deserialize_part_document_from_json(text));
    auto fresh = cache();
    execute(reopened, fresh);
    CHECK(volume(fresh) == Catch::Approx(volume(c)).margin(1e-6));
    REQUIRE(p.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(40))));
    const auto saved = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
    CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
    CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(saved));
  }
}
TEST_CASE("Full-round derives half width removes the middle and preserves actual native G2",
          "[geometry][face-fillet][parity-b178]") {
  for (auto continuity : {FilletContinuity::Tangent, FilletContinuity::Curvature}) {
    auto p = f::native_base();
    auto c = cache();
    execute(p, c);
    const double before = volume(c);
    REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_full_round(
        FeatureId("finish"), "Full round", BodyId("body.base"), round(continuity)))));
    execute(p, c);
    const double removed = continuity == FilletContinuity::Tangent
                               ? (2 - std::numbers::pi / 2) * 25 * 20
                               : (2. / 7) * 25 * 20;
    CHECK(volume(c) == Catch::Approx(before - removed).margin(2e-4));
    const auto* native = OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
    REQUIRE(native);
    CHECK(BRepCheck_Analyzer(*native, true).IsValid());
    bool middle_remains = false;
    for (TopExp_Explorer it(*native, TopAbs_FACE); it.More(); it.Next()) {
      BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
      if (surface.GetType() == GeomAbs_Plane &&
          std::abs(surface.Plane().Distance(gp_Pnt(0, 0, 8))) < 1e-7 &&
          std::abs(surface.Plane().Axis().Direction().Z()) > .99)
        middle_remains = true;
    }
    CHECK_FALSE(middle_remains);
    auto reopened = f::checked(
        deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(p))));
    auto fresh = cache();
    execute(reopened, fresh);
    CHECK(volume(fresh) == Catch::Approx(volume(c)).margin(1e-6));
  }
}
TEST_CASE("A circular G1 cap cannot establish curvature continuity against its planar side",
          "[geometry][face-fillet][parity-b178]") {
  auto p = f::native_base();
  REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_full_round(
      FeatureId("finish"), "Full round", BodyId("body.base"), round(FilletContinuity::Tangent)))));
  auto c = cache();
  execute(p, c);
  auto native = OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  TopoDS_Face cap, side;
  for (TopExp_Explorer it(*native, TopAbs_FACE); it.More(); it.Next()) {
    auto f = TopoDS::Face(it.Current());
    BRepAdaptor_Surface s(f);
    if (s.GetType() == GeomAbs_Cylinder)
      cap = f;
    else if (s.GetType() == GeomAbs_Plane && std::abs(s.Plane().Axis().Direction().Y()) > .99)
      side = f;
  }
  REQUIRE_FALSE(cap.IsNull());
  REQUIRE_FALSE(side.IsNull());
  TopoDS_Edge join;
  for (TopExp_Explorer a(cap, TopAbs_EDGE); a.More(); a.Next())
    for (TopExp_Explorer b(side, TopAbs_EDGE); b.More(); b.Next())
      if (a.Current().IsSame(b.Current()))
        join = TopoDS::Edge(a.Current());
  REQUIRE_FALSE(join.IsNull());
  CHECK(detail::verify_surface_join(FeatureId("check"), join, cap, side, LoftBoundaryContinuity::G1,
                                    2e-5, 1e-4, 1e-4));
  auto g2 = detail::verify_surface_join(FeatureId("check"), join, cap, side,
                                        LoftBoundaryContinuity::G2, 2e-5, 1e-4, 1e-4);
  CHECK_FALSE(g2);
  if (!g2)
    CHECK(g2.error().message().find("curvature") != std::string::npos);
}
TEST_CASE("Imported coaxial full-round preserves bore and derives circular or G2 annular caps",
          "[geometry][face-fillet][parity-b178]") {
  for (auto continuity : {FilletContinuity::Tangent, FilletContinuity::Curvature}) {
    imported_body_test::Fixture fixture;
    auto tube = BRepAlgoAPI_Cut(BRepPrimAPI_MakeCylinder(8, 10).Shape(),
                                BRepPrimAPI_MakeCylinder(6, 10).Shape())
                    .Shape();
    fixture.write(tube);
    auto p = fixture.editable();
    auto c = cache();
    StepImportTopologyContext context(fixture.root);
    execute(p, c, &context);
    const BodyId body("body.1");
    const double before = volume(c, body);
    std::vector<FaceReference> first, second, middle;
    const auto& catalog = p.imported_topology_catalogs().front();
    for (const auto& entry : catalog.entries()) {
      if (entry.descriptor.kind != ImportedTopologyKind::Face)
        continue;
      auto matched = StepImportTopology{}.match_subshape(
          entry.descriptor, *OcctShapeView::get(*c.find_body_shape(body)));
      if (!matched)
        continue;
      BRepAdaptor_Surface s(TopoDS::Face(*OcctShapeView::get(matched.value().shape)));
      auto role = PartFeatureInputRole::FilletSupportFace;
      if (s.GetType() == GeomAbs_Plane && std::abs(s.Plane().Distance(gp_Pnt(0, 0, 10))) < 1e-6)
        role = PartFeatureInputRole::FilletMiddleFace;
      else if (s.GetType() != GeomAbs_Cylinder)
        continue;
      auto imported = f::checked(ImportedTopologyReference::create(
          catalog.id(), entry.id, ImportedTopologyCapability::Face));
      auto reference = f::checked(FaceReference::create_imported(role, imported));
      if (role == PartFeatureInputRole::FilletMiddleFace)
        middle.push_back(reference);
      else if (s.Cylinder().Radius() < 7)
        first.push_back(reference);
      else
        second.push_back(reference);
    }
    REQUIRE_FALSE(first.empty());
    REQUIRE_FALSE(second.empty());
    REQUIRE(middle.size() == 1);
    FullRoundIntent intent{{"inside", "Bore", first},
                           {"outside", "Outside", second},
                           {"middle", "Top", middle},
                           continuity,
                           {}};
    REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_full_round(
        FeatureId("finish"), "Annular full round", body, intent))));
    execute(p, c, &context);
    double removed = continuity == FilletContinuity::Tangent
                         ? (4 * std::numbers::pi - std::numbers::pi * std::numbers::pi) * 7
                         : 4 * std::numbers::pi / 7 * 7;
    CHECK(volume(c, body) == Catch::Approx(before - removed).margin(2e-4));
  }
}

TEST_CASE(
    "Face blend hold lines constrain complete contacts and reject an incompatible edit atomically",
    "[geometry][face-fillet][parity-b178]") {
  auto p = f::native_base();
  REQUIRE(p.add_construction_plane(
      f::checked(ConstructionPlane::create_explicit(ConstructionPlaneId("contact.plane"), "Contact",
                                                    {0, 5, 7}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  auto sketch = f::checked(
      Sketch::create(SketchId("contact.sketch"), "Contact", DatumPlaneId("contact.plane")));
  REQUIRE(sketch.add_entity(
      f::checked(LineSegment::create(SketchEntityId("contact.line"), {-10, 0}, {10, 0}))));
  REQUIRE(p.add_sketch(sketch));
  REQUIRE(p.add_path_curve(f::checked(PathCurve::create(
      PathCurveId("contact.path"), "Contact",
      {f::checked(PathSegmentReference::create_planar(sketch.id(), SketchEntityId("contact.line"),
                                                      PlanarPathCurveKind::Line))},
      PathClosure::Open, PathOrientationRule::MinimumTwist))));
  auto intent = blend(FilletContinuity::Tangent);
  intent.hold_lines = {{"held.front", FilletSide::Second, PathCurveId("contact.path")}};
  REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_face_blend(
      FeatureId("finish"), "Held blend", BodyId("body.base"), intent))));
  auto c = cache();
  execute(p, c);
  const auto before = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  REQUIRE(p.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(2))));
  const auto failed = GeometryRecomputeExecutor{}.execute_document(p, c);
  REQUIRE_FALSE(failed);
  CHECK(failed.error().message().find("held.front") != std::string::npos);
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(before));
}
TEST_CASE("Full-round preserves an upstream edited Hole and physical STEP material",
          "[geometry][face-fillet][parity-b178]") {
  auto p = f::native_base();
  REQUIRE(p.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(2))));
  REQUIRE(p.add_hole_feature(blcad::test::hole_fixture::hole()));
  REQUIRE(p.add_fillet_feature(f::checked(
      FilletFeature::create_full_round(FeatureId("finish"), "Perforated full round",
                                       BodyId("body.base"), round(FilletContinuity::Curvature)))));
  REQUIRE(p.apply_history_intent({{"base", "hole", "finish"}, {}}));
  auto c = cache();
  execute(p, c);
  const auto before = volume(c);
  REQUIRE(p.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(3))));
  execute(p, c);
  CHECK(volume(c) < before);
  const auto& shape = *c.find_body_shape(BodyId("body.base"));
  CHECK(blcad::test::classify_solid_point(shape, {0, 0, 1}) ==
        blcad::test::SolidPointLocation::Outside);
  CHECK(blcad::test::classify_solid_point(shape, {8, 0, 1}) ==
        blcad::test::SolidPointLocation::Inside);
  imported_body_test::Fixture fixture;
  auto path = fixture.root / "rounded.step";
  REQUIRE(StepExporter{}.write_part_step(p, c, path.string()));
  auto source = f::checked(
      StepImportSource::create(StepImportSourceId("physical"), "Physical", "rounded.step",
                               f::checked(step_import_sha256(imported_body_test::bytes(path))),
                               StepImportMode::EditableBody));
  auto imported = f::checked(StepImportReader{}.read(source, fixture.root));
  REQUIRE(imported.products.size() == 1);
  REQUIRE(imported.products[0].bodies.size() == 1);
  const auto& actual = imported.products[0].bodies[0].shape;
  CHECK(blcad::test::integrated_solid_volume(actual).volume_mm3 ==
        Catch::Approx(volume(c)).margin(2e-4));
  CHECK(blcad::test::classify_solid_point(actual, {0, 0, 1}) ==
        blcad::test::SolidPointLocation::Outside);
}
TEST_CASE("Full-round resolves tiny middle widths and rejects insufficient side height",
          "[geometry][face-fillet][parity-b178]") {
  auto p = f::native_base();
  REQUIRE(p.set_parameter_value(ParameterId("height"), f::checked(Quantity::length_mm(.002))));
  auto c = cache();
  execute(p, c);
  const auto before = volume(c);
  REQUIRE(p.add_fillet_feature(f::checked(
      FilletFeature::create_full_round(FeatureId("finish"), "Tiny full round", BodyId("body.base"),
                                       round(FilletContinuity::Curvature)))));
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(before - (2. / 7) * .001 * .001 * 20).margin(1e-8));
  REQUIRE(p.set_parameter_value(ParameterId("height"), f::checked(Quantity::length_mm(40))));
  const auto last = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(last));
}

TEST_CASE("Full-round consumes normal Thicken output and retains its source Body",
          "[geometry][face-fillet][parity-b178]") {
  auto p = blcad::test::surface_offset::thickened();
  REQUIRE(p.set_parameter_value(ParameterId("distance"), f::checked(Quantity::length_mm(8))));
  auto c = cache();
  execute(p, c);
  const auto before = volume(c, BodyId("thick.body")), retained = volume(c, BodyId("base.body"));
  const auto picks = f::checked(FinishingSelectionBuilder::build(
      p, c, BodyId("thick.body"), PartFeatureInputRole::FilletSupportFace));
  for (const auto side : {SemanticFace::Front, SemanticFace::Back})
    CHECK(std::any_of(picks.begin(), picks.end(), [&](const auto& pick) {
      const auto& source = std::get<FaceReference>(pick.reference).source();
      const auto* face = std::get_if<SemanticFaceReference>(&source);
      return face && face->source_feature() == FeatureId("base.feature") && face->face() == side;
    }));
  const auto selected =
      f::checked(ResultFaceSupportGeometry::selectable_faces(p, c, BodyId("thick.body")));
  REQUIRE(selected.size() == 2);
  const auto upper =
      std::max_element(selected.begin(), selected.end(),
                       [](const auto& a, const auto& b) { return a.anchor.z < b.anchor.z; });
  const auto support = [&](SemanticFace side) {
    return f::checked(FaceReference::create_planar(
        PartFeatureInputRole::FilletSupportFace,
        f::checked(SemanticFaceReference::create(FeatureId("base.feature"), side))));
  };
  FullRoundIntent intent{{"first", "Front", {support(SemanticFace::Front)}},
                         {"second", "Back", {support(SemanticFace::Back)}},
                         {"middle",
                          "Upper skin",
                          {f::checked(FaceReference::create_result(
                              PartFeatureInputRole::FilletMiddleFace, upper->reference))}},
                         FilletContinuity::Curvature,
                         {}};
  REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_full_round(
      FeatureId("finish"), "Thickened full round", BodyId("thick.body"), intent))));
  execute(p, c);
  CHECK(volume(c, BodyId("thick.body")) ==
        Catch::Approx(before - (2. / 7) * 3 * 3 * 8).margin(2e-5));
  CHECK(volume(c, BodyId("base.body")) == Catch::Approx(retained).margin(1e-8));
  auto opened = f::checked(
      deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(p))));
  auto fresh = cache();
  execute(opened, fresh);
  CHECK(volume(fresh, BodyId("thick.body")) ==
        Catch::Approx(volume(c, BodyId("thick.body"))).margin(1e-7));
}

TEST_CASE("Full-round consumes a stitched closed shell and preserves its original Body",
          "[geometry][face-fillet][parity-b178]") {
  auto p = f::native_base();
  REQUIRE(p.add_parameter(
      f::checked(Parameter::create_length(ParameterId("stitch.tolerance"), "Stitch tolerance",
                                          f::checked(Quantity::length_mm(1e-6))))));
  REQUIRE(p.add_body(f::checked(Body::create(BodyId("quilt"), "Quilt", BodyKind::Surface))));
  REQUIRE(p.add_body(f::checked(Body::create(BodyId("solid"), "Closed quilt", BodyKind::Solid))));
  std::vector<SurfaceReference> inputs;
  for (const auto side : {SemanticFace::Top, SemanticFace::Bottom, SemanticFace::Front,
                          SemanticFace::Back, SemanticFace::Left, SemanticFace::Right})
    inputs.push_back(f::checked(SurfaceReference::create_semantic_face(
        f::checked(SemanticFaceReference::create(FeatureId("base"), side)))));
  REQUIRE(p.add_surface_feature(f::checked(SurfaceStitchFeature::create(
      FeatureId("stitch"), "Stitch", inputs, ParameterId("stitch.tolerance"), BodyId("quilt")))));
  REQUIRE(p.add_surface_feature(f::checked(ClosedShellToSolidFeature::create(
      FeatureId("close"), "Close quilt", f::checked(SurfaceReference::create_body(BodyId("quilt"))),
      BodyId("solid")))));
  auto unrelated = p.features().front().with_body_result_context(f::new_body(p, "unrelated"));
  REQUIRE(unrelated);
  const auto& old = unrelated.value();
  auto other = f::checked(
      Feature::create_additive_extrude(FeatureId("unrelated"), "Coincident unrelated body",
                                       old.input_sketch(), old.length_parameter()));
  if (old.profile())
    other = f::checked(other.with_profile(*old.profile()));
  other = f::checked(other.with_body_result_context(*old.body_result_context()));
  REQUIRE(p.add_feature(other));
  auto c = cache();
  execute(p, c);
  const auto before = volume(c, BodyId("solid")), retained = volume(c);
  const auto picks = f::checked(FinishingSelectionBuilder::build(
      p, c, BodyId("solid"), PartFeatureInputRole::FilletSupportFace));
  CHECK(picks.size() == 6);
  for (const auto& pick : picks) {
    const auto& reference = std::get<FaceReference>(pick.reference);
    REQUIRE(std::holds_alternative<SemanticFaceReference>(reference.source()));
    CHECK(std::get<SemanticFaceReference>(reference.source()).source_feature() ==
          FeatureId("base"));
  }
  REQUIRE(p.add_fillet_feature(f::checked(
      FilletFeature::create_full_round(FeatureId("finish"), "Quilt full round", BodyId("solid"),
                                       round(FilletContinuity::Curvature)))));
  execute(p, c);
  CHECK(volume(c, BodyId("solid")) == Catch::Approx(before - (2. / 7) * 25 * 20).margin(2e-4));
  CHECK(volume(c) == Catch::Approx(retained).margin(1e-8));
}
