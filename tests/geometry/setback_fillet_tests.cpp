#include "../support/geometry_volume_oracle.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace {
PartDocument part() {
  auto p = f::native_base();
  for (const auto* id : {"width", "height", "depth"})
    REQUIRE(p.set_parameter_value(ParameterId(id), f::checked(Quantity::length_mm(30))));
  REQUIRE(p.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(2))));
  REQUIRE(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("setback"), "Setback", f::checked(Quantity::length_mm(6))))));
  return p;
}
VariableFilletIntent intent(bool setback) {
  VariableFilletIntent result;
  int i = 0;
  for (auto edge : {SemanticEdge::TopFront, SemanticEdge::TopLeft, SemanticEdge::FrontLeft}) {
    const auto id = "corner.edge." + std::to_string(i++);
    auto ref = f::checked(EdgeReference::create_linear(
        PartFeatureInputRole::FilletEdge,
        f::checked(SemanticEdgeReference::create(FeatureId("base"), edge))));
    result.groups.push_back(
        {id,
         id,
         {{ref, false}},
         FilletPropagation::Explicit,
         FilletRadiusInterpolation::Linear,
         {{id + ".start", FilletStationLocation::Start, 0, ParameterId("radius")},
          {id + ".end", FilletStationLocation::End, 1, ParameterId("radius")}}});
  }
  if (setback)
    result.setback = FilletSetbackCorner{"setback.corner",
                                         {"corner.edge.0", "corner.edge.1", "corner.edge.2"},
                                         ParameterId("setback")};
  return result;
}
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("corners.cache")));
}
void execute(PartDocument& p, ShapeCache& c) {
  auto result = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
}
std::string bytes(const TopoDS_Shape& shape) {
  std::ostringstream s;
  BRepTools::Write(shape, s);
  return s.str();
}
gp_Pnt point(double x, double y, double z) {
  return {-15 + x, 15 - y, 30 - z};
}
} // namespace
TEST_CASE("Three explicit constant edge groups retain a native spherical rolling corner",
          "[geometry][setback-fillet][parity-b178]") {
  auto p = part();
  REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_variable(
      FeatureId("finish"), "Rolling corner", BodyId("body.base"), intent(false)))));
  auto c = cache();
  execute(p, c);
  auto shape = OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  TopoDS_Face sphere;
  for (TopExp_Explorer it(*shape, TopAbs_FACE); it.More(); it.Next()) {
    auto f = TopoDS::Face(it.Current());
    BRepAdaptor_Surface s(f);
    if (s.GetType() == GeomAbs_Sphere)
      sphere = f;
  }
  REQUIRE_FALSE(sphere.IsNull());
  const auto s = BRepAdaptor_Surface(sphere).Sphere();
  CHECK(s.Radius() == Catch::Approx(2).margin(1e-6));
  CHECK(s.Location().Distance(point(2, 2, 2)) < 1e-6);
  for (const auto& v : std::array<gp_Vec, 3>{gp_Vec(1, 1, 1), gp_Vec(1, 2, 3), gp_Vec(3, 2, 1)}) {
    auto u = v.Normalized();
    auto expected = point(2 - 2 * u.X(), 2 - 2 * u.Y(), 2 - 2 * u.Z());
    BRepExtrema_DistShapeShape d(BRepBuilderAPI_MakeVertex(expected).Vertex(), sphere);
    REQUIRE(d.IsDone());
    CHECK(d.Value() < 2e-5);
  }
}
TEST_CASE("Setback builds analytic tangent transitions and preserves source and far radii",
          "[geometry][setback-fillet][parity-b178]") {
  auto p = part();
  auto c = cache();
  execute(p, c);
  const auto source = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  const auto before = bytes(source);
  REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_variable(
      FeatureId("finish"), "Setback corner", BodyId("body.base"), intent(true)))));
  execute(p, c);
  CHECK(bytes(source) == before);
  const auto shape = OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  REQUIRE(shape);
  CHECK(BRepCheck_Analyzer(*shape, true).IsValid());
  int spheres = 0;
  bool center = false;
  for (TopExp_Explorer it(*shape, TopAbs_FACE); it.More(); it.Next()) {
    BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
    if (surface.GetType() == GeomAbs_Sphere) {
      const auto sphere = surface.Sphere();
      CHECK(sphere.Radius() == Catch::Approx(2).margin(1e-6));
      const auto p = sphere.Location();
      const std::array<double, 3> coordinates{p.X() + 15, 15 - p.Y(), 30 - p.Z()};
      CHECK(std::count_if(coordinates.begin(), coordinates.end(),
                          [](double v) { return std::abs(v - 2) < 1e-6; }) == 2);
      CHECK(std::count_if(coordinates.begin(), coordinates.end(),
                          [](double v) { return std::abs(v - 6) < 1e-6; }) == 1);
      ++spheres;
    } else if (surface.GetType() == GeomAbs_Plane) {
      const gp_Dir normal(1, -1, -1);
      const auto plane = surface.Plane();
      if (std::abs(std::abs(plane.Axis().Direction().Dot(normal)) - 1) < 1e-9)
        center = true;
    }
  }
  CHECK(spheres == 3);
  CHECK(center);
  for (double angle : {.2, .5, .9, 1.3}) {
    const auto expected = point(18, 2 - 2 * std::cos(angle), 2 - 2 * std::sin(angle));
    CHECK(blcad::test::distance_to_faces(*c.find_body_shape(BodyId("body.base")),
                                         {expected.X(), expected.Y(), expected.Z()}) < 2e-5);
  }
  const auto serialized = f::checked(serialize_part_document_to_json(p));
  auto reopened = f::checked(deserialize_part_document_from_json(serialized));
  auto fresh = cache();
  execute(reopened, fresh);
  CHECK(
      blcad::test::integrated_solid_volume(*fresh.find_body_shape(BodyId("body.base")))
          .volume_mm3 ==
      Catch::Approx(
          blcad::test::integrated_solid_volume(*c.find_body_shape(BodyId("body.base"))).volume_mm3)
          .margin(1e-6));
  imported_body_test::Fixture files;
  const auto output = files.root / "setback.step";
  REQUIRE(StepExporter{}.write_part_step(p, c, output.string()));
  const auto source_record = f::checked(
      StepImportSource::create(StepImportSourceId("physical"), "Physical", "setback.step",
                               f::checked(step_import_sha256(imported_body_test::bytes(output))),
                               StepImportMode::EditableBody));
  const auto physical = f::checked(StepImportReader{}.read(source_record, files.root));
  REQUIRE(physical.products.size() == 1);
  REQUIRE(physical.products[0].bodies.size() == 1);
  CHECK(
      blcad::test::integrated_solid_volume(physical.products[0].bodies[0].shape).volume_mm3 ==
      Catch::Approx(
          blcad::test::integrated_solid_volume(*c.find_body_shape(BodyId("body.base"))).volume_mm3)
          .margin(2e-4));
  const auto old_volume =
      blcad::test::integrated_solid_volume(*c.find_body_shape(BodyId("body.base"))).volume_mm3;
  REQUIRE(p.set_parameter_value(ParameterId("setback"), f::checked(Quantity::length_mm(8))));
  execute(p, c);
  CHECK(blcad::test::integrated_solid_volume(*c.find_body_shape(BodyId("body.base"))).volume_mm3 <
        old_volume);
  const auto last = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  REQUIRE(p.set_parameter_value(ParameterId("setback"), f::checked(Quantity::length_mm(1))));
  auto failed = GeometryRecomputeExecutor{}.execute_document(p, c);
  REQUIRE_FALSE(failed);
  CHECK(failed.error().object_id() == "setback.corner");
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(last));
}
