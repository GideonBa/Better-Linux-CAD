from pathlib import Path

header = Path("include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp")
text = header.read_text(encoding="utf-8")
text = text.replace(
    '#include "blcad/geometry/assembly_constraint_equation_builder.hpp"\n',
    '#include "blcad/geometry/assembly_constraint_equation_builder.hpp"\n'
    '#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"\n',
    1,
)
insert_marker = "struct AssemblyHierarchyInsertConstraintTargetDescriptor {\n"
generic_struct = '''struct AssemblyHierarchyGenericConstraintTargetDescriptor {
  std::vector<SubassemblyInstanceId> occurrence_path;
  DocumentId assembly_document;
  ComponentInstanceId component_instance;
  AssemblyGeometricTargetSourceMetadata source_metadata;
  std::string semantic_reference;
  AssemblyGeometricTargetCapability selected_capability = AssemblyGeometricTargetCapability::Point;
  AssemblyGeometricTargetDescriptor descriptor;

  friend bool operator==(const AssemblyHierarchyGenericConstraintTargetDescriptor&,
                         const AssemblyHierarchyGenericConstraintTargetDescriptor&) = default;
};

'''
if insert_marker not in text:
    raise RuntimeError("hierarchy insert target marker not found")
text = text.replace(insert_marker, generic_struct + insert_marker, 1)
old_targets = '''using AssemblyHierarchyConstraintTargetDescriptor =
    std::variant<AssemblyHierarchyPlanarConstraintTargetDescriptor,
                 AssemblyHierarchyAxisConstraintTargetDescriptor,
                 AssemblyHierarchyInsertConstraintTargetDescriptor>;
'''
new_targets = '''using AssemblyHierarchyConstraintTargetDescriptor =
    std::variant<AssemblyHierarchyPlanarConstraintTargetDescriptor,
                 AssemblyHierarchyAxisConstraintTargetDescriptor,
                 AssemblyHierarchyGenericConstraintTargetDescriptor,
                 AssemblyHierarchyInsertConstraintTargetDescriptor>;
'''
if old_targets not in text:
    raise RuntimeError("hierarchy target variant marker not found")
text = text.replace(old_targets, new_targets, 1)
old_residuals = '''using AssemblyHierarchyConstraintResidualDescriptor =
    std::variant<PlanarMateResidualDescriptor,
                 PlanarDistanceResidualDescriptor,
                 PlanarAngleResidualDescriptor,
                 ConcentricResidualDescriptor,
                 InsertResidualDescriptor>;
'''
new_residuals = '''using AssemblyHierarchyConstraintResidualDescriptor =
    std::variant<PlanarMateResidualDescriptor,
                 PlanarDistanceResidualDescriptor,
                 PlanarAngleResidualDescriptor,
                 ConcentricResidualDescriptor,
                 InsertResidualDescriptor,
                 AssemblyGenericRelationshipResidualDescriptor>;
'''
if old_residuals not in text:
    raise RuntimeError("hierarchy residual variant marker not found")
text = text.replace(old_residuals, new_residuals, 1)
text = text.replace(
    '''// Builds read-only Mate/Distance/Angle/Concentric/Insert residual semantics for
// endpoints that may live in different AssemblyDocument occurrences. The query
// is not solved, persisted, or inserted into a local constraint graph.
''',
    '''// Builds read-only Mate/Distance/Angle/Concentric/Insert and generic
// Coincident/Parallel/Perpendicular residual semantics for endpoints that may
// live in different AssemblyDocument occurrences. The query is not solved,
// persisted, or inserted into a local constraint graph.
''',
    1,
)
header.write_text(text, encoding="utf-8")

cpp = Path("src/geometry/assembly_hierarchy_constraint_equation_builder.cpp")
text = cpp.read_text(encoding="utf-8")
selected_marker = '''[[nodiscard]] bool selected_plane_pair(const AssemblyTargetCompatibility& compatibility) noexcept {
'''
helpers = '''[[nodiscard]] bool generic_relationship_type(AssemblyConstraintType type) noexcept {
  return type == AssemblyConstraintType::Coincident || type == AssemblyConstraintType::Parallel ||
         type == AssemblyConstraintType::Perpendicular;
}

[[nodiscard]] AssemblyHierarchyGenericConstraintTargetDescriptor
project_hierarchy_generic_target(const ResolvedHierarchyGeometricTarget& resolved,
                                 const AssemblyGenericRelationshipTargetDescriptor& target) {
  return AssemblyHierarchyGenericConstraintTargetDescriptor{
      resolved.occurrence_path, resolved.assembly_document, resolved.component_instance,
      target.target.source_metadata, resolved.semantic_reference, target.selected_capability,
      target.target.descriptor};
}

'''
if selected_marker not in text:
    raise RuntimeError("selected plane pair marker not found")
text = text.replace(selected_marker, helpers + selected_marker, 1)
build_marker = '''Result<AssemblyHierarchyConstraintEquationDescriptor>
AssemblyHierarchyConstraintEquationBuilder::build(
    const Project& project, const AssemblyHierarchyConstraintQuery& query) const {
  if (query.type() == AssemblyConstraintType::Concentric) {
'''
generic_branch = '''Result<AssemblyHierarchyConstraintEquationDescriptor>
AssemblyHierarchyConstraintEquationBuilder::build(
    const Project& project, const AssemblyHierarchyConstraintQuery& query) const {
  if (generic_relationship_type(query.type())) {
    auto geometric_target_a = resolve_hierarchy_geometric_target(project, query.target_a());
    if (geometric_target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_a.error());
    }
    auto geometric_target_b = resolve_hierarchy_geometric_target(project, query.target_b());
    if (geometric_target_b.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_b.error());
    }

    const AssemblyGenericRelationshipEquationBuilder generic_builder;
    auto equation = generic_builder.build(query.id(), query.type(), geometric_target_a.value().target,
                                          geometric_target_b.value().target, query.angle());
    if (equation.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(equation.error());
    }
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(),
            project_hierarchy_generic_target(geometric_target_a.value(), equation.value().target_a),
            project_hierarchy_generic_target(geometric_target_b.value(), equation.value().target_b),
            equation.value().residual});
  }

  if (query.type() == AssemblyConstraintType::Concentric) {
'''
if build_marker not in text:
    raise RuntimeError("hierarchy equation build marker not found")
text = text.replace(build_marker, generic_branch, 1)
plane_guard = '''  if (!selected_plane_pair(compatibility.value())) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(validation_error(
        query.id().value(),
        "cross-hierarchy planar equation currently requires Plane/Plane target compatibility"));
  }
'''
angle_and_plane_guard = '''  if (query.type() == AssemblyConstraintType::Angle &&
      !selected_plane_pair(compatibility.value())) {
    const AssemblyGenericRelationshipEquationBuilder generic_builder;
    auto equation = generic_builder.build(query.id(), query.type(), geometric_target_a.value().target,
                                          geometric_target_b.value().target, query.angle());
    if (equation.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(equation.error());
    }
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(),
            project_hierarchy_generic_target(geometric_target_a.value(), equation.value().target_a),
            project_hierarchy_generic_target(geometric_target_b.value(), equation.value().target_b),
            equation.value().residual});
  }
  if (!selected_plane_pair(compatibility.value())) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(validation_error(
        query.id().value(),
        "cross-hierarchy planar equation currently requires Plane/Plane target compatibility"));
  }
'''
if plane_guard not in text:
    raise RuntimeError("hierarchy planar compatibility guard not found")
text = text.replace(plane_guard, angle_and_plane_guard, 1)
cpp.write_text(text, encoding="utf-8")

print("Block 39 hierarchy normalization applied")
