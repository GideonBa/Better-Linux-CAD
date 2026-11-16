#pragma once
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "constrained_loft_fixture.hpp"
#include "curved_surface_fixture.hpp"
namespace blcad::test::curve_surface_milestone {
namespace f = curved_surface;
namespace lf = loft_refinement;
namespace sf = spatial_fixture;
using namespace blcad::geometry;
inline void fixed_guide(PartDocument& part, const char* id) {
  auto sketch = *part.find_sketch_3d(Sketch3DId(id));
  const auto points = sketch.points();
  for (const auto& point : points) {
    const Point3 value{point.x().explicit_coordinate()->millimeters(),
                       point.y().explicit_coordinate()->millimeters(),
                       point.z().explicit_coordinate()->millimeters()};
    (void)f::checked(sketch.add_constraint(f::checked(SpatialConstraint::create(
        SketchConstraintId("fixed." + point.id().value()), SpatialConstraintKind::Fixed,
        {f::checked(SpatialConstraintTarget::local_point(point.id()))}, {}, value))));
  }
  (void)f::checked(part.update_sketch_3d(sketch));
}
inline PartDocument inputs() {
  auto part = constrained_loft::airfoil();
  fixed_guide(part, "main.left.sketch3d");
  fixed_guide(part, "main.right.sketch3d");
  const auto curved = f::combination();
  for (const auto& parameter : curved.parameters())
    if (!part.find_parameter(parameter.id()))
      (void)f::checked(parameter.is_expression()
                           ? part.add_expression_parameter(parameter.id(), parameter.name(),
                                                           parameter.type(), *parameter.formula())
                           : part.add_parameter(parameter));
  for (const auto& plane : curved.datum_planes())
    if (!part.find_datum_plane(plane.id()))
      (void)f::checked(part.add_datum_plane(plane));
  for (const auto& plane : curved.construction_planes())
    (void)f::checked(part.add_construction_plane(plane));
  for (const auto& sketch : curved.sketches())
    (void)f::checked(part.add_sketch(sketch));
  for (const auto& sketch : curved.sketches_3d())
    (void)f::checked(part.add_sketch_3d(sketch));
  for (const auto& path : curved.path_curves())
    (void)f::checked(part.add_path_curve(path));
  for (const auto& body : curved.bodies())
    (void)f::checked(part.add_body(body));
  for (const auto& feature : curved.features())
    (void)f::checked(part.add_feature(feature));
  for (const auto& feature : curved.surface_features())
    if (surface_feature_id(feature) != FeatureId("thick"))
      (void)f::checked(part.add_surface_feature(feature));
  f::length(part, "milestone.offset", .5);
  (void)f::checked(part.add_body(
      f::checked(Body::create(BodyId("offset.body"), "Offset quilt", BodyKind::Surface))));
  const auto context = f::checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, {}, BodyId("offset.body")));
  (void)f::checked(part.add_surface_feature(f::checked(SurfaceOffsetFeature::create_offset(
      FeatureId("offset"), "Offset quilt", f::body("quilt.body"),
      {ParameterId("milestone.offset"),
       ParameterId("tolerance"),
       SurfaceOffsetSide::AlongNormal,
       SurfaceOffsetApproximation::ExactAnalytic,
       {}},
      context))));
  const auto& old =
      std::get<SurfaceOffsetFeature>(*curved.find_surface_feature(FeatureId("thick")));
  (void)f::checked(part.add_surface_feature(f::checked(SurfaceOffsetFeature::create_thicken(
      old.id(), old.name(), f::body("offset.body"), old.parameters(), old.body_result_context()))));
  f::length(part, "equation.length", 20);
  f::length(part, "pipe.diameter", 2);
  auto middle = *part.find_sketch(SketchId("section.1"));
  (void)f::checked(middle.add_entity(f::checked(EquationCurve::create(
      SketchEntityId("duct.path"), EquationCurveMode::Parametric2D, {"u", CurveUnit::Scalar}, "0",
      "1", {"0 mm", "u * @{equation.length}", ""}, 1e-7))));
  (void)f::checked(part.update_sketch(middle));
  const auto segment = f::checked(PathSegmentReference::create_planar(
      middle.id(), SketchEntityId("duct.path"), PlanarPathCurveKind::EquationCurve));
  (void)f::checked(part.add_path_curve(f::checked(
      PathCurve::create(PathCurveId("duct.path"), "Equation duct spine", {segment},
                        PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                        f::checked(PathExecutionPolicy::curve_preserving(1e-7))))));
  (void)f::checked(part.add_construction_plane(f::checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("duct.plane"), "Duct profile",
                                         {3, 2, 20}, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}))));
  auto profile = f::checked(
      Sketch::create(SketchId("duct.profile"), "Duct profile", DatumPlaneId("duct.plane")));
  (void)f::checked(profile.add_profile(
      f::checked(CircleProfile::create(ProfileId("circle"), ParameterId("pipe.diameter")))));
  (void)f::checked(part.add_sketch(profile));
  (void)f::checked(part.add_body(
      f::checked(Body::create(BodyId("duct.body"), "Equation duct", BodyKind::Solid))));
  const auto region = f::checked(ProfileRegionReference::create(
      profile.id(), ProfileId("circle"), PartFeatureInputRole::SweepProfile));
  (void)f::checked(part.add_sweep_feature(f::checked(SweepFeature::create_sweep(
      FeatureId("duct"), "Equation duct",
      f::checked(SweepProfileReference::create_closed_region(region)), PathCurveId("duct.path"),
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {},
                                                  BodyId("duct.body")))))));
  auto hole_cache = surface_offset::cache();
  (void)f::checked(GeometryRecomputeExecutor{}.execute_document(part, hole_cache));
  const auto faces = f::checked(
      ResultFaceSupportGeometry::selectable_faces(part, hole_cache, BodyId("thick.body")));
  REQUIRE(faces.size() == 2);
  const auto outer = std::max_element(faces.begin(), faces.end(), [](const auto& a, const auto& b) {
    return .8 * a.anchor.x + .6 * a.anchor.y < .8 * b.anchor.x + .6 * b.anchor.y;
  });
  (void)f::checked(part.add_result_face_workplane(f::checked(ResultFaceWorkplane::create(
      DatumPlaneId("milestone.skin"), "Offset quilt skin", outer->reference))));
  f::length(part, "milestone.bore.diameter", .4);
  (void)f::checked(part.add_hole_feature(f::checked(HoleFeature::create(
      FeatureId("milestone.bore"), "Quilt bore", HoleKind::Plain, BodyId("thick.body"),
      f::checked(PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane,
                                                    DatumPlaneId("milestone.skin"))),
      {{HolePlacementId("center"), Point2{0, 0}}}, ParameterId("milestone.bore.diameter")))));
  (void)f::checked(part.add_body(
      f::checked(Body::create(BodyId("joined.body"), "Connected result", BodyKind::Solid))));
  (void)f::checked(part.add_body_boolean_feature(f::checked(
      BodyBooleanFeature::create(FeatureId("joined"), BodyBooleanOperation::Add, BodyId("main"),
                                 {BodyId("thick.body"), BodyId("duct.body")},
                                 BodyBooleanResultMode::NewBody, BodyId("joined.body"), true))));
  return part;
}
inline PartDocument finished() {
  auto part = inputs();
  (void)f::checked(part.add_parameter(f::checked(Parameter::create_count(
      ParameterId("milestone.count"), "milestone_count", f::checked(Quantity::count(2))))));
  f::length(part, "milestone.spacing", 80);
  f::length(part, "milestone.fillet", .2);
  (void)f::checked(part.add_datum_axis(f::checked(
      DatumAxis::create_explicit(DatumAxisId("milestone.x"), "Pattern X", {}, {1, 0, 0}))));
  (void)f::checked(part.add_body(
      f::checked(Body::create(BodyId("pattern.body"), "Finished pattern", BodyKind::Solid))));
  (void)f::checked(part.add_linear_pattern_feature(f::checked(LinearPatternFeature::create(
      FeatureId("pattern"), "Connected pattern",
      {f::checked(PatternSourceReference::body(BodyId("joined.body")))},
      f::checked(AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis,
                                                  DatumAxisId("milestone.x"))),
      ParameterId("milestone.count"), LinearPatternExtentMode::Spacing,
      ParameterId("milestone.spacing"), PatternDirectionSign::Positive,
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {},
                                                  BodyId("pattern.body")))))));
  auto cache = surface_offset::cache();
  (void)f::checked(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto edges = f::checked(FinishingSelectionBuilder::build(
      part, cache, BodyId("pattern.body"), PartFeatureInputRole::FilletEdge));
  const auto edge = std::find_if(edges.begin(), edges.end(), [](const auto& target) {
    const auto* ref = std::get_if<EdgeReference>(&target.reference);
    if (!ref)
      return false;
    const auto* curve = std::get_if<ResultEdgeReference>(&ref->source());
    return curve && curve->curve_type() == 1 &&
           std::all_of(curve->samples().begin(), curve->samples().end(), [](Point3 p) {
             return std::abs(p.y - 22) < 1e-6 && std::abs(std::hypot(p.x - 3, p.z - 20) - 1) < 1e-6;
           });
  });
  REQUIRE(edge != edges.end());
  (void)f::checked(part.add_fillet_feature(f::checked(FilletFeature::create(
      FeatureId("finish"), "Duct rim", BodyId("pattern.body"),
      {std::get<EdgeReference>(edge->reference)}, ParameterId("milestone.fillet")))));
  const auto bodies = part.bodies();
  for (const auto& body : bodies)
    if (body.id() != BodyId("pattern.body"))
      (void)f::checked(part.set_body_visibility(body.id(), BodyVisibility::Hidden));
  return part;
}
inline void edit_middle_and_guide(PartDocument& part, double shift, bool repair_anchor = true) {
  const auto original = *part.find_sketch(SketchId("section.1"));
  auto middle = f::checked(Sketch::create(original.id(), original.name(), original.workplane()));
  for (bool upper : {true, false}) {
    auto poles = lf::poles(1, upper);
    poles[0].x += shift;
    const auto count = lf::counts[1][upper ? 0 : 1];
    for (std::size_t i = 0; i < count; ++i) {
      const double a = double(i) / count, b = double(i + 1) / count, d = (b - a) / 3;
      const auto first = lf::at(poles, a), last = lf::at(poles, b), da = lf::derivative(poles, a),
                 db = lf::derivative(poles, b);
      (void)f::checked(middle.add_entity(f::checked(SplineSegment::create_cubic_bezier(
          SketchEntityId(std::string(upper ? "upper." : "lower.") + std::to_string(i)), first,
          {first.x + d * da.x, first.y + d * da.y}, {last.x - d * db.x, last.y - d * db.y},
          last))));
    }
  }
  for (const auto& profile : original.arc_closed_profiles())
    (void)f::checked(middle.add_profile(profile));
  for (const auto& equation : original.equation_curves())
    (void)f::checked(middle.add_entity(equation));
  (void)f::checked(part.update_sketch(middle));
  auto guide = *part.find_sketch_3d(Sketch3DId("main.left.sketch3d"));
  const auto points = guide.points();
  std::vector<Point3> poles;
  for (const auto& point : points) {
    Point3 value{point.x().explicit_coordinate()->millimeters(),
                 point.y().explicit_coordinate()->millimeters(),
                 point.z().explicit_coordinate()->millimeters()};
    if (point.id() == SketchEntityId("p4")) {
      value.x += 1.5 * shift * std::cos(.1);
      value.z += 1.5 * shift * std::sin(.1);
      (void)f::checked(guide.replace_point(sf::point("p4", value)));
      (void)f::checked(guide.replace_constraint(f::checked(
          SpatialConstraint::create(SketchConstraintId("fixed.p4"), SpatialConstraintKind::Fixed,
                                    {sf::target("p4")}, {}, value))));
    }
    poles.push_back(value);
  }
  (void)f::checked(part.update_sketch_3d(guide));
  if (repair_anchor) {
    TColgp_Array1OfPnt cp(1, 9);
    for (int i = 1; i <= 9; ++i)
      cp.SetValue(i, {poles[i - 1].x, poles[i - 1].y, poles[i - 1].z});
    TColStd_Array1OfReal knots(1, 7);
    TColStd_Array1OfInteger mult(1, 7);
    for (int i = 1; i <= 7; ++i) {
      knots(i) = i - 1;
      mult(i) = i == 1 || i == 7 ? 4 : 1;
    }
    const Handle(Geom_BSplineCurve) curve = new Geom_BSplineCurve(cp, knots, mult, 3);
    GeomAdaptor_Curve adaptor(curve);
    const double fraction = GCPnts_AbscissaPoint::Length(adaptor, 0, 3, 1e-11) /
                            GCPnts_AbscissaPoint::Length(adaptor, 0, 6, 1e-11);
    const auto loft = *part.find_loft_feature(FeatureId("main"));
    auto intent = *loft.constraints();
    for (auto& binding : intent.guides)
      if (binding.guide == PathCurveId("main.left"))
        binding.anchors[1].guide_fraction = fraction;
    (void)f::checked(part.update_loft_feature(f::checked(loft.with_constraints(intent))));
  }
}
} // namespace blcad::test::curve_surface_milestone
