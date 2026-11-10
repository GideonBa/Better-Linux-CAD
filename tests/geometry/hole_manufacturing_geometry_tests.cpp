#include "../support/hole_manufacturing_fixture.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <gp_Cylinder.hxx>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace fixture = blcad::test::manufacturing_fixture;
using fixture::checked;
namespace {
ShapeCache evaluate(const PartDocument& p) {
  auto cache = checked(ShapeCache::create(ShapeCacheId("manufacturing")));
  (void)checked(GeometryRecomputeExecutor{}.execute_document(p, cache));
  REQUIRE(cache.validate_complete_geometry_result(p));
  return cache;
}
double volume(const ShapeCache& cache) {
  return RectangleExtrusionAdapter{}
      .summarize(*cache.find_body_shape(BodyId("body.base")))
      .volume_mm3;
}
bool material(const ShapeCache& cache, Point3 point) {
  BRepClass3d_SolidClassifier classifier(
      *OcctShapeView::get(*cache.find_body_shape(BodyId("body.base"))),
      gp_Pnt(point.x, point.y, point.z), 1e-7);
  return classifier.State() == TopAbs_IN;
}
ResultFaceReference reference(std::string role) {
  return checked(ResultFaceReference::create(FeatureId("hole"), BodyId("body.base"),
                                             {{FeatureId("hole"), "hole/6:center/" + role}}));
}
} // namespace
TEST_CASE(
    "Manufacturing bores match analytic seat step cone and slot volumes and independent sections",
    "[geometry][parity-b154][hole-manufacturing-geometry]") {
  const int mode = GENERATE(1, 2, 3, 4, 5, 6, 7, 8);
  const bool blind = GENERATE(false, true);
  auto p = fixture::part(mode, blind ? HoleExtent{HoleBlindExtent{ParameterId("blind")}}
                                     : HoleExtent{HoleThroughAllExtent{}});
  auto c = evaluate(p);
  const double depth = blind || mode == 7 ? 5 : 8;
  double removed = std::numbers::pi * depth;
  if (mode == 1 || mode == 3)
    removed += std::numbers::pi * 6;
  if (mode == 2 || mode == 7)
    removed += std::numbers::pi * 4 / 3;
  if (mode == 4)
    removed += std::numbers::pi * 11;
  if (mode == 7)
    removed += std::numbers::pi / 3;
  if (mode == 8)
    removed = (8 + std::numbers::pi) * depth;
  CHECK(volume(c) == Catch::Approx(1600 - removed).margin(1e-6));
  CHECK_FALSE(material(c, {0, 0, 7.5}));
  if (mode == 1 || mode == 3) {
    CHECK_FALSE(material(c, {1.5, 0, 7}));
    CHECK(material(c, {1.5, 0, 5.5}));
    REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, reference("seat/bottom")));
  }
  if (mode == 2 || mode == 7) {
    CHECK_FALSE(material(c, {1.5, 0, 7.75}));
    CHECK(material(c, {1.5, 0, 7.25}));
    const auto face =
        checked(ResultFaceSupportGeometry::resolve_face(p, c, reference("seat/wall")));
    CHECK(BRepAdaptor_Surface(TopoDS::Face(*OcctShapeView::get(face))).GetType() == GeomAbs_Cone);
  }
  if (mode == 4) {
    CHECK_FALSE(material(c, {2.5, 0, 7.5}));
    CHECK(material(c, {2.5, 0, 6.5}));
    CHECK_FALSE(material(c, {1.5, 0, 6.5}));
  }
  if (mode == 8) {
    CHECK_FALSE(material(c, {2.5, 0, 7}));
    CHECK(material(c, {2.9, .8, 7}));
  }
  if (blind && mode != 7) {
    CHECK(material(c, {0, 0, 2.9}));
    CHECK_FALSE(material(c, {0, 0, 3.1}));
  }
  if (mode == 7) {
    CHECK_FALSE(material(c, {0, 0, 2.5}));
    CHECK(material(c, {.8, 0, 2.5}));
  }
  const auto json = checked(serialize_part_document_to_json(p));
  CHECK(volume(evaluate(checked(deserialize_part_document_from_json(json)))) ==
        Catch::Approx(volume(c)).margin(1e-6));
}
TEST_CASE("Manufacturing mode edits keep primary walls and intentional seat identities with poses",
          "[geometry][parity-b154][hole-manufacturing-references]") {
  auto p = fixture::part(0);
  auto c = evaluate(p);
  const auto wall = reference("wall");
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, wall));
  for (int mode : {1, 3, 2, 4, 5, 6, 7}) {
    REQUIRE(p.update_hole_feature(fixture::feature(mode)));
    c = evaluate(p);
    REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, wall));
  }
  REQUIRE(p.update_hole_feature(fixture::feature(1)));
  c = evaluate(p);
  const auto seat = reference("seat/bottom");
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, seat));
  REQUIRE(p.update_hole_feature(fixture::feature(3)));
  c = evaluate(p);
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, seat));
  REQUIRE(p.update_hole_feature(fixture::feature(2)));
  c = evaluate(p);
  CHECK_FALSE(ResultFaceSupportGeometry::resolve_face(p, c, seat));
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("pose"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, checked(BodyTransformRotationAxis::create_explicit({}, {1, 0, 0})), 90, false,
      false))));
  c = evaluate(p);
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, wall));
  CHECK(volume(c) == Catch::Approx(1600 - std::numbers::pi * (8 + 4. / 3)).margin(1e-6));
}
TEST_CASE("Invalid seats slots and terminal extents retain prior complete Body results atomically",
          "[geometry][parity-b154][hole-manufacturing-invalid]") {
  const int invalid = GENERATE(0, 1, 2, 3);
  auto p = fixture::part(1);
  auto c = evaluate(p);
  const auto before = p;
  const auto shape = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  if (invalid == 0)
    REQUIRE(p.set_parameter_value(ParameterId("seat.depth"), checked(Quantity::length_mm(9))));
  if (invalid == 1)
    REQUIRE(p.set_parameter_value(ParameterId("seat.diameter"), checked(Quantity::length_mm(22))));
  if (invalid == 2) {
    REQUIRE(p.update_hole_feature(fixture::feature(8)));
    REQUIRE(p.set_parameter_value(ParameterId("diameter"), checked(Quantity::length_mm(3))));
  }
  if (invalid == 3)
    REQUIRE(
        p.update_hole_feature(fixture::feature(1, HoleBlindExtent{ParameterId("outer.depth")})));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  REQUIRE(c.validate_complete_geometry_result(before));
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(shape));
}
TEST_CASE("Manufacturing holes combine standard adoption material mass history and nonblocking "
          "spacing advice",
          "[geometry][parity-b154][hole-manufacturing-combined]") {
  auto p = fixture::seed(true);
  REQUIRE(p.add_engineering_resource(fixture::table()));
  blcad::test::engineering_fixture::annotate(p);
  auto hole = fixture::feature(1, HoleThroughAllExtent{}, true);
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      hole.id(), hole.name(), hole.kind(), hole.target_body(), hole.support(),
      {{HolePlacementId("left"), Point2{-3, 0}}, {HolePlacementId("right"), Point2{3, 0}}},
      hole.diameter(), hole.extent(), hole.direction(), hole.tip(), hole.manufacturing()))));
  auto c = evaluate(p);
  const auto before = checked(serialize_part_document_to_json(p));
  const auto advice =
      checked(GeometryRecomputeExecutor{}.hole_spacing_advisories(p, FeatureId("hole"), c, 3));
  REQUIRE(advice.size() == 1);
  CHECK(advice.front().actual_web_mm == Catch::Approx(2));
  CHECK(checked(serialize_part_document_to_json(p)) == before);
  REQUIRE(c.validate_complete_geometry_result(p));
  const auto physical = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(physical.data().complete_mass()->mass_kg ==
        Catch::Approx((1600 - 28 * std::numbers::pi) * 4e-6));
  REQUIRE(p.adopt_engineering_resource_revision(blcad::test::hole_fixture::key(),
                                                fixture::table("r2", false, 5)));
  c = evaluate(p);
  CHECK(volume(c) ==
        Catch::Approx(1600 - 2 * std::numbers::pi * (8 + (6.25 - 1) * 2)).margin(1e-6));
  REQUIRE(p.apply_history_intent({{"base", "hole"}, {"hole"}}));
  c = evaluate(p);
  CHECK(volume(c) == Catch::Approx(1600));
  REQUIRE(p.apply_history_intent({{"base", "hole"}, {}}));
  c = evaluate(p);
  auto prefix = checked(ShapeCache::create(ShapeCacheId("prefix")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(p, 1, prefix));
  CHECK_FALSE(GeometryRecomputeExecutor{}.hole_spacing_advisories(p, FeatureId("hole"), prefix, 3));
}

