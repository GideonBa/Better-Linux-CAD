#include "blcad/geometry/assembly_joint_target_compatibility.hpp"

#include <algorithm>
#include <string>

namespace blcad::geometry {
namespace {

constexpr const char* kCompatibilityObjectId = "geometry.assembly_joint_target_compatibility";

[[nodiscard]] bool has_capability(const AssemblyResolvedGeometricTarget& target,
                                  AssemblyGeometricTargetCapability capability) {
  return std::find(target.capabilities.begin(), target.capabilities.end(), capability) !=
         target.capabilities.end();
}

[[nodiscard]] std::string
describe_capabilities(const std::vector<AssemblyGeometricTargetCapability>& capabilities) {
  std::string description = "[";
  for (std::size_t index = 0U; index < capabilities.size(); ++index) {
    if (index != 0U) {
      description += ", ";
    }
    description += to_string(capabilities[index]);
  }
  description += "]";
  return description;
}

} // namespace

Result<AssemblyJointTargetCompatibility> AssemblyJointTargetCompatibilityResolver::resolve(
    AssemblyJointType joint_type, const AssemblyResolvedGeometricTarget& target_a,
    const AssemblyResolvedGeometricTarget& target_b) const {
  auto valid_a = validate_resolved_geometric_target(target_a);
  if (valid_a.has_error()) {
    return Result<AssemblyJointTargetCompatibility>::failure(valid_a.error());
  }
  auto valid_b = validate_resolved_geometric_target(target_b);
  if (valid_b.has_error()) {
    return Result<AssemblyJointTargetCompatibility>::failure(valid_b.error());
  }

  if ((joint_type == AssemblyJointType::Revolute || joint_type == AssemblyJointType::Prismatic ||
       joint_type == AssemblyJointType::Cylindrical || joint_type == AssemblyJointType::Planar) &&
      has_capability(target_a, AssemblyGeometricTargetCapability::Frame) &&
      has_capability(target_b, AssemblyGeometricTargetCapability::Frame)) {
    return Result<AssemblyJointTargetCompatibility>::success(
        AssemblyJointTargetCompatibility{joint_type, AssemblyGeometricTargetCapability::Frame,
                                         AssemblyGeometricTargetCapability::Frame});
  }

  if (joint_type == AssemblyJointType::Spherical &&
      has_capability(target_a, AssemblyGeometricTargetCapability::Point) &&
      has_capability(target_b, AssemblyGeometricTargetCapability::Point)) {
    return Result<AssemblyJointTargetCompatibility>::success(
        AssemblyJointTargetCompatibility{joint_type, AssemblyGeometricTargetCapability::Point,
                                         AssemblyGeometricTargetCapability::Point});
  }

  return Result<AssemblyJointTargetCompatibility>::failure(Error::validation(
      kCompatibilityObjectId,
      "assembly " + std::string(to_string(joint_type)) +
          " joint target capabilities are incompatible: " +
          (joint_type == AssemblyJointType::Spherical ? "Point/Point is required"
                                                      : "oriented Frame/Frame is required; Axis "
                                                        "alone has no deterministic reference X "
                                                        "direction") +
          "; target A exposes " + describe_capabilities(target_a.capabilities) +
          ", target B exposes " + describe_capabilities(target_b.capabilities)));
}

} // namespace blcad::geometry
