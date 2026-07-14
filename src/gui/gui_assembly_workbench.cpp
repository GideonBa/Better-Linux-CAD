#include "blcad/gui/gui_assembly_workbench.hpp"

#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <utility>

namespace blcad::gui {
namespace {
template <typename Mutation>
Result<std::size_t> commit(GuiDocumentSession& session, std::string label, Mutation mutation) {
  if (!session.project())
    return Result<std::size_t>::failure(Error::validation("gui.assembly", "operation requires an active Project"));
  return session.commit_project_transaction(std::move(label), std::move(mutation));
}
template <typename Mutation>
Result<std::size_t> preview(const GuiDocumentSession& session, Mutation mutation) {
  if (!session.project())
    return Result<std::size_t>::failure(Error::validation("gui.assembly", "preview requires an active Project"));
  Project candidate = *session.project();
  auto changed = mutation(candidate);
  if (changed.has_error()) return changed;
  auto valid = candidate.validate_assembly_structure();
  return valid.has_error() ? Result<std::size_t>::failure(valid.error()) : changed;
}
} // namespace

GuiAssemblySelectionPrompt GuiAssemblyWorkbench::prompt_for(GuiAssemblyCommand command) {
  switch (command) {
  case GuiAssemblyCommand::InsertPart: return {"Select a project-owned Part definition and authored occurrence placement", GuiSelectionKind::Component, "part_definition"};
  case GuiAssemblyCommand::InsertSubassembly: return {"Select a project-owned child Assembly and rigid occurrence path", GuiSelectionKind::Component, "subassembly_definition"};
  case GuiAssemblyCommand::Placement: return {"Select one occurrence; edit placement, visibility, suppression, or grounding", GuiSelectionKind::Component, "component_state"};
  case GuiAssemblyCommand::Relationship: return {"Select first and second compatible semantic targets", GuiSelectionKind::AssemblyTarget, "relationship_compatible_targets"};
  case GuiAssemblyCommand::Joint: return {"Select two joint-compatible targets and typed coordinate slots", GuiSelectionKind::AssemblyTarget, "joint_compatible_targets"};
  case GuiAssemblyCommand::Solve: return {"Select one connected component solve group", GuiSelectionKind::Component, "connected_solve_group"};
  case GuiAssemblyCommand::DriveJoint: return {"Select an active driven joint coordinate role", GuiSelectionKind::AssemblyTarget, "drivable_joint_coordinate"};
  }
  return {"Select compatible Assembly input", std::nullopt, "assembly_input"};
}

Result<std::size_t> GuiAssemblyWorkbench::add_part(GuiDocumentSession& s, PartDocument v) const { return commit(s, "Insert Part definition", [v=std::move(v)](Project& p) mutable { return p.add_part_document(std::move(v)); }); }
Result<std::size_t> GuiAssemblyWorkbench::add_child_assembly(GuiDocumentSession& s, AssemblyDocument v) const { return commit(s, "Insert child Assembly", [v=std::move(v)](Project& p) mutable { return p.add_child_assembly_document(std::move(v)); }); }
Result<std::size_t> GuiAssemblyWorkbench::add_root_member_part(GuiDocumentSession& s, DocumentId id) const { return commit(s, "Register Assembly Part", [id](Project& p) { return p.assembly().add_member_part(id); }); }
Result<std::size_t> GuiAssemblyWorkbench::add_root_component(GuiDocumentSession& s, ComponentInstance v) const { return commit(s, "Insert component", [v=std::move(v)](Project& p) mutable { return p.assembly().add_component_instance(std::move(v)); }); }
Result<std::size_t> GuiAssemblyWorkbench::add_root_subassembly(GuiDocumentSession& s, SubassemblyInstance v) const { return commit(s, "Insert subassembly occurrence", [v=std::move(v)](Project& p) mutable { return p.assembly().add_subassembly_instance(std::move(v)); }); }
Result<std::size_t> GuiAssemblyWorkbench::add_root_parameter(GuiDocumentSession& s, Parameter v) const { return commit(s, "Create Assembly parameter", [v=std::move(v)](Project& p) mutable { return p.assembly().add_parameter(std::move(v)); }); }
Result<std::size_t> GuiAssemblyWorkbench::add_root_binding(GuiDocumentSession& s, ParameterBinding v) const { return commit(s, "Create parameter binding", [v=std::move(v)](Project& p) mutable { return p.assembly().add_binding(std::move(v)); }); }
Result<std::size_t> GuiAssemblyWorkbench::set_component_transform(GuiDocumentSession& s, ComponentInstanceId id, RigidTransform v) const { return commit(s, "Set component placement", [id,v](Project& p) { return p.assembly().set_component_instance_transform(id,v); }); }
Result<std::size_t> GuiAssemblyWorkbench::set_component_visibility(GuiDocumentSession& s, ComponentInstanceId id, ComponentVisibility v) const { return commit(s, "Set component visibility", [id,v](Project& p) { return p.assembly().set_component_instance_visibility(id,v); }); }
Result<std::size_t> GuiAssemblyWorkbench::set_component_suppression(GuiDocumentSession& s, ComponentInstanceId id, ComponentSuppressionState v) const { return commit(s, "Set component suppression", [id,v](Project& p) { return p.assembly().set_component_instance_suppression_state(id,v); }); }
Result<std::size_t> GuiAssemblyWorkbench::set_component_grounding(GuiDocumentSession& s, ComponentInstanceId id, ComponentGroundingState v) const { return commit(s, "Set component grounding", [id,v](Project& p) { return p.assembly().set_component_instance_grounding_state(id,v); }); }

Result<std::size_t> GuiAssemblyWorkbench::preview_root_constraint(const GuiDocumentSession& s, AssemblyConstraint v) const { return preview(s, [v=std::move(v)](Project& p) mutable { return p.assembly().add_constraint(std::move(v)); }); }
Result<std::size_t> GuiAssemblyWorkbench::apply_root_constraint(GuiDocumentSession& s, AssemblyConstraint v) const { auto ok=preview_root_constraint(s,v); if(ok.has_error()) return ok; return commit(s,"Apply Assembly relationship",[v=std::move(v)](Project& p) mutable{return p.assembly().add_constraint(std::move(v));}); }
Result<std::size_t> GuiAssemblyWorkbench::preview_root_joint(const GuiDocumentSession& s, AssemblyJoint v) const { return preview(s, [v=std::move(v)](Project& p) mutable { return p.assembly().add_joint(std::move(v)); }); }
Result<std::size_t> GuiAssemblyWorkbench::apply_root_joint(GuiDocumentSession& s, AssemblyJoint v) const { auto ok=preview_root_joint(s,v); if(ok.has_error()) return ok; return commit(s,"Apply Assembly joint",[v=std::move(v)](Project& p) mutable{return p.assembly().add_joint(std::move(v));}); }
Result<std::size_t> GuiAssemblyWorkbench::apply_cross_constraint(GuiDocumentSession& s, AssemblyHierarchyConstraint v) const { return commit(s,"Apply cross-hierarchy relationship",[v=std::move(v)](Project& p) mutable{return p.add_cross_hierarchy_constraint(std::move(v));}); }
Result<std::size_t> GuiAssemblyWorkbench::apply_cross_joint(GuiDocumentSession& s, AssemblyHierarchyJoint v) const { return commit(s,"Apply cross-hierarchy joint",[v=std::move(v)](Project& p) mutable{return p.add_cross_hierarchy_joint(std::move(v));}); }
Result<std::size_t> GuiAssemblyWorkbench::set_root_joint_coordinate(GuiDocumentSession& s, AssemblyJointId id, AssemblyJointCoordinateRole role, Quantity value) const { return commit(s,"Set joint coordinate",[id,role,value](Project& p){return p.assembly().set_joint_coordinate_value(id,role,value);}); }

Result<geometry::AssemblySolveDiagnostics> GuiAssemblyWorkbench::inspect_root_group(const GuiDocumentSession& s, std::vector<ComponentInstanceId> group) const { if(!s.project()) return Result<geometry::AssemblySolveDiagnostics>::failure(Error::validation("gui.assembly","inspection requires Project")); return geometry::AssemblySolveDiagnosticsAnalyzer{}.analyze(*s.project(),group); }
Result<geometry::AssemblySolveResult> GuiAssemblyWorkbench::solve_root_group(const GuiDocumentSession& s, std::vector<ComponentInstanceId> group) const { if(!s.project()) return Result<geometry::AssemblySolveResult>::failure(Error::validation("gui.assembly","solve requires Project")); return geometry::AssemblyRigidBodySolver{}.solve(*s.project(),group); }
Result<std::size_t> GuiAssemblyWorkbench::apply_solve(GuiDocumentSession& s,const geometry::AssemblySolveResult& result) const { if(!result.converged()) return Result<std::size_t>::failure(Error::validation("gui.assembly","only converged solve results can be applied")); return commit(s,"Apply Assembly solve",[result](Project& p){return geometry::AssemblySolveResultApplier{}.apply(p,result);}); }
Result<geometry::AssemblyJointMotionResult> GuiAssemblyWorkbench::preview_joint_motion(const GuiDocumentSession& s,AssemblyJointId id,Quantity q) const { if(!s.project()) return Result<geometry::AssemblyJointMotionResult>::failure(Error::validation("gui.assembly","motion requires Project")); return geometry::AssemblyJointMotionSolver{}.move(*s.project(),id,q); }
Result<std::size_t> GuiAssemblyWorkbench::apply_joint_motion(GuiDocumentSession& s,const geometry::AssemblyJointMotionResult& result) const { if(!result.converged()) return Result<std::size_t>::failure(Error::validation("gui.assembly","only converged motion can be applied")); return commit(s,"Apply joint motion",[result](Project& p){return geometry::AssemblyJointMotionResultApplier{}.apply(p,result);}); }

} // namespace blcad::gui
