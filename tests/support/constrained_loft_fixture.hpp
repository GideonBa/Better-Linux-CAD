#pragma once
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "loft_section_refinement_fixture.hpp"
#include "spatial_constraint_fixture.hpp"
#include <BRepGProp.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GProp_GProps.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
namespace blcad::test::constrained_loft {
namespace f = loft_refinement;
namespace sf = spatial_fixture;
using namespace blcad::geometry;
inline ProfileSectionReference section(std::string id) {
  return f::checked(
      ProfileSectionReference::create_closed_region(f::checked(ProfileRegionReference::create(
          SketchId(std::move(id)), ProfileId("outline"), PartFeatureInputRole::LoftSection))));
}
inline FeatureBodyResultContext context(std::string id) {
  return f::checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {}, BodyId(id)));
}
inline std::string role(std::string from, std::string to) {
  return "loft/patch/" + std::to_string(from.size()) + ":" + from + "/" +
         std::to_string(to.size()) + ":" + to;
}
inline LoftGuideBinding binding(const std::string& id,
                                const std::vector<ProfileSectionReference>& sections, bool right,
                                const std::vector<double>& fractions) {
  LoftGuideBinding result{PathCurveId(id), false, {}};
  for (std::size_t s = 0; s < sections.size(); ++s) {
    const std::size_t station = sections[s].source_node_id() == "section.1"
                                    ? 1
                                    : (sections[s].source_node_id() == "section.2" ||
                                               sections[s].source_node_id() == "after"
                                           ? 2
                                           : 0);
    result.anchors.push_back(
        {sections[s].identity(),
         SketchEntityId("upper." + std::to_string(right ? f::counts[station][0] - 1 : 0)),
         right ? 1.0 : 0.0, fractions[s]});
  }
  return result;
}
inline double add_rail(PartDocument& part, const std::string& id, Point3 a, Point3 mid, Point3 b,
                       bool straight = false) {
  auto sketch = f::checked(Sketch3D::create(Sketch3DId(id + ".sketch3d"), id));
  std::vector<Point3> points;
  if (straight)
    points = {a, b};
  else {
    const std::array<double, 9> greville{0,      1. / 18, 1. / 6,   1. / 3, .5,
                                         2. / 3, 5. / 6,  17. / 18, 1};
    for (int i = 0; i < 9; ++i) {
      if (i < 3)
        points.push_back({a.x, a.y, a.z + 40 * greville[i]});
      else if (i > 5)
        points.push_back({b.x, b.y, b.z - 40 * (1 - greville[i])});
      else
        points.push_back({std::lerp(a.x, b.x, greville[i]), std::lerp(a.y, b.y, greville[i]),
                          std::lerp(a.z, b.z, greville[i])});
    }
    points[4] = {(mid.x - (points[3].x + points[5].x) / 6) * 1.5,
                 (mid.y - (points[3].y + points[5].y) / 6) * 1.5,
                 (mid.z - (points[3].z + points[5].z) / 6) * 1.5};
  }
  std::vector<SketchPointReference3D> refs;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto name = "p" + std::to_string(i);
    REQUIRE(sketch.add_point(sf::point(name, points[i])));
    refs.push_back(f::checked(SketchPointReference3D::create_local_point(SketchEntityId(name))));
  }
  if (straight)
    REQUIRE(sketch.add_line(f::checked(
        SketchLine3D::create(SketchEntityId("rail"), SketchEntityId("p0"), SketchEntityId("p1")))));
  else
    REQUIRE(sketch.add_spline(f::checked(
        SketchSpline3D::create(SketchEntityId("rail"), SketchSpline3DRepresentation::ControlPoints,
                               refs, 3, SketchSpline3DContinuity::Curvature))));
  (void)f::checked(part.add_sketch_3d(sketch));
  const auto segment = f::checked(PathSegmentReference::create_sketch_3d(
      sketch.id(), SketchEntityId("rail"),
      straight ? Sketch3DPathCurveKind::Line : Sketch3DPathCurveKind::Spline));
  (void)f::checked(part.add_path_curve(f::checked(PathCurve::create(
      PathCurveId(id), id, {segment}, PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {},
      1e-7, f::checked(PathExecutionPolicy::curve_preserving(1e-7))))));
  if (straight)
    return .5;
  TColgp_Array1OfPnt poles(1, 9);
  for (int i = 1; i <= 9; ++i)
    poles.SetValue(i, {points[i - 1].x, points[i - 1].y, points[i - 1].z});
  TColStd_Array1OfReal knots(1, 7);
  TColStd_Array1OfInteger mult(1, 7);
  for (int i = 1; i <= 7; ++i) {
    knots(i) = i - 1;
    mult(i) = i == 1 || i == 7 ? 4 : 1;
  }
  const Handle(Geom_BSplineCurve) curve = new Geom_BSplineCurve(poles, knots, mult, 3);
  GeomAdaptor_Curve adaptor(curve);
  return GCPnts_AbscissaPoint::Length(adaptor, 0, 3, 1e-11) /
         GCPnts_AbscissaPoint::Length(adaptor, 0, 6, 1e-11);
}
inline Point3 endpoint(std::size_t station, bool right) {
  return f::model(station, f::poles(station, true)[right ? 3 : 0]);
}
inline PartDocument airfoil(bool solid = true, bool supports = true, bool center = true) {
  auto part = f::inputs();
  (void)f::checked(part.add_parameter(f::checked(Parameter::create_length(
      ParameterId("tolerance"), "Tolerance", f::checked(Quantity::length_mm(1e-5))))));
  for (const bool first : {true, false}) {
    const std::size_t station = first ? 0 : 2;
    const std::string name = first ? "before" : "after", prefix = first ? "root" : "tip";
    auto origin = f::model(station, {0, 0});
    origin.z += first ? -5 : 5;
    const double angle = first ? 0 : -.08;
    const Vector3 x{std::cos(angle), 0, std::sin(angle)}, y{0, 1, 0};
    (void)f::checked(part.add_construction_plane(f::checked(ConstructionPlane::create_explicit(
        ConstructionPlaneId(name + ".plane"), name, origin, x, y, cross(x, y)))));
    auto sketch = f::checked(Sketch::create(SketchId(name), name, DatumPlaneId(name + ".plane")));
    std::array<std::vector<SketchEntityId>, 2> ids;
    for (int side = 0; side < 2; ++side)
      for (std::size_t i = 0; i < f::counts[station][side]; ++i) {
        const SketchEntityId id(std::string(side == 0 ? "upper." : "lower.") + std::to_string(i));
        const auto p = f::piece(station, side == 0, i, f::counts[station][side]);
        REQUIRE(sketch.add_entity(
            f::checked(SplineSegment::create_cubic_bezier(id, p[0], p[1], p[2], p[3]))));
        ids[side].push_back(id);
      }
    auto contour = ids[0];
    contour.insert(contour.end(), ids[1].rbegin(), ids[1].rend());
    REQUIRE(
        sketch.add_profile(f::checked(ArcClosedProfile::create(ProfileId("outline"), contour))));
    (void)f::checked(part.add_sketch(sketch));
    std::vector<ProfileSectionReference> sections =
        first ? std::vector{section(name), section("section.0")}
              : std::vector{section("section.2"), section(name)};
    for (const bool right : {false, true}) {
      auto a = endpoint(station, right), b = a;
      if (first)
        a.z -= 5;
      else
        b.z += 5;
      add_rail(part, prefix + (right ? ".right" : ".left"), a, {}, b, true);
    }
    (void)f::checked(
        part.add_body(f::checked(Body::create(BodyId(prefix), prefix, BodyKind::Surface))));
    auto feature = f::checked(LoftFeature::create_loft_surface(
        FeatureId(prefix), prefix, sections, context(prefix), {},
        {PathCurveId(prefix + ".left"), PathCurveId(prefix + ".right")}));
    LoftConstraintIntent intent{ParameterId("tolerance"),
                                1e-4,
                                1e-4,
                                {},
                                {},
                                {binding(prefix + ".left", sections, false, {0, 1}),
                                 binding(prefix + ".right", sections, true, {0, 1})}};
    (void)f::checked(part.add_loft_feature(f::checked(feature.with_constraints(intent))));
  }
  std::vector<ProfileSectionReference> sections{section("section.0"), section("section.1"),
                                                section("section.2")};
  LoftConstraintIntent intent{ParameterId("tolerance"), 1e-4, 1e-4, {}, {}, {}};
  for (const bool right : {false, true}) {
    const std::string id = right ? "main.right" : "main.left";
    const double mid =
        add_rail(part, id, endpoint(0, right), endpoint(1, right), endpoint(2, right));
    intent.guides.push_back(binding(id, sections, right, {0, mid, 1}));
  }
  if (center)
    add_rail(part, "center", f::model(0, {0, 0}), {}, f::model(2, {0, 0}), true);
  (void)f::checked(part.add_body(f::checked(
      Body::create(BodyId("main"), "Main", solid ? BodyKind::Solid : BodyKind::Surface))));
  const auto feature = f::checked(
      solid ? LoftFeature::create_loft(FeatureId("main"), "Main", sections, context("main"),
                                       center ? std::optional(PathCurveId("center")) : std::nullopt,
                                       {PathCurveId("main.left"), PathCurveId("main.right")})
            : LoftFeature::create_loft_surface(
                  FeatureId("main"), "Main", sections, context("main"),
                  center ? std::optional(PathCurveId("center")) : std::nullopt,
                  {PathCurveId("main.left"), PathCurveId("main.right")}));
  if (supports)
    for (const bool first : {true, false})
      for (const bool upper : {true, false}) {
        const std::string producer = first ? "root" : "tip";
        const auto face = f::checked(ResultFaceReference::create(
            FeatureId(producer), BodyId(producer),
            {{FeatureId(producer), role(producer + (upper ? ".left" : ".right"),
                                        producer + (upper ? ".right" : ".left"))}}));
        auto reference = f::checked(
            FaceReference::create_result(PartFeatureInputRole::LoftBoundarySupport, face));
        intent.patch_boundaries.push_back({{(first ? sections.front() : sections.back()).identity(),
                                            reference,
                                            {},
                                            LoftBoundaryContinuity::G2,
                                            solid,
                                            1},
                                           PathCurveId(upper ? "main.left" : "main.right"),
                                           PathCurveId(upper ? "main.right" : "main.left")});
      }
  (void)f::checked(part.add_loft_feature(f::checked(feature.with_constraints(intent))));
  return part;
}
inline ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("verified.loft")));
}
inline double volume(const ShapeCache& cache, const char* body = "main") {
  const auto* shape = cache.find_body_shape(BodyId(body));
  REQUIRE(shape);
  GProp_GProps properties;
  BRepGProp::VolumeProperties(*OcctShapeView::get(*shape), properties);
  return properties.Mass();
}
} // namespace blcad::test::constrained_loft