TEST_CASE("Seat termination checks the complete oblique limit and preserves its nonparticipant",
          "[geometry][parity-b154][hole-manufacturing-limits]") {
  const bool body_limit = GENERATE(false, true);
  const bool valid = GENERATE(false, true);
  auto p = fixture::seed();
  fixture::dimension(p, "limit.depth", valid ? 3 : 5);
  fixture::dimension(p, "limit.height", 20);
  auto limit_sketch =
      checked(Sketch::create(SketchId("limit.profile"), "Wide limit", DatumPlaneId("datum.xy")));
  REQUIRE(limit_sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("limit.rectangle"), ParameterId("width"), ParameterId("limit.height")))));
  REQUIRE(p.add_sketch(limit_sketch));
  REQUIRE(p.add_feature(
      checked(checked(Feature::create_additive_extrude(FeatureId("limit"), "Limit",
                                                       SketchId("limit.profile"),
                                                       ParameterId("limit.depth")))
                  .with_body_result_context(blcad::test::history_fixture::new_body(p, "limit")))));
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("limit.pose"), BodyId("body.limit"), BodyTransformCoordinateSpace::World,
      std::nullopt, checked(BodyTransformRotationAxis::create_explicit({}, {1, 0, 0})), 30, false,
      false))));
  auto cache = evaluate(p);
  const auto previous = p;
  const auto target = *OcctShapeView::get(*cache.find_body_shape(BodyId("body.base")));
  const auto limit = *OcctShapeView::get(*cache.find_body_shape(BodyId("body.limit")));
  const auto face = checked(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      checked(SemanticFaceReference::create(FeatureId("limit"), SemanticFace::Top))));
  REQUIRE(p.add_hole_feature(
      fixture::feature(1, body_limit ? HoleExtent{HoleUpToBodyExtent{BodyId("body.limit")}}
                                     : HoleExtent{HoleToFaceExtent{face}})));
  const auto result = GeometryRecomputeExecutor{}.execute_document(p, cache);
  INFO((result ? "ok" : result.error().message()));
  if (!valid) {
    REQUIRE_FALSE(result);
    CHECK(result.error().message().find("complete seat") != std::string::npos);
    REQUIRE(cache.validate_complete_geometry_result(previous));
    CHECK(OcctShapeView::get(*cache.find_body_shape(BodyId("body.base")))->IsSame(target));
    CHECK(OcctShapeView::get(*cache.find_body_shape(BodyId("body.limit")))->IsSame(limit));
    return;
  }
  REQUIRE(result);
  const double depth = 8 - 3 / std::cos(std::numbers::pi / 6);
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * (depth + 6)).margin(1e-6));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.limit")))
            .volume_mm3 == Catch::Approx(1200).margin(1e-6));
}

