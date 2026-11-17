#include "../../src/geometry/fillet_radius_law.hpp"
#include "../../src/geometry/semantic_edge_resolution.hpp"
#include "../../src/geometry/variable_fillet_adapter.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/hole_feature_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRepTools_ReShape.hxx>
#include <BRep_Tool.hxx>
#include <GeomConvert.hxx>
#include <Geom_BezierCurve.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
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
  return f::checked(ShapeCache::create(ShapeCacheId("variable.cache")));
}
EdgeReference edge(SemanticEdge side = SemanticEdge::FrontLeft) {
  return f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::FilletEdge,
      f::checked(SemanticEdgeReference::create(FeatureId("base"), side))));
}
VariableFilletIntent intent(EdgeReference seed = edge()) {
  return {{{"chain",
            "Outer chain",
            {{seed, false}},
            FilletPropagation::Explicit,
            FilletRadiusInterpolation::Linear,
            {{"start", FilletStationLocation::Start, 0, ParameterId("r0")},
             {"end", FilletStationLocation::End, 1, ParameterId("r1")}}}}};
}
PartDocument document(double a = 1, double b = 2) {
  auto p = f::native_base();
  for (auto [id, value] : {std::pair{"r0", a}, {"r1", b}})
    f::accept(p.add_parameter(f::checked(
        Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
  return p;
}
FilletFeature feature(VariableFilletIntent law) {
  return f::checked(FilletFeature::create_variable(FeatureId("variable"), "Variable",
                                                   BodyId("body.base"), std::move(law)));
}
void execute(PartDocument& p, ShapeCache& c) {
  auto r = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((r ? "ok" : r.error().object_id() + ": " + r.error().message()));
  REQUIRE(r);
}
double distance(const TopoDS_Shape& shape, gp_Pnt p) {
  double best = 1e100;
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
    BRepExtrema_DistShapeShape d(BRepBuilderAPI_MakeVertex(p).Vertex(), it.Current());
    REQUIRE(d.IsDone());
    best = std::min(best, d.Value());
  }
  return best;
}
// The native contract is circular sections normal to the source spine, with tangent contacts
// on both supports. This is independent of the production radius law and native fillet builder.
void circles(const TopoDS_Shape& shape, const auto& at, const auto& radius) {
  for (double f : {.12, .24, .37, .51, .66, .79, .88}) {
    const auto [p, n] = at(f);
    const double r = radius(f);
    for (double angle : {0., .2, .45, .7, 1., 1.3, std::numbers::pi / 2}) {
      const auto contact =
          p.Translated(n * (r * (1 - std::cos(angle))) + gp_Vec(0, 0, r * (-1 + std::sin(angle))));
      CHECK(distance(shape, contact) < 2e-4);
    }
  }
}
} // namespace
TEST_CASE("Variable Fillet builds independent increasing and decreasing section circles and exact "
          "reversal",
          "[geometry][variable-fillet][parity-b176]") {
  for (bool decreasing : {false, true}) {
    auto p = document(decreasing ? 2 : 1, decreasing ? 1 : 2);
    auto law = intent();
    // Along the top/front edge, semantic direction runs left -> right in +X.
    law.groups[0].edges[0].edge = edge(SemanticEdge::TopFront);
    f::accept(p.add_fillet_feature(feature(law)));
    auto c = cache();
    execute(p, c);
    const auto original = *c.find_body_shape(BodyId("body.base"));
    const auto* shape = OcctShapeView::get(original);
    REQUIRE(shape);
    CHECK(BRepCheck_Analyzer(*shape, true).IsValid());
    circles(
        *shape, [](double f) { return std::pair{gp_Pnt(-10 + 20 * f, 5, 8), gp_Vec(0, -1, 0)}; },
        [&](double f) { return decreasing ? 2 - f : 1 + f; });
    const auto json = f::checked(serialize_part_document_to_json(p));
    law.groups[0] = reverse_fillet_group(law.groups[0]);
    f::accept(p.update_fillet_feature(feature(law)));
    execute(p, c);
    circles(
        *OcctShapeView::get(*c.find_body_shape(BodyId("body.base"))),
        [](double f) { return std::pair{gp_Pnt(-10 + 20 * f, 5, 8), gp_Vec(0, -1, 0)}; },
        [&](double f) { return decreasing ? 2 - f : 1 + f; });
    law.groups[0] = reverse_fillet_group(law.groups[0]);
    f::accept(p.update_fillet_feature(feature(law)));
    CHECK(f::checked(serialize_part_document_to_json(p)) == json);
    auto opened = f::checked(deserialize_part_document_from_json(json));
    auto fresh = cache();
    execute(opened, fresh);
    CHECK(blcad::test::integrated_solid_volume(*fresh.find_body_shape(BodyId("body.base")))
              .volume_mm3 ==
          Catch::Approx(blcad::test::integrated_solid_volume(original).volume_mm3).margin(1e-6));
  }
}
TEST_CASE(
    "Variable radius laws retain station values monotone bounds derivatives and oriented intervals",
    "[geometry][variable-fillet][parity-b176]") {
  using detail::RadiusKnot;
  std::vector<RadiusKnot> knots{{0, 1}, {2, 2}, {6, 2.5}, {10, 4}};
  for (bool cubic : {false, true}) {
    auto law = detail::radius_law(knots, cubic);
    auto reverse = detail::radius_law(knots, cubic, false, -1, 10);
    for (const auto& k : knots)
      CHECK(law->Value(k.length_mm) == Catch::Approx(k.radius_mm).margin(1e-13));
    for (int i = 1; i < 100; ++i) {
      const double x = i * .1;
      const auto it = std::upper_bound(knots.begin(), knots.end(), x,
                                       [](double x, const auto& k) { return x < k.length_mm; });
      const auto a = *(it - 1), b = *it;
      CHECK(law->Value(x) >= a.radius_mm - 1e-12);
      CHECK(law->Value(x) <= b.radius_mm + 1e-12);
      double v, d, dd;
      law->D2(x, v, d, dd);
      CHECK(v == Catch::Approx(reverse->Value(10 - x)).margin(1e-12));
      if (std::abs(x - 2) > .01 && std::abs(x - 6) > .01)
        CHECK(d ==
              Catch::Approx((law->Value(x + 1e-5) - law->Value(x - 1e-5)) / 2e-5).margin(1e-7));
    }
    auto trim = reverse->Trim(1, 9, 1e-7);
    CHECK(trim->NbIntervals(GeomAbs_C2) == 3);
    CHECK(trim->Value(3) == Catch::Approx(law->Value(7)).margin(1e-13));
  }
  auto periodic = detail::radius_law({{0, 1}, {5, 2}, {10, 1}}, true, true, -1, 3);
  CHECK(periodic->Value(2) == Catch::Approx(periodic->Value(12)).margin(1e-13));
  CHECK_THROWS(detail::radius_law({{0, 1}, {0, 2}}, true));
  CHECK_THROWS(detail::radius_law({{0, 1}, {2, 2}}, true, true));
  CHECK_THROWS(detail::radius_law({{0, 1}, {2, 2}}, false)->Value(-3));
}
TEST_CASE(
    "Variable Fillet diagnoses unresolved stations lost edges and disconnected chains atomically",
    "[geometry][variable-fillet][parity-b176]") {
  auto p = document();
  auto law = intent(edge(SemanticEdge::TopFront));
  f::accept(p.add_fillet_feature(feature(law)));
  auto c = cache();
  execute(p, c);
  const auto last = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  auto bad = law;
  bad.groups[0].edges.push_back({edge(SemanticEdge::BackLeft), false});
  f::accept(p.update_fillet_feature(feature(bad)));
  auto rejected = GeometryRecomputeExecutor{}.execute_document(p, c);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().object_id() == "chain");
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(last));
  bad = law;
  bad.groups[0].stations.insert(
      bad.groups[0].stations.begin() + 1,
      {"tiny", FilletStationLocation::Fraction, 1e-20, ParameterId("r0")});
  f::accept(p.update_fillet_feature(feature(bad)));
  rejected = GeometryRecomputeExecutor{}.execute_document(p, c);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().object_id() == "tiny");
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(last));
  bad = law;
  bad.groups[0].stations.back().radius = ParameterId("width");
  f::accept(p.update_fillet_feature(feature(bad)));
  rejected = GeometryRecomputeExecutor{}.execute_document(p, c);
  REQUIRE_FALSE(rejected);
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(last));
}
TEST_CASE("Variable Fillet checks actual curved periodic rim sections and seam closure",
          "[geometry][variable-fillet][parity-b176]") {
  auto p = document();
  auto c = cache();
  TopoDS_Shape shape = BRepPrimAPI_MakeCylinder(10, 8).Shape();
  TopoDS_Edge rim;
  for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next()) {
    auto e = TopoDS::Edge(it.Current());
    BRepAdaptor_Curve a(e);
    if (a.GetType() == GeomAbs_Circle && a.Value(a.FirstParameter()).Z() > 7) {
      rim = e;
      break;
    }
  }
  REQUIRE_FALSE(rim.IsNull());
  auto ref = f::checked(
      EdgeReference::create_result(PartFeatureInputRole::FilletEdge,
                                   f::checked(make_result_edge_reference(FeatureId("base"), rim))));
  auto law = intent(ref);
  law.groups[0].interpolation = FilletRadiusInterpolation::MonotoneCubic;
  law.groups[0].stations.back().radius = ParameterId("r0");
  law.groups[0].stations.insert(law.groups[0].stations.begin() + 1,
                                {"middle", FilletStationLocation::Fraction, .5, ParameterId("r1")});
  const auto record = feature(law);
  auto native = detail::variable_fillet(p, record, shape, c, nullptr, {});
  INFO((native ? "ok" : native.error().message()));
  REQUIRE(native);
  for (double theta : {.3, .8, 1.4, 2.1, 2.8, 3.5, 4.2, 5., 5.7}) {
    double t = theta / std::numbers::pi;
    if (t > 1)
      t = 2 - t;
    const double r = 1 + 3 * t * t - 2 * t * t * t;
    for (double a : {.1, .4, .8, 1.2, 1.5})
      CHECK(distance(native.value().shape, {(10 - r + r * std::cos(a)) * std::cos(theta),
                                            (10 - r + r * std::cos(a)) * std::sin(theta),
                                            8 - r + r * std::sin(a)}) < 2e-4);
  }
  law.groups[0].stations.back().radius = ParameterId("r1");
  auto invalid = detail::variable_fillet(p, feature(law), shape, c, nullptr, {});
  REQUIRE_FALSE(invalid);
  CHECK(invalid.error().message().find("seam") != std::string::npos);
}

