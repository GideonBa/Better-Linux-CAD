from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def replace_exact(path: str, old: str, new: str, expected: int = 1) -> None:
    content = read(path)
    actual = content.count(old)
    if actual != expected:
        raise RuntimeError(
            f"{path}: expected {expected} occurrence(s) of replacement seed, found {actual}"
        )
    write(path, content.replace(old, new))


def create_exact(path: str, content: str) -> None:
    target = ROOT / path
    if target.exists():
        raise RuntimeError(f"{path}: file already exists")
    write(path, content)


# ---------------------------------------------------------------------------
# Shared generic relationship equation semantics.
# ---------------------------------------------------------------------------
create_exact(
    "include/blcad/geometry/assembly_generic_relationship_equation_builder.hpp",
    r'''#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"

#include <optional>
#include <variant>

namespace blcad::geometry {

enum class AssemblyGenericRelationshipTargetRole { TargetA, TargetB };

struct CoincidentPointPointResidualDescriptor {
  Vector3 point_delta_mm;

  friend bool operator==(const CoincidentPointPointResidualDescriptor&,
                         const CoincidentPointPointResidualDescriptor&) = default;
};

struct CoincidentPointLineResidualDescriptor {
  AssemblyGenericRelationshipTargetRole point_target =
      AssemblyGenericRelationshipTargetRole::TargetA;
  Vector3 point_line_cross_mm;

  friend bool operator==(const CoincidentPointLineResidualDescriptor&,
                         const CoincidentPointLineResidualDescriptor&) = default;
};

struct CoincidentPointPlaneResidualDescriptor {
  AssemblyGenericRelationshipTargetRole point_target =
      AssemblyGenericRelationshipTargetRole::TargetA;
  double signed_distance_mm = 0.0;

  friend bool operator==(const CoincidentPointPlaneResidualDescriptor&,
                         const CoincidentPointPlaneResidualDescriptor&) = default;
};

struct ParallelDirectionResidualDescriptor {
  Vector3 direction_parallelism;

  friend bool operator==(const ParallelDirectionResidualDescriptor&,
                         const ParallelDirectionResidualDescriptor&) = default;
};

struct PerpendicularDirectionResidualDescriptor {
  double direction_dot = 0.0;

  friend bool operator==(const PerpendicularDirectionResidualDescriptor&,
                         const PerpendicularDirectionResidualDescriptor&) = default;
};

struct DirectionalAngleResidualDescriptor {
  double target_angle_deg = 0.0;
  double direction_dot = 0.0;
  double angle_alignment = 0.0;

  friend bool operator==(const DirectionalAngleResidualDescriptor&,
                         const DirectionalAngleResidualDescriptor&) = default;
};

using AssemblyGenericRelationshipResidualValue =
    std::variant<CoincidentPointPointResidualDescriptor,
                 CoincidentPointLineResidualDescriptor,
                 CoincidentPointPlaneResidualDescriptor,
                 ParallelDirectionResidualDescriptor,
                 PerpendicularDirectionResidualDescriptor,
                 DirectionalAngleResidualDescriptor>;

struct AssemblyGenericRelationshipResidualDescriptor {
  AssemblyGenericRelationshipResidualValue value;

  friend bool operator==(const AssemblyGenericRelationshipResidualDescriptor&,
                         const AssemblyGenericRelationshipResidualDescriptor&) = default;
};

struct AssemblyGenericRelationshipTargetDescriptor {
  AssemblyResolvedGeometricTarget target;
  AssemblyGeometricTargetCapability selected_capability =
      AssemblyGeometricTargetCapability::Point;

  friend bool operator==(const AssemblyGenericRelationshipTargetDescriptor&,
                         const AssemblyGenericRelationshipTargetDescriptor&) = default;
};

struct AssemblyGenericRelationshipEquationDescriptor {
  AssemblyConstraintId relationship;
  AssemblyConstraintType type = AssemblyConstraintType::Coincident;
  AssemblyTargetCompatibility compatibility;
  AssemblyGenericRelationshipTargetDescriptor target_a;
  AssemblyGenericRelationshipTargetDescriptor target_b;
  AssemblyGenericRelationshipResidualDescriptor residual;

  friend bool operator==(const AssemblyGenericRelationshipEquationDescriptor&,
                         const AssemblyGenericRelationshipEquationDescriptor&) = default;
};

// Shared capability-driven relationship equation construction. Component-local
// targets are evaluated through their exact authored transform chains into the
// same root-assembly coordinate space used by hierarchy targets. The builder is
// read-only and owns no graph, optimizer, finite-difference, or transform state.
class AssemblyGenericRelationshipEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyGenericRelationshipEquationDescriptor>
  build(const Project& project, const AssemblyConstraint& constraint) const;

  [[nodiscard]] Result<AssemblyGenericRelationshipEquationDescriptor>
  build(AssemblyConstraintId relationship, AssemblyConstraintType type,
        const AssemblyResolvedGeometricTarget& target_a,
        const AssemblyResolvedGeometricTarget& target_b,
        std::optional<Quantity> angle = std::nullopt) const;
};

} // namespace blcad::geometry
''',
)