TEST_CASE("Support based seat depth and near-edge material are evaluated without hidden extension",
          "[geometry][parity-b154][hole-manufacturing-limits]") {
  SECTION("support one millimeter above material") {
    auto p = fixture::seed();
    const auto old = fixture::feature(1);
    REQUIRE(p.add_construction_plane(
        checked(ConstructionPlane::create_explicit(ConstructionPlaneId("air"), "Offset support",
                                                   {0, 0, 9}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
    const auto support = checked(PlaneReference::create_construction_plane(
        PartFeatureInputRole::HoleSupportPlane, ConstructionPlaneId("air")));
    REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
        old.id(), old.name(), old.kind(), old.target_body(), support, old.placements(),
        old.diameter(), old.extent(), old.direction(), old.tip(), old.manufacturing()))));
    auto c = evaluate(p);
    CHECK(volume(c) == Catch::Approx(1600 - 11 * std::numbers::pi).margin(1e-6));
    REQUIRE(p.set_parameter_value(ParameterId("seat.depth"), checked(Quantity::length_mm(.5))));
    CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
    REQUIRE(p.set_parameter_value(ParameterId("seat.depth"), checked(Quantity::length_mm(2))));
    REQUIRE(p.update_hole_feature(checked(HoleFeature::create(
        old.id(), old.name(), old.kind(), old.target_body(), support, old.placements(),
        old.diameter(), old.extent(), old.direction(), old.tip(), fixture::variant(4)))));
    // The outer step ends in air even though the inner step reaches material.
    CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  }
  SECTION("complete outer opening near the selected finite face edge") {
    const bool inside = GENERATE(false, true);
    auto p = fixture::seed();
    const auto old = fixture::feature(1);
    REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
        old.id(), old.name(), old.kind(), old.target_body(), old.support(),
        {{HolePlacementId("center"), Point2{0, inside ? 2.99 : 3.01}}}, old.diameter(),
        old.extent(), old.direction(), old.tip(), old.manufacturing()))));
    auto c = checked(ShapeCache::create(ShapeCacheId("edge")));
    const auto result = GeometryRecomputeExecutor{}.execute_document(p, c);
    CHECK(static_cast<bool>(result) == inside);
    if (inside)
      CHECK(volume(c) == Catch::Approx(1600 - 14 * std::numbers::pi).margin(1e-6));
  }
}

