#include "blcad/core/assembly_document.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] bool finite_vector(Vector3 vector) noexcept {
  return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

[[nodiscard]] bool finite_transform(const RigidTransform& transform) noexcept {
  return finite_vector(transform.translation_mm) && finite_vector(transform.rotation_deg);
}

template <typename Update>
[[nodiscard]] Result<std::size_t>
update_subassembly_instance(std::vector<SubassemblyInstance>& instances,
                            SubassemblyInstanceId id,
                            Update update) {
  if (id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation("subassembly_instance", "subassembly instance id must not be empty"));
  }

  const auto found = std::find_if(instances.begin(), instances.end(), [&id](const auto& instance) {
    return instance.id() == id;
  });
  if (found == instances.end()) {
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "subassembly instance must exist in assembly document"));
  }

  auto updated = update(*found);
  if (updated.has_error()) {
    return Result<std::size_t>::failure(updated.error());
  }

  const auto index = static_cast<std::size_t>(found - instances.begin());
  *found = std::move(updated.value());
  return Result<std::size_t>::success(index);
}

} // namespace

Result<SubassemblyInstance> SubassemblyInstance::create(
    SubassemblyInstanceId id, std::string name, DocumentId referenced_assembly_document,
    ComponentVisibility visibility, ComponentSuppressionState suppression_state,
    RigidTransform transform) {
  const auto object_id = id.empty() ? std::string("subassembly_instance") : id.value();
  if (id.empty()) {
    return Result<SubassemblyInstance>::failure(
        Error::validation(object_id, "subassembly instance id must not be empty"));
  }
  if (name.empty()) {
    return Result<SubassemblyInstance>::failure(
        Error::validation(object_id, "subassembly instance name must not be empty"));
  }
  if (referenced_assembly_document.empty()) {
    return Result<SubassemblyInstance>::failure(Error::validation(
        object_id, "subassembly instance referenced assembly document must not be empty"));
  }
  if (!finite_transform(transform)) {
    return Result<SubassemblyInstance>::failure(
        Error::validation(object_id, "subassembly instance transform values must be finite"));
  }

  return Result<SubassemblyInstance>::success(SubassemblyInstance(
      std::move(id), std::move(name), std::move(referenced_assembly_document), visibility,
      suppression_state, transform));
}

Result<SubassemblyInstance>
SubassemblyInstance::with_transform(RigidTransform transform) const {
  return create(id_, name_, referenced_assembly_document_, visibility_, suppression_state_, transform);
}

Result<SubassemblyInstance>
SubassemblyInstance::with_visibility(ComponentVisibility visibility) const {
  return create(id_, name_, referenced_assembly_document_, visibility, suppression_state_, transform_);
}

Result<SubassemblyInstance> SubassemblyInstance::with_suppression_state(
    ComponentSuppressionState suppression_state) const {
  return create(id_, name_, referenced_assembly_document_, visibility_, suppression_state, transform_);
}

SubassemblyInstance::SubassemblyInstance(SubassemblyInstanceId id, std::string name,
                                         DocumentId referenced_assembly_document,
                                         ComponentVisibility visibility,
                                         ComponentSuppressionState suppression_state,
                                         RigidTransform transform)
    : id_(std::move(id)), name_(std::move(name)),
      referenced_assembly_document_(std::move(referenced_assembly_document)),
      visibility_(visibility), suppression_state_(suppression_state), transform_(transform) {}

const SubassemblyInstanceId& SubassemblyInstance::id() const noexcept { return id_; }
const std::string& SubassemblyInstance::name() const noexcept { return name_; }
const DocumentId& SubassemblyInstance::referenced_assembly_document() const noexcept {
  return referenced_assembly_document_;
}
ComponentVisibility SubassemblyInstance::visibility() const noexcept { return visibility_; }
ComponentSuppressionState SubassemblyInstance::suppression_state() const noexcept {
  return suppression_state_;
}
const RigidTransform& SubassemblyInstance::transform() const noexcept { return transform_; }

Result<std::size_t> AssemblyDocument::add_subassembly_instance(SubassemblyInstance instance) {
  if (has_subassembly_instance_id(instance.id())) {
    return Result<std::size_t>::failure(Error::validation(
        instance.id().value(), "subassembly instance id must be unique within assembly document"));
  }
  if (instance.referenced_assembly_document() == id_) {
    return Result<std::size_t>::failure(Error::validation(
        instance.id().value(), "subassembly instance cannot reference its containing assembly document"));
  }

  subassembly_instances_.push_back(std::move(instance));
  return Result<std::size_t>::success(subassembly_instances_.size() - 1U);
}

Result<std::size_t> AssemblyDocument::set_subassembly_instance_transform(
    SubassemblyInstanceId id, RigidTransform transform) {
  return update_subassembly_instance(
      subassembly_instances_, std::move(id),
      [transform](const SubassemblyInstance& instance) { return instance.with_transform(transform); });
}

Result<std::size_t> AssemblyDocument::set_subassembly_instance_visibility(
    SubassemblyInstanceId id, ComponentVisibility visibility) {
  return update_subassembly_instance(
      subassembly_instances_, std::move(id), [visibility](const SubassemblyInstance& instance) {
        return instance.with_visibility(visibility);
      });
}

Result<std::size_t> AssemblyDocument::set_subassembly_instance_suppression_state(
    SubassemblyInstanceId id, ComponentSuppressionState suppression_state) {
  return update_subassembly_instance(
      subassembly_instances_, std::move(id),
      [suppression_state](const SubassemblyInstance& instance) {
        return instance.with_suppression_state(suppression_state);
      });
}

const std::vector<SubassemblyInstance>& AssemblyDocument::subassembly_instances() const noexcept {
  return subassembly_instances_;
}

std::size_t AssemblyDocument::subassembly_instance_count() const noexcept {
  return subassembly_instances_.size();
}

const SubassemblyInstance*
AssemblyDocument::find_subassembly_instance(SubassemblyInstanceId id) const noexcept {
  const auto found = std::find_if(subassembly_instances_.begin(), subassembly_instances_.end(),
                                  [&id](const auto& instance) { return instance.id() == id; });
  return found == subassembly_instances_.end() ? nullptr : &*found;
}

bool AssemblyDocument::has_subassembly_instance_id(const SubassemblyInstanceId& id) const noexcept {
  return find_subassembly_instance(id) != nullptr;
}

} // namespace blcad
