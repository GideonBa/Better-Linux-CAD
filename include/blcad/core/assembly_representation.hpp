#pragma once

#include "blcad/core/assembly_occurrence_pose.hpp"
#include "blcad/core/project_design_reference.hpp"

namespace blcad {
struct AssemblyPositionIdTag;
struct AssemblyDesignViewIdTag;
using AssemblyPositionId = TypedId<AssemblyPositionIdTag>;
using AssemblyDesignViewId = TypedId<AssemblyDesignViewIdTag>;
inline AssemblyPositionId master_position_id() {
  return AssemblyPositionId("master");
}
inline AssemblyDesignViewId default_design_view_id() {
  return AssemblyDesignViewId("default");
}

struct AssemblyRootComponentPosition {
  ComponentInstanceId component;
  DocumentId part;
  RigidTransform transform;
  friend bool operator==(const AssemblyRootComponentPosition&,
                         const AssemblyRootComponentPosition&) = default;
};
struct AssemblyRootSubassemblyPosition {
  SubassemblyInstanceId subassembly;
  DocumentId assembly_document;
  RigidTransform transform;
  friend bool operator==(const AssemblyRootSubassemblyPosition&,
                         const AssemblyRootSubassemblyPosition&) = default;
};
struct AssemblyPositionCoordinate {
  AssemblyJointId joint;
  AssemblyJointCoordinateRole role;
  Quantity value;
  friend bool operator==(const AssemblyPositionCoordinate&,
                         const AssemblyPositionCoordinate&) = default;
};
// A positional representation stores local root placement and independent
// occurrence pose/joint settings. Shared child definitions are never copied or
// rewritten by recall. Manufacturing/suppression/material and display are absent.
struct AssemblyPosition {
  AssemblyPositionId id;
  std::string name;
  std::vector<AssemblyRootComponentPosition> components;
  std::vector<AssemblyRootSubassemblyPosition> subassemblies;
  std::vector<AssemblyOccurrencePose> poses;
  std::vector<AssemblyPositionCoordinate> joints;
  std::vector<AssemblyPositionCoordinate> cross_joints;
  friend bool operator==(const AssemblyPosition&, const AssemblyPosition&) = default;
};
struct AssemblyComponentDisplay {
  ProjectDesignOccurrence occurrence;
  bool visible = true;
  friend bool operator==(const AssemblyComponentDisplay&,
                         const AssemblyComponentDisplay&) = default;
};
struct AssemblySubassemblyDisplay {
  std::vector<SubassemblyInstanceId> path;
  DocumentId assembly_document;
  bool visible = true;
  friend bool operator==(const AssemblySubassemblyDisplay&,
                         const AssemblySubassemblyDisplay&) = default;
};
struct AssemblyDesignView {
  AssemblyDesignViewId id;
  std::string name;
  std::vector<AssemblyComponentDisplay> components;
  std::vector<AssemblySubassemblyDisplay> subassemblies;
  friend bool operator==(const AssemblyDesignView&, const AssemblyDesignView&) = default;
};
class Project;
[[nodiscard]] Result<AssemblyPosition> capture_assembly_position(const Project&, AssemblyPositionId,
                                                                 std::string);
[[nodiscard]] Result<AssemblyDesignView>
capture_assembly_design_view(const Project&, AssemblyDesignViewId, std::string);
} // namespace blcad
