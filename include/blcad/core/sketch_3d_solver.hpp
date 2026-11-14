#pragma once
#include "blcad/core/sketch_3d.hpp"
#include "blcad/core/sketch_constraint_solver.hpp"
#include <functional>

namespace blcad {
struct SpatialCurveJet {
  Point3 point;
  Vector3 first;
  Vector3 second;
};
struct SpatialAxisFrame {
  Point3 origin;
  Vector3 direction;
};
struct SpatialPlaneFrame {
  Point3 origin;
  Vector3 normal;
};
// Queries read the current numerical candidate and fixed external sources. Core
// never owns resolved topology. prepare_candidate may invalidate a derived
// per-evaluation curve cache.
struct SpatialSolverGeometry {
  std::function<Result<std::size_t>(const Sketch3D&)> prepare_candidate;
  std::function<Result<Point3>(const SketchPointReference3D&)> external_point;
  std::function<Result<SpatialCurveJet>(const Sketch3D&, const SpatialConstraintTarget&, double)>
      curve;
  std::function<Result<SpatialAxisFrame>(const AxisReference&)> axis;
  std::function<Result<SpatialPlaneFrame>(const PlaneReference&)> plane;
  std::function<Result<Point3>(const FaceReference&, Point3)> project_face;
  std::function<Result<double>(const Sketch3D&, const SpatialConstraintTarget&)> radius;
  std::function<Result<double>(const Sketch3D&, const SpatialConstraintTarget&)> curve_length;
  std::function<Result<std::size_t>()> validate_sources;
  std::function<Result<std::size_t>(const Sketch3D&)> validate_candidate;
};
struct SpatialSolverOptions {
  SketchSolverOptions numeric;
  std::size_t maximum_variables{256};
  std::size_t maximum_entities{512};
  std::size_t maximum_constraints{256};
  // Work units include residual evaluations and auxiliary branch initialization queries.
  std::size_t maximum_evaluations{20000};
  std::size_t maximum_curve_projection_iterations{16};
  // Zero selects twice the centroid radius and active Length dimensions.
  double length_scale_mm{0};
};
struct SpatialConstraintReport {
  SketchConstraintId id;
  double residual_norm{0};
  std::size_t rank_contribution{0};
  bool redundant{false};
  bool conflicting{false};
};
struct SpatialSolveResult {
  Sketch3D sketch;
  SketchSolveStatus status{SketchSolveStatus::InvalidReference};
  std::vector<SpatialVariable> variable_order{};
  std::vector<SpatialConstraintReport> constraints{};
  std::size_t jacobian_rank{0};
  std::size_t degrees_of_freedom{0};
  std::size_t gauge_degrees_of_freedom{0};
  std::size_t internal_degrees_of_freedom{0};
  std::size_t iterations{0};
  std::size_t evaluations{0};
  double length_scale_mm{1};
  double normalized_rms{0};
  std::string diagnostic{};
  [[nodiscard]] bool accepted() const noexcept {
    return status == SketchSolveStatus::FullyConstrained ||
           status == SketchSolveStatus::UnderConstrained || status == SketchSolveStatus::Redundant;
  }
};
// Every rejected result contains the exact source sketch, including bindings
// and authored seeds. Solved auxiliary curve parameters are transient; they
// never replace the authored branch seed.
[[nodiscard]] SpatialSolveResult solve_spatial_sketch(const PartDocument&, const Sketch3D&,
                                                      const SpatialSolverGeometry& = {},
                                                      const SpatialSolverOptions& = {});
} // namespace blcad
