#include "blcad/core/structured_step_import.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace blcad {
namespace {
Error invalid(std::string message) {
  return Error::validation("structured_step_import", std::move(message));
}

bool finite(const RigidTransform& transform) {
  return std::isfinite(transform.translation_mm.x) && std::isfinite(transform.translation_mm.y) &&
         std::isfinite(transform.translation_mm.z) && std::isfinite(transform.rotation_deg.x) &&
         std::isfinite(transform.rotation_deg.y) && std::isfinite(transform.rotation_deg.z);
}

Result<std::size_t> fresh_assembly(const AssemblyDocument& assembly) {
  if (assembly.parameter_count() || assembly.binding_count() || assembly.constraint_count() ||
      assembly.joint_count())
    return Result<std::size_t>::failure(invalid("initial structured import must not infer Assembly "
                                                "parameters, bindings, constraints or joints"));
  std::set<std::string> members, referenced;
  for (const auto& member : assembly.member_parts())
    members.insert(member.value());
  for (const auto& occurrence : assembly.component_instances()) {
    if (!finite(occurrence.transform()) ||
        occurrence.visibility() != ComponentVisibility::Visible ||
        occurrence.suppression_state() != ComponentSuppressionState::Active ||
        occurrence.grounding_state() != ComponentGroundingState::Free)
      return Result<std::size_t>::failure(invalid(
          "initial imported components require finite placement and visible, active, free state"));
    referenced.insert(occurrence.referenced_part_document().value());
  }
  for (const auto& occurrence : assembly.subassembly_instances())
    if (!finite(occurrence.transform()) ||
        occurrence.visibility() != ComponentVisibility::Visible ||
        occurrence.suppression_state() != ComponentSuppressionState::Active)
      return Result<std::size_t>::failure(invalid(
          "initial imported subassemblies require finite placement and visible, active state"));
  if (members != referenced)
    return Result<std::size_t>::failure(
        invalid("initial Assembly members must exactly match its component definitions"));
  return Result<std::size_t>::success(assembly.component_instance_count() +
                                      assembly.subassembly_instance_count());
}

Result<AssemblyDocument> canonical_assembly(const AssemblyDocument& original) {
  auto valid = fresh_assembly(original);
  if (!valid)
    return Result<AssemblyDocument>::failure(valid.error());
  auto result = AssemblyDocument::create(original.id(), original.name());
  if (!result)
    return result;
  auto members = original.member_parts();
  std::sort(members.begin(), members.end(),
            [](const auto& a, const auto& b) { return a.value() < b.value(); });
  for (const auto& member : members) {
    auto added = result.value().add_member_part(member);
    if (!added)
      return Result<AssemblyDocument>::failure(added.error());
  }
  const auto append = [&](auto occurrences, auto add) -> Result<std::size_t> {
    std::sort(occurrences.begin(), occurrences.end(),
              [](const auto& a, const auto& b) { return a.id().value() < b.id().value(); });
    for (const auto& occurrence : occurrences) {
      auto added = (result.value().*add)(occurrence);
      if (!added)
        return added;
    }
    return Result<std::size_t>::success(occurrences.size());
  };
  auto added = append(original.component_instances(), &AssemblyDocument::add_component_instance);
  if (!added)
    return Result<AssemblyDocument>::failure(added.error());
  added = append(original.subassembly_instances(), &AssemblyDocument::add_subassembly_instance);
  return added ? result : Result<AssemblyDocument>::failure(added.error());
}

Result<std::size_t> fresh_part(const PartDocument& part, const StepImportSource& source) {
  const auto* owned_source = part.find_step_import_source(source.id());
  if (part.step_import_source_count() != 1 || !owned_source || *owned_source != source ||
      part.imported_topology_catalogs().size() != 1)
    return Result<std::size_t>::failure(
        invalid("each imported Part must own the exact selected source and one product catalog"));
  const ImportedPartDefinition* definition = part.reference_part_definition();
  if (source.mode() == StepImportMode::Reference) {
    if (!part.is_reference_part() || !part.imported_body_features().empty())
      return Result<std::size_t>::failure(
          invalid("Reference import requires immutable Reference Parts"));
  } else if (source.mode() == StepImportMode::EditableBody) {
    if (part.is_reference_part() || part.imported_body_features().size() != 1)
      return Result<std::size_t>::failure(
          invalid("Editable import requires exactly one actual imported base per Part"));
    definition = &part.imported_body_features().front().definition();
  } else
    return Result<std::size_t>::failure(
        invalid("structured import requires an explicit source mode"));
  if (!definition || definition->source() != source.id())
    return Result<std::size_t>::failure(
        invalid("imported Part definition must bind the selected source"));
  const auto* catalog = part.find_imported_topology_catalog(definition->catalog());
  if (!catalog || catalog->source() != source.id() ||
      catalog->captured_sha256_digest() != source.sha256_digest() ||
      part.body_count() != definition->outputs().size())
    return Result<std::size_t>::failure(
        invalid("initial imported Part requires its fresh catalog and exact Body output mapping"));
  std::set<std::string> selected, bodies;
  for (const auto& output : definition->outputs()) {
    const auto* entry = catalog->find(output.imported_body);
    const auto* body = part.find_body(output.body);
    if (!entry || entry->descriptor.kind != ImportedTopologyKind::Body ||
        entry->descriptor.body_kind != output.kind || !body || body->kind() != output.kind ||
        body->visibility() != BodyVisibility::Visible ||
        !selected.insert(output.imported_body.value()).second ||
        !bodies.insert(output.body.value()).second)
      return Result<std::size_t>::failure(
          invalid("imported Body outputs must be unique, visible and owned with matching kinds"));
  }
  for (const auto& entry : catalog->entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Body && !selected.contains(entry.id.value()))
      return Result<std::size_t>::failure(
          invalid("structured Part must cover every catalog Body definition"));
  if (!part.parameters().empty() || !part.datum_axes().empty() ||
      !part.construction_points().empty() || !part.construction_lines().empty() ||
      !part.construction_planes().empty() || !part.derived_workplanes().empty() ||
      !part.result_face_workplanes().empty() || !part.sketches().empty() ||
      !part.sketches_3d().empty() || !part.path_curves().empty() || !part.features().empty() ||
      !part.revolve_features().empty() || !part.sweep_features().empty() ||
      !part.loft_features().empty() || !part.surface_features().empty() ||
      !part.linear_pattern_features().empty() || !part.circular_pattern_features().empty() ||
      !part.mirror_features().empty() || !part.fillet_features().empty() ||
      !part.chamfer_features().empty() || !part.shell_features().empty() ||
      !part.draft_features().empty() || !part.body_boolean_features().empty() ||
      !part.sketch_ownerships().empty() || !part.body_transforms().empty() ||
      !part.reference_statuses().empty() || !part.reference_remaps().empty() ||
      !part.sketch_origin_overrides().empty() || part.datum_planes().size() != 3)
    return Result<std::size_t>::failure(
        invalid("initial imported Parts must contain no downstream authored intent"));
  std::size_t plane_index = 0;
  for (const auto& expected : {DatumPlane::xy(), DatumPlane::xz(), DatumPlane::yz()}) {
    if (!expected)
      return Result<std::size_t>::failure(expected.error());
    const auto& actual = part.datum_planes()[plane_index++];
    const auto& origin = expected.value();
    if (actual.id() != origin.id() || actual.name() != origin.name() ||
        actual.origin() != origin.origin() || actual.x_axis() != origin.x_axis() ||
        actual.y_axis() != origin.y_axis() || actual.normal() != origin.normal())
      return Result<std::size_t>::failure(
          invalid("initial imported Parts require exact BLCAD origin planes"));
  }
  return Result<std::size_t>::success(definition->outputs().size());
}
} // namespace