TEST_CASE("Variable Fillet honors nonmonotone multiple stations on a native edge",
          "[geometry][variable-fillet][parity-b176]") {
  for (bool cubic : {false, true}) {
    auto p = document();
    auto law = intent(edge(SemanticEdge::TopFront));
    law.groups[0].stations.back().radius = ParameterId("r0");
    law.groups[0].stations.insert(
        law.groups[0].stations.begin() + 1,
        {"middle", FilletStationLocation::Fraction, .5, ParameterId("r1")});
    law.groups[0].interpolation =
        cubic ? FilletRadiusInterpolation::MonotoneCubic : FilletRadiusInterpolation::Linear;
    f::accept(p.add_fillet_feature(feature(law)));
    auto c = cache();
    execute(p, c);
    circles(
        *OcctShapeView::get(*c.find_body_shape(BodyId("body.base"))),
        [](double f) { return std::pair{gp_Pnt(-10 + 20 * f, 5, 8), gp_Vec(0, -1, 0)}; },
        [&](double f) {
          double t = f * 2;
          if (t > 1)
            t = 2 - t;
          return cubic ? 1 + 2 * t - t * t : 1 + t;
        });
  }
}

namespace {
Handle(Geom_BezierCurve) curved_source() {
  TColgp_Array1OfPnt poles(1, 4);
  poles(1) = {-15, -5, 0};
  poles(2) = {-13, -5, 0};
  poles(3) = {-7, -3, 0};
  poles(4) = {15, 2, 0};
  return new Geom_BezierCurve(poles);
}
TopoDS_Shape curved_body(bool split) {
  // Exact cubic B-spline representation avoids OCCT 7.6 raw-Bezier support reconstruction.
  auto curve = GeomConvert::CurveToBSplineCurve(curved_source());
  BRepBuilderAPI_MakeWire wire;
  if (split) {
    wire.Add(BRepBuilderAPI_MakeEdge(curve, 0, .5).Edge());
    wire.Add(BRepBuilderAPI_MakeEdge(curve, .5, 1).Edge());
  } else
    wire.Add(BRepBuilderAPI_MakeEdge(curve).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(curve->Value(1), gp_Pnt(15, 15, 0)).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(15, 15, 0), gp_Pnt(-15, 15, 0)).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(-15, 15, 0), curve->Value(0)).Edge());
  REQUIRE(wire.IsDone());
  return BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(), gp_Vec(0, 0, 8))
      .Shape();
}
std::vector<EdgeReference> curved_refs(const TopoDS_Shape& shape,
                                       FeatureId source = FeatureId("base")) {
  std::vector<std::pair<double, EdgeReference>> found;
  TopTools_IndexedMapOfShape edges;
  TopExp::MapShapes(shape, TopAbs_EDGE, edges);
  for (int i = 1; i <= edges.Extent(); ++i) {
    auto edge = TopoDS::Edge(edges(i));
    BRepAdaptor_Curve c(edge);
    if ((c.GetType() != GeomAbs_BezierCurve && c.GetType() != GeomAbs_BSplineCurve) ||
        c.Value(c.FirstParameter()).Z() < 7.9)
      continue;
    auto ref = f::checked(EdgeReference::create_result(
        PartFeatureInputRole::FilletEdge, f::checked(make_result_edge_reference(source, edge))));
    found.push_back({c.Value(c.FirstParameter()).X(), ref});
  }
  std::sort(found.begin(), found.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  std::vector<EdgeReference> refs;
  for (auto& [x, r] : found)
    refs.push_back(r);
  return refs;
}
// Independent Simpson integration of the analytic Bernstein derivative, rather than OCCT
// spine parameters or production law evaluation.
double arc(double u) {
  const int count = 2000;
  const double h = u / count;
  double sum = 0;
  for (int i = 0; i <= count; ++i) {
    const double t = h * i;
    const double dx = 6 * (1 - t) * (1 - t) + 36 * (1 - t) * t + 66 * t * t;
    const double dy = 12 * (1 - t) * t + 15 * t * t;
    sum += (i == 0 || i == count ? 1 : (i % 2 ? 4 : 2)) * std::hypot(dx, dy);
  }
  return sum * h / 3;
}
void curved_circles(const TopoDS_Shape& shape) {
  auto source = curved_source();
  const double total = arc(1);
  CHECK(std::abs(arc(.25) / total - .25) > .1);
  for (double u : {.12, .24, .37, .51, .66, .79, .88}) {
    gp_Pnt p;
    gp_Vec d;
    source->D1(u, p, d);
    p.SetZ(8);
    const auto n = gp_Vec(-d.Y(), d.X(), 0).Normalized();
    const double r = .3 + .3 * arc(u) / total;
    for (double a : {0., .2, .5, .8, 1.1, 1.4, std::numbers::pi / 2})
      CHECK(distance(shape, p.Translated(n * (r * (1 - std::cos(a))) +
                                         gp_Vec(0, 0, r * (-1 + std::sin(a))))) < 2e-4);
  }
}
} // namespace
TEST_CASE("Variable Fillet uses true Bezier length through explicit and expanded tangent chains",
          "[geometry][variable-fillet][parity-b175][parity-b176]") {
  for (bool split : {false, true}) {
    auto target = curved_body(split);
    const auto bytes = [](const TopoDS_Shape& shape) {
      std::ostringstream out;
      BRepTools::Write(shape, out);
      return out.str();
    };
    const auto before = bytes(target);
    auto refs = curved_refs(target);
    REQUIRE(refs.size() == (split ? 2 : 1));
    auto p = document(.3, .6);
    auto c = cache();
    auto law = intent(refs.front());
    if (split) {
      const auto original_refs = curved_refs(curved_body(false));
      auto split_lost = detail::variable_fillet(p, feature(intent(original_refs.front())), target,
                                                c, nullptr, {});
      REQUIRE_FALSE(split_lost);
      CHECK(split_lost.error().message().find("changed") != std::string::npos);
      auto missing = detail::variable_fillet(p, feature(law), target, c, nullptr, {});
      REQUIRE_FALSE(missing);
      CHECK(missing.error().message().find("complete") != std::string::npos);
      law.groups[0].propagation = FilletPropagation::TangentChain;
    }
    auto built = detail::variable_fillet(p, feature(law), target, c, nullptr, {});
    INFO((built ? "ok" : built.error().message()));
    REQUIRE(built);
    curved_circles(built.value().shape);
    if (split) {
      law.groups[0].propagation = FilletPropagation::Explicit;
      law.groups[0].edges.push_back({refs.back(), false});
      auto all = detail::variable_fillet(p, feature(law), target, c, nullptr, {});
      INFO((all ? "ok" : all.error().message()));
      REQUIRE(all);
      curved_circles(all.value().shape);
    }
    law.groups[0] = reverse_fillet_group(law.groups[0]);
    auto reversed = detail::variable_fillet(p, feature(law), target, c, nullptr, {});
    INFO((reversed ? "ok" : reversed.error().message()));
    REQUIRE(reversed);
    curved_circles(reversed.value().shape);
    CHECK(bytes(target) == before);
  }
}
TEST_CASE(
    "Variable Fillet replays a curved imported Body and preserves sections through physical STEP",
    "[geometry][variable-fillet][parity-b176]") {
  imported_body_test::Fixture fixture;
  fixture.write(curved_body(false));
  auto p = fixture.editable();
  for (auto [id, value] : {std::pair{"r0", .3}, {"r1", .6}})
    f::accept(p.add_parameter(f::checked(
        Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
  auto c = cache();
  StepImportTopologyContext context(fixture.root);
  GeometryRecomputeExecutor executor(&context);
  auto first = executor.execute_document(p, c);
  INFO((first ? "ok" : first.error().message()));
  REQUIRE(first);
  auto refs = curved_refs(*OcctShapeView::get(*c.find_body_shape(BodyId("body.1"))),
                          FeatureId("import.base"));
  REQUIRE(refs.size() == 1);
  const auto& catalog = p.imported_topology_catalogs().front();
  const auto entry =
      std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
        return entry.descriptor.kind == ImportedTopologyKind::Edge &&
               entry.descriptor.geometry == ImportedGeometryClass::Opaque &&
               entry.descriptor.centroid.z > 7.9;
      });
  REQUIRE(entry != catalog.entries().end());
  const auto imported_ref = f::checked(EdgeReference::create_imported(
      PartFeatureInputRole::FilletEdge,
      f::checked(ImportedTopologyReference::create(catalog.id(), entry->id,
                                                   ImportedTopologyCapability::Edge))));
  auto record = f::checked(FilletFeature::create_variable(
      FeatureId("variable"), "Imported variable", BodyId("body.1"), intent(imported_ref)));
  f::accept(p.add_fillet_feature(record));
  auto built = executor.execute_document(p, c);
  INFO((built ? "ok" : built.error().message()));
  REQUIRE(built);
  curved_circles(*OcctShapeView::get(*c.find_body_shape(BodyId("body.1"))));
  auto opened = f::checked(
      deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(p))));
  auto fresh = cache();
  REQUIRE(executor.execute_document(opened, fresh));
  curved_circles(*OcctShapeView::get(*fresh.find_body_shape(BodyId("body.1"))));
  const auto exported = fixture.root / "finished.step";
  REQUIRE(StepExporter(&context).write_part_step(p, c, exported.string()));
  auto source = f::checked(
      StepImportSource::create(StepImportSourceId("physical"), "Physical", "finished.step",
                               f::checked(step_import_sha256(imported_body_test::bytes(exported))),
                               StepImportMode::EditableBody));
  auto physical = f::checked(StepImportReader{}.read(source, fixture.root));
  REQUIRE(physical.products.size() == 1);
  REQUIRE(physical.products[0].bodies.size() == 1);
  curved_circles(*OcctShapeView::get(physical.products[0].bodies[0].shape));
}

