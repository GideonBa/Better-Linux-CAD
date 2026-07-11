#include "blcad/core/project.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] bool contains_document(const std::vector<DocumentId>& ids,
                                     const DocumentId& id) noexcept {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

[[nodiscard]] std::vector<const AssemblyDocument*>
ordered_assembly_documents(const Project& project) {
  std::vector<const AssemblyDocument*> assemblies{&project.assembly()};
  for (const auto& assembly : project.child_assembly_documents()) {
    assemblies.push_back(&assembly);
  }
  std::sort(assemblies.begin() + 1, assemblies.end(), [](const auto* lhs, const auto* rhs) {
    return lhs->id().value() < rhs->id().value();
  });
  return assemblies;
}

[[nodiscard]] std::vector<const SubassemblyInstance*>
ordered_subassembly_instances(const AssemblyDocument& assembly) {
  std::vector<const SubassemblyInstance*> instances;
  instances.reserve(assembly.subassembly_instances().size());
  for (const auto& instance : assembly.subassembly_instances()) {
    instances.push_back(&instance);
  }
  std::sort(instances.begin(), instances.end(), [](const auto* lhs, const auto* rhs) {
    return lhs->id().value() < rhs->id().value();
  });
  return instances;
}

[[nodiscard]] Result<std::size_t> visit_hierarchy(const Project& project,
                                                  const AssemblyDocument& assembly,
                                                  std::vector<DocumentId>& active_path,
                                                  std::vector<DocumentId>& completed,
                                                  std::size_t& traversed_occurrences) {
  active_path.push_back(assembly.id());

  for (const SubassemblyInstance* instance : ordered_subassembly_instances(assembly)) {
    ++traversed_occurrences;
    const AssemblyDocument* child =
        project.find_assembly_document(instance->referenced_assembly_document());
    if (child == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          instance->id().value(),
          "subassembly instance referenced assembly must resolve to a project-owned child assembly document"));
    }
    if (contains_document(active_path, child->id())) {
      return Result<std::size_t>::failure(Error::validation(
          instance->id().value(), "assembly hierarchy must not contain direct or indirect cycles"));
    }
    if (contains_document(completed, child->id())) {
      continue;
    }

    auto visited = visit_hierarchy(project, *child, active_path, completed, traversed_occurrences);
    if (visited.has_error()) {
      return visited;
    }
  }

  active_path.pop_back();
  completed.push_back(assembly.id());
  return Result<std::size_t>::success(traversed_occurrences);
}

} // namespace

ProjectPartUpdate::ProjectPartUpdate(DocumentId part_document,
                                     BindingApplication binding_application,
                                     RecomputePlan recompute_plan)
    : part_document_(std::move(part_document)),
      binding_application_(std::move(binding_application)),
      recompute_plan_(std::move(recompute_plan)) {}

const DocumentId& ProjectPartUpdate::part_document() const noexcept { return part_document_; }

const BindingApplication& ProjectPartUpdate::binding_application() const noexcept {
  return binding_application_;
}

const RecomputePlan& ProjectPartUpdate::recompute_plan() const noexcept { return recompute_plan_; }

ProjectUpdateResult::ProjectUpdateResult(std::vector<ProjectPartUpdate> part_updates)
    : part_updates_(std::move(part_updates)) {}

const std::vector<ProjectPartUpdate>& ProjectUpdateResult::part_updates() const noexcept {
  return part_updates_;
}

std::size_t ProjectUpdateResult::updated_part_count() const noexcept {
  return part_updates_.size();
}

bool ProjectUpdateResult::empty() const noexcept { return part_updates_.empty(); }

Result<Project> Project::create(DocumentId id, std::string name, AssemblyDocument assembly) {
  const auto object_id = id.empty() ? std::string("project") : id.value();
  if (id.empty()) {
    return Result<Project>::failure(Error::validation(object_id, "project id must not be empty"));
  }
  if (name.empty()) {
    return Result<Project>::failure(Error::validation(object_id, "project name must not be empty"));
  }
  return Result<Project>::success(Project(std::move(id), std::move(name), std::move(assembly)));
}

Project::Project(DocumentId id, std::string name, AssemblyDocument assembly)
    : id_(std::move(id)), name_(std::move(name)), assembly_(std::move(assembly)) {}

Result<std::size_t> Project::add_part_document(PartDocument part_document) {
  if (has_part_document_id(part_document.id())) {
    return Result<std::size_t>::failure(Error::validation(
        part_document.id().value(), "part document id must be unique within project"));
  }
  part_documents_.push_back(std::move(part_document));
  return Result<std::size_t>::success(part_documents_.size() - 1U);
}