Result<std::size_t> validate_structured_step_import_candidate(const Project& project,
                                                              StepImportSourceId source_id) {
  const auto* source = project.find_step_import_source(source_id);
  if (!source || project.step_import_source_count() != 1 ||
      !project.imported_topology_catalogs().empty() || project.part_documents().empty() ||
      project.cross_hierarchy_constraint_count() || project.cross_hierarchy_joint_count())
    return Result<std::size_t>::failure(
        invalid("initial structured Project requires one source, owned imported Parts and no "
                "inferred relationships or top-level catalogs"));
  auto structure = project.validate_assembly_structure();
  if (!structure)
    return structure;
  std::set<std::string> documents{project.id().value()}, occurrences;
  std::vector<const AssemblyDocument*> assemblies{&project.assembly()};
  for (const auto& child : project.child_assembly_documents())
    assemblies.push_back(&child);
  for (const auto* assembly : assemblies) {
    if (!documents.insert(assembly->id().value()).second)
      return Result<std::size_t>::failure(
          invalid("imported Project document identities must be globally unique"));
    auto valid = fresh_assembly(*assembly);
    if (!valid)
      return valid;
    for (const auto& occurrence : assembly->component_instances())
      if (!occurrences.insert(occurrence.id().value()).second)
        return Result<std::size_t>::failure(
            invalid("initial imported occurrence identities must be globally unique"));
    for (const auto& occurrence : assembly->subassembly_instances())
      if (!occurrences.insert(occurrence.id().value()).second)
        return Result<std::size_t>::failure(
            invalid("initial imported occurrence identities must be globally unique"));
  }
  for (const auto& part : project.part_documents()) {
    if (!documents.insert(part.id().value()).second)
      return Result<std::size_t>::failure(
          invalid("imported Project document identities must be globally unique"));
    auto valid = fresh_part(part, *source);
    if (!valid)
      return valid;
  }
  std::set<std::string> reachable_assemblies{project.assembly().id().value()}, reachable_parts;
  std::vector<const AssemblyDocument*> pending{&project.assembly()};
  for (std::size_t index = 0; index < pending.size(); ++index) {
    for (const auto& occurrence : pending[index]->component_instances())
      reachable_parts.insert(occurrence.referenced_part_document().value());
    for (const auto& occurrence : pending[index]->subassembly_instances())
      if (reachable_assemblies.insert(occurrence.referenced_assembly_document().value()).second)
        pending.push_back(
            project.find_assembly_document(occurrence.referenced_assembly_document()));
  }
  if (reachable_assemblies.size() != assemblies.size() ||
      reachable_parts.size() != project.part_document_count())
    return Result<std::size_t>::failure(invalid(
        "initial structured import must not contain unreachable Assembly or Part definitions"));
  return Result<std::size_t>::success(occurrences.size());
}

