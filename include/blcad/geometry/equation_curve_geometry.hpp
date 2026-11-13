#pragma once
#include "blcad/core/equation_curve.hpp"
#include "blcad/geometry/derived_curve.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::geometry {
enum class EquationCurveGeometryKind { AnalyticCircle, Polynomial, Adaptive };
struct EquationCurveGeometryLimits {
  std::size_t maximum_segments{8192}, maximum_depth{28}, maximum_evaluations{32768};
};
struct EquationCurveEndpoint {
  std::string role;
  Point3 position;
};
struct EquationCurveGeometry {
  SketchEntityId source;
  EquationCurveGeometryKind kind;
  DerivedCurveRef curve;
  GeometryShape shape;
  std::array<EquationCurveEndpoint, 2> endpoints;
  double error_bound_mm{};
  std::size_t accepted_intervals{}, evaluations{};
};
class EquationCurveGeometryAdapter {
public:
  [[nodiscard]] Result<EquationCurveGeometry>
  convert(const PartDocument& document, const EquationCurve& curve,
          const EquationCurveGeometryLimits& limits = {}) const;
};
} // namespace blcad::geometry
