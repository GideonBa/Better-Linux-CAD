#pragma once
#include "blcad/core/part_document.hpp"
#include <stdexcept>
#include <utility>

namespace blcad::test::equation_fixture {
template <class T> T checked(Result<T> value) {
  if (!value)
    throw std::runtime_error(value.error().object_id() + ": " + value.error().message());
  return std::move(value.value());
}
inline EquationCurve planar(std::string upper = "@{count}") {
  return checked(EquationCurve::create(SketchEntityId("equation"), EquationCurveMode::Parametric2D,
                                       {"u", CurveUnit::Scalar}, "1", std::move(upper),
                                       {"u*1mm", "sin(u)*1mm", ""}, .01));
}
inline EquationCurve spatial() {
  return checked(EquationCurve::create(
      SketchEntityId("equation"), EquationCurveMode::Parametric3D, {"u", CurveUnit::Scalar}, "0",
      "2*pi", {"@{radius}*cos(u)", "@{radius}*sin(u)", "@{count}*1mm*u"}, .02));
}
inline PartDocument part() {
  auto result = checked(PartDocument::create(DocumentId("equation.part"), "Equation intent"));
  (void)checked(result.add_parameter(checked(
      Parameter::create_length(ParameterId("radius"), "Radius", checked(Quantity::length_mm(5))))));
  (void)checked(result.add_parameter(checked(
      Parameter::create_count(ParameterId("count"), "Count", checked(Quantity::count(3))))));
  (void)checked(result.add_datum_plane(checked(DatumPlane::xy())));
  auto sketch = checked(Sketch::create(SketchId("planar"), "Planar", DatumPlaneId("datum.xy")));
  (void)checked(
      sketch.add_entity(checked(LineSegment::create(SketchEntityId("line"), {0, 0}, {2, 1}))));
  (void)checked(sketch.add_entity(planar()));
  (void)checked(result.add_sketch(sketch));
  auto space = checked(Sketch3D::create(Sketch3DId("spatial"), "Spatial"));
  (void)checked(space.add_equation_curve(spatial()));
  (void)checked(result.add_sketch_3d(space));
  return result;
}
} // namespace blcad::test::equation_fixture
