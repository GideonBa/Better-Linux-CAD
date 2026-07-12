#include "blcad/core/assembly_joint.hpp"

#include <string>
#include <utility>

namespace blcad {
namespace {

struct ValidatedJointAngles {
  AssemblyJointLimits limits;
  double coordinate_deg = 0.0;
};

[[nodiscard]] Result<ValidatedJointAngles> validate_joint_angles(
    const std::string& object_id, Quantity lower_limit, Quantity upper_limit,
    Quantity coordinate) {
  if (lower_limit.kind() != QuantityKind::AngleDeg ||
      upper_limit.kind() != QuantityKind::AngleDeg || coordinate.kind() != QuantityKind::AngleDeg) {
    return Result<ValidatedJointAngles>::failure(Error::validation(
        object_id, "revolute joint limits and coordinate must use angle values in degrees"));
  }

  const double lower_deg = lower_limit.degrees();
  const double upper_deg = upper_limit.degrees();
  const double coordinate_deg = coordinate.degrees();
  if (lower_deg < -180.0 || upper_deg > 180.0) {
    return Result<ValidatedJointAngles>::failure(Error::validation(
        object_id,
        "revolute joint seed limits must stay within the principal -180 to 180 degree range"));
  }
  if (lower_deg >= upper_deg) {
    return Result<ValidatedJointAngles>::failure(
        Error::validation(object_id, "assembly joint lower limit must be less than upper limit"));
  }
  if (coordinate_deg < lower_deg || coordinate_deg > upper_deg) {
    return Result<ValidatedJointAngles>::failure(
        Error::validation(object_id, "assembly joint coordinate must lie within its limits"));
  }

  return Result<ValidatedJointAngles>::success(
      ValidatedJointAngles{AssemblyJointLimits{lower_deg, upper_deg}, coordinate_deg});
}

} // namespace

std::string_view to_string(AssemblyJointType type) noexcept {
  switch (type) {
  case AssemblyJointType::Revolute:
    return "revolute";
  }
  return "revolute";
}

std::string_view to_string(AssemblyJointState state) noexcept {
  switch (state) {
  case AssemblyJointState::Active:
    return "active";
  case AssemblyJointState::Inactive:
    return "inactive";
  }
  return "active";
}

Result<AssemblyJoint>
AssemblyJoint::create(AssemblyJointId id, std::string name, AssemblyJointType type,
                      AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
                      AssemblyJointState state, Quantity lower_limit, Quantity upper_limit,
                      Quantity coordinate) {
  const auto object_id = id.empty() ? std::string("assembly_joint") : id.value();
  if (id.empty()) {
    return Result<AssemblyJoint>::failure(
        Error::validation(object_id, "assembly joint id must not be empty"));
  }
  if (name.empty()) {
    return Result<AssemblyJoint>::failure(
        Error::validation(object_id, "assembly joint name must not be empty"));
  }
  if (target_a.component_instance() == target_b.component_instance()) {
    return Result<AssemblyJoint>::failure(Error::validation(
        object_id, "assembly joint targets must reference distinct component instances"));
  }

  auto angles = validate_joint_angles(object_id, lower_limit, upper_limit, coordinate);
  if (angles.has_error()) {
    return Result<AssemblyJoint>::failure(angles.error());
  }

  return Result<AssemblyJoint>::success(AssemblyJoint(
      std::move(id), std::move(name), type, std::move(target_a), std::move(target_b), state,
      angles.value().limits, angles.value().coordinate_deg));
}

Result<AssemblyJoint> AssemblyJoint::with_coordinate(Quantity coordinate) const {
  auto lower = Quantity::angle_deg(limits_.lower_deg, id_.value());
  if (lower.has_error()) {
    return Result<AssemblyJoint>::failure(lower.error());
  }
  auto upper = Quantity::angle_deg(limits_.upper_deg, id_.value());
  if (upper.has_error()) {
    return Result<AssemblyJoint>::failure(upper.error());
  }
  return create(id_, name_, type_, target_a_, target_b_, state_, lower.value(), upper.value(),
                coordinate);
}

AssemblyJoint::AssemblyJoint(AssemblyJointId id, std::string name, AssemblyJointType type,
                             AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
                             AssemblyJointState state, AssemblyJointLimits limits,
                             double coordinate_deg)
    : id_(std::move(id)), name_(std::move(name)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), state_(state), limits_(limits),
      coordinate_deg_(coordinate_deg) {}

const AssemblyJointId& AssemblyJoint::id() const noexcept { return id_; }
const std::string& AssemblyJoint::name() const noexcept { return name_; }
AssemblyJointType AssemblyJoint::type() const noexcept { return type_; }
const AssemblyConstraintTarget& AssemblyJoint::target_a() const noexcept { return target_a_; }
const AssemblyConstraintTarget& AssemblyJoint::target_b() const noexcept { return target_b_; }
AssemblyJointState AssemblyJoint::state() const noexcept { return state_; }
const AssemblyJointLimits& AssemblyJoint::limits() const noexcept { return limits_; }
double AssemblyJoint::coordinate_deg() const noexcept { return coordinate_deg_; }

Result<AssemblyHierarchyJoint>
AssemblyHierarchyJoint::create(AssemblyJointId id, std::string name, AssemblyJointType type,
                               AssemblyHierarchyConstraintEndpoint target_a,
                               AssemblyHierarchyConstraintEndpoint target_b,
                               AssemblyJointState state, Quantity lower_limit,
                               Quantity upper_limit, Quantity coordinate) {
  const auto object_id = id.empty() ? std::string("assembly_hierarchy_joint") : id.value();
  if (id.empty()) {
    return Result<AssemblyHierarchyJoint>::failure(
        Error::validation(object_id, "cross-hierarchy assembly joint id must not be empty"));
  }
  if (name.empty()) {
    return Result<AssemblyHierarchyJoint>::failure(
        Error::validation(object_id, "cross-hierarchy assembly joint name must not be empty"));
  }
  if (target_a == target_b) {
    return Result<AssemblyHierarchyJoint>::failure(Error::validation(
        object_id, "cross-hierarchy assembly joint targets must be distinct endpoint identities"));
  }

  auto angles = validate_joint_angles(object_id, lower_limit, upper_limit, coordinate);
  if (angles.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(angles.error());
  }

  return Result<AssemblyHierarchyJoint>::success(AssemblyHierarchyJoint(
      std::move(id), std::move(name), type, std::move(target_a), std::move(target_b), state,
      angles.value().limits, angles.value().coordinate_deg));
}

Result<AssemblyHierarchyJoint>
AssemblyHierarchyJoint::with_coordinate(Quantity coordinate) const {
  auto lower = Quantity::angle_deg(limits_.lower_deg, id_.value());
  if (lower.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(lower.error());
  }
  auto upper = Quantity::angle_deg(limits_.upper_deg, id_.value());
  if (upper.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(upper.error());
  }
  return create(id_, name_, type_, target_a_, target_b_, state_, lower.value(), upper.value(),
                coordinate);
}

AssemblyHierarchyJoint::AssemblyHierarchyJoint(
    AssemblyJointId id, std::string name, AssemblyJointType type,
    AssemblyHierarchyConstraintEndpoint target_a, AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyJointState state, AssemblyJointLimits limits, double coordinate_deg)
    : id_(std::move(id)), name_(std::move(name)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), state_(state), limits_(limits),
      coordinate_deg_(coordinate_deg) {}

const AssemblyJointId& AssemblyHierarchyJoint::id() const noexcept { return id_; }
const std::string& AssemblyHierarchyJoint::name() const noexcept { return name_; }
AssemblyJointType AssemblyHierarchyJoint::type() const noexcept { return type_; }
const AssemblyHierarchyConstraintEndpoint& AssemblyHierarchyJoint::target_a() const noexcept {
  return target_a_;
}
const AssemblyHierarchyConstraintEndpoint& AssemblyHierarchyJoint::target_b() const noexcept {
  return target_b_;
}
AssemblyJointState AssemblyHierarchyJoint::state() const noexcept { return state_; }
const AssemblyJointLimits& AssemblyHierarchyJoint::limits() const noexcept { return limits_; }
double AssemblyHierarchyJoint::coordinate_deg() const noexcept { return coordinate_deg_; }

} // namespace blcad