TEST_CASE("Step and slot side references follow authored identities through contour ordering edits",
          "[geometry][parity-b154][hole-manufacturing-references]") {
  auto p = fixture::part(4);
  auto c = evaluate(p);
  const auto outer = reference("step/5:outer/bottom");
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, outer));
  const auto old = p.hole_features().front();
  auto steps = std::get<HoleStepped>(old.manufacturing());
  std::reverse(steps.steps.begin(), steps.steps.end());
  REQUIRE(p.update_hole_feature(checked(HoleFeature::create(
      old.id(), old.name(), old.kind(), old.target_body(), old.support(), old.placements(),
      old.diameter(), old.extent(), old.direction(), old.tip(), steps))));
  c = evaluate(p);
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, outer));
  CHECK(volume(c) == Catch::Approx(1600 - 19 * std::numbers::pi).margin(1e-6));
  REQUIRE(p.update_hole_feature(fixture::feature(8)));
  c = evaluate(p);
  const auto right = reference("slot/10:slot.right");
  const auto verify_side = [&] {
    const auto shape = checked(ResultFaceSupportGeometry::resolve_face(p, c, right));
    const auto cylinder = BRepAdaptor_Surface(TopoDS::Face(*OcctShapeView::get(shape))).Cylinder();
    CHECK(cylinder.Radius() == Catch::Approx(1));
    CHECK(cylinder.Location().X() == Catch::Approx(2));
    CHECK(cylinder.Location().Y() == Catch::Approx(0).margin(1e-7));
    REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, reference("slot/8:slot.top")));
  };
  verify_side();
  const auto original = *p.find_sketch(SketchId("slot"));
  auto sketch = checked(Sketch::create(original.id(), original.name(), original.workplane()));
  for (const auto& line : original.line_segments())
    REQUIRE(sketch.add_entity(line));
  for (const auto& arc : original.arc_segments())
    REQUIRE(sketch.add_entity(arc));
  REQUIRE(sketch.add_profile(checked(ArcClosedProfile::create(
      ProfileId("slot"), {SketchEntityId("slot.right"), SketchEntityId("slot.bottom"),
                          SketchEntityId("slot.left"), SketchEntityId("slot.top")}))));
  REQUIRE(p.update_sketch(sketch));
  c = evaluate(p);
  verify_side();
  CHECK(volume(c) == Catch::Approx(1600 - (8 + std::numbers::pi) * 8).margin(1e-6));
  REQUIRE(p.update_construction_plane(
      checked(ConstructionPlane::create_explicit(ConstructionPlaneId("slot.plane"), "Wrong plane",
                                                 {0, 0, 7}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
}

TEST_CASE("A counterbore combines a real rim Chamfer with material mass and portable STEP",
          "[geometry][parity-b154][hole-manufacturing-combined]") {
  auto p = fixture::part(1);
  fixture::dimension(p, "chamfer.distance", .25);
  blcad::test::engineering_fixture::annotate(p);
  auto c = evaluate(p);
  const auto targets = checked(FinishingSelectionBuilder::build(p, c, BodyId("body.base"),
                                                                PartFeatureInputRole::ChamferEdge));
  const auto top = std::find_if(targets.begin(), targets.end(), [](const auto& target) {
    const auto* witness =
        std::get_if<ResultEdgeReference>(&std::get<EdgeReference>(target.reference).source());
    return witness && witness->curve_type() == 1 && target.anchor.z > 7.99;
  });
  REQUIRE(top != targets.end());
  REQUIRE(p.add_chamfer_feature(checked(ChamferFeature::create_equal_distance(
      FeatureId("chamfer"), "Seat rim", BodyId("body.base"),
      {std::get<EdgeReference>(top->reference)}, ParameterId("chamfer.distance")))));
  c = evaluate(p);
  // Integral of pi*((R+t)^2-R^2), t=0..c, for the 45-degree inner rim.
  const double expected = 1600 - std::numbers::pi * (14 + 2 * .25 * .25 + .25 * .25 * .25 / 3);
  CHECK(volume(c) == Catch::Approx(expected).margin(1e-6));
  CHECK_FALSE(material(c, {2.1, 0, 7.95}));
  CHECK(material(c, {2.2, 0, 7.9}));
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, reference("seat/bottom")));
  const auto physical = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(physical.data().complete_mass()->mass_kg == Catch::Approx(expected * 4e-6));
  const auto saved = checked(serialize_part_document_to_json(p));
  CHECK(volume(evaluate(checked(deserialize_part_document_from_json(saved)))) ==
        Catch::Approx(expected).margin(1e-6));
  imported_body_test::Fixture output;
  REQUIRE(StepExporter{}.write_part_step(p, c, output.path()));
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("export"), "Export", "supplier.step",
      checked(step_import_sha256(output.source_bytes())), StepImportMode::EditableBody));
  const auto reopened = checked(StepImportReader{}.read(source, output.root));
  REQUIRE(reopened.body_count() == 1);
  CHECK(reopened.products.front().bodies.front().volume_mm3 ==
        Catch::Approx(expected).margin(1e-6));
  CHECK(checked(serialize_part_document_to_json(p)) == saved);
}