create_exact(
    "src/geometry/assembly_generic_relationship_equation_builder.cpp",
    r'''#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"

#include <cmath>
#include <numbers>
#include <string>
#include <type_traits>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool supported_relationship(AssemblyConstraintType type) noexcept {
  return type == AssemblyConstraintType::Coincident || type == AssemblyConstraintType::Parallel ||
         type == AssemblyConstraintType::Perpendicular || type == AssemblyConstraintType::Angle;
}

[[nodiscard]] Vector3 difference(const Point3& target, const Point3& source) noexcept {
  return Vector3{target.x - source.x, target.y - source.y, target.z - source.z};
}

[[nodiscard]] double dot(const Vector3& lhs, const Vector3& rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] AssemblyGeometricTargetDescriptor evaluate_descriptor(
    const std::vector<RigidTransform>& transforms,
    const AssemblyGeometricTargetDescriptor& descriptor) {
  const AssemblyHierarchyTransformEvaluator evaluator;
  return std::visit(
      [&](const auto& value) -> AssemblyGeometricTargetDescriptor {
        using Descriptor = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Descriptor, AssemblyPlanarTargetDescriptor>) {
          return AssemblyPlanarTargetDescriptor{evaluator.evaluate_point(transforms, value.origin),
                                                evaluator.evaluate_vector(transforms, value.x_axis),
                                                evaluator.evaluate_vector(transforms, value.y_axis),
                                                evaluator.evaluate_vector(transforms, value.normal)};
        } else if constexpr (std::is_same_v<Descriptor, AssemblyAxisTargetDescriptor>) {
          return AssemblyAxisTargetDescriptor{evaluator.evaluate_point(transforms, value.origin),
                                              evaluator.evaluate_vector(transforms,
                                                                        value.direction)};
        } else if constexpr (std::is_same_v<Descriptor, AssemblyLineTargetDescriptor>) {
          return AssemblyLineTargetDescriptor{evaluator.evaluate_point(transforms, value.origin),
                                              evaluator.evaluate_vector(transforms,
                                                                        value.direction)};
        } else if constexpr (std::is_same_v<Descriptor, AssemblyPointTargetDescriptor>) {
          return AssemblyPointTargetDescriptor{evaluator.evaluate_point(transforms, value.point)};
        } else if constexpr (std::is_same_v<Descriptor,
                                            AssemblyCircularEdgeTargetDescriptor>) {
          return AssemblyCircularEdgeTargetDescriptor{
              evaluator.evaluate_point(transforms, value.center),
              evaluator.evaluate_vector(transforms, value.x_axis),
              evaluator.evaluate_vector(transforms, value.y_axis),
              evaluator.evaluate_vector(transforms, value.normal), value.radius_mm};
        } else if constexpr (std::is_same_v<Descriptor,
                                            AssemblyCylindricalSurfaceTargetDescriptor>) {
          return AssemblyCylindricalSurfaceTargetDescriptor{
              evaluator.evaluate_point(transforms, value.axis_origin),
              evaluator.evaluate_vector(transforms, value.axis_direction), value.radius_mm};
        } else {
          return AssemblyFrameTargetDescriptor{
              evaluator.evaluate_point(transforms, value.origin),
              evaluator.evaluate_vector(transforms, value.x_axis),
              evaluator.evaluate_vector(transforms, value.y_axis),
              evaluator.evaluate_vector(transforms, value.z_axis)};
        }
      },
      descriptor);
}

[[nodiscard]] Result<AssemblyResolvedGeometricTarget>
normalize_to_root_space(const AssemblyResolvedGeometricTarget& target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(valid.error());
  }
  if (target.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly) {
    return Result<AssemblyResolvedGeometricTarget>::success(target);
  }
  if (target.transforms_inner_to_outer.empty()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(validation_error(
        "geometry.assembly_generic_relationship",
        "component-local generic relationship target must retain a transform chain"));
  }

  AssemblyResolvedGeometricTarget normalized = target;
  normalized.descriptor = evaluate_descriptor(target.transforms_inner_to_outer, target.descriptor);
  normalized.coordinate_space = AssemblyGeometricTargetCoordinateSpace::RootAssembly;
  auto normalized_valid = validate_resolved_geometric_target(normalized);
  if (normalized_valid.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(normalized_valid.error());
  }
  return Result<AssemblyResolvedGeometricTarget>::success(std::move(normalized));
}

[[nodiscard]] Result<Point3> selected_point(const AssemblyResolvedGeometricTarget& target,
                                            AssemblyGeometricTargetCapability capability) {
  if (capability != AssemblyGeometricTargetCapability::Point) {
    return Result<Point3>::failure(validation_error(
        "geometry.assembly_generic_relationship",
        "generic Coincident point projection requires Point compatibility"));
  }
  auto point = project_point(target);
  if (point.has_error()) {
    return Result<Point3>::failure(point.error());
  }
  return Result<Point3>::success(point.value().point);
}

[[nodiscard]] Result<AssemblyLineTargetDescriptor>
selected_line(const AssemblyResolvedGeometricTarget& target,
              AssemblyGeometricTargetCapability capability) {
  if (capability != AssemblyGeometricTargetCapability::Line) {
    return Result<AssemblyLineTargetDescriptor>::failure(validation_error(
        "geometry.assembly_generic_relationship",
        "generic Coincident line projection requires Line compatibility"));
  }
  return project_line(target);
}

[[nodiscard]] Result<AssemblyPlanarTargetDescriptor>
selected_plane(const AssemblyResolvedGeometricTarget& target,
               AssemblyGeometricTargetCapability capability) {
  if (capability != AssemblyGeometricTargetCapability::Plane) {
    return Result<AssemblyPlanarTargetDescriptor>::failure(validation_error(
        "geometry.assembly_generic_relationship",
        "generic Coincident plane projection requires Plane compatibility"));
  }
  return project_plane(target);
}

[[nodiscard]] Result<Vector3>
selected_direction(const AssemblyResolvedGeometricTarget& target,
                   AssemblyGeometricTargetCapability capability) {
  switch (capability) {
  case AssemblyGeometricTargetCapability::Line: {
    auto line = project_line(target);
    if (line.has_error()) {
      return Result<Vector3>::failure(line.error());
    }
    return Result<Vector3>::success(line.value().direction);
  }
  case AssemblyGeometricTargetCapability::Axis: {
    auto axis = project_axis(target);
    if (axis.has_error()) {
      return Result<Vector3>::failure(axis.error());
    }
    return Result<Vector3>::success(axis.value().direction);
  }
  case AssemblyGeometricTargetCapability::Plane: {
    auto plane = project_plane(target);
    if (plane.has_error()) {
      return Result<Vector3>::failure(plane.error());
    }
    return Result<Vector3>::success(plane.value().normal);
  }
  case AssemblyGeometricTargetCapability::Point:
  case AssemblyGeometricTargetCapability::Circle:
  case AssemblyGeometricTargetCapability::Cylinder:
  case AssemblyGeometricTargetCapability::Frame:
    break;
  }
  return Result<Vector3>::failure(validation_error(
      "geometry.assembly_generic_relationship",
      "generic directional relationship requires Line, Axis, or Plane compatibility"));
}

} // namespace

Result<AssemblyGenericRelationshipEquationDescriptor>
AssemblyGenericRelationshipEquationBuilder::build(const Project& project,
                                                   const AssemblyConstraint& constraint) const {
  if (constraint.state() != AssemblyConstraintState::Active) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "generic relationship equation construction requires an active relationship"));
  }
  if (!supported_relationship(constraint.type())) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "generic relationship equation construction requires Coincident, Parallel, "
        "Perpendicular, or Angle intent"));
  }

  const AssemblyConstraintTargetResolver resolver;
  auto target_a = resolver.resolve_geometric(project, constraint.target_a());
  if (target_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(target_a.error());
  }
  auto target_b = resolver.resolve_geometric(project, constraint.target_b());
  if (target_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(target_b.error());
  }
  return build(constraint.id(), constraint.type(), target_a.value(), target_b.value(),
               constraint.angle());
}

Result<AssemblyGenericRelationshipEquationDescriptor>
AssemblyGenericRelationshipEquationBuilder::build(
    AssemblyConstraintId relationship, AssemblyConstraintType type,
    const AssemblyResolvedGeometricTarget& target_a,
    const AssemblyResolvedGeometricTarget& target_b, std::optional<Quantity> angle) const {
  if (!supported_relationship(type)) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
        relationship.value(),
        "generic relationship equation construction requires Coincident, Parallel, "
        "Perpendicular, or Angle intent"));
  }

  auto normalized_a = normalize_to_root_space(target_a);
  if (normalized_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(normalized_a.error());
  }
  auto normalized_b = normalize_to_root_space(target_b);
  if (normalized_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(normalized_b.error());
  }

  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto compatibility = compatibility_resolver.resolve(type, normalized_a.value(), normalized_b.value());
  if (compatibility.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(compatibility.error());
  }

  const auto capability_a = compatibility.value().target_a_capability;
  const auto capability_b = compatibility.value().target_b_capability;
  const AssemblyGenericRelationshipTargetDescriptor equation_target_a{normalized_a.value(),
                                                                      capability_a};
  const AssemblyGenericRelationshipTargetDescriptor equation_target_b{normalized_b.value(),
                                                                      capability_b};

  if (type == AssemblyConstraintType::Coincident) {
    if (capability_a == AssemblyGeometricTargetCapability::Point &&
        capability_b == AssemblyGeometricTargetCapability::Point) {
      auto point_a = selected_point(normalized_a.value(), capability_a);
      auto point_b = selected_point(normalized_b.value(), capability_b);
      if (point_a.has_error())
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point_a.error());
      if (point_b.has_error())
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point_b.error());
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(
          AssemblyGenericRelationshipEquationDescriptor{
              relationship, type, compatibility.value(), equation_target_a, equation_target_b,
              AssemblyGenericRelationshipResidualDescriptor{
                  CoincidentPointPointResidualDescriptor{
                      difference(point_b.value(), point_a.value())}}}});
    }

    const bool point_line = capability_a == AssemblyGeometricTargetCapability::Point &&
                            capability_b == AssemblyGeometricTargetCapability::Line;
    const bool line_point = capability_a == AssemblyGeometricTargetCapability::Line &&
                            capability_b == AssemblyGeometricTargetCapability::Point;
    if (point_line || line_point) {
      const AssemblyGenericRelationshipTargetRole point_role =
          point_line ? AssemblyGenericRelationshipTargetRole::TargetA
                     : AssemblyGenericRelationshipTargetRole::TargetB;
      auto point = point_line ? selected_point(normalized_a.value(), capability_a)
                              : selected_point(normalized_b.value(), capability_b);
      auto line = point_line ? selected_line(normalized_b.value(), capability_b)
                             : selected_line(normalized_a.value(), capability_a);
      if (point.has_error())
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point.error());
      if (line.has_error())
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(line.error());
      const Vector3 point_from_line = difference(point.value(), line.value().origin);
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(
          AssemblyGenericRelationshipEquationDescriptor{
              relationship, type, compatibility.value(), equation_target_a, equation_target_b,
              AssemblyGenericRelationshipResidualDescriptor{
                  CoincidentPointLineResidualDescriptor{
                      point_role, cross(point_from_line, line.value().direction)}}}});
    }

    const bool point_plane = capability_a == AssemblyGeometricTargetCapability::Point &&
                             capability_b == AssemblyGeometricTargetCapability::Plane;
    const bool plane_point = capability_a == AssemblyGeometricTargetCapability::Plane &&
                             capability_b == AssemblyGeometricTargetCapability::Point;
    if (point_plane || plane_point) {
      const AssemblyGenericRelationshipTargetRole point_role =
          point_plane ? AssemblyGenericRelationshipTargetRole::TargetA
                      : AssemblyGenericRelationshipTargetRole::TargetB;
      auto point = point_plane ? selected_point(normalized_a.value(), capability_a)
                               : selected_point(normalized_b.value(), capability_b);
      auto plane = point_plane ? selected_plane(normalized_b.value(), capability_b)
                               : selected_plane(normalized_a.value(), capability_a);
      if (point.has_error())
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point.error());
      if (plane.has_error())
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(plane.error());
      const double signed_distance =
          dot(difference(point.value(), plane.value().origin), plane.value().normal);
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(
          AssemblyGenericRelationshipEquationDescriptor{
              relationship, type, compatibility.value(), equation_target_a, equation_target_b,
              AssemblyGenericRelationshipResidualDescriptor{
                  CoincidentPointPlaneResidualDescriptor{point_role, signed_distance}}});
    }
  }

  auto direction_a = selected_direction(normalized_a.value(), capability_a);
  if (direction_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(direction_a.error());
  }
  auto direction_b = selected_direction(normalized_b.value(), capability_b);
  if (direction_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(direction_b.error());
  }

  if (type == AssemblyConstraintType::Parallel) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(
        AssemblyGenericRelationshipEquationDescriptor{
            relationship, type, compatibility.value(), equation_target_a, equation_target_b,
            AssemblyGenericRelationshipResidualDescriptor{ParallelDirectionResidualDescriptor{
                cross(direction_a.value(), direction_b.value())}}});
  }
  if (type == AssemblyConstraintType::Perpendicular) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(
        AssemblyGenericRelationshipEquationDescriptor{
            relationship, type, compatibility.value(), equation_target_a, equation_target_b,
            AssemblyGenericRelationshipResidualDescriptor{PerpendicularDirectionResidualDescriptor{
                dot(direction_a.value(), direction_b.value())}}});
  }
  if (type == AssemblyConstraintType::Angle) {
    if (!angle.has_value() || angle->kind() != QuantityKind::AngleDeg) {
      return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
          relationship.value(),
          "directional Angle equation construction requires an angle value in degrees"));
    }
    const double target_angle_deg = angle->degrees();
    const double target_cosine =
        std::cos(target_angle_deg * (std::numbers::pi_v<double> / 180.0));
    const double direction_dot = dot(direction_a.value(), direction_b.value());
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(
        AssemblyGenericRelationshipEquationDescriptor{
            relationship, type, compatibility.value(), equation_target_a, equation_target_b,
            AssemblyGenericRelationshipResidualDescriptor{DirectionalAngleResidualDescriptor{
                target_angle_deg, direction_dot, direction_dot - target_cosine}}});
  }

  return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
      relationship.value(), "generic relationship compatibility selected an unsupported pair"));
}

} // namespace blcad::geometry
''',
)

# ---------------------------------------------------------------------------
# Compatibility and graph participation.
# ---------------------------------------------------------------------------
replace_exact(
    "src/geometry/assembly_target_compatibility.cpp",
    '''  case AssemblyConstraintType::Coincident:
  case AssemblyConstraintType::Parallel:
  case AssemblyConstraintType::Perpendicular:
    return {};
''',
    '''  case AssemblyConstraintType::Coincident:
    return {{Capability::Point, Capability::Point},
            {Capability::Point, Capability::Line},
            {Capability::Line, Capability::Point},
            {Capability::Point, Capability::Plane},
            {Capability::Plane, Capability::Point}};
  case AssemblyConstraintType::Parallel:
  case AssemblyConstraintType::Perpendicular:
    return {{Capability::Line, Capability::Line},
            {Capability::Axis, Capability::Axis},
            {Capability::Line, Capability::Axis},
            {Capability::Axis, Capability::Line},
            {Capability::Plane, Capability::Plane}};
''',
)

replace_exact(
    "src/core/assembly_constraint_graph_participation.hpp",
    '''// Block 38 adds persistent generic relationship intent only. Keep the existing
// five equation-enabled families in solve/motion connectivity until Block 39
// implements generic relationship equations and shared graph integration.
''',
    '''// Equation-enabled geometric relationship participation authority. Block 39
// enables the three generic relationship families in the same local, hierarchy,
// and motion connectivity used by the historical five families.
''',
)
replace_exact(
    "src/core/assembly_constraint_graph_participation.hpp",
    '''  case AssemblyConstraintType::Angle:
    return true;
  case AssemblyConstraintType::Coincident:
  case AssemblyConstraintType::Parallel:
  case AssemblyConstraintType::Perpendicular:
    return false;
''',
    '''  case AssemblyConstraintType::Angle:
  case AssemblyConstraintType::Coincident:
  case AssemblyConstraintType::Parallel:
  case AssemblyConstraintType::Perpendicular:
    return true;
''',
)

# ---------------------------------------------------------------------------
# Numeric residual flattening and local solve integration.
# ---------------------------------------------------------------------------
replace_exact(
    "src/geometry/assembly_constraint_numeric_system.hpp",
    '#include "blcad/geometry/assembly_constraint_equation_builder.hpp"\n',
    '#include "blcad/geometry/assembly_constraint_equation_builder.hpp"\n'
    '#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"\n',
)
replace_exact(
    "src/geometry/assembly_constraint_numeric_system.hpp",
    '''[[nodiscard]] Result<std::size_t> append_scaled_residuals(
    const InsertResidualDescriptor& residual,
    double length_residual_scale_mm,
    NumericVector& residuals);
''',
    '''[[nodiscard]] Result<std::size_t> append_scaled_residuals(
    const InsertResidualDescriptor& residual,
    double length_residual_scale_mm,
    NumericVector& residuals);
[[nodiscard]] Result<std::size_t> append_scaled_residuals(
    const AssemblyGenericRelationshipResidualDescriptor& residual,
    double length_residual_scale_mm,
    NumericVector& residuals);
''',
)