Result<std::size_t>
Project::add_child_assembly_document(AssemblyDocument assembly_document) {
  if (has_assembly_document_id(assembly_document.id())) {
    return Result<std::size_t>::failure(Error::validation(
        assembly_document.id().value(), "assembly document id must be unique within project"));
  }
  child_assembly_documents_.push_back(std::move(assembly_document));
  return Result<std::size_t>::success(child_assembly_documents_.size() - 1U);
}

Result<std::size_t> Project::validate_member_parts() const {
  std::size_t count = 0U;
  for (const AssemblyDocument* assembly : ordered_assembly_documents(*this)) {
    for (const DocumentId& member : assembly->member_parts()) {
      if (find_part_document(member) == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            member.value(), "assembly member part must resolve to an owned project part document"));
      }
      ++count;
    }
  }
  return Result<std::size_t>::success(count);
}

Result<std::size_t> Project::validate_component_instances() const {
  std::size_t count = 0U;
  for (const AssemblyDocument* assembly : ordered_assembly_documents(*this)) {
    for (const ComponentInstance& instance : assembly->component_instances()) {
      if (!assembly->has_member_part(instance.referenced_part_document())) {
        return Result<std::size_t>::failure(Error::validation(
            instance.id().value(), "component instance referenced part must be an assembly member part"));
      }
      if (find_part_document(instance.referenced_part_document()) == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            instance.id().value(),
            "component instance referenced part must resolve to an owned project part document"));
      }
      ++count;
    }
  }
  return Result<std::size_t>::success(count);
}

Result<std::size_t> Project::validate_assembly_constraints() const {
  std::size_t count = 0U;
  for (const AssemblyDocument* assembly : ordered_assembly_documents(*this)) {
    for (const AssemblyConstraint& constraint : assembly->constraints()) {
      if (assembly->find_component_instance(constraint.target_a().component_instance()) == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            constraint.id().value(), "assembly constraint target A component instance must exist"));
      }
      if (assembly->find_component_instance(constraint.target_b().component_instance()) == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            constraint.id().value(), "assembly constraint target B component instance must exist"));
      }
      ++count;
    }
  }
  return Result<std::size_t>::success(count);
}

Result<std::size_t> Project::validate_assembly_joints() const {
  std::size_t count = 0U;
  for (const AssemblyDocument* assembly : ordered_assembly_documents(*this)) {
    for (const AssemblyJoint& joint : assembly->joints()) {
      if (assembly->find_component_instance(joint.target_a().component_instance()) == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            joint.id().value(), "assembly joint target A component instance must exist"));
      }
      if (assembly->find_component_instance(joint.target_b().component_instance()) == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            joint.id().value(), "assembly joint target B component instance must exist"));
      }
      ++count;
    }
  }
  return Result<std::size_t>::success(count);
}

Result<std::size_t> Project::validate_subassembly_instances() const {
  std::size_t count = 0U;
  for (const AssemblyDocument* assembly : ordered_assembly_documents(*this)) {
    for (const SubassemblyInstance& instance : assembly->subassembly_instances()) {
      const AssemblyDocument* referenced =
          find_assembly_document(instance.referenced_assembly_document());
      if (referenced == nullptr || referenced->id() == assembly_.id()) {
        return Result<std::size_t>::failure(Error::validation(
            instance.id().value(),
            "subassembly instance referenced assembly must resolve to a project-owned child assembly document"));
      }
      if (referenced->id() == assembly->id()) {
        return Result<std::size_t>::failure(Error::validation(
            instance.id().value(), "subassembly instance cannot reference its containing assembly document"));
      }
      ++count;
    }
  }
  return Result<std::size_t>::success(count);
}

Result<std::size_t> Project::validate_assembly_hierarchy() const {
  auto references = validate_subassembly_instances();
  if (references.has_error()) {
    return references;
  }

  std::vector<DocumentId> active_path;
  std::vector<DocumentId> completed;
  std::size_t traversed_occurrences = 0U;
  for (const AssemblyDocument* assembly : ordered_assembly_documents(*this)) {
    if (contains_document(completed, assembly->id())) {
      continue;
    }
    auto visited =
        visit_hierarchy(*this, *assembly, active_path, completed, traversed_occurrences);
    if (visited.has_error()) {
      return visited;
    }
  }
  return Result<std::size_t>::success(references.value());
}