TEST_CASE("Variable Fillet retains multi-solid Pattern results and rolls back excessive radii",
          "[geometry][variable-fillet][parity-b176]") {
  auto p = document();
  f::accept(p.add_datum_axis(
      f::checked(DatumAxis::create_explicit(DatumAxisId("pattern.x"), "X", {}, {1, 0, 0}))));
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("spacing"), "spacing", f::checked(Quantity::length_mm(40))))));
  f::accept(p.add_parameter(f::checked(
      Parameter::create_count(ParameterId("copies"), "copies", f::checked(Quantity::count(2))))));
  f::accept(
      p.add_body(f::checked(Body::create(BodyId("pattern.body"), "Pattern", BodyKind::Solid))));
  const auto direction = f::checked(AxisReference::create_datum_axis(
      PartFeatureInputRole::PatternAxis, DatumAxisId("pattern.x")));
  const auto result = f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                  {}, BodyId("pattern.body")));
  auto pattern = f::checked(LinearPatternFeature::create(
      FeatureId("pattern"), "Pattern",
      {f::checked(PatternSourceReference::body(BodyId("body.base")))}, direction,
      ParameterId("copies"), LinearPatternExtentMode::Spacing, ParameterId("spacing"),
      PatternDirectionSign::Positive, result));
  f::accept(p.add_linear_pattern_feature(std::move(pattern)));
  auto c = cache();
  execute(p, c);
  const auto* native = OcctShapeView::get(*c.find_body_shape(BodyId("pattern.body")));
  TopoDS_Edge seed;
  for (TopExp_Explorer it(*native, TopAbs_EDGE); it.More(); it.Next()) {
    auto e = TopoDS::Edge(it.Current());
    BRepAdaptor_Curve curve(e);
    auto a = curve.Value(curve.FirstParameter()), b = curve.Value(curve.LastParameter());
    if (std::abs(a.Y() - 5) < 1e-7 && std::abs(b.Y() - 5) < 1e-7 && a.Z() > 7.9 && b.Z() > 7.9 &&
        std::max(a.X(), b.X()) < 11) {
      seed = e;
      break;
    }
  }
  REQUIRE_FALSE(seed.IsNull());
  auto ref = f::checked(EdgeReference::create_result(
      PartFeatureInputRole::FilletEdge,
      f::checked(make_result_edge_reference(FeatureId("pattern"), seed))));
  auto record = f::checked(FilletFeature::create_variable(FeatureId("variable"), "Variable pattern",
                                                          BodyId("pattern.body"), intent(ref)));
  f::accept(p.add_fillet_feature(record));
  execute(p, c);
  auto last = *c.find_body_shape(BodyId("pattern.body"));
  CHECK(RectangleExtrusionAdapter{}.summarize(last).solid_count == 2);
  CHECK(blcad::test::integrated_solid_volume(*c.find_body_shape(BodyId("body.base"))).volume_mm3 ==
        Catch::Approx(1600).margin(1e-6));
  CHECK(blcad::test::integrated_solid_volume(last).volume_mm3 ==
        Catch::Approx(3200 - (1 - std::numbers::pi / 4) * 140 / 3).margin(5e-5));
  circles(
      *OcctShapeView::get(last),
      [](double f) { return std::pair{gp_Pnt(-10 + 20 * f, 5, 8), gp_Vec(0, -1, 0)}; },
      [](double f) { return 1 + f; });
  f::checked(p.set_parameter_value(ParameterId("r1"), f::checked(Quantity::length_mm(40))));
  auto bad = GeometryRecomputeExecutor{}.execute_document(p, c);
  REQUIRE_FALSE(bad);
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("pattern.body")))
            ->IsSame(*OcctShapeView::get(last)));
  f::checked(p.set_parameter_value(ParameterId("r1"), f::checked(Quantity::length_mm(2))));
  f::checked(p.set_parameter_value(ParameterId("spacing"), f::checked(Quantity::length_mm(45))));
  execute(p, c);
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*c.find_body_shape(BodyId("pattern.body")))
            .solid_count == 2);
}
TEST_CASE("Variable Fillet recovers an edited new Hole rim without changing radius stations",
          "[geometry][variable-fillet][parity-b176]") {
  auto p = document(.2, .4);
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("hole.diameter"), "hole_diameter", f::checked(Quantity::length_mm(4))))));
  f::accept(
      p.add_hole_feature(blcad::test::hole_fixture::hole("hole", ParameterId("hole.diameter"))));
  auto c = cache();
  execute(p, c);
  auto select = [&]() {
    auto targets = f::checked(FinishingSelectionBuilder::build(p, c, BodyId("body.base"),
                                                               PartFeatureInputRole::FilletEdge));
    for (const auto& target : targets) {
      auto ref = std::get_if<EdgeReference>(&target.reference);
      if (!ref)
        continue;
      auto edge = OcctShapeView::get(target.shape);
      if (!edge || edge->ShapeType() != TopAbs_EDGE)
        continue;
      BRepAdaptor_Curve curve(TopoDS::Edge(*edge));
      if (curve.GetType() == GeomAbs_Circle && curve.Value(curve.FirstParameter()).Z() > 7.9)
        return *ref;
    }
    throw std::runtime_error("No top Hole rim");
  };
  auto law = intent(select());
  law.groups[0].interpolation = FilletRadiusInterpolation::MonotoneCubic;
  law.groups[0].stations.back().radius = ParameterId("r0");
  law.groups[0].stations.insert(law.groups[0].stations.begin() + 1,
                                {"middle", FilletStationLocation::Fraction, .5, ParameterId("r1")});
  const auto verify_hole = [&](const GeometryShape& shape, double hole_radius) {
    const auto& ref = std::get<ResultEdgeReference>(law.groups[0].edges[0].edge.source());
    const auto seam = ref.samples().front();
    const double phase = std::atan2(seam.y, seam.x);
    const auto radius = [](double fraction) {
      double t = 2 * fraction;
      if (t > 1)
        t = 2 - t;
      return .2 + .2 * (3 * t * t - 2 * t * t * t);
    };
    for (double f : {.12, .24, .37, .51, .66, .79, .88}) {
      const double theta = phase + 2 * std::numbers::pi * f, r = radius(f);
      for (double angle : {0., .3, .7, 1.1, std::numbers::pi / 2}) {
        const double rho = hole_radius + r * (1 - std::cos(angle));
        CHECK(distance(*OcctShapeView::get(shape), {rho * std::cos(theta), rho * std::sin(theta),
                                                    8 - r + r * std::sin(angle)}) < 2e-4);
      }
    }
    double removed = 0;
    const int count = 2000;
    const double pi = std::numbers::pi;
    for (int i = 0; i <= count; ++i) {
      const double r = radius(double(i) / count);
      removed += (i == 0 || i == count ? 1 : (i % 2 ? 4 : 2)) *
                 (hole_radius * r * r * (1 - pi / 4) + r * r * r * (5. / 6 - pi / 4));
    }
    removed *= 2 * pi / count / 3;
    CHECK(blcad::test::integrated_solid_volume(shape).volume_mm3 ==
          Catch::Approx(1600 - 8 * pi * hole_radius * hole_radius - removed).margin(5e-5));
  };
  f::accept(p.add_fillet_feature(feature(law)));
  execute(p, c);
  verify_hole(*c.find_body_shape(BodyId("body.base")), 2);
  auto last = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  f::checked(
      p.set_parameter_value(ParameterId("hole.diameter"), f::checked(Quantity::length_mm(6))));
  auto lost = GeometryRecomputeExecutor{}.execute_document(p, c);
  REQUIRE_FALSE(lost);
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(last));
  // Repair resolves against the current pre-Fillet prefix, keeping all station identities/bindings.
  auto prefix = f::checked(p.with_history_intent({{"base", "hole", "variable"}, {"variable"}}));
  auto before = cache();
  execute(prefix, before);
  auto choices = f::checked(FinishingSelectionBuilder::build(prefix, before, BodyId("body.base"),
                                                             PartFeatureInputRole::FilletEdge));
  bool repaired = false;
  for (const auto& target : choices) {
    const auto* edge = OcctShapeView::get(target.shape);
    if (!edge || edge->ShapeType() != TopAbs_EDGE)
      continue;
    BRepAdaptor_Curve curve(TopoDS::Edge(*edge));
    if (curve.GetType() == GeomAbs_Circle && curve.Value(curve.FirstParameter()).Z() > 7.9) {
      law.groups[0].edges[0].edge = std::get<EdgeReference>(target.reference);
      repaired = true;
      break;
    }
  }
  REQUIRE(repaired);
  f::accept(p.update_fillet_feature(feature(law)));
  execute(p, c);
  verify_hole(*c.find_body_shape(BodyId("body.base")), 3);
  const auto& saved =
      std::get<VariableFilletIntent>(
          p.find_fillet_feature(FeatureId("variable"))->advanced_intent()->definition)
          .groups[0];
  CHECK(saved.stations[1].id == "middle");
  CHECK(saved.stations[1].fraction == .5);
  CHECK(saved.stations[1].radius == ParameterId("r1"));
}

