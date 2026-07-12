#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/assembly_document.hpp"
#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/recompute_plan.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace blcad {

class ProjectPartUpdate {
public:
  ProjectPartUpdate(DocumentId part_document, BindingApplication binding_application,
                    RecomputePlan recompute_plan);

  [[nodiscard]] const DocumentId& part_document() const noexcept;
  [[nodiscard]] const BindingApplication& binding_application() const noexcept;
  [[nodiscard]] const RecomputePlan& recompute_plan() const noexcept;

private:
  DocumentId part_document_;
  BindingApplication binding_application_;
  RecomputePlan recompute_plan_;
};

class ProjectUpdateResult {
public:
  explicit ProjectUpdateResult(std::vector<ProjectPartUpdate> part_updates);

  [[nodiscard]] const std::vector<ProjectPartUpdate>& part_updates() const noexcept;
  [[nodiscard]] std::size_t updated_part_count() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

private:
  std::vector<ProjectPartUpdate> part_updates_;
};

// Project-level container for one explicit root assembly, project-owned child
// assembly documents, owned part documents, project-level cross-hierarchy
// geometric relationship intent, and project-level occurrence-qualified motion
// joint intent. Hierarchy traversal, target resolution, graph connectivity, and
// numeric solve/motion state stay derived.
class Project {
public:
  [[nodiscard]] static Result<Project> create(DocumentId id, std::string name,
                                               AssemblyDocument assembly);

  [[nodiscard]] Result<std::size_t> add_part_document(PartDocument part_document);
  [[nodiscard]] Result<std::size_t>
  add_child_assembly_document(AssemblyDocument assembly_document);
  [[nodiscard]] Result<std::size_t>
  add_cross_hierarchy_constraint(AssemblyHierarchyConstraint constraint);
  [[nodiscard]] Result<std::size_t> add_cross_hierarchy_joint(AssemblyHierarchyJoint joint);
  [[nodiscard]] Result<std::size_t> validate_member_parts() const;
  [[nodiscard]] Result<std::size_t> validate_component_instances() const;
  [[nodiscard]] Result<std::size_t> validate_assembly_constraints() const;
  [[nodiscard]] Result<std::size_t> validate_cross_hierarchy_constraints() const;
  [[nodiscard]] Result<std::size_t> validate_assembly_joints() const;
  [[nodiscard]] Result<std::size_t> validate_cross_hierarchy_joints() const;
  [[nodiscard]] Result<std::size_t> validate_subassembly_instances() const;
  [[nodiscard]] Result<std::size_t> validate_assembly_hierarchy() const;
  [[nodiscard]] Result<std::size_t> validate_assembly_structure() const;

  // Changes one root-assembly parameter, applies all root assembly bindings to
  // owned member parts, and returns per-part recompute plans for affected parts.
  [[nodiscard]] Result<ProjectUpdateResult> set_assembly_parameter_value(ParameterId id,
                                                                          Quantity value);
  // Explicit authored coordinate update for one Project-level cross-hierarchy
  // joint. Geometry movement remains an application-layer motion operation.
  [[nodiscard]] Result<std::size_t>
  set_cross_hierarchy_joint_coordinate(AssemblyJointId id, Quantity coordinate);

  [[nodiscard]] const DocumentId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const AssemblyDocument& assembly() const noexcept;
  [[nodiscard]] AssemblyDocument& assembly() noexcept;
  [[nodiscard]] const std::vector<AssemblyDocument>& child_assembly_documents() const noexcept;
  [[nodiscard]] std::vector<AssemblyDocument>& child_assembly_documents() noexcept;
  [[nodiscard]] std::size_t child_assembly_document_count() const noexcept;
  [[nodiscard]] const AssemblyDocument* find_assembly_document(DocumentId id) const noexcept;
  [[nodiscard]] AssemblyDocument* find_assembly_document(DocumentId id) noexcept;
  [[nodiscard]] const std::vector<AssemblyHierarchyConstraint>&
  cross_hierarchy_constraints() const noexcept;
  [[nodiscard]] std::vector<AssemblyHierarchyConstraint>& cross_hierarchy_constraints() noexcept;
  [[nodiscard]] std::size_t cross_hierarchy_constraint_count() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraint*
  find_cross_hierarchy_constraint(AssemblyConstraintId id) const noexcept;
  [[nodiscard]] AssemblyHierarchyConstraint*
  find_cross_hierarchy_constraint(AssemblyConstraintId id) noexcept;
  [[nodiscard]] const std::vector<AssemblyHierarchyJoint>& cross_hierarchy_joints() const noexcept;
  [[nodiscard]] std::vector<AssemblyHierarchyJoint>& cross_hierarchy_joints() noexcept;
  [[nodiscard]] std::size_t cross_hierarchy_joint_count() const noexcept;
  [[nodiscard]] const AssemblyHierarchyJoint*
  find_cross_hierarchy_joint(AssemblyJointId id) const noexcept;
  [[nodiscard]] AssemblyHierarchyJoint* find_cross_hierarchy_joint(AssemblyJointId id) noexcept;
  [[nodiscard]] const std::vector<PartDocument>& part_documents() const noexcept;
  [[nodiscard]] std::vector<PartDocument>& part_documents() noexcept;
  [[nodiscard]] std::size_t part_document_count() const noexcept;
  [[nodiscard]] const PartDocument* find_part_document(DocumentId id) const noexcept;
  [[nodiscard]] PartDocument* find_part_document(DocumentId id) noexcept;

private:
  Project(DocumentId id, std::string name, AssemblyDocument assembly);

  [[nodiscard]] bool has_assembly_document_id(const DocumentId& id) const noexcept;
  [[nodiscard]] bool has_part_document_id(const DocumentId& id) const noexcept;

  DocumentId id_;
  std::string name_;
  AssemblyDocument assembly_;
  std::vector<AssemblyDocument> child_assembly_documents_;
  std::vector<AssemblyHierarchyConstraint> cross_hierarchy_constraints_;
  std::vector<AssemblyHierarchyJoint> cross_hierarchy_joints_;
  std::vector<PartDocument> part_documents_;
};

} // namespace blcad