Result<Project> create_structured_step_project(DocumentId project_id, std::string name,
                                               StepImportSource source, AssemblyDocument root,
                                               std::vector<AssemblyDocument> assemblies,
                                               std::vector<PartDocument> parts) {
  auto canonical_root = canonical_assembly(root);
  if (!canonical_root)
    return Result<Project>::failure(canonical_root.error());
  auto result =
      Project::create(std::move(project_id), std::move(name), std::move(canonical_root.value()));
  if (!result)
    return result;
  auto added = result.value().add_step_import_source(source);
  if (!added)
    return Result<Project>::failure(added.error());
  const auto ordered = [](const auto& a, const auto& b) { return a.id().value() < b.id().value(); };
  std::sort(assemblies.begin(), assemblies.end(), ordered);
  std::sort(parts.begin(), parts.end(), ordered);
  for (const auto& assembly : assemblies) {
    auto canonical = canonical_assembly(assembly);
    if (!canonical)
      return Result<Project>::failure(canonical.error());
    added = result.value().add_child_assembly_document(std::move(canonical.value()));
    if (!added)
      return Result<Project>::failure(added.error());
  }
  for (auto& part : parts) {
    added = result.value().add_part_document(std::move(part));
    if (!added)
      return Result<Project>::failure(added.error());
  }
  auto valid = validate_structured_step_import_candidate(result.value(), source.id());
  return valid ? result : Result<Project>::failure(valid.error());
}
} // namespace blcad