TEST_CASE("Variable Fillet preserves radius side and units after Body scale and Mirror history",
          "[geometry][variable-fillet][parity-b176]") {
  auto p = document(.5, 1);
  f::accept(p.add_body_transform(f::checked(BodyTransform::create_uniform_scale(
      BodyTransformId("scale"), BodyId("body.base"), BodyTransformCoordinateSpace::World, {}, 2, {},
      false, false))));
  f::accept(
      p.add_body(f::checked(Body::create(BodyId("mirrored.body"), "Mirrored", BodyKind::Solid))));
  auto plane = f::checked(PlaneReference::create_datum_plane(PartFeatureInputRole::MirrorPlane,
                                                             DatumPlaneId("datum.xy")));
  auto body_context = f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                  {}, BodyId("mirrored.body")));
  auto mirror = f::checked(MirrorFeature::create(
      FeatureId("mirror"), "Mirror", {f::checked(MirrorSourceReference::body(BodyId("body.base")))},
      plane, body_context));
  f::accept(p.add_mirror_feature(std::move(mirror)));
  auto c = cache();
  execute(p, c);
  auto* source = OcctShapeView::get(*c.find_body_shape(BodyId("mirrored.body")));
  TopoDS_Edge seed;
  for (TopExp_Explorer it(*source, TopAbs_EDGE); it.More(); it.Next()) {
    auto e = TopoDS::Edge(it.Current());
    BRepAdaptor_Curve curve(e);
    auto a = curve.Value(curve.FirstParameter()), b = curve.Value(curve.LastParameter());
    if (std::abs(a.Y() - 10) < 1e-7 && std::abs(b.Y() - 10) < 1e-7 && std::abs(a.Z() + 16) < 1e-7 &&
        std::abs(b.Z() + 16) < 1e-7) {
      seed = e;
      break;
    }
  }
  REQUIRE_FALSE(seed.IsNull());
  auto ref = f::checked(EdgeReference::create_result(
      PartFeatureInputRole::FilletEdge,
      f::checked(make_result_edge_reference(FeatureId("mirror"), seed))));
  f::accept(p.add_fillet_feature(f::checked(FilletFeature::create_variable(
      FeatureId("variable"), "Mirrored variable", BodyId("mirrored.body"), intent(ref)))));
  execute(p, c);
  auto verify = [](const GeometryShape& shape) {
    const auto* native = OcctShapeView::get(shape);
    CHECK(BRepCheck_Analyzer(*native, true).IsValid());
    for (double f : {.15, .35, .65, .85}) {
      double r = .5 + .5 * f;
      for (double a : {0., .3, .7, 1.1, std::numbers::pi / 2})
        CHECK(distance(*native, {-20 + 40 * f, 10 - r * (1 - std::cos(a)),
                                 -16 + r * (1 - std::sin(a))}) < 2e-4);
    }
  };
  verify(*c.find_body_shape(BodyId("mirrored.body")));
  auto opened = f::checked(
      deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(p))));
  auto fresh = cache();
  execute(opened, fresh);
  verify(*fresh.find_body_shape(BodyId("mirrored.body")));
}
TEST_CASE("Variable tangent propagation rejects a shared-vertex branch before publishing geometry",
          "[geometry][variable-fillet][parity-b175][parity-b176]") {
  auto first = BRepPrimAPI_MakeBox(10, 10, 10).Shape();
  TopoDS_Edge seed;
  TopoDS_Vertex common;
  for (TopExp_Explorer it(first, TopAbs_EDGE); it.More(); it.Next()) {
    auto e = TopoDS::Edge(it.Current());
    BRepAdaptor_Curve c(e);
    auto a = c.Value(c.FirstParameter()), b = c.Value(c.LastParameter());
    if (std::hypot(a.X(), a.Y()) < 1e-8 && std::hypot(b.X(), b.Y()) < 1e-8) {
      seed = e;
      for (TopExp_Explorer v(e, TopAbs_VERTEX); v.More(); v.Next())
        if (BRep_Tool::Pnt(TopoDS::Vertex(v.Current())).Z() > 9)
          common = TopoDS::Vertex(v.Current());
      break;
    }
  }
  REQUIRE_FALSE(seed.IsNull());
  REQUIRE_FALSE(common.IsNull());
  auto attach = [&](TopoDS_Shape shape) {
    Handle(BRepTools_ReShape) replace = new BRepTools_ReShape;
    for (TopExp_Explorer it(shape, TopAbs_VERTEX); it.More(); it.Next())
      if (BRep_Tool::Pnt(TopoDS::Vertex(it.Current())).Distance(gp_Pnt(0, 0, 10)) < 1e-8) {
        replace->Replace(it.Current().Oriented(TopAbs_FORWARD), common.Oriented(TopAbs_FORWARD));
        break;
      }
    auto joined = replace->Apply(shape);
    BRep_Builder builder;
    for (TopExp_Explorer it(joined, TopAbs_EDGE); it.More(); it.Next()) {
      auto edge = TopoDS::Edge(it.Current());
      BRepAdaptor_Curve curve(edge);
      for (TopExp_Explorer v(edge, TopAbs_VERTEX); v.More(); v.Next())
        if (v.Current().IsSame(common)) {
          double parameter = curve.Value(curve.FirstParameter()).Distance(gp_Pnt(0, 0, 10)) < 1e-8
                                 ? curve.FirstParameter()
                                 : curve.LastParameter();
          builder.UpdateVertex(common, parameter, edge, 1e-7);
        }
    }
    return joined;
  };
  auto target = imported_body_test::compound(
      {first, attach(BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 10), 10, 10, 10).Shape()),
       attach(BRepPrimAPI_MakeBox(gp_Pnt(-10, -10, 10), 10, 10, 10).Shape())});
  REQUIRE(BRepCheck_Analyzer(target, true).IsValid());
  auto p = document(.2, .3);
  auto c = cache();
  auto ref = f::checked(EdgeReference::create_result(
      PartFeatureInputRole::FilletEdge,
      f::checked(make_result_edge_reference(FeatureId("base"), seed))));
  auto law = intent(ref);
  law.groups[0].propagation = FilletPropagation::TangentChain;
  auto rejected = detail::variable_fillet(p, feature(law), target, c, nullptr, {});
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().object_id() == "chain");
  CHECK(rejected.error().message().find("branch") != std::string::npos);
}

