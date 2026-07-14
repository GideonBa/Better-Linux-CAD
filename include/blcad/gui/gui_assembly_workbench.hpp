#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/geometry/assembly_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_solve_diagnostics.hpp"

#include <optional>
#include <string>

namespace blcad::gui {

enum class GuiAssemblyCommand { InsertPart, InsertSubassembly, Placement, Relationship, Joint, Solve, DriveJoint };

struct GuiAssemblySelectionPrompt {
  std::string text;
  std::optional<GuiSelectionKind> required_kind;
  std::string required_capability;
};

class GuiAssemblyWorkbench {
public:
  [[nodiscard]] static GuiAssemblySelectionPrompt prompt_for(GuiAssemblyCommand);
  [[nodiscard]] Result<std::size_t> add_part(GuiDocumentSession&, PartDocument) const;
  [[nodiscard]] Result<std::size_t> add_child_assembly(GuiDocumentSession&, AssemblyDocument) const;
  [[nodiscard]] Result<std::size_t> add_root_member_part(GuiDocumentSession&, DocumentId) const;
  [[nodiscard]] Result<std::size_t> add_root_component(GuiDocumentSession&, ComponentInstance) const;
  [[nodiscard]] Result<std::size_t> add_root_subassembly(GuiDocumentSession&, SubassemblyInstance) const;
  [[nodiscard]] Result<std::size_t> add_root_parameter(GuiDocumentSession&, Parameter) const;
  [[nodiscard]] Result<std::size_t> add_root_binding(GuiDocumentSession&, ParameterBinding) const;
  [[nodiscard]] Result<std::size_t> set_component_transform(GuiDocumentSession&, ComponentInstanceId,
                                                           RigidTransform) const;
  [[nodiscard]] Result<std::size_t> set_component_visibility(GuiDocumentSession&, ComponentInstanceId,
                                                            ComponentVisibility) const;
  [[nodiscard]] Result<std::size_t> set_component_suppression(GuiDocumentSession&, ComponentInstanceId,
                                                             ComponentSuppressionState) const;
  [[nodiscard]] Result<std::size_t> set_component_grounding(GuiDocumentSession&, ComponentInstanceId,
                                                           ComponentGroundingState) const;

  [[nodiscard]] Result<std::size_t> preview_root_constraint(const GuiDocumentSession&,
                                                            AssemblyConstraint) const;
  [[nodiscard]] Result<std::size_t> apply_root_constraint(GuiDocumentSession&, AssemblyConstraint) const;
  [[nodiscard]] Result<std::size_t> preview_root_joint(const GuiDocumentSession&, AssemblyJoint) const;
  [[nodiscard]] Result<std::size_t> apply_root_joint(GuiDocumentSession&, AssemblyJoint) const;
  [[nodiscard]] Result<std::size_t> apply_cross_constraint(GuiDocumentSession&,
                                                          AssemblyHierarchyConstraint) const;
  [[nodiscard]] Result<std::size_t> apply_cross_joint(GuiDocumentSession&,
                                                     AssemblyHierarchyJoint) const;
  [[nodiscard]] Result<std::size_t> set_root_joint_coordinate(
      GuiDocumentSession&, AssemblyJointId, AssemblyJointCoordinateRole, Quantity) const;

  [[nodiscard]] Result<geometry::AssemblySolveDiagnostics>
  inspect_root_group(const GuiDocumentSession&, std::vector<ComponentInstanceId>) const;
  [[nodiscard]] Result<geometry::AssemblySolveResult>
  solve_root_group(const GuiDocumentSession&, std::vector<ComponentInstanceId>) const;
  [[nodiscard]] Result<std::size_t> apply_solve(GuiDocumentSession&,
                                               const geometry::AssemblySolveResult&) const;
  [[nodiscard]] Result<geometry::AssemblyJointMotionResult>
  preview_joint_motion(const GuiDocumentSession&, AssemblyJointId, Quantity) const;
  [[nodiscard]] Result<std::size_t> apply_joint_motion(
      GuiDocumentSession&, const geometry::AssemblyJointMotionResult&) const;
};

} // namespace blcad::gui
