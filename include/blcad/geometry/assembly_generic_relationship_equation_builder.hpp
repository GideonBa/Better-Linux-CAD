#pragma once

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
    std::variant<CoincidentPointPointResidualDescriptor, CoincidentPointLineResidualDescriptor,
                 CoincidentPointPlaneResidualDescriptor, ParallelDirectionResidualDescriptor,
                 PerpendicularDirectionResidualDescriptor, DirectionalAngleResidualDescriptor>;

struct AssemblyGenericRelationshipResidualDescriptor {
  AssemblyGenericRelationshipResidualValue value;

  friend bool operator==(const AssemblyGenericRelationshipResidualDescriptor&,
                         const AssemblyGenericRelationshipResidualDescriptor&) = default;
};

struct AssemblyGenericRelationshipTargetDescriptor {
  AssemblyResolvedGeometricTarget target;
  AssemblyGeometricTargetCapability selected_capability = AssemblyGeometricTargetCapability::Point;

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
