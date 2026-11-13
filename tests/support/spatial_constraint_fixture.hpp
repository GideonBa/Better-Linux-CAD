#pragma once
#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch_3d_constraints.hpp"
#include <stdexcept>
#include <utility>
namespace blcad::test::spatial_fixture {
template <class T> T checked(Result<T> result) {
  if (!result)
    throw std::runtime_error(result.error().object_id() + ": " + result.error().message());
  return std::move(result.value());
}
inline SketchCoordinate3D coordinate(double value) {
  return checked(
      SketchCoordinate3D::create_explicit(checked(Quantity::linear_displacement_mm(value))));
}
inline SketchPoint3D point(std::string id, Point3 value) {
  return checked(SketchPoint3D::create(SketchEntityId(std::move(id)), coordinate(value.x),
                                       coordinate(value.y), coordinate(value.z)));
}
inline SketchPointReference3D local(const char* id) {
  return checked(SketchPointReference3D::create_local_point(SketchEntityId(id)));
}
inline SpatialConstraintTarget target(const char* id) {
  return checked(SpatialConstraintTarget::local_point(SketchEntityId(id)));
}
inline SpatialConstraintTarget curve(const char* id,
                                     SpatialCurveLocation location = SpatialCurveLocation::Whole) {
  return checked(SpatialConstraintTarget::curve(SketchEntityId(id), location));
}
inline SpatialDimensionValue length(double value) {
  return checked(SpatialDimensionValue::literal(checked(Quantity::length_mm(value))));
}
inline SpatialDimensionValue angle(double value) {
  return checked(SpatialDimensionValue::literal(checked(Quantity::angle_deg(value))));
}
inline PartDocument part() {
  auto doc = checked(PartDocument::create(DocumentId("spatial.part"), "Spatial intent"));
  for (const auto& [id, value] : {std::pair{"coordinate.x", 8.},
                                  {"width", 8.},
                                  {"height", 6.},
                                  {"depth", 5.},
                                  {"radius", 4.},
                                  {"pitch", 2.},
                                  {"distance", 10.}})
    (void)checked(doc.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  (void)checked(doc.add_parameter(checked(
      Parameter::create_count(ParameterId("turns"), "Turns", checked(Quantity::count(3))))));
  (void)checked(doc.add_parameter(checked(
      Parameter::create_angle(ParameterId("angle"), "Angle", checked(Quantity::angle_deg(45))))));
  (void)checked(doc.add_datum_plane(checked(DatumPlane::xy())));
  (void)checked(doc.add_datum_axis(
      checked(DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z", {}, {0, 0, 1}))));
  (void)checked(doc.add_construction_point(checked(ConstructionPoint::create_explicit(
      ConstructionPointId("external.point"), "External", {1, 2, 3}))));
  auto planar =
      checked(Sketch::create(SketchId("reference.planar"), "Reference", DatumPlaneId("datum.xy")));
  (void)checked(planar.add_entity(
      checked(LineSegment::create(SketchEntityId("source.line"), {2, 3}, {7, 3}))));
  (void)checked(doc.add_sketch(std::move(planar)));
  auto base = checked(Sketch::create(SketchId("base.sketch"), "Base", DatumPlaneId("datum.xy")));
  (void)checked(base.add_profile(checked(RectangleProfile::create(
      ProfileId("base.profile"), ParameterId("width"), ParameterId("height")))));
  (void)checked(doc.add_sketch(std::move(base)));
  (void)checked(doc.add_body(checked(Body::create(BodyId("base.body"), "Base", BodyKind::Solid))));
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("base.feature"), "Base", SketchId("base.sketch"), ParameterId("depth")));
  (void)checked(doc.add_feature(
      checked(feature.with_body_result_context(checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("base.body")))))));
  return doc;
}
inline Sketch3D sketch(std::string id = "spatial", bool equation = false) {
  auto s = checked(Sketch3D::create(Sketch3DId(std::move(id)), "Spatial curves"));
  (void)checked(s.add_point(point("p2", {2, 1, 0})));
  (void)checked(s.add_point(checked(SketchPoint3D::create(
      SketchEntityId("p0"),
      checked(SketchCoordinate3D::create_parameter(ParameterId("coordinate.x"))), coordinate(0),
      coordinate(1)))));
  (void)checked(s.add_point(point("p3", {0, 3, 2})));
  (void)checked(s.add_point(point("p1", {10, 0, 1})));
  (void)checked(s.add_line(checked(
      SketchLine3D::create(SketchEntityId("line"), SketchEntityId("p0"), SketchEntityId("p1")))));
  (void)checked(s.add_polyline(checked(SketchPolyline3D::create(
      SketchEntityId("polyline"),
      {SketchEntityId("p0"), SketchEntityId("p2"), SketchEntityId("p1")}))));
  (void)checked(s.add_arc(checked(SketchArc3D::create_three_point(
      SketchEntityId("arc"), local("p0"), local("p2"), local("p1")))));
  for (const auto& [name, representation] :
       {std::pair{"spline", SketchSpline3DRepresentation::ControlPoints},
        {"fit", SketchSpline3DRepresentation::FitPoints}})
    (void)checked(s.add_spline(checked(SketchSpline3D::create(
        SketchEntityId(name), representation, {local("p0"), local("p1"), local("p2"), local("p3")},
        3, SketchSpline3DContinuity::Curvature))));
  (void)checked(s.add_helix(checked(SketchHelix3D::create(
      SketchEntityId("helix"), checked(SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.z"))),
      ParameterId("radius"), ParameterId("pitch"), ParameterId("turns"),
      SketchHelix3DHandedness::RightHanded))));
  (void)checked(s.add_guide_curve(
      checked(SketchGuideCurve3D::create(SketchEntityId("guide"), SketchEntityId("spline"),
                                         SketchGuideCurve3DRole::LoftGuide, "Guide"))));
  if (equation)
    (void)checked(s.add_equation_curve(checked(EquationCurve::create(
        SketchEntityId("equation"), EquationCurveMode::Parametric3D, {"u", CurveUnit::Scalar}, "0",
        "1", {"u*1mm", "u*u*1mm", "u*u*u*1mm"}, .001))));
  return s;
}
inline SpatialConstraintTarget axis() {
  return checked(SpatialConstraintTarget::axis(checked(AxisReference::create_datum_axis(
      PartFeatureInputRole::SpatialConstraint, DatumAxisId("axis.z")))));
}
inline SpatialConstraintTarget plane() {
  return checked(SpatialConstraintTarget::plane(checked(PlaneReference::create_datum_plane(
      PartFeatureInputRole::SpatialConstraint, DatumPlaneId("datum.xy")))));
}
inline SpatialConstraintTarget face() {
  return checked(SpatialConstraintTarget::face(checked(FaceReference::create_planar(
      PartFeatureInputRole::SpatialConstraint,
      checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))))));
}
} // namespace blcad::test::spatial_fixture
