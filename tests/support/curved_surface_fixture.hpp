#pragma once
#include "surface_offset_fixture.hpp"
#include <cmath>
#include <numbers>
namespace blcad::test::curved_surface {
using surface_offset::checked;
inline void length(PartDocument& p, const std::string& id, double value) {
  auto name = id;
  std::replace(name.begin(), name.end(), '.', '_');
  (void)checked(p.add_parameter(checked(
      Parameter::create_length(ParameterId(id), name, checked(Quantity::length_mm(value))))));
}
inline SurfaceReference quadrant(std::vector<int> quadrants = {3, 0}) {
  std::vector<FaceReference> faces;
  for (const int q : quadrants) {
    const auto role = std::string("extrude/circle/outline") +
                      (q == 0 ? "/wall" : "/wall/quadrant/" + std::to_string(q));
    faces.push_back(checked(FaceReference::create_result(
        PartFeatureInputRole::SurfaceSourceFace,
        checked(ResultFaceReference::create(FeatureId("source.feature"), BodyId("source.body"),
                                            {{FeatureId("source.feature"), role}})))));
  }
  return checked(SurfaceReference::create_faces(BodyId("source.body"), std::move(faces)));
}
inline PartDocument part() {
  auto p = checked(PartDocument::create(DocumentId("curved.surface"), "Curved Surface"));
  for (const auto& [id, value] : {std::pair{"source.diameter", 20.},
                                  {"source.height", 20.},
                                  {"tolerance", 1e-6},
                                  {"deviation", 1e-4},
                                  {"extension", 3.},
                                  {"wall", 1.},
                                  {"window.width", 4.},
                                  {"window.height", 4.}})
    length(p, id, value);
  (void)checked(p.add_expression_parameter(ParameterId("window.top"), "window_top",
                                           ParameterType::Length, "10 mm + window_height / 2"));
  (void)checked(p.add_expression_parameter(ParameterId("window.bottom"), "window_bottom",
                                           ParameterType::Length, "10 mm - window_height / 2"));
  (void)checked(p.add_expression_parameter(ParameterId("extension.top"), "extension_top",
                                           ParameterType::Length, "window_top + extension"));
  (void)checked(p.add_datum_plane(checked(DatumPlane::xy())));
  auto sketch =
      checked(Sketch::create(SketchId("source.sketch"), "Source", DatumPlaneId("datum.xy")));
  (void)checked(sketch.add_profile(
      checked(CircleProfile::create(ProfileId("outline"), ParameterId("source.diameter")))));
  (void)checked(p.add_sketch(sketch));
  (void)checked(
      p.add_body(checked(Body::create(BodyId("source.body"), "Source", BodyKind::Solid))));
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("source.feature"), "Source", sketch.id(), ParameterId("source.height")));
  const auto context = checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("source.body")));
  (void)checked(p.add_feature(checked(feature.with_body_result_context(context))));
  (void)checked(p.add_construction_plane(checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("window.plane"), "Window plane",
                                         {0, -20, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}))));
  auto window =
      checked(Sketch::create(SketchId("window.sketch"), "Window", DatumPlaneId("window.plane")));
  (void)checked(window.add_profile(checked(RectangleProfile::create(
      ProfileId("window"), ParameterId("window.width"), ParameterId("window.height"), {10, 10}))));
  (void)checked(p.add_sketch(window));
  return p;
}
inline void trim(PartDocument& p, const std::string& id = "trim",
                 SurfaceTrimSide side = SurfaceTrimSide::KeepInside,
                 SurfaceReference source = quadrant(), Vector3 direction = {0, 1, 0}) {
  (void)checked(p.add_body(checked(Body::create(BodyId(id + ".body"), id, BodyKind::Surface))));
  auto feature = checked(TrimSurfaceFeature::create(
      FeatureId(id), id, std::move(source),
      checked(TrimmingReference::create_profile_region(
          checked(ProfileRegionReference::create(SketchId("window.sketch"), ProfileId("window"))))),
      BodyId(id + ".body")));
  (void)checked(p.add_surface_feature(checked(feature.with_options(SurfaceTrimOptions{
      side, SurfaceTrimProjection::AlongDirection, direction, ParameterId("tolerance")}))));
}
inline void arc_path(PartDocument& p, const std::string& id, double a, double b,
                     const std::string& height) {
  auto sketch = checked(Sketch3D::create(Sketch3DId(id + ".sketch3d"), id));
  for (const auto& [name, angle] : {std::pair{"start", a}, {"middle", (a + b) / 2}, {"end", b}})
    (void)checked(sketch.add_point(checked(SketchPoint3D::create(
        SketchEntityId(name), spatial_fixture::coordinate(10 * std::cos(angle)),
        spatial_fixture::coordinate(10 * std::sin(angle)),
        checked(SketchCoordinate3D::create_parameter(ParameterId(height)))))));
  (void)checked(sketch.add_arc(checked(SketchArc3D::create_three_point(
      SketchEntityId("arc"), spatial_fixture::local("start"), spatial_fixture::local("middle"),
      spatial_fixture::local("end")))));
  (void)checked(p.add_sketch_3d(sketch));
  const auto segment = checked(PathSegmentReference::create_sketch_3d(
      sketch.id(), SketchEntityId("arc"), Sketch3DPathCurveKind::Arc));
  (void)checked(p.add_path_curve(checked(PathCurve::create(
      PathCurveId(id), id, {segment}, PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {},
      1e-7, checked(PathExecutionPolicy::curve_preserving(1e-7))))));
}
inline void extend(PartDocument& p, const std::string& id, SurfaceReference target,
                   const std::string& path, SurfaceExtendMode mode = SurfaceExtendMode::Natural) {
  (void)checked(p.add_body(checked(Body::create(BodyId(id + ".body"), id, BodyKind::Surface))));
  auto feature = checked(ExtendSurfaceFeature::create(
      FeatureId(id), id, std::move(target),
      checked(BoundaryCurveReference::create_path_curve(PathCurveId(path))),
      ParameterId("extension"), BodyId(id + ".body")));
  (void)checked(p.add_surface_feature(checked(feature.with_options(SurfaceExtendOptions{
      mode, ParameterId("tolerance"),
      mode == SurfaceExtendMode::Tangent ? std::optional(ParameterId("deviation"))
                                         : std::nullopt}))));
}
inline void line_path(PartDocument& p, const std::string& id, Point2 point,
                      const std::string& first, const std::string& last) {
  auto sketch = checked(Sketch3D::create(Sketch3DId(id + ".sketch3d"), id));
  for (const auto& [name, height] : {std::pair{"first", first}, {"last", last}})
    (void)checked(sketch.add_point(checked(SketchPoint3D::create(
        SketchEntityId(name), spatial_fixture::coordinate(point.x),
        spatial_fixture::coordinate(point.y),
        checked(SketchCoordinate3D::create_parameter(ParameterId(height)))))));
  (void)checked(sketch.add_line(checked(SketchLine3D::create(
      SketchEntityId("line"), SketchEntityId("first"), SketchEntityId("last")))));
  (void)checked(p.add_sketch_3d(sketch));
  const auto segment = checked(PathSegmentReference::create_sketch_3d(
      sketch.id(), SketchEntityId("line"), Sketch3DPathCurveKind::Line));
  (void)checked(p.add_path_curve(checked(PathCurve::create(
      PathCurveId(id), id, {segment}, PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {},
      1e-7, checked(PathExecutionPolicy::curve_preserving(1e-7))))));
}
inline SurfaceReference body(const std::string& id) {
  return checked(SurfaceReference::create_body(BodyId(id)));
}
inline PartDocument combination() {
  auto p = part();
  const double angle = std::acos(.8);
  trim(p, "left", SurfaceTrimSide::KeepInside, quadrant({3}));
  trim(p, "right", SurfaceTrimSide::KeepInside, quadrant({0}));
  arc_path(p, "left.top", -angle, 0, "window.top");
  arc_path(p, "right.top", 0, angle, "window.top");
  extend(p, "left.extend", body("left.body"), "left.top");
  extend(p, "right.extend", body("right.body"), "right.top");
  line_path(p, "outer", {8, 6}, "window.bottom", "extension.top");
  extend(p, "tangent", body("right.extend.body"), "outer", SurfaceExtendMode::Tangent);
  (void)checked(
      p.add_body(checked(Body::create(BodyId("quilt.body"), "Quilt", BodyKind::Surface))));
  (void)checked(p.add_surface_feature(checked(SurfaceStitchFeature::create(
      FeatureId("stitch"), "Quilt", {body("left.extend.body"), body("tangent.body")},
      ParameterId("tolerance"), BodyId("quilt.body")))));
  (void)checked(p.add_body(checked(Body::create(BodyId("thick.body"), "Thick", BodyKind::Solid))));
  const SurfaceOffsetParameters parameters{ParameterId("wall"), ParameterId("tolerance"),
                                           SurfaceOffsetSide::Symmetric,
                                           SurfaceOffsetApproximation::ExactAnalytic, std::nullopt};
  const auto context = checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("thick.body")));
  (void)checked(p.add_surface_feature(checked(SurfaceOffsetFeature::create_thicken(
      FeatureId("thick"), "Thickness", body("quilt.body"), parameters, context))));
  return p;
}

} // namespace blcad::test::curved_surface