TEST_CASE("Editing a Fillet station relocates measured physical radii and retains its identity",
          "[geometry][variable-fillet][parity-b175][parity-b176]") {
  auto p = document(2, 2.2);
  auto law = intent(edge(SemanticEdge::TopFront));
  law.groups[0].stations.back().radius = ParameterId("r0");
  law.groups[0].stations.insert(
      law.groups[0].stations.begin() + 1,
      {"moving", FilletStationLocation::Fraction, .25, ParameterId("r1")});
  f::accept(p.add_fillet_feature(feature(law)));
  auto c = cache();
  for (double station : {.25, .6}) {
    law.groups[0].stations[1].fraction = station;
    f::accept(p.update_fillet_feature(feature(law)));
    execute(p, c);
    circles(
        *OcctShapeView::get(*c.find_body_shape(BodyId("body.base"))),
        [](double f) { return std::pair{gp_Pnt(-10 + 20 * f, 5, 8), gp_Vec(0, -1, 0)}; },
        [&](double f) { return 2 + .2 * (f <= station ? f / station : (1 - f) / (1 - station)); });
    CHECK(std::get<VariableFilletIntent>(
              p.find_fillet_feature(FeatureId("variable"))->advanced_intent()->definition)
              .groups[0]
              .stations[1]
              .id == "moving");
  }
  auto opened = f::checked(
      deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(p))));
  CHECK(std::get<VariableFilletIntent>(
            opened.find_fillet_feature(FeatureId("variable"))->advanced_intent()->definition)
            .groups[0]
            .stations[1]
            .fraction == .6);
}
