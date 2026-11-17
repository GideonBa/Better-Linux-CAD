#include "../support/engineering_resource_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/outline_area_oracle.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepTools.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace {
void parameter(PartDocument& p, const char* id, double value) {
  f::accept(p.add_parameter(f::checked(
      Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
}
PartDocument part() {
  auto p = f::native_base();
  parameter(p, "text.height", 4);
  parameter(p, "text.depth", .5);
  return p;
}
EmbossIntent intent(const std::string& text = "OBi", std::vector<SketchTextBinding> bindings = {}) {
  return {EmbossTextSource{f::checked(SketchText::create(SketchTextId("label"), SketchId("profile"),
                                                         text, "Barlow", ParameterId("text.height"),
                                                         {-4, -2}, std::nullopt, bindings)),
                           *bundled_outline_font("Barlow")},
          {f::checked(FaceReference::create_planar(
              PartFeatureInputRole::SurfaceSourceFace,
              f::checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))))},
          EmbossOperation::Raise,
          ParameterId("text.depth"),
          EmbossMapping::Planar,
          {},
          0,
          std::nullopt};
}
void add(PartDocument& p, EmbossIntent e) {
  f::accept(p.add_sketch_driven_feature(f::checked(SketchDrivenFeature::create(
      FeatureId("text"), "Text", BodyId("body.base"), SketchDrivenDefinition{e}))));
}
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("text")));
}
void execute(PartDocument& p, ShapeCache& c) {
  auto r = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((r ? "ok" : r.error().object_id() + ": " + r.error().message()));
  REQUIRE(r);
}
double volume(const ShapeCache& c) {
  return blcad::test::integrated_solid_volume(*c.find_body_shape(BodyId("body.base"))).volume_mm3;
}
std::string bytes(const ShapeCache& c) {
  std::ostringstream stream;
  BRepTools::Write(*OcctShapeView::get(*c.find_body_shape(BodyId("body.base"))), stream);
  return stream.str();
}
bool material(const GeometryShape& s, Point3 p) {
  BRepClass3d_SolidClassifier c(*OcctShapeView::get(s), gp_Pnt(p.x, p.y, p.z), 1e-7);
  return c.State() == TopAbs_IN;
}
} // namespace
TEST_CASE("Pinned text counters and dots retain independently integrated material and mass",
          "[geometry][emboss-text][parity-b183]") {
  auto p = part();
  blcad::test::engineering_fixture::annotate(p);
  auto c = cache();
  execute(p, c);
  add(p, intent());
  execute(p, c);
  blcad::test::OutlineAreaOracle oracle;
  const double area = oracle.area("OBi", 4);
  CHECK(volume(c) == Catch::Approx(1600 + .5 * area).margin(3e-4));
  auto physical = f::checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(physical.data().known_mass);
  CHECK(physical.data().known_mass->mass_kg ==
        Catch::Approx((1600 + .5 * area) * 4e-6).margin(2e-9));
  double pen = 0;
  for (unsigned char code : std::string("OBi")) {
    REQUIRE(FT_Load_Char(oracle.face, code,
                         FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
    const auto m = oracle.face->glyph->metrics;
    const double s = 4. / oracle.face->units_per_EM;
    const double x = -4 + (pen + m.horiBearingX + .55 * m.width) * s;
    const double y = -2 + (m.horiBearingY - .5 * m.height) * s;
    if (code == 'O')
      CHECK_FALSE(material(*c.find_body_shape(BodyId("body.base")), {x, y, 8.25}));
    if (code == 'B')
      for (double fraction : {.25, .75})
        CHECK_FALSE(material(*c.find_body_shape(BodyId("body.base")),
                             {x, -2 + (m.horiBearingY - fraction * m.height) * s, 8.25}));
    if (code == 'i')
      CHECK(material(*c.find_body_shape(BodyId("body.base")),
                     {x, -2 + (m.horiBearingY - .1 * m.height) * s, 8.25}));
    pen += oracle.face->glyph->advance.x;
  }
  REQUIRE(p.set_parameter_value(ParameterId("text.height"), f::checked(Quantity::length_mm(5))));
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1600 + .5 * oracle.area("OBi", 5)).margin(4e-4));
  auto text = f::checked(serialize_part_document_to_json(p));
  auto opened = f::checked(deserialize_part_document_from_json(text));
  auto fresh = cache();
  execute(opened, fresh);
  CHECK(f::checked(serialize_part_document_to_json(opened)) == text);
  CHECK(volume(fresh) == Catch::Approx(volume(c)).margin(2e-5));
  imported_body_test::Fixture files;
  auto path = files.root / "glyphs.step";
  REQUIRE(StepExporter{}.write_part_step(p, c, path.string()));
  auto source = f::checked(
      StepImportSource::create(StepImportSourceId("glyphs"), "Glyphs", "glyphs.step",
                               f::checked(step_import_sha256(imported_body_test::bytes(path))),
                               StepImportMode::EditableBody));
  auto read = f::checked(StepImportReader{}.read(source, files.root));
  CHECK(
      blcad::test::integrated_solid_volume(read.products.front().bodies.front().shape).volume_mm3 ==
      Catch::Approx(volume(c)).margin(5e-4));
}
TEST_CASE("Text tokens and source changes recompute with independent area while unsupported glyphs "
          "reject",
          "[geometry][emboss-text][parity-b183]") {
  auto p = part();
  add(p, intent("O ${n}", {{"n", ParameterId("count"), 0}}));
  auto c = cache();
  execute(p, c);
  blcad::test::OutlineAreaOracle oracle;
  CHECK(volume(c) == Catch::Approx(1600 + .5 * oracle.area("O 3", 4)).margin(3e-4));
  REQUIRE(p.set_parameter_value(ParameterId("count"), f::checked(Quantity::count(5))));
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1600 + .5 * oracle.area("O 5", 4)).margin(3e-4));
  const auto before = bytes(c);
  auto bad =
      f::checked(SketchDrivenFeature::create(FeatureId("text"), "Text", BodyId("body.base"),
                                             SketchDrivenDefinition{intent("\xF4\x8F\xBF\xBF")}));
  REQUIRE(p.update_sketch_driven_feature(bad));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  CHECK(bytes(c) == before);
}
TEST_CASE("Missing or changed pinned font bytes never substitute an ambient font",
          "[geometry][emboss-text][emboss-failure][parity-b183]") {
  auto p = part();
  auto c = cache();
  execute(p, c);
  const auto before = bytes(c);
  add(p, intent("O"));
  imported_body_test::Fixture files;
  bool corrupt = false;
  SECTION("Missing asset") {}
  SECTION("Digest mismatch") {
    corrupt = true;
  }
  if (corrupt) {
    std::ofstream font(files.root / "Barlow-Regular.ttf", std::ios::binary);
    font << "not the authored font";
  }
  struct Restore {
    std::optional<std::string> previous;
    Restore(const std::string& root) {
      if (const char* old = std::getenv("BLCAD_FONT_DIR"))
        previous = old;
      ::setenv("BLCAD_FONT_DIR", root.c_str(), 1);
    }
    ~Restore() {
      if (previous)
        ::setenv("BLCAD_FONT_DIR", previous->c_str(), 1);
      else
        ::unsetenv("BLCAD_FONT_DIR");
    }
  } restore(files.root.string());
  const auto outcome = GeometryRecomputeExecutor{}.execute_document(p, c);
  CHECK_FALSE(outcome);
  CHECK(outcome.error().message().find(corrupt ? "digest" : "unavailable") != std::string::npos);
  CHECK(bytes(c) == before);
}