replace_exact(
    "src/geometry/assembly_constraint_numeric_system.cpp",
    '''[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const PlanarAngleResidualDescriptor& angle, double, NumericVector& residuals) {
  residuals.push_back(angle.angle_alignment);
  return Result<std::size_t>::success(1U);
}
''',
    '''[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const PlanarAngleResidualDescriptor& angle, double, NumericVector& residuals) {
  residuals.push_back(angle.angle_alignment);
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const CoincidentPointPointResidualDescriptor& coincident, double length_residual_scale_mm,
    NumericVector& residuals) {
  residuals.push_back(coincident.point_delta_mm.x / length_residual_scale_mm);
  residuals.push_back(coincident.point_delta_mm.y / length_residual_scale_mm);
  residuals.push_back(coincident.point_delta_mm.z / length_residual_scale_mm);
  return Result<std::size_t>::success(3U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const CoincidentPointLineResidualDescriptor& coincident, double length_residual_scale_mm,
    NumericVector& residuals) {
  residuals.push_back(coincident.point_line_cross_mm.x / length_residual_scale_mm);
  residuals.push_back(coincident.point_line_cross_mm.y / length_residual_scale_mm);
  residuals.push_back(coincident.point_line_cross_mm.z / length_residual_scale_mm);
  return Result<std::size_t>::success(3U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const CoincidentPointPlaneResidualDescriptor& coincident, double length_residual_scale_mm,
    NumericVector& residuals) {
  residuals.push_back(coincident.signed_distance_mm / length_residual_scale_mm);
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const ParallelDirectionResidualDescriptor& parallel, double, NumericVector& residuals) {
  residuals.push_back(parallel.direction_parallelism.x);
  residuals.push_back(parallel.direction_parallelism.y);
  residuals.push_back(parallel.direction_parallelism.z);
  return Result<std::size_t>::success(3U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const PerpendicularDirectionResidualDescriptor& perpendicular, double,
    NumericVector& residuals) {
  residuals.push_back(perpendicular.direction_dot);
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const DirectionalAngleResidualDescriptor& angle, double, NumericVector& residuals) {
  residuals.push_back(angle.angle_alignment);
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const AssemblyGenericRelationshipResidualDescriptor& generic, double length_residual_scale_mm,
    NumericVector& residuals) {
  return std::visit(
      [&](const auto& value) {
        return append_scaled_residual(value, length_residual_scale_mm, residuals);
      },
      generic.value);
}
''',
)

replace_exact(
    "src/geometry/assembly_constraint_numeric_system.cpp",
    '''  if (constraint->type() == AssemblyConstraintType::Concentric) {
''',
    '''  if (constraint->type() == AssemblyConstraintType::Coincident ||
      constraint->type() == AssemblyConstraintType::Parallel ||
      constraint->type() == AssemblyConstraintType::Perpendicular ||
      constraint->type() == AssemblyConstraintType::Angle) {
    const AssemblyGenericRelationshipEquationBuilder builder;
    auto equation = builder.build(project, *constraint);
    if (equation.has_error()) return Result<std::size_t>::failure(equation.error());
    return append_scaled_residuals(equation.value().residual, length_residual_scale_mm,
                                   residuals);
  }
  if (constraint->type() == AssemblyConstraintType::Concentric) {
''',
)

replace_exact(
    "src/geometry/assembly_constraint_numeric_system.cpp",
    '''Result<std::size_t> append_scaled_residuals(const InsertResidualDescriptor& residual,
                                             double length_residual_scale_mm,
                                             NumericVector& residuals) {
  return append_scaled_residual(residual, length_residual_scale_mm, residuals);
}
''',
    '''Result<std::size_t> append_scaled_residuals(const InsertResidualDescriptor& residual,
                                             double length_residual_scale_mm,
                                             NumericVector& residuals) {
  return append_scaled_residual(residual, length_residual_scale_mm, residuals);
}

Result<std::size_t> append_scaled_residuals(
    const AssemblyGenericRelationshipResidualDescriptor& residual,
    double length_residual_scale_mm, NumericVector& residuals) {
  return append_scaled_residual(residual, length_residual_scale_mm, residuals);
}
''',
)

# ---------------------------------------------------------------------------
# Cross-hierarchy equation descriptor and builder integration.
# ---------------------------------------------------------------------------
replace_exact(
    "include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp",
    '#include "blcad/geometry/assembly_constraint_equation_builder.hpp"\n',
    '#include "blcad/geometry/assembly_constraint_equation_builder.hpp"\n'
    '#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"\n',
)
replace_exact(
    "include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp",
    '''struct AssemblyHierarchyInsertConstraintTargetDescriptor {
''',
    '''struct AssemblyHierarchyGenericConstraintTargetDescriptor {
  std::vector<SubassemblyInstanceId> occurrence_path;
  DocumentId assembly_document;
  ComponentInstanceId component_instance;
  AssemblyGeometricTargetSourceMetadata source_metadata;
  std::string semantic_reference;
  AssemblyGeometricTargetCapability selected_capability = AssemblyGeometricTargetCapability::Point;
  AssemblyGeometricTargetDescriptor descriptor;

  friend bool operator==(const AssemblyHierarchyGenericConstraintTargetDescriptor&,
                         const AssemblyHierarchyGenericConstraintTargetDescriptor&) = default;
};

struct AssemblyHierarchyInsertConstraintTargetDescriptor {
''',
)
replace_exact(
    "include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp",
    '''using AssemblyHierarchyConstraintTargetDescriptor =
    std::variant<AssemblyHierarchyPlanarConstraintTargetDescriptor,
                  AssemblyHierarchyAxisConstraintTargetDescriptor,
                  AssemblyHierarchyInsertConstraintTargetDescriptor>;
''',
    '''using AssemblyHierarchyConstraintTargetDescriptor =
    std::variant<AssemblyHierarchyPlanarConstraintTargetDescriptor,
                 AssemblyHierarchyAxisConstraintTargetDescriptor,
                 AssemblyHierarchyGenericConstraintTargetDescriptor,
                 AssemblyHierarchyInsertConstraintTargetDescriptor>;
''',
)
replace_exact(
    "include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp",
    '''                  PlanarAngleResidualDescriptor,
                  ConcentricResidualDescriptor,
                  InsertResidualDescriptor>;
''',
    '''                  PlanarAngleResidualDescriptor,
                  ConcentricResidualDescriptor,
                  InsertResidualDescriptor,
                  AssemblyGenericRelationshipResidualDescriptor>;
''',
)
replace_exact(
    "include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp",
    '''// Builds read-only Mate/Distance/Angle/Concentric/Insert residual semantics for
// endpoints that may live in different AssemblyDocument occurrences. The query
// is not solved, persisted, or inserted into a local constraint graph.
''',
    '''// Builds read-only Mate/Distance/Angle/Concentric/Insert and generic
// Coincident/Parallel/Perpendicular residual semantics for endpoints that may
// live in different AssemblyDocument occurrences. The query is not solved,
// persisted, or inserted into a local constraint graph.
''',
)

replace_exact(
    "src/geometry/assembly_hierarchy_constraint_equation_builder.cpp",
    '''[[nodiscard]] bool selected_plane_pair(const AssemblyTargetCompatibility& compatibility) noexcept {
''',
    '''[[nodiscard]] bool generic_relationship_type(AssemblyConstraintType type) noexcept {
  return type == AssemblyConstraintType::Coincident || type == AssemblyConstraintType::Parallel ||
         type == AssemblyConstraintType::Perpendicular;
}

[[nodiscard]] AssemblyHierarchyGenericConstraintTargetDescriptor
project_hierarchy_generic_target(const ResolvedHierarchyGeometricTarget& resolved,
                                 const AssemblyGenericRelationshipTargetDescriptor& target) {
  return AssemblyHierarchyGenericConstraintTargetDescriptor{
      resolved.occurrence_path, resolved.assembly_document, resolved.component_instance,
      target.target.source_metadata, resolved.semantic_reference, target.selected_capability,
      target.target.descriptor};
}

[[nodiscard]] bool selected_plane_pair(const AssemblyTargetCompatibility& compatibility) noexcept {
''',
)

replace_exact(
    "src/geometry/assembly_hierarchy_constraint_equation_builder.cpp",
    '''Result<AssemblyHierarchyConstraintEquationDescriptor>
AssemblyHierarchyConstraintEquationBuilder::build(
    const Project& project, const AssemblyHierarchyConstraintQuery& query) const {
  if (query.type() == AssemblyConstraintType::Concentric) {
''',
    '''Result<AssemblyHierarchyConstraintEquationDescriptor>
AssemblyHierarchyConstraintEquationBuilder::build(
    const Project& project, const AssemblyHierarchyConstraintQuery& query) const {
  if (generic_relationship_type(query.type())) {
    auto geometric_target_a = resolve_hierarchy_geometric_target(project, query.target_a());
    if (geometric_target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_a.error());
    }
    auto geometric_target_b = resolve_hierarchy_geometric_target(project, query.target_b());
    if (geometric_target_b.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_b.error());
    }

    const AssemblyGenericRelationshipEquationBuilder generic_builder;
    auto equation = generic_builder.build(query.id(), query.type(), geometric_target_a.value().target,
                                          geometric_target_b.value().target, query.angle());
    if (equation.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(equation.error());
    }
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(),
            project_hierarchy_generic_target(geometric_target_a.value(), equation.value().target_a),
            project_hierarchy_generic_target(geometric_target_b.value(), equation.value().target_b),
            equation.value().residual});
  }

  if (query.type() == AssemblyConstraintType::Concentric) {
''',
)

replace_exact(
    "src/geometry/assembly_hierarchy_constraint_equation_builder.cpp",
    '''  if (!selected_plane_pair(compatibility.value())) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(validation_error(
        query.id().value(),
        "cross-hierarchy planar equation currently requires Plane/Plane target compatibility"));
  }
''',
    '''  if (query.type() == AssemblyConstraintType::Angle &&
      !selected_plane_pair(compatibility.value())) {
    const AssemblyGenericRelationshipEquationBuilder generic_builder;
    auto equation = generic_builder.build(query.id(), query.type(), geometric_target_a.value().target,
                                          geometric_target_b.value().target, query.angle());
    if (equation.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(equation.error());
    }
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(),
            project_hierarchy_generic_target(geometric_target_a.value(), equation.value().target_a),
            project_hierarchy_generic_target(geometric_target_b.value(), equation.value().target_b),
            equation.value().residual});
  }
  if (!selected_plane_pair(compatibility.value())) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(validation_error(
        query.id().value(),
        "cross-hierarchy planar equation currently requires Plane/Plane target compatibility"));
  }
''',
)

# ---------------------------------------------------------------------------
# CMake integration.
# ---------------------------------------------------------------------------
replace_exact(
    "CMakeLists.txt",
    '''    src/geometry/assembly_constraint_equation_builder.cpp
    src/geometry/assembly_constraint_numeric_system.cpp
''',
    '''    src/geometry/assembly_constraint_equation_builder.cpp
    src/geometry/assembly_generic_relationship_equation_builder.cpp
    src/geometry/assembly_constraint_numeric_system.cpp
''',
)
replace_exact(
    "CMakeLists.txt",
    '''      tests/geometry/assembly_constraint_equation_builder_tests.cpp
      tests/geometry/assembly_constraint_target_resolver_tests.cpp
''',
    '''      tests/geometry/assembly_constraint_equation_builder_tests.cpp
      tests/geometry/assembly_generic_relationship_equation_tests.cpp
      tests/geometry/assembly_constraint_target_resolver_tests.cpp
''',
)

