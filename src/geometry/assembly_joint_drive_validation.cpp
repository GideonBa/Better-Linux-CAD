#include "assembly_joint_drive_validation.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] Error drive_error(const AssemblyJointId& joint, std::string message) {
  return Error::validation(joint.empty() ? std::string("assembly_joint") : joint.value(),
                           std::move(message));
}

template <typename Joint>
[[nodiscard]] Result<ResolvedAssemblyJointDrive> resolve(const Joint& joint,
                                                         const AssemblyJointDrive& drive) {
  if (drive.joint != joint.id()) {
    return Result<ResolvedAssemblyJointDrive>::failure(
        drive_error(drive.joint, "assembly joint drive identity must match the selected joint"));
  }
  if (drive.requested_coordinates.empty()) {
    return Result<ResolvedAssemblyJointDrive>::failure(
        drive_error(drive.joint, "assembly joint drive must request at least one coordinate role"));
  }

  for (std::size_t index = 0U; index < drive.requested_coordinates.size(); ++index) {
    const auto& requested = drive.requested_coordinates[index];
    if (std::find_if(drive.requested_coordinates.begin(),
                     drive.requested_coordinates.begin() + static_cast<std::ptrdiff_t>(index),
                     [&](const auto& candidate) { return candidate.role == requested.role; }) !=
        drive.requested_coordinates.begin() + static_cast<std::ptrdiff_t>(index)) {
      return Result<ResolvedAssemblyJointDrive>::failure(
          drive_error(drive.joint, "assembly joint drive contains a duplicate coordinate role"));
    }
    if (joint.find_coordinate_slot(requested.role) == nullptr) {
      return Result<ResolvedAssemblyJointDrive>::failure(drive_error(
          drive.joint, "assembly joint drive role is not defined by the selected joint family"));
    }
    auto validated = joint.with_coordinate_value(requested.role, requested.requested_value);
    if (validated.has_error())
      return Result<ResolvedAssemblyJointDrive>::failure(validated.error());
  }

  ResolvedAssemblyJointDrive resolved;
  resolved.complete_coordinates.reserve(joint.coordinate_slots().size());
  resolved.requested_coordinates.reserve(drive.requested_coordinates.size());
  for (const auto& slot : joint.coordinate_slots()) {
    const auto requested =
        std::find_if(drive.requested_coordinates.begin(), drive.requested_coordinates.end(),
                     [&](const auto& candidate) { return candidate.role == slot.role(); });
    const Quantity value =
        requested == drive.requested_coordinates.end() ? slot.value() : requested->requested_value;
    resolved.complete_coordinates.push_back({slot.role(), value});
    if (requested != drive.requested_coordinates.end())
      resolved.requested_coordinates.push_back({slot.role(), value});
  }
  return Result<ResolvedAssemblyJointDrive>::success(std::move(resolved));
}

} // namespace

Result<ResolvedAssemblyJointDrive> resolve_joint_drive(const AssemblyJoint& joint,
                                                       const AssemblyJointDrive& drive) {
  return resolve(joint, drive);
}

Result<ResolvedAssemblyJointDrive> resolve_joint_drive(const AssemblyHierarchyJoint& joint,
                                                       const AssemblyJointDrive& drive) {
  return resolve(joint, drive);
}

Result<double>
revolute_rotation_degrees(const std::vector<AssemblyJointCoordinateDrive>& complete_coordinates,
                          const AssemblyJointId& joint) {
  const auto rotation = std::find_if(
      complete_coordinates.begin(), complete_coordinates.end(), [](const auto& coordinate) {
        return coordinate.role == AssemblyJointCoordinateRole::Rotation;
      });
  if (rotation == complete_coordinates.end() ||
      rotation->requested_value.kind() != QuantityKind::AngleDeg) {
    return Result<double>::failure(
        drive_error(joint, "Revolute joint drive requires one angular rotation coordinate"));
  }
  return Result<double>::success(rotation->requested_value.degrees());
}

} // namespace blcad::geometry::detail