TEST_CASE("Every manufacturing variant cuts verified imported material and exports actual solids",
          "[geometry][parity-b154][hole-manufacturing-import]") {
  const int mode = GENERATE(1, 2, 3, 4, 5, 6, 7, 8);
  CAPTURE(mode);
  imported_body_test::Fixture input;
  input.write(BRepPrimAPI_MakeBox(20, 10, 8).Shape());
  const auto supplier = input.source_bytes();
  auto p = input.editable();
  const auto seed = fixture::seed();
  for (const auto& parameter : seed.parameters())
    REQUIRE(p.add_parameter(parameter));
  REQUIRE(p.add_engineering_resource(fixture::table()));
  REQUIRE(p.add_construction_plane(checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("slot.plane"), "Existing slot plane",
                                         {10, 5, 8}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  REQUIRE(p.add_sketch(*seed.find_sketch(SketchId("slot"))));
  const auto& catalog = p.imported_topology_catalogs().front();
  const ImportedTopologyEntry* top = nullptr;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face &&
        entry.descriptor.geometry == ImportedGeometryClass::Plane &&
        (!top || entry.descriptor.centroid.z > top->descriptor.centroid.z))
      top = &entry;
  REQUIRE(top);
  const auto support = checked(PlaneReference::create_imported(
      PartFeatureInputRole::HoleSupportPlane,
      checked(ImportedTopologyReference::create(catalog.id(), top->id,
                                                ImportedTopologyCapability::Plane))));
  const auto body = p.imported_body_features().front().definition().outputs().front().body;
  const auto old = fixture::feature(mode, HoleThroughAllExtent{}, mode != 8);
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      old.id(), old.name(), old.kind(), body, support,
      {{HolePlacementId("center"), mode == 8 ? Point2{} : Point2{-5, 10}}}, old.diameter(),
      old.extent(), old.direction(), old.tip(), old.manufacturing()))));
  const auto saved = checked(serialize_part_document_to_json(p));
  p = checked(deserialize_part_document_from_json(saved));
  StepImportTopologyContext context(input.root);
  auto c = checked(ShapeCache::create(ShapeCacheId("import.manufacturing")));
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(p, c));
  REQUIRE(c.validate_complete_geometry_result(p));
  const double removed = mode == 1 || mode == 3 ? 14 * std::numbers::pi
                         : mode == 2            ? (8 + 4. / 3) * std::numbers::pi
                         : mode == 4            ? 19 * std::numbers::pi
                         : mode == 7            ? (5 + 5. / 3) * std::numbers::pi
                         : mode == 8            ? (8 + std::numbers::pi) * 8
                                                : 8 * std::numbers::pi;
  CHECK(RectangleExtrusionAdapter{}.summarize(*c.find_body_shape(body)).volume_mm3 ==
        Catch::Approx(1600 - removed).margin(1e-6));
  imported_body_test::Fixture output;
  REQUIRE(StepExporter{&context}.write_part_step(p, c, output.path()));
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("export"), "Export", "supplier.step",
      checked(step_import_sha256(output.source_bytes())), StepImportMode::EditableBody));
  const auto reopened = checked(StepImportReader{}.read(source, output.root));
  REQUIRE(reopened.body_count() == 1);
  CHECK(reopened.products.front().bodies.front().volume_mm3 ==
        Catch::Approx(1600 - removed).margin(1e-6));
  CHECK(input.source_bytes() == supplier);
  CHECK(checked(serialize_part_document_to_json(p)) == saved);
  const auto previous = *OcctShapeView::get(*c.find_body_shape(body));
  input.write_bytes(supplier + "\n");
  StepImportTopologyContext changed(input.root);
  CHECK_FALSE(GeometryRecomputeExecutor{&changed}.execute_document(p, c));
  CHECK(OcctShapeView::get(*c.find_body_shape(body))->IsSame(previous));
  input.write_bytes(supplier);
  StepImportTopologyContext recovered(input.root);
  REQUIRE(GeometryRecomputeExecutor{&recovered}.execute_document(p, c));
}