# ---------------------------------------------------------------------------
# Focused Block-39 acceptance coverage.
# ---------------------------------------------------------------------------
create_exact(
    "tests/geometry/assembly_generic_relationship_equation_tests.cpp",
    r'''#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_solve_diagnostics.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
namespace ts = blcad::geometry::test_support;
using Catch::Approx;

namespace {

constexpr double kTolerance = 1.0e-6;

AssemblyResolvedGeometricTarget synthetic_target(AssemblyGeometricTargetSourceKind kind,
                                                  AssemblyGeometricTargetDescriptor descriptor,
                                                  const char* semantic_reference) {
  AssemblyGeometricTargetSourceMetadata metadata;
  metadata.referenced_part_document = DocumentId("part.synthetic");
  if (kind == AssemblyGeometricTargetSourceKind::GeneratedPlanarFace ||
      kind == AssemblyGeometricTargetSourceKind::GeneratedLinearEdge ||
      kind == AssemblyGeometricTargetSourceKind::GeneratedVertex) {
    metadata.source_feature = FeatureId("feature.synthetic");
  }
  if (kind == AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace) {
    metadata.source_feature = FeatureId("feature.synthetic");
    metadata.source_profile = ProfileId("profile.synthetic");
  }
  return AssemblyResolvedGeometricTarget{
      AssemblyLocalGeometricTargetEndpointIdentity{ComponentInstanceId("component.synthetic"),
                                                   semantic_reference},
      kind,
      std::move(metadata),
      std::move(descriptor),
      assembly_geometric_target_capabilities(kind),
      AssemblyGeometricTargetCoordinateSpace::RootAssembly,
      {}};
}

AssemblyResolvedGeometricTarget point_target(Point3 point, const char* reference = "point") {
  return synthetic_target(AssemblyGeometricTargetSourceKind::ConstructionPoint,
                          AssemblyPointTargetDescriptor{point}, reference);
}

AssemblyResolvedGeometricTarget line_target(Point3 origin, Vector3 direction,
                                             const char* reference = "line") {
  return synthetic_target(AssemblyGeometricTargetSourceKind::ConstructionLine,
                          AssemblyLineTargetDescriptor{origin, direction}, reference);
}

AssemblyResolvedGeometricTarget axis_target(Point3 origin, Vector3 direction,
                                             const char* reference = "axis") {
  return synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
                          AssemblyCylindricalSurfaceTargetDescriptor{origin, direction, 5.0},
                          reference);
}

AssemblyResolvedGeometricTarget plane_target(Point3 origin, Vector3 normal,
                                              const char* reference = "plane") {
  const Vector3 x_axis = std::abs(normal.x) > 0.5 ? Vector3{0.0, 1.0, 0.0}
                                                  : Vector3{1.0, 0.0, 0.0};
  const Vector3 y_axis = std::abs(normal.x) > 0.5 ? Vector3{0.0, 0.0, 1.0}
                                                  : Vector3{0.0, 1.0, 0.0};
  return synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedPlanarFace,
                          AssemblyPlanarTargetDescriptor{origin, x_axis, y_axis, normal}, reference);
}

Quantity angle(double degrees, const char* id) {
  auto value = Quantity::angle_deg(degrees, id);
  REQUIRE(value);
  return value.value();
}

template <typename Id> std::string reference_spelling(Id id) {
  auto spelling = make_assembly_reference_target_spelling(AssemblyReferenceTargetIdentity{id});
  REQUIRE(spelling);
  return spelling.value();
}

AssemblyConstraintTarget local_target(const char* component_id, const std::string& reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component_id), reference);
  REQUIRE(target);
  return target.value();
}

AssemblyHierarchyConstraintEndpoint hierarchy_target(std::initializer_list<const char*> path,
                                                      const char* component_id,
                                                      const std::string& reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* id : path) {
    occurrence_path.emplace_back(id);
  }
  auto target = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component_id), reference);
  REQUIRE(target);
  return target.value();
}

Project local_coincident_project() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.generic.local"), "GenericLocal");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(assembly.value().add_component_instance(
      ts::component("component.a", ComponentGroundingState::Grounded)));
  REQUIRE(assembly.value().add_component_instance(ts::component(
      "component.b", ComponentGroundingState::Free,
      RigidTransform{Vector3{10.0, -4.0, 2.0}, Vector3{}})));

  const std::string point = reference_spelling(ConstructionPointId("construction_point.anchor"));
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.coincident"), "Coincident",
      AssemblyConstraintType::Coincident, local_target("component.a", point),
      local_target("component.b", point));
  REQUIRE(constraint);
  REQUIRE(assembly.value().add_constraint(constraint.value()));

  auto project = Project::create(DocumentId("project.generic.local"), "GenericLocal", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

Project cross_coincident_project(bool repeated = false) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      ts::component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(
      ts::occurrence("subassembly.left", identity_rigid_transform())));
  if (repeated) {
    REQUIRE(root.value().add_subassembly_instance(ts::occurrence(
        "subassembly.right", RigidTransform{Vector3{20.0, 0.0, 0.0}, Vector3{}})));
  }

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(ts::component(
      "component.child", ComponentGroundingState::Free,
      RigidTransform{Vector3{10.0, -4.0, 2.0}, Vector3{}})));

  auto project = Project::create(DocumentId("project.generic.cross"), "GenericCross", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(ts::solver_part()));

  const std::string point = reference_spelling(ConstructionPointId("construction_point.anchor"));
  auto left = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.left"), "Coincident left",
      AssemblyConstraintType::Coincident, hierarchy_target({}, "component.root", point),
      hierarchy_target({"subassembly.left"}, "component.child", point));
  REQUIRE(left);
  REQUIRE(project.value().add_cross_hierarchy_constraint(left.value()));

  if (repeated) {
    auto right = AssemblyHierarchyConstraint::create(
        AssemblyConstraintId("constraint.cross.right"), "Coincident right",
        AssemblyConstraintType::Coincident, hierarchy_target({}, "component.root", point),
        hierarchy_target({"subassembly.right"}, "component.child", point));
    REQUIRE(right);
    REQUIRE(project.value().add_cross_hierarchy_constraint(right.value()));
  }

  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

std::vector<ComponentInstanceId> local_group() {
  return {ComponentInstanceId("component.a"), ComponentInstanceId("component.b")};
}

const ProposedComponentTransform& local_proposal(const AssemblySolveResult& result) {
  REQUIRE(result.proposed_transforms.size() == 1U);
  return result.proposed_transforms.front();
}

RigidTransform child_transform(const Project& project) {
  return project.find_assembly_document(DocumentId("assembly.child"))
      ->find_component_instance(ComponentInstanceId("component.child"))
      ->transform();
}

} // namespace

TEST_CASE("Generic relationship equations freeze Coincident capability semantics",
          "[geometry][assembly-generic-relationships]") {
  const AssemblyGenericRelationshipEquationBuilder builder;

  auto point_point = builder.build(
      AssemblyConstraintId("relationship.point_point"), AssemblyConstraintType::Coincident,
      point_target(Point3{1.0, 2.0, 3.0}, "point.a"),
      point_target(Point3{4.0, 6.0, 3.0}, "point.b"));
  REQUIRE(point_point);
  const auto& point_delta = std::get<CoincidentPointPointResidualDescriptor>(
      point_point.value().residual.value);
  CHECK(point_delta.point_delta_mm == Vector3{3.0, 4.0, 0.0});

  auto point_line = builder.build(
      AssemblyConstraintId("relationship.point_line"), AssemblyConstraintType::Coincident,
      point_target(Point3{1.0, 2.0, 0.0}, "point.a"),
      line_target(Point3{1.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0}, "line.b"));
  REQUIRE(point_line);
  const auto& point_line_residual = std::get<CoincidentPointLineResidualDescriptor>(
      point_line.value().residual.value);
  CHECK(point_line_residual.point_target == AssemblyGenericRelationshipTargetRole::TargetA);
  CHECK(point_line_residual.point_line_cross_mm == Vector3{0.0, 0.0, -2.0});

  auto line_point = builder.build(
      AssemblyConstraintId("relationship.line_point"), AssemblyConstraintType::Coincident,
      line_target(Point3{1.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0}, "line.a"),
      point_target(Point3{1.0, 2.0, 0.0}, "point.b"));
  REQUIRE(line_point);
  const auto& line_point_residual = std::get<CoincidentPointLineResidualDescriptor>(
      line_point.value().residual.value);
  CHECK(line_point_residual.point_target == AssemblyGenericRelationshipTargetRole::TargetB);
  CHECK(line_point_residual.point_line_cross_mm == Vector3{0.0, 0.0, -2.0});

  auto point_plane = builder.build(
      AssemblyConstraintId("relationship.point_plane"), AssemblyConstraintType::Coincident,
      point_target(Point3{2.0, 3.0, 5.0}, "point.a"),
      plane_target(Point3{}, Vector3{0.0, 0.0, 1.0}, "plane.b"));
  REQUIRE(point_plane);
  const auto& point_plane_residual = std::get<CoincidentPointPlaneResidualDescriptor>(
      point_plane.value().residual.value);
  CHECK(point_plane_residual.point_target == AssemblyGenericRelationshipTargetRole::TargetA);
  CHECK(point_plane_residual.signed_distance_mm == Approx(5.0));

  auto plane_point = builder.build(
      AssemblyConstraintId("relationship.plane_point"), AssemblyConstraintType::Coincident,
      plane_target(Point3{}, Vector3{0.0, 0.0, 1.0}, "plane.a"),
      point_target(Point3{2.0, 3.0, 5.0}, "point.b"));
  REQUIRE(plane_point);
  const auto& plane_point_residual = std::get<CoincidentPointPlaneResidualDescriptor>(
      plane_point.value().residual.value);
  CHECK(plane_point_residual.point_target == AssemblyGenericRelationshipTargetRole::TargetB);
  CHECK(plane_point_residual.signed_distance_mm == Approx(5.0));
}

TEST_CASE("Generic direction equations support Line Axis and Plane capabilities",
          "[geometry][assembly-generic-relationships]") {
  const AssemblyGenericRelationshipEquationBuilder builder;

  auto parallel_line_axis = builder.build(
      AssemblyConstraintId("relationship.parallel.line_axis"), AssemblyConstraintType::Parallel,
      line_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
      axis_target(Point3{}, Vector3{-1.0, 0.0, 0.0}));
  REQUIRE(parallel_line_axis);
  const auto& line_axis_parallel = std::get<ParallelDirectionResidualDescriptor>(
      parallel_line_axis.value().residual.value);
  CHECK(line_axis_parallel.direction_parallelism == Vector3{});
  CHECK(parallel_line_axis.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Line);
  CHECK(parallel_line_axis.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Axis);

  auto parallel_planes = builder.build(
      AssemblyConstraintId("relationship.parallel.planes"), AssemblyConstraintType::Parallel,
      plane_target(Point3{}, Vector3{0.0, 0.0, 1.0}),
      plane_target(Point3{}, Vector3{0.0, 0.0, -1.0}));
  REQUIRE(parallel_planes);
  CHECK(std::get<ParallelDirectionResidualDescriptor>(parallel_planes.value().residual.value)
            .direction_parallelism == Vector3{});

  auto perpendicular_line_axis = builder.build(
      AssemblyConstraintId("relationship.perpendicular.line_axis"),
      AssemblyConstraintType::Perpendicular,
      line_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
      axis_target(Point3{}, Vector3{0.0, 1.0, 0.0}));
  REQUIRE(perpendicular_line_axis);
  CHECK(std::get<PerpendicularDirectionResidualDescriptor>(
            perpendicular_line_axis.value().residual.value)
            .direction_dot == Approx(0.0));

  auto perpendicular_planes = builder.build(
      AssemblyConstraintId("relationship.perpendicular.planes"),
      AssemblyConstraintType::Perpendicular,
      plane_target(Point3{}, Vector3{0.0, 0.0, 1.0}),
      plane_target(Point3{}, Vector3{1.0, 0.0, 0.0}));
  REQUIRE(perpendicular_planes);
  CHECK(std::get<PerpendicularDirectionResidualDescriptor>(
            perpendicular_planes.value().residual.value)
            .direction_dot == Approx(0.0));

  auto line_angle = builder.build(
      AssemblyConstraintId("relationship.angle.lines"), AssemblyConstraintType::Angle,
      line_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
      line_target(Point3{}, Vector3{0.0, 1.0, 0.0}),
      angle(90.0, "relationship.angle.lines"));
  REQUIRE(line_angle);
  const auto& angle_residual =
      std::get<DirectionalAngleResidualDescriptor>(line_angle.value().residual.value);
  CHECK(angle_residual.target_angle_deg == Approx(90.0));
  CHECK(angle_residual.direction_dot == Approx(0.0));
  CHECK(angle_residual.angle_alignment == Approx(0.0).margin(1.0e-12));
  CHECK(line_angle.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Line);
  CHECK(line_angle.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Line);

  auto incompatible = builder.build(
      AssemblyConstraintId("relationship.coincident.bad"), AssemblyConstraintType::Coincident,
      line_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
      line_target(Point3{}, Vector3{1.0, 0.0, 0.0}));
  CHECK(incompatible.has_error());
}

TEST_CASE("Local generic relationships enter the shared solve freshness and rank paths",
          "[geometry][assembly-generic-relationships-solver]"
          "[geometry][assembly-generic-relationships-diagnostics]") {
  Project project = local_coincident_project();
  auto graph = AssemblyConstraintGraph::build(project.assembly());
  REQUIRE(graph);
  CHECK(graph.value().edge_count() == 1U);

  const RigidTransform source = project.assembly()
                                    .find_component_instance(ComponentInstanceId("component.b"))
                                    ->transform();
  const AssemblyRigidBodySolver solver;
  auto solved = solver.solve(project, local_group());
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  CHECK(solved.value().residual_summary.residual_component_count == 3U);
  CHECK(solved.value().residual_summary.final_rms <= 1.0e-8);
  const auto& proposal = local_proposal(solved.value());
  CHECK(proposal.source_transform == source);
  CHECK(proposal.proposed_transform.translation_mm.x == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.translation_mm.y == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.translation_mm.z == Approx(0.0).margin(kTolerance));
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.b"))
            ->transform() == source);

  const AssemblySolveDiagnosticsAnalyzer diagnostics_analyzer;
  auto diagnostics = diagnostics_analyzer.analyze(project, local_group());
  REQUIRE(diagnostics);
  REQUIRE(diagnostics.value().jacobian_rank.evaluated);
  CHECK(diagnostics.value().jacobian_rank.rank == 3U);
  CHECK(diagnostics.value().jacobian_rank.constrained_dof == 3U);
  CHECK(diagnostics.value().jacobian_rank.remaining_dof == 3U);
  CHECK(diagnostics.value().dof_classification == AssemblyDofClassification::Underconstrained);

  auto thickness = Quantity::length_mm(9.0, "part.thickness");
  REQUIRE(thickness);
  REQUIRE(project.find_part_document(DocumentId("part.block27_plate"))
              ->set_parameter_value(ParameterId("part.thickness"), thickness.value()));
  const AssemblySolveResultApplier applier;
  CHECK(applier.apply(project, solved.value()).has_error());
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.b"))
            ->transform() == source);
}

TEST_CASE("Cross-hierarchy generic relationships reuse authorities solver application and diagnostics",
          "[geometry][assembly-generic-relationships-cross-hierarchy]"
          "[geometry][assembly-generic-relationships-diagnostics]") {
  Project project = cross_coincident_project();
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);
  REQUIRE(graph.value().solve_group_count() == 1U);
  CHECK(graph.value().relationship_count() == 1U);
  CHECK(graph.value().authority_count() == 2U);

  const RigidTransform source = child_transform(project);
  const AssemblyCrossHierarchyConstraintSolver solver;
  auto solved = solver.solve(project, graph.value().solve_groups().front());
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  CHECK(solved.value().residual_summary.residual_component_count == 3U);
  REQUIRE(solved.value().proposed_transforms.size() == 1U);
  CHECK(solved.value().proposed_transforms.front().proposed_transform.translation_mm.x ==
        Approx(0.0).margin(kTolerance));
  CHECK(solved.value().proposed_transforms.front().proposed_transform.translation_mm.y ==
        Approx(0.0).margin(kTolerance));
  CHECK(solved.value().proposed_transforms.front().proposed_transform.translation_mm.z ==
        Approx(0.0).margin(kTolerance));
  CHECK(child_transform(project) == source);

  const AssemblyCrossHierarchySolveDiagnosticsAnalyzer diagnostics_analyzer;
  auto diagnostics =
      diagnostics_analyzer.analyze(project, graph.value().solve_groups().front());
  REQUIRE(diagnostics);
  REQUIRE(diagnostics.value().jacobian_rank.evaluated);
  CHECK(diagnostics.value().jacobian_rank.rank == 3U);
  CHECK(diagnostics.value().jacobian_rank.constrained_dof == 3U);
  CHECK(diagnostics.value().jacobian_rank.remaining_dof == 3U);

  const AssemblyCrossHierarchySolveResultApplier applier;
  auto applied = applier.apply(project, solved.value());
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  CHECK(child_transform(project).translation_mm.x == Approx(0.0).margin(kTolerance));
  CHECK(child_transform(project).translation_mm.y == Approx(0.0).margin(kTolerance));
  CHECK(child_transform(project).translation_mm.z == Approx(0.0).margin(kTolerance));
}

TEST_CASE("Repeated generic endpoints preserve one shared child transform authority",
          "[geometry][assembly-generic-relationships-cross-hierarchy]") {
  const Project project = cross_coincident_project(true);
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);
  CHECK(graph.value().relationship_count() == 2U);
  CHECK(graph.value().authority_count() == 2U);
  REQUIRE(graph.value().solve_group_count() == 1U);
  CHECK(graph.value().solve_groups().front().relationships.size() == 2U);
  CHECK(graph.value().solve_groups().front().authorities.size() == 2U);
  CHECK(graph.value().solve_groups().front().authorities[0] ==
        ComponentTransformAuthority{DocumentId("assembly.child"),
                                    ComponentInstanceId("component.child")});
  CHECK(graph.value().solve_groups().front().authorities[1] ==
        ComponentTransformAuthority{DocumentId("assembly.root"),
                                    ComponentInstanceId("component.root")});
}

TEST_CASE("Cross-hierarchy generic solve results stale on semantic target part edits",
          "[geometry][assembly-generic-relationships-cross-hierarchy]") {
  Project project = cross_coincident_project();
  const auto group = ts::only_group(project);
  const AssemblyCrossHierarchyConstraintSolver solver;
  auto solved = solver.solve(project, group);
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  const RigidTransform source = child_transform(project);

  auto thickness = Quantity::length_mm(9.0, "part.thickness");
  REQUIRE(thickness);
  REQUIRE(project.find_part_document(DocumentId("part.block27_plate"))
              ->set_parameter_value(ParameterId("part.thickness"), thickness.value()));

  const AssemblyCrossHierarchySolveResultApplier applier;
  CHECK(applier.apply(project, solved.value()).has_error());
  CHECK(child_transform(project) == source);
}
''',
)

