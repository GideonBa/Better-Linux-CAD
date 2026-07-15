#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_topology.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

class PartDocument;

enum class SketchSolverTargetKind { Point, Entity };
[[nodiscard]] std::string_view to_string(SketchSolverTargetKind kind) noexcept;

class SketchSolverTarget {
public:
  [[nodiscard]] static Result<SketchSolverTarget> point(SketchPointId id);
  [[nodiscard]] static Result<SketchSolverTarget> entity(std::string id);

  [[nodiscard]] SketchSolverTargetKind kind() const noexcept;
  [[nodiscard]] const std::string& id() const noexcept;

  friend bool operator==(const SketchSolverTarget&, const SketchSolverTarget&) = default;

private:
  SketchSolverTarget(SketchSolverTargetKind kind, std::string id);

  SketchSolverTargetKind kind_;
  std::string id_;
};

enum class SketchSolverConstraintKind {
  Coincident,
  Fixed,
  Horizontal,
  Vertical,
  Parallel,
  Perpendicular,
  Collinear,
  Equal,
  Tangent,
  Concentric,
  Midpoint,
  Symmetric,
  PointOnObject,
  HorizontalDistance,
  VerticalDistance,
  AlignedDistance,
  Radial,
  Diameter,
  Angular,
};
[[nodiscard]] std::string_view to_string(SketchSolverConstraintKind kind) noexcept;

class SketchSolverConstraint {
public:
  [[nodiscard]] static Result<SketchSolverConstraint>
  create(std::string id, SketchSolverConstraintKind kind, std::vector<SketchSolverTarget> targets,
         std::optional<double> value = std::nullopt);

  [[nodiscard]] const std::string& id() const noexcept;
  [[nodiscard]] SketchSolverConstraintKind kind() const noexcept;
  [[nodiscard]] const std::vector<SketchSolverTarget>& targets() const noexcept;
  [[nodiscard]] const std::optional<double>& value() const noexcept;

  friend bool operator==(const SketchSolverConstraint&, const SketchSolverConstraint&) = default;

private:
  SketchSolverConstraint(std::string id, SketchSolverConstraintKind kind,
                         std::vector<SketchSolverTarget> targets,
                         std::optional<double> value);

  std::string id_;
  SketchSolverConstraintKind kind_;
  std::vector<SketchSolverTarget> targets_;
  std::optional<double> value_;
};

class SketchConstraintSystem {
public:
  [[nodiscard]] static Result<SketchConstraintSystem>
  create(SketchId sketch, std::vector<SketchSolverConstraint> constraints);

  [[nodiscard]] const SketchId& sketch() const noexcept;
  [[nodiscard]] const std::vector<SketchSolverConstraint>& constraints() const noexcept;

  friend bool operator==(const SketchConstraintSystem&, const SketchConstraintSystem&) = default;

private:
  SketchConstraintSystem(SketchId sketch, std::vector<SketchSolverConstraint> constraints);

  SketchId sketch_;
  std::vector<SketchSolverConstraint> constraints_;
};

enum class SketchSolverVariableAxis { X, Y };
[[nodiscard]] std::string_view to_string(SketchSolverVariableAxis axis) noexcept;

struct SketchSolverVariable {
  SketchPointId point;
  SketchSolverVariableAxis axis;

  friend bool operator==(const SketchSolverVariable&, const SketchSolverVariable&) = default;
};

struct SketchSolverOptions {
  double convergence_rms{1.0e-9};
  double finite_difference_relative_step{1.0e-7};
  double rank_absolute_tolerance{1.0e-10};
  double rank_relative_tolerance{1.0e-8};
  double initial_damping{1.0e-6};
  std::size_t maximum_iterations{80U};
  std::size_t maximum_damping_attempts{8U};
  std::size_t maximum_line_search_steps{12U};

  friend bool operator==(const SketchSolverOptions&, const SketchSolverOptions&) = default;
};

enum class SketchSolveStatus {
  FullyConstrained,
  UnderConstrained,
  Redundant,
  Conflicting,
  NonConvergent,
  InvalidReference,
};
[[nodiscard]] std::string_view to_string(SketchSolveStatus status) noexcept;

enum class SketchSolverDiagnosticKind {
  InvalidReference,
  RedundantConstraint,
  ConflictingConstraint,
  NonConvergent,
};
[[nodiscard]] std::string_view to_string(SketchSolverDiagnosticKind kind) noexcept;

struct SketchSolverDiagnostic {
  SketchSolverDiagnosticKind kind;
  std::string constraint_id;
  std::string message;

  friend bool operator==(const SketchSolverDiagnostic&, const SketchSolverDiagnostic&) = default;
};

struct SketchSolverResidualSummary {
  std::size_t residual_count{0U};
  double characteristic_length{1.0};
  double initial_rms{0.0};
  double final_rms{0.0};
  double final_max_abs{0.0};

  friend bool operator==(const SketchSolverResidualSummary&,
                         const SketchSolverResidualSummary&) = default;
};

struct SketchSolveResult {
  SketchSolveStatus status;
  std::size_t iterations{0U};
  SketchTopology topology;
  std::vector<SketchSolverVariable> variable_order;
  std::size_t jacobian_rank{0U};
  std::size_t remaining_dof{0U};
  std::vector<std::string> redundant_constraint_ids;
  std::vector<std::string> conflicting_constraint_ids;
  std::vector<SketchSolverDiagnostic> diagnostics;
  SketchSolverResidualSummary residuals;

  friend bool operator==(const SketchSolveResult&, const SketchSolveResult&) = default;
};

class SketchConstraintSolver {
public:
  [[nodiscard]] Result<SketchSolveResult>
  solve(const SketchTopology& topology, const SketchConstraintSystem& system,
        SketchSolverOptions options = {}) const;
};

class SketchConstraintSystemBuilder {
public:
  [[nodiscard]] static Result<SketchConstraintSystem>
  from_legacy(const SketchTopology& topology, const Sketch& sketch,
              const PartDocument& document);
};

} // namespace blcad
