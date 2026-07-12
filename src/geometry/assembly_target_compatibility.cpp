#include "blcad/geometry/assembly_target_compatibility.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr const char* kCompatibilityObjectId = "geometry.assembly_target_compatibility";

struct CapabilityPair {
  AssemblyGeometricTargetCapability target_a;
  AssemblyGeometricTargetCapability target_b;
};

[[nodiscard]] Error validation_error(std::string message) {
  return Error::validation(kCompatibilityObjectId, std::move(message));
}

[[nodiscard]] bool has_capability(const AssemblyResolvedGeometricTarget& target,
                                  AssemblyGeometricTargetCapability capability) {
  return std::find(target.capabilities.begin(), target.capabilities.end(), capability) !=
         target.capabilities.end();
}

[[nodiscard]] std::vector<CapabilityPair>
compatibility_matrix(AssemblyConstraintType relationship_type) {
  using Capability = AssemblyGeometricTargetCapability;
  switch (relationship_type) {
  case AssemblyConstraintType::Mate:
    return {{Capability::Plane, Capability::Plane}};
  case AssemblyConstraintType::Distance:
    return {{Capability::Plane, Capability::Plane},
            {Capability::Point, Capability::Point},
            {Capability::Point, Capability::Plane},
            {Capability::Plane, Capability::Point}};
  case AssemblyConstraintType::Angle:
    return {{Capability::Plane, Capability::Plane},
            {Capability::Line, Capability::Line},
            {Capability::Axis, Capability::Axis},
            {Capability::Line, Capability::Axis},
            {Capability::Axis, Capability::Line}};
  case AssemblyConstraintType::Concentric:
    return {{Capability::Axis, Capability::Axis}};
  case AssemblyConstraintType::Insert:
    return {{Capability::Frame, Capability::Frame}};
  case AssemblyConstraintType::Coincident:
  case AssemblyConstraintType::Parallel:
  case AssemblyConstraintType::Perpendicular:
    return {};
  }
  return {};
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

Result<AssemblyTargetCompatibility> AssemblyTargetCompatibilityResolver::resolve(
    AssemblyConstraintType relationship_type, const AssemblyResolvedGeometricTarget& target_a,
    const AssemblyResolvedGeometricTarget& target_b) const {
  auto valid_a = validate_resolved_geometric_target(target_a);
  if (valid_a.has_error()) {
    return Result<AssemblyTargetCompatibility>::failure(valid_a.error());
  }
  auto valid_b = validate_resolved_geometric_target(target_b);
  if (valid_b.has_error()) {
    return Result<AssemblyTargetCompatibility>::failure(valid_b.error());
  }

  for (const CapabilityPair& pair : compatibility_matrix(relationship_type)) {
    if (has_capability(target_a, pair.target_a) && has_capability(target_b, pair.target_b)) {
      return Result<AssemblyTargetCompatibility>::success(
          AssemblyTargetCompatibility{relationship_type, pair.target_a, pair.target_b});
    }
  }

  return Result<AssemblyTargetCompatibility>::failure(
      validation_error(std::string("assembly ") + std::string(to_string(relationship_type)) +
                       " target capabilities are incompatible: target A exposes " +
                       describe_capabilities(target_a.capabilities) + ", target B exposes " +
                       describe_capabilities(target_b.capabilities)));
}

} // namespace blcad::geometry