# ---------------------------------------------------------------------------
# Block-39 canonical documentation.
# ---------------------------------------------------------------------------
create_exact(
    "docs/assembly-generic-relationship-equations-mvp5.md",
    r'''# Assembly Generic Relationship Equations and Shared Solve Integration MVP-5

Status: implemented as Block 39 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for Coincident, Parallel, and Perpendicular capability compatibility, residual semantics, local/root-space equation construction, shared numeric integration, freshness/application behavior, Jacobian-rank diagnostics, and the Block-40 handoff.

## Scope

Implemented:

- Block-37 compatibility entries for Coincident, Parallel, and Perpendicular;
- shared capability-driven equation construction over typed resolved targets;
- component-local target normalization through exact authored transform chains;
- root-space hierarchy target reuse without a second coordinate authority;
- Point/Point, Point/Line, Line/Point, Point/Plane, and Plane/Point Coincident residuals;
- Line/Axis direction-pair and Plane-normal Parallel residuals;
- Line/Axis direction-pair and Plane-normal Perpendicular residuals;
- Line/Axis non-planar Angle execution through the same capability builder;
- local `AssemblyConstraintGraph` participation;
- Project-level `AssemblyCrossHierarchyConstraintGraph` participation;
- cross-hierarchy motion-connectivity participation;
- existing deterministic relationship ordering;
- shared residual flattening and length scaling;
- shared central finite differences and damped Gauss-Newton solve;
- existing semantic PartDocument freshness and relationship/group freshness;
- existing atomic local and authority-qualified application;
- existing local and cross-hierarchy Jacobian-rank diagnostics.

Not implemented:

- Point/Point or Point/Plane Distance equations;
- Circle/Cylinder/Frame generic relationship pairs beyond existing historical families;
- tangent, symmetric, midpoint, lock, gear, rack, path, cam, contact, or limit relationships;
- joint target compatibility migration;
- a second graph, optimizer, finite-difference implementation, transform authority, or endpoint model;
- any persistence or JSON change.

## Compatibility boundary

The implemented generic relationship matrix is:

```text
Coincident
  Point <-> Point
  Point <-> Line
  Line  <-> Point
  Point <-> Plane
  Plane <-> Point

Parallel
  Line <-> Line
  Axis <-> Axis
  Line <-> Axis
  Axis <-> Line
  Plane <-> Plane

Perpendicular
  Line <-> Line
  Axis <-> Axis
  Line <-> Axis
  Axis <-> Line
  Plane <-> Plane
```

Capability choice remains deterministic and ordered through `AssemblyTargetCompatibilityResolver`.

For multi-capability targets the first supported matrix pair wins exactly as in Block 37. No source-kind-specific special case is added to equation construction.

## Shared target normalization

`AssemblyGenericRelationshipEquationBuilder` consumes typed `AssemblyResolvedGeometricTarget` values.

A component-local target is normalized as:

```text
persistent semantic endpoint
  -> typed component-local descriptor
  -> exact transforms_inner_to_outer
  -> AssemblyHierarchyTransformEvaluator
  -> root-assembly descriptor
```

An already root-space hierarchy target is reused as-is after typed target validation.

The normalized target retains:

```text
exact endpoint identity
source kind
source metadata
capability vector
exact transform chain
```

Only the descriptor coordinate space changes from `ComponentLocal` to `RootAssembly` for local equation evaluation.

No Euler-angle recomposition, OCCT topology identity, or persisted Geometry product is introduced.

## Coincident residuals

### Point to Point

For ordered target points `pA` and `pB`:

```text
r = pB - pA
```

Residual components: 3 length components.

A satisfied relationship has:

```text
pA == pB
```

### Point to Line

For point `p`, line origin `o`, and unit direction `d`:

```text
r = cross(p - o, d)
```

Residual components: 3 length components.

The ordered `Point <-> Line` and `Line <-> Point` pairs use the same geometric distance semantics. `CoincidentPointLineResidualDescriptor::point_target` records whether the point is Target A or Target B so target order remains explicit in derived diagnostics.

### Point to Plane

For point `p`, plane origin `o`, and unit normal `n`:

```text
r = dot(p - o, n)
```

Residual components: 1 length component.

The ordered `Point <-> Plane` and `Plane <-> Point` pairs likewise record which target owns the point.

## Parallel residual

For two selected unit directions `dA` and `dB`:

```text
r = cross(dA, dB)
```

Residual components: 3 dimensionless components.

For Plane targets:

```text
d = plane.normal
```

For Line and Axis targets:

```text
d = selected line/axis direction
```

Parallel and anti-parallel directions are both accepted.

## Perpendicular residual

For two selected unit directions `dA` and `dB`:

```text
r = dot(dA, dB)
```

Residual components: 1 dimensionless component.

Plane targets use normals; Line and Axis targets use directions.

## Directional Angle closure

Block 37 already selected:

```text
Line <-> Line
Axis <-> Axis
Line <-> Axis
Axis <-> Line
```

for Angle, but pre-Block-39 planar equation construction rejected those non-Plane pairs.

Block 39 closes that compatibility/execution gap through the shared generic equation builder:

```text
direction_dot = dot(dA, dB)
angle_alignment = direction_dot - cos(target_angle_deg)
```

Residual components: 1 dimensionless component.

Existing Plane/Plane `PlanarAngleResidualDescriptor` remains a public compatible direct-equation contract. Local numeric execution may use the equivalent shared directional formulation; cross-hierarchy Plane/Plane direct equation construction retains the historical planar descriptor.

## Residual flattening and scaling

The shared numeric system now recognizes:

```text
Coincident Point/Point  -> 3 scaled length rows
Coincident Point/Line   -> 3 scaled length rows
Coincident Point/Plane  -> 1 scaled length row
Parallel                -> 3 dimensionless rows
Perpendicular           -> 1 dimensionless row
Directional Angle       -> 1 dimensionless row
```

Length components divide by the existing:

```text
length_residual_scale_mm
```

Direction/dot/angle components are already dimensionless and are appended unchanged.

All relationship blocks remain concatenated in existing deterministic graph order.

## Graph and authority integration

The shared graph participation authority now classifies all eight relationship families as equation-enabled:

```text
Mate
Concentric
Distance
Insert
Angle
Coincident
Parallel
Perpendicular
```

Therefore generic families enter:

```text
AssemblyConstraintGraph
AssemblyCrossHierarchyConstraintGraph
AssemblyCrossHierarchyMotionGraph
```

No graph type changes.

Local solving still derives one six-variable block per free active local component in one exact connected group.

Cross-hierarchy solving still derives one six-variable block per unique free active:

```text
ComponentTransformAuthority =
  (assembly_document,
   local ComponentInstanceId)
```

Repeated rooted child occurrences that resolve to the same authority still share one direct transform variable block and at most one proposal.

## Shared numeric execution

Block 39 reuses unchanged:

```text
absolute candidate transform vectors
central finite differences
shared residual evaluator contract
damped Gauss-Newton normal equations
partial-pivot dense solve
damping escalation
backtracking line search
solve-state classification
```

No generic-target-specific optimizer exists.

The local numeric adapter routes Coincident, Parallel, Perpendicular, and Angle through `AssemblyGenericRelationshipEquationBuilder`.

Project-level cross-hierarchy generic relationships are resolved through exact occurrence paths into root space, built by the same generic equation semantics, and flattened through the same residual implementation.

## Freshness and atomic application

No new freshness model is introduced.

Participating generic relationships automatically enter existing relationship snapshots because complete records already store:

```text
id
name
type
target A
target B
state
optional Distance value
optional Angle value
```

Generic relationship types carry neither scalar value.

Existing solve results already snapshot every participating transform authority and the canonical serialized PartDocument intent for every participating referenced part.

Therefore edits to semantic target-producing part model intent stale old generic solve results through the same exact byte-for-byte PartDocument snapshot contract.

Local application remains copy-then-replace after complete freshness validation.

Cross-hierarchy application remains:

```text
validate Project structure
validate authority/proposal freshness
validate relationship snapshots
validate exact current solve group
validate hierarchy boundaries
validate semantic PartDocument snapshots
copy Project
apply direct authority transform proposals
replace source Project
```

No component or authority transform is mutated during equation construction or solving.

## Diagnostics

Local and cross-hierarchy diagnostics continue to build the actual central-difference Jacobian at the converged solution and call:

```text
compute_assembly_matrix_rank
```

No relationship-family DOF table is added.

Example: one Point/Point Coincident relationship against one free rigid component yields three residual rows and, for a regular configuration, Jacobian rank 3. With six transform variables the diagnostics report three constrained DOF and three remaining local DOF because that is the measured Jacobian rank, not because Coincident is assigned a hard-coded DOF count.

## Focused acceptance coverage

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-diagnostics]"
```

`tests/geometry/assembly_generic_relationship_equation_tests.cpp` proves:

- Point/Point Coincident residual semantics;
- ordered Point/Line and Line/Point Coincident semantics;
- ordered Point/Plane and Plane/Point Coincident semantics;
- Parallel Line/Axis and Plane pairs, including anti-parallel acceptance;
- Perpendicular Line/Axis and Plane pairs;
- Line/Line Angle through Block-37 compatibility;
- local generic relationship graph participation and solve convergence;
- local source immutability and atomic stale-result rejection;
- cross-hierarchy generic relationship graph participation and solve convergence;
- authority-qualified application;
- repeated occurrence endpoints sharing one child transform authority;
- semantic PartDocument edits staling old generic solve results;
- local and cross-hierarchy diagnostics using measured Jacobian rank.

The complete existing Geometry suite remains the regression proof for historical Mate, Distance, Angle, Concentric, Insert, DatumPlane-to-generated-face Mate, and the unified Axis semantics used by DatumAxis, cylindrical-face, circular-edge, and current `.axis` Concentric targets.

## Handoff

The next technical step is Block 40: joint target compatibility and the oriented Frame contract.

Block 40 must migrate joint target compatibility onto typed target capabilities while preserving current Revolute `Frame <-> Frame` execution. Axis alone remains insufficient for deterministic signed twist because it has no reference X direction.

Block 40 must not change joint persistence, introduce a new joint family, or synthesize a Frame from Axis using an arbitrary world axis.
''',
)

