#pragma once
#include "blcad/core/sketch_3d_solver.hpp"
namespace blcad::gui {
enum class SpatialCurveGripKind { ArcRadius, HelixRadius, HelixPitch };
struct SpatialCurveGrip {
  Sketch3DId owner;
  SketchEntityId entity;
  SpatialCurveGripKind kind;
  Point3 origin;
  Vector3 direction;
  Vector3 plane_normal;
  double value;
  double model_units_per_value{1};
  std::vector<SketchEntityId> points;
};
struct SpatialCurveGripCandidate {
  Sketch3D sketch;
  SpatialSolverOptions options;
};
inline constexpr double spatial_curve_grip_minimum = 1e-9;
inline constexpr double spatial_curve_grip_maximum = 1e9;
[[nodiscard]] Result<std::vector<SpatialCurveGrip>>
spatial_curve_grips(const PartDocument&, const Sketch3D&, SketchEntityId,
                    const SpatialSolverGeometry&);
[[nodiscard]] Result<SpatialCurveGripCandidate>
move_spatial_curve_grip(const Sketch3D&, const SpatialCurveGrip&, double);
[[nodiscard]] bool valid_spatial_curve_grip_text(std::string_view);
} // namespace blcad::gui
