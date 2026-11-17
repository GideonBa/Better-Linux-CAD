#pragma once

#include "blcad/core/assembly_document.hpp"

#include <vector>

namespace blcad {

// A rooted occurrence owns only local internal transforms. Definition poses
// remain the fallback; no composed world transform becomes authored intent.
enum class AssemblyOccurrenceRigidity { Rigid, Flexible };

struct AssemblyComponentPoseOverride {
  ComponentInstanceId component;
  RigidTransform transform;
  friend bool operator==(const AssemblyComponentPoseOverride&,
                         const AssemblyComponentPoseOverride&) = default;
};

struct AssemblyOccurrenceJointCoordinate {
  AssemblyJointId joint;
  AssemblyJointCoordinateRole role;
  Quantity value;
  friend bool operator==(const AssemblyOccurrenceJointCoordinate&,
                         const AssemblyOccurrenceJointCoordinate&) = default;
};

class AssemblyOccurrencePose {
public:
  [[nodiscard]] static Result<AssemblyOccurrencePose>
  create(std::vector<SubassemblyInstanceId> path, DocumentId assembly_document,
         AssemblyOccurrenceRigidity rigidity,
         std::vector<AssemblyComponentPoseOverride> components = {},
         std::vector<AssemblyOccurrenceJointCoordinate> joints = {});
  [[nodiscard]] const std::vector<SubassemblyInstanceId>& path() const noexcept {
    return path_;
  }
  [[nodiscard]] const DocumentId& assembly_document() const noexcept {
    return assembly_document_;
  }
  [[nodiscard]] AssemblyOccurrenceRigidity rigidity() const noexcept {
    return rigidity_;
  }
  [[nodiscard]] const std::vector<AssemblyComponentPoseOverride>& components() const noexcept {
    return components_;
  }
  [[nodiscard]] const std::vector<AssemblyOccurrenceJointCoordinate>& joints() const noexcept {
    return joints_;
  }
  friend bool operator==(const AssemblyOccurrencePose&, const AssemblyOccurrencePose&) = default;

private:
  AssemblyOccurrencePose(std::vector<SubassemblyInstanceId> path, DocumentId assembly_document,
                         AssemblyOccurrenceRigidity rigidity,
                         std::vector<AssemblyComponentPoseOverride> components,
                         std::vector<AssemblyOccurrenceJointCoordinate> joints);
  std::vector<SubassemblyInstanceId> path_;
  DocumentId assembly_document_;
  AssemblyOccurrenceRigidity rigidity_;
  std::vector<AssemblyComponentPoseOverride> components_;
  std::vector<AssemblyOccurrenceJointCoordinate> joints_;
};

class Project;
// Transform scopes used by cross-hierarchy graphs. Empty means the historical
// shared definition authority; flexible paths each have their own authority.
[[nodiscard]] std::vector<std::vector<SubassemblyInstanceId>>
assembly_transform_scopes(const Project&, const DocumentId&);

} // namespace blcad