# ---------------------------------------------------------------------------
# Status and handoff documentation.
# ---------------------------------------------------------------------------
replace_exact(
    "docs/mvp-plan.md",
    '''implemented_through: Block 38
current_block: 39
current_boundary: Generic relationship equations and shared solve integration
current_tag: "(assigned when Block 39 starts)"''',
    '''implemented_through: Block 39
current_block: 40
current_boundary: Joint target compatibility and oriented Frame contract
current_tag: "(assigned when Block 40 starts)"''',
)
replace_exact(
    "docs/mvp-plan.md",
    '  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–38 implemented, Blocks 39–47 planned"',
    '  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–39 implemented, Blocks 40–47 planned"',
)
replace_exact(
    "docs/mvp-plan.md",
    '''## Blocks 39–47 — Planned general relationship and joint continuation

**Status:** Planned
**Canonical:** roadmap `docs/assembly-general-geometric-target-roadmap.md`

Mandatory order:

```text
39 generic relationship equations + shared solve integration
40 joint target compatibility + oriented Frame contract
41 general joint coordinate/limit Core model
42 general joint coordinate JSON/backward compatibility
43 vector joint drives + holding/freshness/atomic application
44 Prismatic joint
45 Cylindrical joint
46 Planar joint
47 Ball/Spherical joint
```

**Notes:** Do not merge these blocks.
''',
    '''### Block 39 — Generic relationship equations and shared solve integration

**Status:** Implemented
**Canonical:** `docs/assembly-generic-relationship-equations-mvp5.md`

Implemented capability-driven Coincident, Parallel, and Perpendicular equations for the documented Point/Line/Axis/Plane matrix. The same builder closes non-planar Line/Axis Angle execution selected by Block 37.

The new families now participate in existing local, cross-hierarchy solve, and cross-hierarchy motion connectivity. Residuals use existing scaling, central finite differences, Gauss-Newton solving, semantic PartDocument freshness, atomic application, and measured Jacobian-rank diagnostics.

No new graph, transform authority, optimizer, finite-difference engine, endpoint model, persistence field, or JSON spelling was introduced.

**Focused tags:**

```text
[geometry][assembly-generic-relationships]
[geometry][assembly-generic-relationships-solver]
[geometry][assembly-generic-relationships-cross-hierarchy]
[geometry][assembly-generic-relationships-diagnostics]
```

## Blocks 40–47 — Planned joint continuation

**Status:** Planned
**Canonical:** roadmap `docs/assembly-general-geometric-target-roadmap.md`

Mandatory order:

```text
40 joint target compatibility + oriented Frame contract
41 general joint coordinate/limit Core model
42 general joint coordinate JSON/backward compatibility
43 vector joint drives + holding/freshness/atomic application
44 Prismatic joint
45 Cylindrical joint
46 Planar joint
47 Ball/Spherical joint
```

**Notes:** Do not merge these blocks.
''',
)

replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "Status: Blocks 31–38 are implemented. Block 39 is the current next technical step. Blocks 39–47 remain planned headless architecture.",
    "Status: Blocks 31–39 are implemented. Block 40 is the current next technical step. Blocks 40–47 remain planned headless architecture.",
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "- `docs/assembly-generic-relationship-intent-mvp5.md`",
    "- `docs/assembly-generic-relationship-intent-mvp5.md`\n- `docs/assembly-generic-relationship-equations-mvp5.md`",
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "Blocks 32 through 38 are implemented. The remaining order is unchanged.",
    "Blocks 32 through 39 are implemented. The remaining order is unchanged.",
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "  -> 38 generic geometric relationship Core intent + JSON (implemented)\n  -> 39 generic relationship equations + shared solve integration\n  -> 40 joint target compatibility + oriented Frame contract",
    "  -> 38 generic geometric relationship Core intent + JSON (implemented)\n  -> 39 generic relationship equations + shared solve integration (implemented)\n  -> 40 joint target compatibility + oriented Frame contract",
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    '''## Block 39 — Generic relationship equations and shared solve integration

Primary boundary: Geometry equation semantics plus existing numeric/diagnostic execution.

Initial Coincident pairs:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Initial residual seeds:

```text
Point-Point
  pB - pA

Point-Line
  cross(point - line.origin, line.direction)

Point-Plane
  dot(point - plane.origin, plane.normal)
```

Parallel and Perpendicular initially support Line/Axis direction pairs and Plane normal pairs.

All new families reuse existing local/cross graphs, `ComponentTransformAuthority`, Block-31 projection, Block-37 compatibility selection, shared residual scaling, central finite differences, Gauss-Newton solving, PartDocument freshness, atomic application, and Jacobian-rank diagnostics. No generic-target-specific solver is allowed.
''',
    '''## Block 39 — Generic relationship equations and shared solve integration — Implemented

Canonical document: `docs/assembly-generic-relationship-equations-mvp5.md`.

Primary boundary: Geometry equation semantics plus existing numeric/diagnostic execution.

Implemented Coincident pairs:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Canonical residuals are:

```text
Point-Point
  pB - pA

Point-Line
  cross(point - line.origin, line.direction)

Point-Plane
  dot(point - plane.origin, plane.normal)
```

Parallel and Perpendicular support Line/Axis direction pairs and Plane normal pairs. Parallel uses `cross(dA, dB)` and accepts parallel or anti-parallel directions; Perpendicular uses `dot(dA, dB)`.

The shared capability-driven equation builder also closes Line/Axis Angle execution selected by Block 37 through `dot(dA, dB) - cos(target_angle_deg)`.

Coincident/Parallel/Perpendicular now participate in existing local/cross solve and motion connectivity. All new equations reuse existing `ComponentTransformAuthority`, target projection, deterministic relationship order, residual scaling, central finite differences, Gauss-Newton solving, PartDocument freshness, atomic application, and Jacobian-rank diagnostics.

Focused tags:

```text
[geometry][assembly-generic-relationships]
[geometry][assembly-generic-relationships-solver]
[geometry][assembly-generic-relationships-cross-hierarchy]
[geometry][assembly-generic-relationships-diagnostics]
```

No generic-target-specific solver, graph, transform authority, finite-difference implementation, endpoint model, persistence field, or JSON change was added.
''',
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "Block 39 is the current next technical step.\n\nImplement Block 39 only: generic relationship equations and shared solve integration for `Coincident`, `Parallel`, and `Perpendicular` through Block-37 capability compatibility, existing local/cross-hierarchy graphs, the shared residual/Jacobian engine, freshness, atomic application, and diagnostics.\n\nDo not introduce a second graph, optimizer, finite-difference implementation, transform authority, or endpoint model in Block 39.",
    "Block 40 is the current next technical step.\n\nImplement Block 40 only: migrate joint target compatibility to typed capability semantics and freeze the oriented `Frame` contract while preserving current Revolute `Frame <-> Frame` execution.\n\nDo not change joint persistence, add a joint family, or synthesize a Frame from Axis using an arbitrary world axis in Block 40.",
)

replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "Status: Blocks 23–38 are implemented. Block 39 is the current next technical step. Blocks 39–47 remain the explicit general assembly relationship and richer-joint continuation.",
    "Status: Blocks 23–39 are implemented. Block 40 is the current next technical step. Blocks 40–47 remain the explicit richer-joint continuation.",
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "- `docs/assembly-generic-relationship-intent-mvp5.md`\n- `docs/file-format.md`",
    "- `docs/assembly-generic-relationship-intent-mvp5.md`\n- `docs/assembly-generic-relationship-equations-mvp5.md`\n- `docs/file-format.md`",
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "38 generic geometric relationship Core intent + JSON (implemented)\n39 generic relationship equations + shared solve integration\n40 joint target compatibility + oriented Frame contract",
    "38 generic geometric relationship Core intent + JSON (implemented)\n39 generic relationship equations + shared solve integration (implemented)\n40 joint target compatibility + oriented Frame contract",
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    '''## Blocks 39–40 — Generic relationship execution and joint target semantics

Block 39 adds Coincident/Parallel/Perpendicular equations through existing graph, authority, residual/Jacobian, freshness, atomic application, and diagnostics paths.

Block 40 migrates joint target compatibility to capability semantics and freezes oriented `Frame` requirements. Current Revolute remains `Frame <-> Frame`; Axis alone is insufficient for signed twist.
''',
    '''## Block 39 — Generic relationship equations and shared solve integration — Implemented

Canonical: `docs/assembly-generic-relationship-equations-mvp5.md`.

Coincident, Parallel, and Perpendicular now consume Block-37 capability compatibility and produce shared residual descriptors over Point/Line/Axis/Plane projections. Local and cross-hierarchy numeric paths flatten those residuals through the existing scale rules and reuse the single central-difference/Gauss-Newton engine.

The three Block-38 families now participate in existing solve/motion graphs. Repeated occurrence authority identity, semantic PartDocument freshness, relationship/group freshness, atomic application, and actual Jacobian-rank diagnostics remain unchanged and apply to the new families automatically.

Block 39 also executes non-planar Line/Axis Angle compatibility selected in Block 37 through the same direction-dot equation semantics.

Focused tags:

```text
[geometry][assembly-generic-relationships]
[geometry][assembly-generic-relationships-solver]
[geometry][assembly-generic-relationships-cross-hierarchy]
[geometry][assembly-generic-relationships-diagnostics]
```

## Block 40 — Joint target compatibility and oriented Frame contract

Block 40 migrates joint target compatibility to capability semantics and freezes oriented `Frame` requirements. Current Revolute remains `Frame <-> Frame`; Axis alone is insufficient for signed twist.
''',
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "Implement Block 39 only: generic relationship equations and shared solve integration for `Coincident`, `Parallel`, and `Perpendicular`.\n\nReuse Block-37 compatibility, existing local/cross-hierarchy graphs and authority mapping, the shared residual/Jacobian engine, freshness, atomic application, and diagnostics. Do not introduce parallel execution infrastructure.",
    "Implement Block 40 only: joint target compatibility and the oriented `Frame` contract.\n\nPreserve current Revolute `Frame <-> Frame` execution, migrate compatibility selection onto typed target capabilities, and do not change joint persistence or add a new joint family.",
)

replace_exact(
    "docs/assembly-generic-relationship-intent-mvp5.md",
    "Status: implemented as Block 38 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.",
    "Status: implemented as Block 38 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`. Block 39 equation/solve integration is implemented in `docs/assembly-generic-relationship-equations-mvp5.md`.",
)
replace_exact(
    "docs/assembly-generic-relationship-intent-mvp5.md",
    '''## Handoff

The next technical step is Block 39: generic relationship equations and shared solve integration.

Block 39 must extend Block-37 compatibility and Geometry execution for the documented initial capability pairs, then integrate `Coincident`, `Parallel`, and `Perpendicular` through the existing local/cross-hierarchy graphs, residual/Jacobian engine, freshness, atomic application, and diagnostics boundaries.

Block 39 must not create a second graph, optimizer, finite-difference implementation, transform authority, or endpoint model.
''',
    '''## Implemented Block-39 continuation

Block 39 is implemented in `docs/assembly-generic-relationship-equations-mvp5.md`.

The three persistent families now have capability-driven equations and enter existing local/cross-hierarchy solve and motion graphs. Shared residual scaling, central finite differences, Gauss-Newton solving, freshness, atomic application, and Jacobian-rank diagnostics are reused without parallel infrastructure.

The next technical step is Block 40: joint target compatibility and the oriented `Frame` contract.
''',
)

replace_exact(
    "docs/assembly-geometric-target-taxonomy-mvp5.md",
    "Blocks 32–38 have since extended semantic source identity, generated-topology resolution, explicit target compatibility, and generic relationship intent without changing persistent endpoint authority.",
    "Blocks 32–39 have since extended semantic source identity, generated-topology resolution, explicit target compatibility, generic relationship intent, and shared generic equations without changing persistent endpoint authority.",
)
replace_exact(
    "docs/assembly-geometric-target-taxonomy-mvp5.md",
    "Block 38 adds persistent generic relationship intent without extending the compatibility matrix. Their canonical contracts live in `docs/assembly-general-geometric-target-roadmap.md` and `docs/assembly-generic-relationship-intent-mvp5.md`.",
    "Block 38 adds persistent generic relationship intent. Block 39 extends capability compatibility and shared equation execution for the documented Point/Line/Axis/Plane generic relationship matrix. Their canonical contracts live in `docs/assembly-general-geometric-target-roadmap.md`, `docs/assembly-generic-relationship-intent-mvp5.md`, and `docs/assembly-generic-relationship-equations-mvp5.md`.",
)
replace_exact(
    "docs/assembly-geometric-target-taxonomy-mvp5.md",
    '''## Block 38 — generic relationship intent — Implemented

Block 38 adds persistent `Coincident`, `Parallel`, and `Perpendicular` relationship types and JSON spellings at Core level. It does not change this target taxonomy or the Block-37 compatibility matrix.

`AssemblyTargetCompatibilityResolver` therefore has no compatibility entries for the three generic families yet. Direct compatibility/equation requests remain explicitly incompatible until Block 39 integrates their documented capability pairs and equations.

Canonical Block-38 contract: `docs/assembly-generic-relationship-intent-mvp5.md`.

## Next technical step — Block 39

The next technical step is Block 39: generic relationship equations and shared solve integration for `Coincident`, `Parallel`, and `Perpendicular`.
''',
    '''## Blocks 38–39 — generic relationship intent and equations — Implemented

Block 38 adds persistent `Coincident`, `Parallel`, and `Perpendicular` relationship types and JSON spellings at Core level.

Block 39 extends `AssemblyTargetCompatibilityResolver` for the documented Coincident Point/Line/Plane pairs and Parallel/Perpendicular Line/Axis/Plane direction pairs. The selected capability pair is projected through this taxonomy and consumed by `AssemblyGenericRelationshipEquationBuilder`.

The same builder closes non-planar Line/Axis Angle execution already selected by Block 37. No source-kind-specific equation table is introduced: source kinds expose capabilities, compatibility selects a pair, and equation semantics consume the selected projection.

Canonical contracts:

- `docs/assembly-generic-relationship-intent-mvp5.md`
- `docs/assembly-generic-relationship-equations-mvp5.md`

## Next technical step — Block 40

The next technical step is Block 40: joint target compatibility and the oriented `Frame` contract.
''',
)

replace_exact(
    "docs/assembly-generated-topology-reference-mvp5.md",
    "Block 37 explicit target compatibility is implemented. Block 38 generic relationship Core intent and JSON is also implemented in `docs/assembly-generic-relationship-intent-mvp5.md`; neither block adds a generated-topology JSON field.\n\nThe next technical step is Block 39: generic relationship equations and shared solve integration.",
    "Block 37 explicit target compatibility, Block 38 generic relationship intent/JSON, and Block 39 generic relationship equations/shared solve integration are implemented. Block 39 consumes generated target capabilities through the same projection contract and adds no generated-topology JSON field.\n\nThe next technical step is Block 40: joint target compatibility and the oriented `Frame` contract.",
)

replace_exact(
    "docs/architecture-summary.md",
    "Current equation-enabled relationship families remain the historical five: Mate, Distance, Angle, Concentric, and Insert. Block-38 Coincident/Parallel/Perpendicular intent remains persistent-only until Block 39.",
    "All eight persistent relationship families are equation-enabled: Mate, Distance, Angle, Concentric, Insert, Coincident, Parallel, and Perpendicular. Block 39 adds capability-driven generic equations and closes Line/Axis Angle execution selected by Block 37.",
)
replace_exact(
    "docs/architecture-summary.md",
    "Current local and cross-hierarchy solve/motion graphs derive deterministic relationship incidence over `ComponentTransformAuthority` values for equation-enabled relationship families. Block-38 generic relationship intent is explicitly excluded from graph participation until Block 39 adds equations and shared integration.",
    "Local and cross-hierarchy solve/motion graphs derive deterministic relationship incidence over `ComponentTransformAuthority` values for all equation-enabled relationship families, including Block-39 Coincident/Parallel/Perpendicular relationships.",
)
replace_exact(
    "docs/architecture-summary.md",
    "Block 38 extends shared Core relationship intent and existing local/Project JSON type spellings with `Coincident`, `Parallel`, and `Perpendicular`. They reuse endpoint/order/state/id-scope semantics, carry no scalar quantity, resolve no Geometry during Core construction/loading, and remain outside current solve/motion graphs until Block 39.\n",
    "Block 38 extends shared Core relationship intent and existing local/Project JSON type spellings with `Coincident`, `Parallel`, and `Perpendicular`. They reuse endpoint/order/state/id-scope semantics and carry no scalar quantity.\n\nBlock 39 adds capability-driven Coincident/Parallel/Perpendicular equations, enables their existing graph participation, reuses shared residual scaling/finite differences/Gauss-Newton/freshness/application/rank diagnostics, and closes Line/Axis Angle execution selected by Block 37.\n",
)
replace_exact(
    "docs/architecture-summary.md",
    "Blocks 23–38 of the current assembly sequence are implemented.",
    "Blocks 23–39 of the current assembly sequence are implemented.",
)
replace_exact(
    "docs/architecture-summary.md",
    "- `docs/assembly-generic-relationship-intent-mvp5.md`\n- `docs/assembly-general-geometric-target-roadmap.md`",
    "- `docs/assembly-generic-relationship-intent-mvp5.md`\n- `docs/assembly-generic-relationship-equations-mvp5.md`\n- `docs/assembly-general-geometric-target-roadmap.md`",
)
replace_exact(
    "docs/architecture-summary.md",
    "The next technical step is Block 39 only: generic relationship equations and shared solve integration for `Coincident`, `Parallel`, and `Perpendicular`.\n\nBlock 38 persistent intent/JSON is implemented. Generic relationship equations and graph participation, occurrence-local child pose overrides, whole-subassembly solve variables, general physics, and richer joints remain deferred according to their roadmap blocks.",
    "The next technical step is Block 40 only: joint target compatibility and the oriented `Frame` contract.\n\nBlock 39 generic relationship equations and shared solve integration are implemented. Occurrence-local child pose overrides, whole-subassembly solve variables, general physics, and richer joints remain deferred according to their roadmap blocks.",
)

