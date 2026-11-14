#pragma once
#include "spatial_constraint_fixture.hpp"
namespace blcad::test::spatial_feature_fixture {
namespace f = spatial_fixture;
inline PartDocument straight_sweep() {
  auto part = f::part();
  (void)f::checked(part.add_parameter(f::checked(Parameter::create_length(
      ParameterId("spatial.length"), "Spatial length", f::checked(Quantity::length_mm(10))))));
  (void)f::checked(part.add_parameter(f::checked(Parameter::create_length(
      ParameterId("profile.side"), "Profile side", f::checked(Quantity::length_mm(.8))))));
  auto spatial = f::checked(Sketch3D::create(Sketch3DId("driven"), "Driven wire"));
  (void)f::checked(spatial.add_point(f::point("a", {})));
  (void)f::checked(spatial.add_point(f::point("b", {0, 0, 10})));
  (void)f::checked(spatial.add_line(f::checked(
      SketchLine3D::create(SketchEntityId("line"), SketchEntityId("a"), SketchEntityId("b")))));
  (void)f::checked(spatial.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("anchor"), SpatialConstraintKind::Fixed,
                                {f::target("a")}, {}, Point3{}))));
  (void)f::checked(spatial.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("axis"), SpatialConstraintKind::PointOnLine,
                                {f::target("b"), f::axis()}))));
  (void)f::checked(spatial.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("distance"), SpatialConstraintKind::Distance,
      {f::target("a"), f::target("b")},
      f::checked(SpatialDimensionValue::parameter(ParameterId("spatial.length")))))));
  (void)f::checked(part.add_sketch_3d(spatial));
  const auto segment = f::checked(PathSegmentReference::create_sketch_3d(
      spatial.id(), SketchEntityId("line"), Sketch3DPathCurveKind::Line));
  (void)f::checked(part.add_path_curve(
      f::checked(PathCurve::create(PathCurveId("driven.path"), "Driven path", {segment},
                                   PathClosure::Open, PathOrientationRule::MinimumTwist))));
  auto profile =
      f::checked(Sketch::create(SketchId("sweep.profile"), "Profile", DatumPlaneId("datum.xy")));
  (void)f::checked(profile.add_profile(f::checked(RectangleProfile::create(
      ProfileId("square"), ParameterId("profile.side"), ParameterId("profile.side")))));
  (void)f::checked(part.add_sketch(profile));
  (void)f::checked(
      part.add_body(f::checked(Body::create(BodyId("driven.body"), "Driven", BodyKind::Solid))));
  const auto region = f::checked(ProfileRegionReference::create(
      profile.id(), ProfileId("square"), PartFeatureInputRole::SweepProfile));
  (void)f::checked(part.add_sweep_feature(f::checked(SweepFeature::create_sweep(
      FeatureId("driven.sweep"), "Driven sweep",
      f::checked(SweepProfileReference::create_closed_region(region)), PathCurveId("driven.path"),
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {},
                                                  BodyId("driven.body")))))));
  return part;
}
} // namespace blcad::test::spatial_feature_fixture