TEST_CASE(
    "A Slot refuses inward semicircles even when the Sketch is closed and geometrically valid",
    "[geometry][parity-b154][hole-manufacturing-invalid]") {
  const int inward = GENERATE(0, 1, 2, 3);
  auto p = fixture::part(8);
  auto c = evaluate(p);
  const auto previous = p;
  const auto shape = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  const auto original = *p.find_sketch(SketchId("slot"));
  auto candidate = checked(Sketch::create(original.id(), original.name(), original.workplane()));
  for (const auto& line : original.line_segments())
    REQUIRE(candidate.add_entity(line));
  for (const auto& arc : original.arc_segments()) {
    const bool right = arc.id() == SketchEntityId("slot.right");
    const bool reversed = right ? (inward & 1) : (inward & 2);
    REQUIRE(candidate.add_entity(checked(ArcSegment::create_three_point(
        arc.id(), arc.start(), reversed ? Point2{right ? 1. : -1., 0} : arc.mid(), arc.end()))));
  }
  REQUIRE(candidate.add_profile(*original.find_arc_closed_profile(ProfileId("slot"))));
  REQUIRE(p.update_sketch(candidate));
  const auto result = GeometryRecomputeExecutor{}.execute_document(p, c);
  if (inward) {
    REQUIRE_FALSE(result);
    REQUIRE(c.validate_complete_geometry_result(previous));
    CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(shape));
  } else {
    REQUIRE(result);
    CHECK(volume(c) == Catch::Approx(1600 - (8 + std::numbers::pi) * 8).margin(1e-6));
  }
}

TEST_CASE("A Slot respects the explicitly selected finite support and only cuts its target Body",
          "[geometry][parity-b154][hole-manufacturing-limits]") {
  const bool fits = GENERATE(false, true);
  auto p = fixture::seed();
  fixture::dimension(p, "support.width", fits ? 8 : 4);
  fixture::dimension(p, "support.height", 4);
  auto sketch = checked(
      Sketch::create(SketchId("support.profile"), "Finite support", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      checked(RectangleProfile::create(ProfileId("support.rectangle"), ParameterId("support.width"),
                                       ParameterId("support.height")))));
  REQUIRE(p.add_sketch(sketch));
  REQUIRE(p.add_feature(checked(
      checked(Feature::create_additive_extrude(FeatureId("support"), "Support Body", sketch.id(),
                                               ParameterId("depth")))
          .with_body_result_context(blcad::test::history_fixture::new_body(p, "support")))));
  auto c = evaluate(p);
  const auto previous = p;
  const auto support_shape = *OcctShapeView::get(*c.find_body_shape(BodyId("body.support")));
  const auto old = fixture::feature(8);
  const auto support = checked(PlaneReference::create_semantic_face(
      PartFeatureInputRole::HoleSupportPlane,
      checked(SemanticFaceReference::create(FeatureId("support"), SemanticFace::Top))));
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      old.id(), old.name(), old.kind(), old.target_body(), support, old.placements(),
      old.diameter(), old.extent(), old.direction(), old.tip(), old.manufacturing()))));
  const auto result = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((result ? "ok" : result.error().message()));
  if (!fits) {
    REQUIRE_FALSE(result);
    REQUIRE(c.validate_complete_geometry_result(previous));
    CHECK(volume(c) == Catch::Approx(1600));
    CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.support")))->IsSame(support_shape));
  } else {
    REQUIRE(result);
    CHECK(volume(c) == Catch::Approx(1600 - (8 + std::numbers::pi) * 8).margin(1e-6));
    CHECK(RectangleExtrusionAdapter{}
              .summarize(*c.find_body_shape(BodyId("body.support")))
              .volume_mm3 == Catch::Approx(8 * 4 * 8));
  }
}