replace_exact(
    "docs/assembly-cross-hierarchy-numeric-solver-mvp5.md",
    "- shared Mate, Distance, Angle, Concentric, and Insert scalar flattening;",
    "- shared Mate, Distance, Angle, Concentric, Insert, Coincident, Parallel, and Perpendicular scalar flattening after Block 39;",
)
replace_exact(
    "docs/assembly-cross-hierarchy-numeric-solver-mvp5.md",
    '''Canonical scalar counts remain:

```text
Mate        4
Distance    4
Angle       1
Concentric  6
Insert      7
```
''',
    '''Canonical historical scalar counts remain:

```text
Mate        4
Distance    4
Angle       1
Concentric  6
Insert      7
```

Block 39 adds capability-dependent generic counts:

```text
Coincident Point/Point  3
Coincident Point/Line   3
Coincident Point/Plane  1
Parallel                 3
Perpendicular            1
Directional Angle        1
```
''',
)
replace_exact(
    "docs/assembly-cross-hierarchy-numeric-solver-mvp5.md",
    "The suite proves all five residual families, root-to-child convergence, nested exact parent chains, repeated child occurrences sharing one variable block/proposal, same-authority endpoints through different parent chains, mixed local/cross residuals, canonical relationship order, insertion-order independence, grounded context, no-grounded failure, solve-start group freshness, and source/boundary immutability.",
    "The historical suite proves the original five residual families, root-to-child convergence, nested exact parent chains, repeated child occurrences sharing one variable block/proposal, same-authority endpoints through different parent chains, mixed local/cross residuals, canonical relationship order, insertion-order independence, grounded context, no-grounded failure, solve-start group freshness, and source/boundary immutability. Block-39 focused coverage extends the same numeric path to Coincident, Parallel, Perpendicular, and directional Angle without changing the solver engine.",
)
replace_exact(
    "docs/assembly-cross-hierarchy-numeric-solver-mvp5.md",
    "## Implemented follow-up",
    "## Block-39 generic relationship follow-up\n\n`docs/assembly-generic-relationship-equations-mvp5.md` is canonical for the Block-39 capability pairs, residual formulas, graph participation, and focused proofs. The shared numeric engine in this document remains authoritative and unchanged in optimizer semantics.\n\n## Implemented follow-up",
)

replace_exact(
    "docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md",
    "This document is canonical for modeled-input freshness, atomic authority-qualified geometric solve-result application, and cross-hierarchy Jacobian-rank/remaining-DOF diagnostics.",
    "This document is canonical for modeled-input freshness, atomic authority-qualified geometric solve-result application, and cross-hierarchy Jacobian-rank/remaining-DOF diagnostics. Block 39 reuses these boundaries unchanged for Coincident, Parallel, and Perpendicular solve results.",
)
replace_exact(
    "docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md",
    "Therefore graph participation is solve input.",
    "Therefore graph participation is solve input. Block 39 enables Coincident/Parallel/Perpendicular graph participation, so adding, removing, activating, or deactivating those relationships affects freshness through this same exact-group rule.",
)
replace_exact(
    "docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md",
    "Local and cross-hierarchy diagnostics use:",
    "Local and cross-hierarchy diagnostics, including Block-39 generic relationship diagnostics, use:",
)

replace_exact(
    "docs/file-format.md",
    "Typed Core constructors remain authoritative after JSON parsing. Block 38 adds only the three accepted lowercase type spellings; local and Project-level relationship record shapes, endpoint shapes, target order, and state fields are unchanged. Historical five-family files remain compatible.",
    "Typed Core constructors remain authoritative after JSON parsing. Block 38 adds only the three accepted lowercase type spellings; local and Project-level relationship record shapes, endpoint shapes, target order, and state fields are unchanged. Historical five-family files remain compatible. Block 39 adds equations and solve participation only and makes no persistence, schema, version, or JSON-shape change.",
)
replace_exact(
    "docs/file-format.md",
    "Local numeric solving currently derives equation-enabled geometric relationship ids and optional transient local Revolute drives. Block-38 Coincident/Parallel/Perpendicular records remain persisted but excluded from solve/motion graph participation until Block 39.",
    "Local numeric solving derives equation-enabled geometric relationship ids and optional transient local Revolute drives. Block 39 enables persisted Coincident/Parallel/Perpendicular records in the same solve/motion graph participation path as the historical five relationship families.",
)

replace_exact(
    "docs/development-setup.md",
    './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"\n',
    './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"\n'
    './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships]"\n'
    './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-solver]"\n'
    './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-cross-hierarchy]"\n'
    './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-diagnostics]"\n',
)
replace_exact(
    "docs/development-setup.md",
    "- Block-38 generic relationships excluded from current solve/motion graph participation until Block 39;",
    "- Block-39 Coincident/Parallel/Perpendicular compatibility, residuals, graph participation, local/cross solving, freshness/application, and Jacobian-rank diagnostics;\n- non-planar Line/Axis Angle execution through shared directional equations;",
)
replace_exact(
    "docs/development-setup.md",
    "- `docs/assembly-generic-relationship-intent-mvp5.md`: Block-38 generic relationship intent/JSON contract\n- `docs/assembly-general-geometric-target-roadmap.md`: implemented Blocks 31–38 and planned Blocks 39–47",
    "- `docs/assembly-generic-relationship-intent-mvp5.md`: Block-38 generic relationship intent/JSON contract\n- `docs/assembly-generic-relationship-equations-mvp5.md`: Block-39 generic relationship equation/solve contract\n- `docs/assembly-general-geometric-target-roadmap.md`: implemented Blocks 31–39 and planned Blocks 40–47",
)
replace_exact(
    "docs/development-setup.md",
    '''For Block 38 production/test files:

```bash
clang-format -i \\
  include/blcad/core/assembly_constraint.hpp \\
  src/core/assembly_constraint.cpp \\
  src/core/assembly_constraint_graph.cpp \\
  src/core/assembly_cross_hierarchy_constraint_graph.cpp \\
  src/core/assembly_cross_hierarchy_motion_graph.cpp \\
  src/core/assembly_constraint_graph_participation.hpp \\
  src/core/assembly_document_json.cpp \\
  src/core/project_json.cpp \\
  src/geometry/assembly_target_compatibility.cpp \\
  tests/core/assembly_generic_relationship_tests.cpp
```
''',
    '''For Block 39 production/test files:

```bash
clang-format -i \\
  include/blcad/geometry/assembly_generic_relationship_equation_builder.hpp \\
  include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp \\
  src/core/assembly_constraint_graph_participation.hpp \\
  src/geometry/assembly_generic_relationship_equation_builder.cpp \\
  src/geometry/assembly_target_compatibility.cpp \\
  src/geometry/assembly_constraint_numeric_system.hpp \\
  src/geometry/assembly_constraint_numeric_system.cpp \\
  src/geometry/assembly_hierarchy_constraint_equation_builder.cpp \\
  tests/geometry/assembly_generic_relationship_equation_tests.cpp
```
''',
)
replace_exact(
    "docs/development-setup.md",
    "Blocks 23–38 are implemented.",
    "Blocks 23–39 are implemented.",
)
replace_exact(
    "docs/development-setup.md",
    "Block 36 resolves those identities into typed Geometry descriptors. Block 37 freezes deterministic target compatibility selection for existing relationship types. Block 38 adds persistent local/Project-level Coincident, Parallel, and Perpendicular intent plus lowercase JSON spellings while keeping the three families out of current solve/motion graphs until Block 39.",
    "Block 36 resolves those identities into typed Geometry descriptors. Block 37 freezes deterministic target compatibility selection. Block 38 adds persistent local/Project-level Coincident, Parallel, and Perpendicular intent plus lowercase JSON spellings. Block 39 adds their capability-driven equations, enables shared graph participation and numeric execution, and closes non-planar Line/Axis Angle execution.",
)
replace_exact(
    "docs/development-setup.md",
    '''Focused Blocks 36–38 tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generated-topology-target-resolution]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-target-compatibility]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"
```

The immediate next step is Block 39: generic relationship equations and shared solve integration. Block 38 persistent intent/JSON is implemented without activating the new families in current graphs or equations. Exact sequencing is maintained in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` and detailed target planning in `docs/assembly-general-geometric-target-roadmap.md`.
''',
    '''Focused Blocks 36–39 tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generated-topology-target-resolution]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-target-compatibility]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-diagnostics]"
```

The immediate next step is Block 40: joint target compatibility and the oriented `Frame` contract. Block 39 generic relationship equations and shared solve integration are implemented without new execution infrastructure or persistence. Exact sequencing is maintained in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` and detailed target planning in `docs/assembly-general-geometric-target-roadmap.md`.
''',
)

replace_exact(
    "docs/project-goal.md",
    "Block 36 resolves the supported Block-35 semantic producers into Cylinder/Axis, Line, Circle/Axis/center Point, and Point capabilities, computed analytically from validated model intent for both component-local and exact rooted transform semantics. Block 37 adds deterministic relationship/target compatibility selection for existing relationship types. Block 38 adds persistent local and Project-level Coincident/Parallel/Perpendicular intent plus lowercase JSON spellings while deliberately deferring equations and graph participation. The next authority step is Block 39: generic relationship equations and shared solve integration.",
    "Block 36 resolves the supported Block-35 semantic producers into Cylinder/Axis, Line, Circle/Axis/center Point, and Point capabilities, computed analytically from validated model intent for both component-local and exact rooted transform semantics. Block 37 adds deterministic relationship/target compatibility selection. Block 38 adds persistent local and Project-level Coincident/Parallel/Perpendicular intent plus lowercase JSON spellings. Block 39 adds capability-driven generic equations, shared local/cross-hierarchy graph and solver integration, freshness/application reuse, measured Jacobian-rank diagnostics, and non-planar Line/Axis Angle execution. The next authority step is Block 40: joint target compatibility and the oriented `Frame` contract.",
)
replace_exact(
    "docs/project-goal.md",
    "- generic relationship equations before persistent generic relationship intent and target capability compatibility exist;",
    "- generic relationship or joint equations before persistent intent and target capability compatibility exist;",
)
replace_exact(
    "docs/project-goal.md",
    "`docs/assembly-generic-relationship-intent-mvp5.md` is canonical for Block-38 persistent Coincident/Parallel/Perpendicular intent and JSON.\n\n`docs/assembly-general-geometric-target-roadmap.md` is canonical for implemented Blocks 31–38 and planned Blocks 39–47.",
    "`docs/assembly-generic-relationship-intent-mvp5.md` is canonical for Block-38 persistent Coincident/Parallel/Perpendicular intent and JSON.\n\n`docs/assembly-generic-relationship-equations-mvp5.md` is canonical for Block-39 generic relationship compatibility, equations, shared solve integration, freshness/application reuse, and rank diagnostics.\n\n`docs/assembly-general-geometric-target-roadmap.md` is canonical for implemented Blocks 31–39 and planned Blocks 40–47.",
)

replace_exact(
    "docs/part-construction-sequence-mvp6.md",
    "Current repository work remains on Block 39 of the assembly target/relationship/joint sequence. Blocks 39-47 remain mandatory before this sequence starts.",
    "Current repository work remains on Block 40 of the assembly target/relationship/joint sequence. Blocks 40-47 remain mandatory before this sequence starts.",
)

# The temporary patch machinery is not part of the final branch state.
(ROOT / "scripts/apply_block39.py").unlink()
workflow = ROOT / ".github/workflows/block39-patch.yml"
if workflow.exists():
    workflow.unlink()

print("Block 39 patch applied successfully")