Result<std::size_t> Project::validate_assembly_structure() const {
  auto members = validate_member_parts();
  if (members.has_error()) return members;

  auto instances = validate_component_instances();
  if (instances.has_error()) return instances;

  auto constraints = validate_assembly_constraints();
  if (constraints.has_error()) return constraints;

  auto joints = validate_assembly_joints();
  if (joints.has_error()) return joints;

  auto hierarchy = validate_assembly_hierarchy();
  if (hierarchy.has_error()) return hierarchy;

  auto cross_hierarchy_constraints = validate_cross_hierarchy_constraints();
  if (cross_hierarchy_constraints.has_error()) return cross_hierarchy_constraints;

  return Result<std::size_t>::success(
      members.value() + instances.value() + constraints.value() + joints.value() +
      hierarchy.value() + cross_hierarchy_constraints.value());
}

Result<ProjectUpdateResult> Project::set_assembly_parameter_value(ParameterId id, Quantity value) {
  auto valid_structure = validate_assembly_structure();
  if (valid_structure.has_error()) {
    return Result<ProjectUpdateResult>::failure(valid_structure.error());
  }

  auto updated = assembly_.set_parameter_value(id, value);
  if (updated.has_error()) {
    return Result<ProjectUpdateResult>::failure(updated.error());
  }

  std::vector<ProjectPartUpdate> part_updates;
  for (PartDocument& part : part_documents_) {
    if (!assembly_.has_member_part(part.id())) {
      continue;
    }

    auto applied = assembly_.apply_bindings_to(part);
    if (applied.has_error()) {
      return Result<ProjectUpdateResult>::failure(applied.error());
    }
    if (applied.value().applied_binding_count == 0U &&
        applied.value().affected_part_nodes.empty()) {
      continue;
    }

    auto plan = part.create_recompute_plan();
    if (plan.has_error()) {
      return Result<ProjectUpdateResult>::failure(plan.error());
    }

    part_updates.emplace_back(part.id(), std::move(applied.value()), std::move(plan.value()));
  }

  return Result<ProjectUpdateResult>::success(ProjectUpdateResult(std::move(part_updates)));
}

const DocumentId& Project::id() const noexcept { return id_; }
const std::string& Project::name() const noexcept { return name_; }
const AssemblyDocument& Project::assembly() const noexcept { return assembly_; }
AssemblyDocument& Project::assembly() noexcept { return assembly_; }
const std::vector<AssemblyDocument>& Project::child_assembly_documents() const noexcept {
  return child_assembly_documents_;
}
std::vector<AssemblyDocument>& Project::child_assembly_documents() noexcept {
  return child_assembly_documents_;
}
std::size_t Project::child_assembly_document_count() const noexcept {
  return child_assembly_documents_.size();
}

const AssemblyDocument* Project::find_assembly_document(DocumentId id) const noexcept {
  if (assembly_.id() == id) {
    return &assembly_;
  }
  const auto found = std::find_if(child_assembly_documents_.begin(), child_assembly_documents_.end(),
                                  [&id](const auto& assembly) { return assembly.id() == id; });
  return found == child_assembly_documents_.end() ? nullptr : &*found;
}

AssemblyDocument* Project::find_assembly_document(DocumentId id) noexcept {
  if (assembly_.id() == id) {
    return &assembly_;
  }
  const auto found = std::find_if(child_assembly_documents_.begin(), child_assembly_documents_.end(),
                                  [&id](const auto& assembly) { return assembly.id() == id; });
  return found == child_assembly_documents_.end() ? nullptr : &*found;
}

const std::vector<PartDocument>& Project::part_documents() const noexcept { return part_documents_; }
std::vector<PartDocument>& Project::part_documents() noexcept { return part_documents_; }
std::size_t Project::part_document_count() const noexcept { return part_documents_.size(); }

const PartDocument* Project::find_part_document(DocumentId id) const noexcept {
  const auto found = std::find_if(part_documents_.begin(), part_documents_.end(),
                                  [&id](const PartDocument& part) { return part.id() == id; });
  return found == part_documents_.end() ? nullptr : &*found;
}

PartDocument* Project::find_part_document(DocumentId id) noexcept {
  const auto found = std::find_if(part_documents_.begin(), part_documents_.end(),
                                  [&id](const PartDocument& part) { return part.id() == id; });
  return found == part_documents_.end() ? nullptr : &*found;
}

bool Project::has_assembly_document_id(const DocumentId& id) const noexcept {
  return find_assembly_document(id) != nullptr;
}

bool Project::has_part_document_id(const DocumentId& id) const noexcept {
  return find_part_document(id) != nullptr;
}

} // namespace blcad
