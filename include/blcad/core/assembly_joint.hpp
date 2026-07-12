#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>

namespace blcad {

enum class AssemblyJointType { Revolute };
[[nodiscard]] std::string_view to_string(AssemblyJointType type) noexcept;

enum class AssemblyJointState { Active, Inactive };
[[nodiscard]] std::string_view to_string(AssemblyJointState state) noexcept;

struct AssemblyJointLimits {
  double lower_deg = 0.0;
  double upper_deg = 0.0;

  friend bool operator==(const AssemblyJointLimits&, const AssemblyJointLimits&) = default;
};

// Persistent solver-independent local motion relationship intent. The first
// family is a revolute joint between two semantic seating endpoints. Limits and
// coordinate are authored angle intent; graph connectivity, residuals,
// Jacobians, and solve state remain derived.
class AssemblyJoint {
public:
  [[nodiscard]] static Result<AssemblyJoint>
  create(AssemblyJointId id, std::string name, AssemblyJointType type,
         AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
         AssemblyJointState state, Quantity lower_limit, Quantity upper_limit,
         Quantity coordinate);

  [[nodiscard]] Result<AssemblyJoint> with_coordinate(Quantity coordinate) const;

  [[nodiscard]] const AssemblyJointId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] AssemblyJointType type() const noexcept;
  [[nodiscard]] const AssemblyConstraintTarget& target_a() const noexcept;
  [[nodiscard]] const AssemblyConstraintTarget& target_b() const noexcept;
  [[nodiscard]] AssemblyJointState state() const noexcept;
  [[nodiscard]] const AssemblyJointLimits& limits() const noexcept;
  [[nodiscard]] double coordinate_deg() const noexcept;

private:
  AssemblyJoint(AssemblyJointId id, std::string name, AssemblyJointType type,
                AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
                AssemblyJointState state, AssemblyJointLimits limits, double coordinate_deg);

  AssemblyJointId id_;
  std::string name_;
  AssemblyJointType type_;
  AssemblyConstraintTarget target_a_;
  AssemblyConstraintTarget target_b_;
  AssemblyJointState state_;
  AssemblyJointLimits limits_;
  double coordinate_deg_ = 0.0;
};

// Persistent Project-level motion relationship intent between exact rooted
// assembly occurrences. Target A/B may map to one shared ComponentTransformAuthority
// when their occurrence-qualified endpoint identities differ. Connectivity,
// root-space target geometry, drive residuals, and motion solve state stay derived.
class AssemblyHierarchyJoint {
public:
  [[nodiscard]] static Result<AssemblyHierarchyJoint>
  create(AssemblyJointId id, std::string name, AssemblyJointType type,
         AssemblyHierarchyConstraintEndpoint target_a,
         AssemblyHierarchyConstraintEndpoint target_b,
         AssemblyJointState state, Quantity lower_limit, Quantity upper_limit,
         Quantity coordinate);

  [[nodiscard]] Result<AssemblyHierarchyJoint> with_coordinate(Quantity coordinate) const;

  [[nodiscard]] const AssemblyJointId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] AssemblyJointType type() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintEndpoint& target_a() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintEndpoint& target_b() const noexcept;
  [[nodiscard]] AssemblyJointState state() const noexcept;
  [[nodiscard]] const AssemblyJointLimits& limits() const noexcept;
  [[nodiscard]] double coordinate_deg() const noexcept;

  friend bool operator==(const AssemblyHierarchyJoint&, const AssemblyHierarchyJoint&) = default;

private:
  AssemblyHierarchyJoint(AssemblyJointId id, std::string name, AssemblyJointType type,
                         AssemblyHierarchyConstraintEndpoint target_a,
                         AssemblyHierarchyConstraintEndpoint target_b,
                         AssemblyJointState state, AssemblyJointLimits limits,
                         double coordinate_deg);

  AssemblyJointId id_;
  std::string name_;
  AssemblyJointType type_;
  AssemblyHierarchyConstraintEndpoint target_a_;
  AssemblyHierarchyConstraintEndpoint target_b_;
  AssemblyJointState state_;
  AssemblyJointLimits limits_;
  double coordinate_deg_ = 0.0;
};

} // namespace blcad
