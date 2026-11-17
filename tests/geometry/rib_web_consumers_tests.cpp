#include "../support/geometry_volume_oracle.hpp"
#include "../support/rib_web_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace r = blcad::test::rib_fixture;
namespace {
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("rib.consumers")));
}
void execute(PartDocument& p, ShapeCache& c, StepImportTopologyContext* context = nullptr) {
  auto result = GeometryRecomputeExecutor(context).execute_document(p, c);
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
}
double volume(const ShapeCache& c, BodyId body = BodyId("body.base")) {
  return blcad::test::integrated_solid_volume(*c.find_body_shape(body)).volume_mm3;
}
std::string bytes(const GeometryShape& shape) {
  std::ostringstream s;
  BRepTools::Write(*OcctShapeView::get(shape), s);
  return s.str();
}
ResultFaceReference face(const PartDocument& p, const ShapeCache& c, Point3 at, Vector3 normal) {
  auto targets = f::checked(ResultFaceSupportGeometry::selectable_faces(p, c, BodyId("body.base")));
  std::optional<ResultFaceReference> result;
  std::ostringstream choices;
  for (const auto& target : targets) {
    choices << target.anchor.x << "," << target.anchor.y << "," << target.anchor.z << " ";
    for (const auto& origin : target.reference.origins())
      choices << origin.feature.value() << ":" << origin.role << " ";
    choices << "\n";
    auto native = TopoDS::Face(*OcctShapeView::get(target.shape));
    BRepAdaptor_Surface surface(native);
    if (surface.GetType() != GeomAbs_Plane ||
        std::abs(surface.Plane().Axis().Direction().Dot(gp_Dir(normal.x, normal.y, normal.z))) <
            1 - 1e-8 ||
        surface.Plane().Distance(gp_Pnt(at.x, at.y, at.z)) > 1e-6)
      continue;
    BRepClass_FaceClassifier inside(native, gp_Pnt(at.x, at.y, at.z), 1e-7);
    if (inside.State() != TopAbs_IN)
      continue;
    REQUIRE_FALSE(result);
    result = target.reference;
  }
  INFO("Requested face at " << at.x << "," << at.y << "," << at.z);
  INFO(choices.str());
  REQUIRE(result);
  return *result;
}
FaceReference support(ResultFaceReference reference,
                      PartFeatureInputRole role = PartFeatureInputRole::FilletSupportFace) {
  return f::checked(FaceReference::create_result(role, std::move(reference)));
}
PartDocument cross() {
  auto p = r::fixture({f::checked(LineSegment::create(SketchEntityId("line"), {-8, 0}, {8, 0})),
                       f::checked(LineSegment::create(SketchEntityId("cross"), {0, -4}, {0, 4}))});
  auto intent = r::intent();
  intent.kind = RibWebKind::Web;
  intent.source.curves.push_back(SketchEntityId("cross"));
  r::add(p, intent);
  return p;
}
void unrelated(PartDocument& p) {
  auto context = f::new_body(p, "other");
  auto record = f::checked(Feature::create_additive_extrude(
      FeatureId("other"), "Other", SketchId("profile"), ParameterId("depth")));
  f::accept(p.add_feature(f::checked(record.with_body_result_context(context))));
  f::accept(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("other.place"), BodyId("body.other"), BodyTransformCoordinateSpace::World,
      std::nullopt, {50, 0, 0}, false, false))));
}
} // namespace
TEST_CASE("Cross Web generated side supports Hole and full-round finishing on one actual arm",
          "[geometry][rib-web-consumers][parity-b181]") {
  for (auto continuity : {FilletContinuity::Tangent, FilletContinuity::Curvature}) {
    auto p = cross();
    unrelated(p);
    auto c = cache();
    execute(p, c);
    const auto unrelated_bytes = bytes(*c.find_body_shape(BodyId("body.other")));
    auto side = face(p, c, {-4, .5, 10}, {0, 1, 0});
    const auto frame = f::checked(ResultFaceSupportGeometry::resolve_current(
                                      p, c, BodyId("body.base"), side.origins()))
                           .plane;
    const auto plane =
        f::checked(ResultFaceWorkplane::create(DatumPlaneId("rib.hole.plane"), "Rib side", side));
    f::accept(p.add_result_face_workplane(plane));
    f::accept(p.add_parameter(f::checked(Parameter::create_length(
        ParameterId("rib.hole.d"), "Rib hole diameter", f::checked(Quantity::length_mm(.2))))));
    const Vector3 delta{-4 - frame.origin.x, .5 - frame.origin.y, 10 - frame.origin.z};
    auto reference = f::checked(
        PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane, plane.id()));
    f::accept(p.add_hole_feature(f::checked(HoleFeature::create(
        FeatureId("rib.hole"), "Rib Hole", HoleKind::Plain, BodyId("body.base"), reference,
        {{HolePlacementId("center"), Point2{dot(delta, frame.x_axis), dot(delta, frame.y_axis)}}},
        ParameterId("rib.hole.d"), HoleThroughAllExtent{}, HoleDirection::OppositeSupportNormal,
        HoleFlatTip{}))));
    execute(p, c);
    CHECK(volume(c) == Catch::Approx(1692 - std::numbers::pi * .01).margin(2e-5));
    FullRoundIntent full{
        {"first", "First", {support(face(p, c, {-4, .5, 11}, {0, 1, 0}))}},
        {"second", "Second", {support(face(p, c, {-4, -.5, 11}, {0, 1, 0}))}},
        {"middle",
         "Arm top",
         {support(face(p, c, {-4, 0, 12}, {0, 0, 1}), PartFeatureInputRole::FilletMiddleFace)}},
        continuity,
        {}};
    // A whole cross cap is branched; two side faces cannot define one rolling span.
    auto branched = p;
    auto whole = full;
    for (const auto& at : {Point3{0, 2, 12}, Point3{4, 0, 12}, Point3{0, -2, 12}})
      whole.middle.faces.push_back(
          support(face(p, c, at, {0, 0, 1}), PartFeatureInputRole::FilletMiddleFace));
    f::accept(branched.add_fillet_feature(f::checked(FilletFeature::create_full_round(
        FeatureId("rib.branched"), "Unsupported whole cross", BodyId("body.base"), whole))));
    const auto retained = bytes(*c.find_body_shape(BodyId("body.base")));
    CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(branched, c));
    CHECK(bytes(*c.find_body_shape(BodyId("body.base"))) == retained);
    f::accept(p.add_fillet_feature(f::checked(FilletFeature::create_full_round(
        FeatureId("rib.round"), "Round arm", BodyId("body.base"), full))));
    execute(p, c);
    const double coefficient =
        continuity == FilletContinuity::Tangent ? 2 - std::numbers::pi / 2 : 2. / 7;
    CHECK(volume(c) ==
          Catch::Approx(1692 - std::numbers::pi * .01 - coefficient * .25 * 7.5).margin(3e-5));
    CHECK(bytes(*c.find_body_shape(BodyId("body.other"))) == unrelated_bytes);
    REQUIRE(p.set_parameter_value(ParameterId("rib.t"), f::checked(Quantity::length_mm(1.2))));
    execute(p, c);
    const double expected =
        1600 + (24 - 1.2) * 1.2 * 4 - std::numbers::pi * .01 * 1.2 - coefficient * .36 * 7.4;
    CHECK(volume(c) == Catch::Approx(expected).margin(4e-5));
    CHECK(bytes(*c.find_body_shape(BodyId("body.other"))) == unrelated_bytes);
    const auto text = f::checked(serialize_part_document_to_json(p));
    auto opened = f::checked(deserialize_part_document_from_json(text));
    auto fresh = cache();
    execute(opened, fresh);
    CHECK(f::checked(serialize_part_document_to_json(opened)) == text);
    CHECK(volume(fresh) == Catch::Approx(expected).margin(4e-5));
    imported_body_test::Fixture directory;
    const auto path = directory.root / "rib.step";
    REQUIRE(StepExporter{}.write_part_step(p, c, path.string()));
    auto source = f::checked(
        StepImportSource::create(StepImportSourceId("export"), "Export", "rib.step",
                                 f::checked(step_import_sha256(imported_body_test::bytes(path))),
                                 StepImportMode::EditableBody));
    const auto imported = f::checked(StepImportReader{}.read(source, directory.root));
    double total = 0;
    for (const auto& product : imported.products)
      for (const auto& body : product.bodies)
        total += blcad::test::integrated_solid_volume(body.shape).volume_mm3;
    CHECK(total == Catch::Approx(expected + 1600).margin(1e-3));
  }
}
TEST_CASE("Rib generated top edges accept the normal constant Fillet consumer",
          "[geometry][rib-web-consumers][parity-b181]") {
  auto p = r::fixture();
  r::add(p);
  auto c = cache();
  execute(p, c);
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("rib.finish.r"), "Finish radius", f::checked(Quantity::length_mm(.1))))));
  auto targets = f::checked(FinishingSelectionBuilder::build(p, c, BodyId("body.base"),
                                                             PartFeatureInputRole::FilletEdge));
  auto found = std::find_if(targets.begin(), targets.end(), [](const auto& target) {
    return std::abs(target.anchor.x + 8) < 1e-6 && std::abs(target.anchor.y) < 1e-6 &&
           std::abs(target.anchor.z - 12) < 1e-6;
  });
  REQUIRE(found != targets.end());
  auto edge = std::get<EdgeReference>(found->reference);
  f::accept(p.add_fillet_feature(
      f::checked(FilletFeature::create(FeatureId("rib.finish"), "Finish Rib", BodyId("body.base"),
                                       {edge}, ParameterId("rib.finish.r")))));
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1664 - (1 - std::numbers::pi / 4) * .01).margin(3e-5));
}
TEST_CASE("Generated Rib top remains an ordinary Shell removal support",
          "[geometry][rib-web-consumers][parity-b181]") {
  auto p = r::fixture({f::checked(LineSegment::create(SketchEntityId("line"), {-10, 0}, {10, 0}))});
  REQUIRE(p.set_parameter_value(ParameterId("height"), f::checked(Quantity::length_mm(1))));
  r::add(p);
  auto c = cache();
  execute(p, c);
  const auto source = bytes(*c.find_feature_shape(FeatureId("base")));
  CHECK(volume(c) == Catch::Approx(240).margin(1e-6));
  auto top = support(face(p, c, {0, 0, 12}, {0, 0, 1}), PartFeatureInputRole::ShellRemovalFace);
  f::accept(p.add_parameter(f::checked(
      Parameter::create_length(ParameterId("skin"), "Skin", f::checked(Quantity::length_mm(.1))))));
  f::accept(p.add_shell_feature(
      f::checked(ShellFeature::create(FeatureId("rib.shell"), "Shell Rib", BodyId("body.base"),
                                      {top}, ParameterId("skin"), ShellDirection::Inward))));
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(240 - 19.8 * .8 * 11.9).margin(1e-4));
  CHECK(bytes(*c.find_feature_shape(FeatureId("base"))) == source);
}
TEST_CASE(
    "Rib joins an imported target while retaining verified immutable source and stale rejection",
    "[geometry][rib-web-consumers][parity-b181]") {
  imported_body_test::Fixture directory;
  directory.write(BRepPrimAPI_MakeBox(gp_Pnt(-10, -5, 0), 20, 10, 8).Shape());
  const auto source_bytes = directory.source_bytes();
  auto p = directory.editable();
  for (const auto& [id, value] : {std::pair{"rib.t", 1.0}, {"rib.l", 12.0}})
    f::accept(p.add_parameter(imported_body_test::length(id, value)));
  auto sketch =
      f::checked(Sketch::create(SketchId("rib.sketch"), "Skeleton", DatumPlaneId("datum.xy")));
  f::accept(
      sketch.add_entity(f::checked(LineSegment::create(SketchEntityId("line"), {-8, 0}, {8, 0}))));
  f::accept(p.add_sketch(sketch));
  StepImportTopologyContext context(directory.root);
  auto c = cache();
  execute(p, c, &context);
  const auto base = *c.find_body_shape(BodyId("body.1"));
  const auto original = bytes(base);
  f::accept(p.add_sketch_driven_feature(f::checked(SketchDrivenFeature::create_rib_web(
      FeatureId("rib"), "Imported Rib", BodyId("body.1"), r::intent()))));
  execute(p, c, &context);
  CHECK(volume(c, BodyId("body.1")) == Catch::Approx(1664).margin(1e-5));
  CHECK(bytes(base) == original);
  CHECK(directory.source_bytes() == source_bytes);
  const auto result = bytes(*c.find_body_shape(BodyId("body.1")));
  directory.write_bytes(source_bytes + "\n");
  CHECK_FALSE(GeometryRecomputeExecutor(&context).execute_document(p, c));
  CHECK(bytes(*c.find_body_shape(BodyId("body.1"))) == result);
  directory.write_bytes(source_bytes);
}
TEST_CASE("Rib Body termination rejects a partially covered first face instead of a farther plate",
          "[geometry][rib-web-consumers][parity-b181]") {
  auto p = r::fixture();
  for (const auto& [id, value] : {std::pair{"peg.w", 2.0}, {"peg.h", 2.0}, {"plate.t", 1.0}})
    f::accept(p.add_parameter(imported_body_test::length(id, value)));
  const auto add_plate = [&](const char* id, double z, ParameterId w, ParameterId h,
                             FeatureBodyResultContext context) {
    const auto plane = f::checked(
        ConstructionPlane::create_explicit(ConstructionPlaneId(std::string(id) + ".plane"), id,
                                           {0, 0, z}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}));
    f::accept(p.add_construction_plane(plane));
    auto sketch = f::checked(Sketch::create(SketchId(std::string(id) + ".sketch"), id,
                                            DatumPlaneId(plane.id().value())));
    f::accept(sketch.add_profile(
        f::checked(RectangleProfile::create(ProfileId(std::string(id) + ".profile"), w, h))));
    f::accept(p.add_sketch(sketch));
    auto feature = f::checked(
        Feature::create_additive_extrude(FeatureId(id), id, sketch.id(), ParameterId("plate.t")));
    f::accept(p.add_feature(f::checked(feature.with_body_result_context(context))));
  };
  add_plate("peg", 16, ParameterId("peg.w"), ParameterId("peg.h"), f::new_body(p, "limit"));
  add_plate("plate", 17, ParameterId("width"), ParameterId("height"),
            f::checked(FeatureBodyResultContext::create(
                FeatureBodyOperationMode::Join, BodyId("body.limit"), BodyId("body.limit"))));
  auto c = cache();
  execute(p, c);
  const auto before = bytes(*c.find_body_shape(BodyId("body.base")));
  const auto limit = bytes(*c.find_body_shape(BodyId("body.limit")));
  const auto input_document = p;
  auto definition = r::intent();
  definition.extent = RibBodyExtent{BodyId("body.limit")};
  r::add(p, definition);
  auto rejected = GeometryRecomputeExecutor{}.execute_document(p, c);
  CHECK_FALSE(rejected);
  if (!rejected)
    CHECK(rejected.error().message().find("first Rib support") != std::string::npos);
  CHECK(bytes(*c.find_body_shape(BodyId("body.base"))) == before);
  CHECK(bytes(*c.find_body_shape(BodyId("body.limit"))) == limit);
  const auto faces = f::checked(
      ResultFaceSupportGeometry::selectable_faces(input_document, c, BodyId("body.limit")));
  auto back = std::find_if(faces.begin(), faces.end(),
                           [](const auto& value) { return std::abs(value.anchor.z - 18) < 1e-6; });
  REQUIRE(back != faces.end());
  definition.extent =
      RibFaceExtent{support(back->reference, PartFeatureInputRole::ExtrudeLimitFace)};
  REQUIRE(p.update_sketch_driven_feature(f::checked(SketchDrivenFeature::create_rib_web(
      FeatureId("rib"), "Rib", BodyId("body.base"), definition))));
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1760).margin(2e-5));
  CHECK(bytes(*c.find_body_shape(BodyId("body.limit"))) == limit);
}
