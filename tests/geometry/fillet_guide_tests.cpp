#include "../support/part_history_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/geometry/fillet_guides.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace {
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("guides")));
}
VariableFilletIntent law(EdgeReference edge) {
  return {{{"group",
            "Group",
            {{edge, false}},
            FilletPropagation::TangentChain,
            FilletRadiusInterpolation::Linear,
            {{"start", FilletStationLocation::Start, 0, ParameterId("radius")},
             {"quarter", FilletStationLocation::Fraction, .25, ParameterId("radius")},
             {"end", FilletStationLocation::End, 1, ParameterId("radius")}}}}};
}
FilletFeature variable(VariableFilletIntent intent, BodyId body = BodyId("body.base")) {
  return f::checked(FilletFeature::create_variable(FeatureId("finish"), "Variable", body, intent));
}
void execute(PartDocument& p, ShapeCache& c, StepImportTopologyContext* context = nullptr) {
  auto result = GeometryRecomputeExecutor(context).execute_document(p, c);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
}
std::string bytes(const GeometryShape& shape) {
  std::ostringstream s;
  BRepTools::Write(*OcctShapeView::get(shape), s);
  return s.str();
}
double separation(Point3 a, Point3 b) {
  return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}
FaceReference face(SemanticFace side,
                   PartFeatureInputRole role = PartFeatureInputRole::FilletSupportFace) {
  return f::checked(FaceReference::create_planar(
      role, f::checked(SemanticFaceReference::create(FeatureId("base"), side))));
}
// Exact cubic Bernstein geometry, with independent Simpson integration of its derivative.
TopoDS_Shape curved() {
  TColgp_Array1OfPnt poles(1, 4);
  poles(1) = {-15, -5, 0};
  poles(2) = {-13, -5, 0};
  poles(3) = {-7, -3, 0};
  poles(4) = {15, 2, 0};
  Handle(Geom_BezierCurve) bezier = new Geom_BezierCurve(poles);
  auto curve = GeomConvert::CurveToBSplineCurve(bezier);
  BRepBuilderAPI_MakeWire wire;
  wire.Add(BRepBuilderAPI_MakeEdge(curve, 0, .5).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(curve, .5, 1).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(curve->Value(1), gp_Pnt(15, 15, 0)).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(15, 15, 0), gp_Pnt(-15, 15, 0)).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(-15, 15, 0), curve->Value(0)).Edge());
  return BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(), gp_Vec(0, 0, 8))
      .Shape();
}
double arc(double u) {
  const int n = 2000;
  const double h = u / n;
  double sum = 0;
  for (int i = 0; i <= n; ++i) {
    double t = h * i;
    double dx = 6 * (1 - t) * (1 - t) + 36 * (1 - t) * t + 66 * t * t,
           dy = 12 * (1 - t) * t + 15 * t * t;
    sum += (i == 0 || i == n ? 1 : (i % 2 ? 4 : 2)) * std::hypot(dx, dy);
  }
  return sum * h / 3;
}
Point3 point_at_fraction(double fraction) {
  double lo = 0, hi = 1;
  for (int i = 0; i < 60; ++i) {
    double mid = (lo + hi) / 2;
    if (arc(mid) < fraction * arc(1))
      lo = mid;
    else
      hi = mid;
  }
  const double u = (lo + hi) / 2, q = 1 - u;
  return {-15 * q * q * q - 39 * q * q * u - 21 * q * u * u + 15 * u * u * u,
          -5 * q * q * q - 15 * q * q * u - 9 * q * u * u + 2 * u * u * u, 8};
}
} // namespace
TEST_CASE("Fillet station guides preserve physical positions and original basis under reversal and "
          "edit scope",
          "[geometry][fillet-guides][parity-b179]") {
  auto p = f::native_base();
  auto c = cache();
  execute(p, c);
  auto edge = f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::FilletEdge,
      f::checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopFront))));
  auto intent = law(edge);
  auto feature = variable(intent);
  const auto before = bytes(*c.find_body_shape(BodyId("body.base")));
  auto original = f::checked(FilletGuideQuery::stations(p, feature, c));
  REQUIRE(original.size() == 3);
  CHECK(separation(original[1].anchor, {-5, 5, 8}) < 1e-7);
  CHECK(original[1].chain_length_mm == Catch::Approx(20).margin(1e-7));
  CHECK(original[1].basis_direction.x > 1 - 1e-9);
  CHECK(separation(original[1].basis_origin, {-10, 5, 8}) < 1e-7);
  CHECK(bytes(*c.find_body_shape(BodyId("body.base"))) == before);
  intent.groups[0] = reverse_fillet_group(intent.groups[0]);
  auto reversed = f::checked(FilletGuideQuery::stations(p, variable(intent), c));
  for (std::size_t i = 0; i < original.size(); ++i) {
    CHECK(reversed[i].station == original[i].station);
    CHECK(reversed[i].radius == original[i].radius);
    CHECK(separation(reversed[i].anchor, original[i].anchor) < 1e-7);
    CHECK(reversed[i].basis_direction.x > 1 - 1e-9);
  }
  REQUIRE(p.add_fillet_feature(feature));
  execute(p, c);
  auto prefix = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(p, 1, prefix));
  CHECK_FALSE(FilletGuideQuery::stations(p, feature, prefix));
  CHECK_FALSE(FilletGuideQuery::stations(p, feature, prefix, nullptr, 0));
  auto edited = f::checked(FilletGuideQuery::stations(p, feature, prefix, nullptr, 1));
  CHECK(separation(edited[1].anchor, original[1].anchor) < 1e-7);
}
TEST_CASE("Fillet guides invert independent physical length on an imported nonuniform split cubic",
          "[geometry][fillet-guides][parity-b179]") {
  imported_body_test::Fixture files;
  files.write(curved());
  auto p = files.editable();
  REQUIRE(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("radius"), "Radius", f::checked(Quantity::length_mm(.3))))));
  auto c = cache();
  StepImportTopologyContext context(files.root);
  execute(p, c, &context);
  const auto& catalog = p.imported_topology_catalogs().front();
  auto found = std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& e) {
    return e.descriptor.kind == ImportedTopologyKind::Edge &&
           e.descriptor.geometry == ImportedGeometryClass::Opaque && e.descriptor.centroid.z > 7.9;
  });
  REQUIRE(found != catalog.entries().end());
  auto edge = f::checked(EdgeReference::create_imported(
      PartFeatureInputRole::FilletEdge,
      f::checked(ImportedTopologyReference::create(catalog.id(), found->id,
                                                   ImportedTopologyCapability::Edge))));
  auto intent = law(edge);
  intent.groups[0].stations.insert(
      intent.groups[0].stations.end() - 1,
      {"middle", FilletStationLocation::Fraction, .5, ParameterId("radius")});
  auto guides =
      f::checked(FilletGuideQuery::stations(p, variable(intent, BodyId("body.1")), c, &context));
  REQUIRE(guides.size() == 4);
  for (std::size_t i = 0; i < guides.size(); ++i) {
    const double fraction = intent.groups[0].stations[i].normalized_length();
    CHECK(separation(guides[i].anchor, point_at_fraction(fraction)) < 2e-5);
    CHECK(guides[i].chain_length_mm == Catch::Approx(arc(1)).margin(2e-5));
  }
  CHECK(separation(guides[1].anchor, point_at_fraction(.25)) < 2e-5);
}
TEST_CASE("Full-round guide size derives from actual side supports without a radius binding",
          "[geometry][fillet-guides][parity-b179]") {
  auto p = f::native_base();
  auto c = cache();
  execute(p, c);
  FullRoundIntent intent{
      {"first", "Front", {face(SemanticFace::Front)}},
      {"second", "Back", {face(SemanticFace::Back)}},
      {"middle", "Top", {face(SemanticFace::Top, PartFeatureInputRole::FilletMiddleFace)}},
      FilletContinuity::Curvature,
      {}};
  auto feature = f::checked(FilletFeature::create_full_round(FeatureId("finish"), "Full round",
                                                             BodyId("body.base"), intent));
  CHECK(feature.parameter_dependencies().empty());
  CHECK(f::checked(FilletGuideQuery::full_round_half_width(p, feature, c)) ==
        Catch::Approx(5).margin(1e-7));
  CHECK_FALSE(FilletGuideQuery::stations(p, feature, c));
  intent.second.faces = {face(SemanticFace::Left)};
  feature = f::checked(FilletFeature::create_full_round(FeatureId("finish"), "Full round",
                                                        BodyId("body.base"), intent));
  CHECK_FALSE(FilletGuideQuery::full_round_half_width(p, feature, c));
}
TEST_CASE("Periodic Fillet guides retain seam identity and original station positions on reversal",
          "[geometry][fillet-guides][parity-b179]") {
  imported_body_test::Fixture files;
  files.write(BRepPrimAPI_MakeCylinder(10, 8).Shape());
  auto p = files.editable();
  REQUIRE(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("radius"), "Radius", f::checked(Quantity::length_mm(.3))))));
  auto c = cache();
  StepImportTopologyContext context(files.root);
  execute(p, c, &context);
  const auto& catalog = p.imported_topology_catalogs().front();
  std::optional<EdgeReference> selected;
  for (const auto& entry : catalog.entries()) {
    if (entry.descriptor.kind != ImportedTopologyKind::Edge)
      continue;
    auto match = StepImportTopology{}.match_subshape(
        entry.descriptor, *OcctShapeView::get(*c.find_body_shape(BodyId("body.1"))));
    if (!match || match.value().status != ImportedTopologyRecoveryStatus::Resolved)
      continue;
    BRepAdaptor_Curve curve(TopoDS::Edge(*OcctShapeView::get(match.value().shape)));
    if (curve.GetType() != GeomAbs_Circle || curve.Value(curve.FirstParameter()).Z() < 7.9)
      continue;
    selected = f::checked(EdgeReference::create_imported(
        PartFeatureInputRole::FilletEdge,
        f::checked(ImportedTopologyReference::create(catalog.id(), entry.id,
                                                     ImportedTopologyCapability::Edge))));
    break;
  }
  REQUIRE(selected);
  auto intent = law(*selected);
  auto original =
      f::checked(FilletGuideQuery::stations(p, variable(intent, BodyId("body.1")), c, &context));
  REQUIRE(original.size() == 3);
  CHECK(original[0].chain_length_mm == Catch::Approx(20 * std::numbers::pi).margin(1e-6));
  CHECK(separation(original[0].anchor, original[2].anchor) < 1e-7);
  CHECK(std::abs(original[1].anchor.x) < 1e-6);
  CHECK(std::abs(original[1].anchor.y) == Catch::Approx(10).margin(1e-6));
  CHECK(original[1].anchor.z == Catch::Approx(8).margin(1e-6));
  intent.groups[0] = reverse_fillet_group(intent.groups[0]);
  auto reversed =
      f::checked(FilletGuideQuery::stations(p, variable(intent, BodyId("body.1")), c, &context));
  for (std::size_t i = 0; i < original.size(); ++i) {
    CHECK(original[i].station == reversed[i].station);
    CHECK(separation(original[i].anchor, reversed[i].anchor) < 1e-6);
    CHECK(dot(original[i].basis_direction, reversed[i].basis_direction) > 1 - 1e-9);
  }
}
TEST_CASE("Fillet guide queries reject stale source geometry and follow upstream dimensions",
          "[geometry][fillet-guides][parity-b179]") {
  auto p = f::native_base();
  auto c = cache();
  execute(p, c);
  auto seed = f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::FilletEdge,
      f::checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopFront))));
  const auto feature = variable(law(seed));
  REQUIRE(p.set_parameter_value(ParameterId("width"), f::checked(Quantity::length_mm(22))));
  CHECK_FALSE(FilletGuideQuery::stations(p, feature, c));
  execute(p, c);
  auto updated = f::checked(FilletGuideQuery::stations(p, feature, c));
  CHECK(separation(updated[1].anchor, {-5.5, 5, 8}) < 1e-7);
  CHECK(updated[1].chain_length_mm == Catch::Approx(22).margin(1e-7));
}
