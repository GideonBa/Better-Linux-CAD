#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class AssemblyJointType { Revolute, Prismatic, Cylindrical, Planar };
[[nodiscard]] std::string_view to_string(AssemblyJointType type) noexcept;

enum class AssemblyJointState { Active, Inactive };
[[nodiscard]] std::string_view to_string(AssemblyJointState state) noexcept;

enum class AssemblyJointCoordinateKind { Angular, Linear };
[[nodiscard]] std::string_view to_string(AssemblyJointCoordinateKind kind) noexcept;

// Stable, family-independent semantic identities. Joint families select an
// ordered subset of these roles instead of assigning meaning by vector index.
enum class AssemblyJointCoordinateRole {
  Rotation,
  Translation,
  TranslationU,
  TranslationV,
  RotationNormal,
};
[[nodiscard]] std::string_view to_string(AssemblyJointCoordinateRole role) noexcept;

class AssemblyJointCoordinateSlot {
public:
  [[nodiscard]] static Result<AssemblyJointCoordinateSlot>
  create(AssemblyJointCoordinateRole role, AssemblyJointCoordinateKind kind, Quantity value,
         std::optional<Quantity> lower_limit = std::nullopt,
         std::optional<Quantity> upper_limit = std::nullopt,
         std::string_view object_id = "assembly_joint_coordinate");

  [[nodiscard]] AssemblyJointCoordinateRole role() const noexcept;
  [[nodiscard]] AssemblyJointCoordinateKind kind() const noexcept;
  [[nodiscard]] const Quantity& value() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& lower_limit() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& upper_limit() const noexcept;

  friend bool operator==(const AssemblyJointCoordinateSlot& lhs,
                         const AssemblyJointCoordinateSlot& rhs) noexcept;

private:
  AssemblyJointCoordinateSlot(AssemblyJointCoordinateRole role, AssemblyJointCoordinateKind kind,
                              Quantity value, std::optional<Quantity> lower_limit,
                              std::optional<Quantity> upper_limit);

  AssemblyJointCoordinateRole role_;
  AssemblyJointCoordinateKind kind_;
  Quantity value_;
  std::optional<Quantity> lower_limit_;
  std::optional<Quantity> upper_limit_;
};

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
         AssemblyJointState state, Quantity lower_limit, Quantity upper_limit, Quantity coordinate);

  [[nodiscard]] static Result<AssemblyJoint>
  create(AssemblyJointId id, std::string name, AssemblyJointType type,
         AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
         AssemblyJointState state, std::vector<AssemblyJointCoordinateSlot> coordinate_slots);

  [[nodiscard]] Result<AssemblyJoint> with_coordinate(Quantity coordinate) const;
  [[nodiscard]] Result<AssemblyJoint> with_coordinate_value(AssemblyJointCoordinateRole role,
                                                            Quantity coordinate) const;

  [[nodiscard]] const AssemblyJointId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] AssemblyJointType type() const noexcept;
  [[nodiscard]] const AssemblyConstraintTarget& target_a() const noexcept;
  [[nodiscard]] const AssemblyConstraintTarget& target_b() const noexcept;
  [[nodiscard]] AssemblyJointState state() const noexcept;
  [[nodiscard]] const std::vector<AssemblyJointCoordinateSlot>& coordinate_slots() const noexcept;
  [[nodiscard]] const AssemblyJointCoordinateSlot*
  find_coordinate_slot(AssemblyJointCoordinateRole role) const noexcept;
  // Legacy Revolute compatibility view over the canonical rotation slot.
  [[nodiscard]] const AssemblyJointLimits& limits() const noexcept;
  [[nodiscard]] double coordinate_deg() const noexcept;

private:
  AssemblyJoint(AssemblyJointId id, std::string name, AssemblyJointType type,
                AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
                AssemblyJointState state, std::vector<AssemblyJointCoordinateSlot> coordinate_slots,
                AssemblyJointLimits legacy_limits);

  AssemblyJointId id_;
  std::string name_;
  AssemblyJointType type_;
  AssemblyConstraintTarget target_a_;
  AssemblyConstraintTarget target_b_;
  AssemblyJointState state_;
  std::vector<AssemblyJointCoordinateSlot> coordinate_slots_;
  AssemblyJointLimits legacy_limits_;
};

// Persistent Project-level motion relationship intent between exact rooted
// assembly occurrences. Target A/B may map to one shared ComponentTransformAuthority
// when their occurrence-qualified endpoint identities differ. Connectivity,
// root-space target geometry, drive residuals, and motion solve state stay derived.
class AssemblyHierarchyJoint {
public:
  [[nodiscard]] static Result<AssemblyHierarchyJoint>
  create(AssemblyJointId id, std::string name, AssemblyJointType type,
         AssemblyHierarchyConstraintEndpoint target_a, AssemblyHierarchyConstraintEndpoint target_b,
         AssemblyJointState state, Quantity lower_limit, Quantity upper_limit, Quantity coordinate);

  [[nodiscard]] static Result<AssemblyHierarchyJoint>
  create(AssemblyJointId id, std::string name, AssemblyJointType type,
         AssemblyHierarchyConstraintEndpoint target_a, AssemblyHierarchyConstraintEndpoint target_b,
         AssemblyJointState state, std::vector<AssemblyJointCoordinateSlot> coordinate_slots);

  [[nodiscard]] Result<AssemblyHierarchyJoint> with_coordinate(Quantity coordinate) const;
  [[nodiscard]] Result<AssemblyHierarchyJoint>
  with_coordinate_value(AssemblyJointCoordinateRole role, Quantity coordinate) const;

  [[nodiscard]] const AssemblyJointId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] AssemblyJointType type() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintEndpoint& target_a() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintEndpoint& target_b() const noexcept;
  [[nodiscard]] AssemblyJointState state() const noexcept;
  [[nodiscard]] const std::vector<AssemblyJointCoordinateSlot>& coordinate_slots() const noexcept;
  [[nodiscard]] const AssemblyJointCoordinateSlot*
  find_coordinate_slot(AssemblyJointCoordinateRole role) const noexcept;
  [[nodiscard]] const AssemblyJointLimits& limits() const noexcept;
  [[nodiscard]] double coordinate_deg() const noexcept;

  friend bool operator==(const AssemblyHierarchyJoint&, const AssemblyHierarchyJoint&) = default;

private:
  AssemblyHierarchyJoint(AssemblyJointId id, std::string name, AssemblyJointType type,
                         AssemblyHierarchyConstraintEndpoint target_a,
                         AssemblyHierarchyConstraintEndpoint target_b, AssemblyJointState state,
                         std::vector<AssemblyJointCoordinateSlot> coordinate_slots,
                         AssemblyJointLimits legacy_limits);

  AssemblyJointId id_;
  std::string name_;
  AssemblyJointType type_;
  AssemblyHierarchyConstraintEndpoint target_a_;
  AssemblyHierarchyConstraintEndpoint target_b_;
  AssemblyJointState state_;
  std::vector<AssemblyJointCoordinateSlot> coordinate_slots_;
  AssemblyJointLimits legacy_limits_;
};

} // namespace blcad
