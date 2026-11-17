#include "../support/geometry_volume_oracle.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <TopoDS.hxx>
#include <numbers>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace {
void parameter(PartDocument& p, const char* id, double value) {
  f::accept(p.add_parameter(f::checked(
      Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
}
void source(PartDocument& p, double width = 4, double height = 3, Point2 center = {0, 0}) {
  parameter(p, "label.w", width);
  parameter(p, "label.h", height);
  parameter(p, "label.d", .5);
  auto sketch =
      f::checked(Sketch::create(SketchId("label.sketch"), "Label", DatumPlaneId("datum.xy")));
  f::accept(sketch.add_profile(f::checked(RectangleProfile::create(
      ProfileId("label.profile"), ParameterId("label.w"), ParameterId("label.h"), center))));
  f::accept(p.add_sketch(sketch));
}
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("emboss.test")));
}
void execute(const PartDocument& p, ShapeCache& c, StepImportTopologyContext* context = nullptr) {
  auto r = GeometryRecomputeExecutor(context).execute_document(p, c);
  INFO((r ? "ok" : r.error().object_id() + ": " + r.error().message()));
  REQUIRE(r);
}
std::string bytes(const GeometryShape& s) {
  std::ostringstream out;
  BRepTools::Write(*OcctShapeView::get(s), out);
  return out.str();
}
double volume(const ShapeCache& c, BodyId body) {
  return blcad::test::integrated_solid_volume(*c.find_body_shape(body)).volume_mm3;
}
bool material(const GeometryShape& s, Point3 p) {
  BRepClass3d_SolidClassifier c(*OcctShapeView::get(s), gp_Pnt(p.x, p.y, p.z), 1e-7);
  return c.State() == TopAbs_IN;
}
FaceReference support(const PartDocument& p, const ShapeCache& c, BodyId body,
                      GeomAbs_SurfaceType type, bool reversed = false,
                      StepImportTopologyContext* context = nullptr) {
  auto choices = f::checked(FinishingSelectionBuilder::build(
      p, c, body, PartFeatureInputRole::SurfaceSourceFace, context));
  for (const auto& f : choices) {
    const auto* ref = std::get_if<FaceReference>(&f.reference);
    if (!ref)
      continue;
    auto native = TopoDS::Face(*OcctShapeView::get(f.shape));
    BRepAdaptor_Surface s(native);
    if (s.GetType() == type &&
        (type == GeomAbs_Plane ? std::abs(s.Plane().Location().Z() - 8) < 1e-6
                               : (native.Orientation() == TopAbs_REVERSED) == reversed))
      return *ref;
  }
  FAIL("No qualified analytic support");
  throw std::runtime_error("support");
}
EmbossIntent intent(FaceReference face, EmbossMapping mapping = EmbossMapping::Planar,
                    EmbossOperation operation = EmbossOperation::Raise) {
  return {f::checked(
              ProfileRegionReference::create(SketchId("label.sketch"), ProfileId("label.profile"))),
          {face},
          operation,
          ParameterId("label.d"),
          mapping,
          {},
          0,
          std::nullopt};
}
void add(PartDocument& p, BodyId body, EmbossIntent e) {
  f::accept(p.add_sketch_driven_feature(f::checked(
      SketchDrivenFeature::create(FeatureId("label"), "Label", body, SketchDrivenDefinition{e}))));
}
void replay(const PartDocument& p, const ShapeCache& original, BodyId body,
            StepImportTopologyContext* context = nullptr) {
  auto json = f::checked(serialize_part_document_to_json(p));
  auto opened = f::checked(deserialize_part_document_from_json(json));
  CHECK(f::checked(serialize_part_document_to_json(opened)) == json);
  auto c = cache();
  execute(opened, c, context);
  CHECK(volume(c, body) == Catch::Approx(volume(original, body)).margin(2e-5));
}
} // namespace
TEST_CASE("Planar Emboss and Engrave change independently known material and preserve source bytes",
          "[geometry][emboss-geometry][parity-b183]") {
  for (auto mode : {EmbossOperation::Raise, EmbossOperation::Recess}) {
    auto p = f::native_base();
    source(p);
    auto c = cache();
    execute(p, c);
    auto before = bytes(*c.find_body_shape(BodyId("body.base")));
    auto immutable = *c.find_body_shape(BodyId("body.base"));
    add(p, BodyId("body.base"),
        intent(support(p, c, BodyId("body.base"), GeomAbs_Plane), EmbossMapping::Planar, mode));
    execute(p, c);
    CHECK(volume(c, BodyId("body.base")) ==
          Catch::Approx(1600 + (mode == EmbossOperation::Raise ? 6 : -6)).margin(1e-5));
    CHECK(bytes(immutable) == before);
    replay(p, c, BodyId("body.base"));
    REQUIRE(p.set_parameter_value(ParameterId("label.d"), f::checked(Quantity::length_mm(.8))));
    execute(p, c);
    CHECK(volume(c, BodyId("body.base")) ==
          Catch::Approx(1600 + (mode == EmbossOperation::Raise ? 9.6 : -9.6)).margin(1e-5));
  }
}
TEST_CASE("Cylindrical wrap keeps physical seam crossing and signed normal depth through STEP",
          "[geometry][emboss-wrap][parity-b183]") {
  for (auto mode : {EmbossOperation::Raise, EmbossOperation::Recess}) {
    imported_body_test::Fixture files;
    files.write(BRepPrimAPI_MakeCylinder(10, 20).Shape());
    auto p = files.editable();
    source(p, 4, 3, {0, 10});
    StepImportTopologyContext context(files.root);
    auto c = cache();
    execute(p, c, &context);
    auto original = bytes(*c.find_body_shape(BodyId("body.1")));
    auto e = intent(support(p, c, BodyId("body.1"), GeomAbs_Cylinder, false, &context),
                    EmbossMapping::Cylindrical, mode);
    e.seam_degrees = 360;
    add(p, BodyId("body.1"), e);
    execute(p, c, &context);
    const double delta = 12 * (.5 + (mode == EmbossOperation::Raise ? 1 : -1) * .25 / 20);
    CHECK(volume(c, BodyId("body.1")) ==
          Catch::Approx(2000 * std::numbers::pi + (mode == EmbossOperation::Raise ? delta : -delta))
              .margin(3e-4));
    const auto& shape = *c.find_body_shape(BodyId("body.1"));
    for (double u : {-.1, .1}) {
      const double r = mode == EmbossOperation::Raise ? 10.25 : 9.75;
      CHECK(material(shape, {r * std::cos(u), r * std::sin(u), 10}) ==
            (mode == EmbossOperation::Raise));
    }
    replay(p, c, BodyId("body.1"), &context);
    const auto path = files.root / "wrapped.step";
    const auto written = StepExporter{&context}.write_part_step(p, c, path.string());
    INFO((written ? "ok" : written.error().message()));
    REQUIRE(written);
    auto imported = f::checked(
        StepImportSource::create(StepImportSourceId("roundtrip"), "Round trip", "wrapped.step",
                                 f::checked(step_import_sha256(imported_body_test::bytes(path))),
                                 StepImportMode::EditableBody));
    auto read = f::checked(StepImportReader{}.read(imported, files.root));
    const auto& restored = read.products.front().bodies.front().shape;
    CHECK(blcad::test::integrated_solid_volume(restored).volume_mm3 ==
          Catch::Approx(volume(c, BodyId("body.1"))).margin(5e-4));
    for (double u : {-.1, .1}) {
      const double r = mode == EmbossOperation::Raise ? 10.25 : 9.75;
      CHECK(material(restored, {r * std::cos(u), r * std::sin(u), 10}) ==
            (mode == EmbossOperation::Raise));
    }
    CHECK(original == bytes(*c.find_feature_shape(FeatureId("import.base"))));
  }
}
TEST_CASE("Conical wrap follows meridian arclength and physical normal depth",
          "[geometry][emboss-wrap][parity-b183]") {
  for (auto mode : {EmbossOperation::Raise, EmbossOperation::Recess}) {
    imported_body_test::Fixture files;
    files.write(BRepPrimAPI_MakeCone(10, 15, 20).Shape());
    auto p = files.editable();
    source(p, 4, 3);
    StepImportTopologyContext context(files.root);
    auto c = cache();
    execute(p, c, &context);
    auto reference = support(p, c, BodyId("body.1"), GeomAbs_Cone, false, &context);
    const double beta = std::atan(.25), anchor = 10 / std::cos(beta), radius = 12.5;
    auto e = intent(reference, EmbossMapping::Conical, mode);
    e.anchor_mm = {0, anchor};
    e.seam_degrees = 360;
    add(p, BodyId("body.1"), e);
    execute(p, c, &context);
    const double sign = mode == EmbossOperation::Raise ? 1. : -1.;
    const double delta = 12 * .5 + sign * .5 * 4 / radius * 3 * std::cos(beta) * .25;
    CHECK(volume(c, BodyId("body.1")) ==
          Catch::Approx(9500 * std::numbers::pi / 3 + sign * delta).margin(5e-4));
    const auto check = [&](const GeometryShape& shape) {
      for (double u : {-.1, .1})
        for (double v : {anchor - .5, anchor + .5}) {
          const double r = 10 + v * std::sin(beta), depth = sign * .25;
          const Point3 at{(r + depth * std::cos(beta)) * std::cos(u),
                          (r + depth * std::cos(beta)) * std::sin(u),
                          v * std::cos(beta) - depth * std::sin(beta)};
          CHECK(material(shape, at) == (mode == EmbossOperation::Raise));
        }
    };
    check(*c.find_body_shape(BodyId("body.1")));
    replay(p, c, BodyId("body.1"), &context);
    const auto path = files.root / "cone.step";
    const auto written = StepExporter{&context}.write_part_step(p, c, path.string());
    INFO((written ? "ok" : written.error().message()));
    REQUIRE(written);
    auto exported = f::checked(
        StepImportSource::create(StepImportSourceId("export"), "Export", "cone.step",
                                 f::checked(step_import_sha256(imported_body_test::bytes(path))),
                                 StepImportMode::EditableBody));
    auto read = f::checked(StepImportReader{}.read(exported, files.root));
    const auto& restored = read.products.front().bodies.front().shape;
    CHECK(blcad::test::integrated_solid_volume(restored).volume_mm3 ==
          Catch::Approx(volume(c, BodyId("body.1"))).margin(6e-4));
    check(restored);
  }
}
TEST_CASE("Reversed cylindrical bore support uses its material normal for raise and recess",
          "[geometry][emboss-wrap][parity-b183]") {
  for (auto mode : {EmbossOperation::Raise, EmbossOperation::Recess}) {
    imported_body_test::Fixture files;
    files.write(BRepAlgoAPI_Cut(BRepPrimAPI_MakeCylinder(12, 20).Shape(),
                                BRepPrimAPI_MakeCylinder(10, 20).Shape())
                    .Shape());
    auto p = files.editable();
    source(p, 4, 3, {0, 10});
    StepImportTopologyContext context(files.root);
    auto c = cache();
    execute(p, c, &context);
    add(p, BodyId("body.1"),
        intent(support(p, c, BodyId("body.1"), GeomAbs_Cylinder, true, &context),
               EmbossMapping::Cylindrical, mode));
    execute(p, c, &context);
    const double delta = 12 * (.5 + (mode == EmbossOperation::Raise ? -1 : 1) * .25 / 20);
    CHECK(volume(c, BodyId("body.1")) ==
          Catch::Approx(880 * std::numbers::pi + (mode == EmbossOperation::Raise ? delta : -delta))
              .margin(5e-4));
  }
}
TEST_CASE("Invalid finite support wraps and erasing recesses preserve the last committed cache",
          "[geometry][emboss-failure][parity-b183]") {
  auto p = f::native_base();
  source(p);
  auto c = cache();
  execute(p, c);
  const auto before = bytes(*c.find_body_shape(BodyId("body.base")));
  auto e = intent(support(p, c, BodyId("body.base"), GeomAbs_Plane));
  SECTION("Wrong analytic mapping") {
    e.mapping = EmbossMapping::Cylindrical;
  }
  SECTION("Uncovered finite support") {
    e.anchor_mm = {100, 0};
  }
  SECTION("Erasing recess") {
    e.operation = EmbossOperation::Recess;
    REQUIRE(p.set_parameter_value(ParameterId("label.w"), f::checked(Quantity::length_mm(20))));
    REQUIRE(p.set_parameter_value(ParameterId("label.h"), f::checked(Quantity::length_mm(10))));
    REQUIRE(p.set_parameter_value(ParameterId("label.d"), f::checked(Quantity::length_mm(8))));
  }
  add(p, BodyId("body.base"), e);
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  CHECK(bytes(*c.find_body_shape(BodyId("body.base"))) == before);
}
