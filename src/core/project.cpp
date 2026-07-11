#include "blcad/core/project.hpp"

#include <algorithm>
#include <utility>

namespace blcad {

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

Result<std::size_t> Project::validate_member_parts() const {
  for (const DocumentId& member : assembly_.member_parts()) {
    if (find_part_document(member) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          member.value(), "assembly member part must resolve to an owned project part document"));
    }
  }
  return Result<std::size_t>::success(assembly_.member_part_count());
}

Result<std::size_t> Project::validate_component_instances() const {
  for (const ComponentInstance& instance : assembly_.component_instances()) {
    if (!assembly_.has_member_part(instance.referenced_part_document())) {
      return Result<std::size_t>::failure(Error::validation(
          instance.id().value(), "component instance referenced part must be an assembly member part"));
    }
    if (find_part_document(instance.referenced_part_document()) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          instance.id().value(),
          "component instance referenced part must resolve to an owned project part document"));
    }
  }
  return Result<std::size_t>::success(assembly_.component_instance_count());
}

Result<std::size_t> Project::validate_assembly_constraints() const {
  for (const AssemblyConstraint& constraint : assembly_.constraints()) {
    if (assembly_.find_component_instance(constraint.target_a().component_instance()) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          constraint.id().value(), "assembly constraint target A component instance must exist"));
    }
    if (assembly_.find_component_instance(constraint.target_b().component_instance()) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          constraint.id().value(), "assembly constraint target B component instance must exist"));
    }
  }
  return Result<std::size_t>::success(assembly_.constraint_count());
}

Result<std::size_t> Project::validate_assembly_structure() const {
  auto members = validate_member_parts();
  if (members.has_error()) return members;

  auto instances = validate_component_instances();
  if (instances.has_error()) return instances;

  auto constraints = validate_assembly_constraints();
  if (constraints.has_error()) return constraints;

  return Result<std::size_t>::success(members.value() + instances.value() + constraints.value());
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

bool Project::has_part_document_id(const DocumentId& id) const noexcept {
  return find_part_document(id) != nullptr;
}

} // namespace blcad
