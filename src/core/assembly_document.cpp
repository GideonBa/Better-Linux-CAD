#include "blcad/core/assembly_document.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] bool is_finite(Vector3 vector) noexcept {
  return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

[[nodiscard]] bool is_finite(const RigidTransform& transform) noexcept {
  return is_finite(transform.translation_mm) && is_finite(transform.rotation_deg);
}

template <typename Update>
[[nodiscard]] Result<std::size_t>
update_component_instance(std::vector<ComponentInstance>& component_instances,
                          ComponentInstanceId id, Update update) {
  if (id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation("component_instance", "component instance id must not be empty"));
  }

  const auto found =
      std::find_if(component_instances.begin(), component_instances.end(),
                   [&id](const ComponentInstance& instance) { return instance.id() == id; });
  if (found == component_instances.end()) {
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "component instance must exist in assembly document"));
  }

  auto updated = update(*found);
  if (updated.has_error()) {
    return Result<std::size_t>::failure(updated.error());
  }

  const auto index = static_cast<std::size_t>(found - component_instances.begin());
  *found = std::move(updated.value());
  return Result<std::size_t>::success(index);
}

} // namespace

Result<ParameterBinding> ParameterBinding::create(ParameterBindingId id, DocumentId part_document,
                                                  ParameterId part_parameter,
                                                  ParameterId assembly_parameter) {
  const auto object_id = id.empty() ? std::string("parameter_binding") : id.value();
  if (id.empty()) {
    return Result<ParameterBinding>::failure(
        Error::validation(object_id, "parameter binding id must not be empty"));
  }
  if (part_document.empty()) {
    return Result<ParameterBinding>::failure(
        Error::validation(object_id, "parameter binding part document must not be empty"));
  }
  if (part_parameter.empty()) {
    return Result<ParameterBinding>::failure(
        Error::validation(object_id, "parameter binding part parameter must not be empty"));
  }
  if (assembly_parameter.empty()) {
    return Result<ParameterBinding>::failure(
        Error::validation(object_id, "parameter binding assembly parameter must not be empty"));
  }
  return Result<ParameterBinding>::success(ParameterBinding(std::move(id), std::move(part_document),
                                                            std::move(part_parameter),
                                                            std::move(assembly_parameter)));
}

ParameterBinding::ParameterBinding(ParameterBindingId id, DocumentId part_document,
                                   ParameterId part_parameter, ParameterId assembly_parameter)
    : id_(std::move(id)), part_document_(std::move(part_document)),
      part_parameter_(std::move(part_parameter)),
      assembly_parameter_(std::move(assembly_parameter)) {}

const ParameterBindingId& ParameterBinding::id() const noexcept {
  return id_;
}
const DocumentId& ParameterBinding::part_document() const noexcept {
  return part_document_;
}
const ParameterId& ParameterBinding::part_parameter() const noexcept {
  return part_parameter_;
}
const ParameterId& ParameterBinding::assembly_parameter() const noexcept {
  return assembly_parameter_;
}

RigidTransform identity_rigid_transform() noexcept {
  return RigidTransform{};
}

std::string_view to_string(ComponentVisibility visibility) noexcept {
  switch (visibility) {
  case ComponentVisibility::Visible:
    return "visible";
  case ComponentVisibility::Hidden:
    return "hidden";
  }
  return "visible";
}

std::string_view to_string(ComponentSuppressionState state) noexcept {
  switch (state) {
  case ComponentSuppressionState::Active:
    return "active";
  case ComponentSuppressionState::Suppressed:
    return "suppressed";
  }
  return "active";
}

std::string_view to_string(ComponentGroundingState state) noexcept {
  switch (state) {
  case ComponentGroundingState::Free:
    return "free";
  case ComponentGroundingState::Grounded:
    return "grounded";
  }
  return "free";
}

Result<ComponentInstance> ComponentInstance::create(ComponentInstanceId id, std::string name,
                                                    DocumentId referenced_part_document,
                                                    ComponentVisibility visibility,
                                                    ComponentSuppressionState suppression_state,
                                                    ComponentGroundingState grounding_state,
                                                    RigidTransform transform) {
  const auto object_id = id.empty() ? std::string("component_instance") : id.value();
  if (id.empty()) {
    return Result<ComponentInstance>::failure(
        Error::validation(object_id, "component instance id must not be empty"));
  }
  if (name.empty()) {
    return Result<ComponentInstance>::failure(
        Error::validation(object_id, "component instance name must not be empty"));
  }
  if (referenced_part_document.empty()) {
    return Result<ComponentInstance>::failure(Error::validation(
        object_id, "component instance referenced part document must not be empty"));
  }
  if (!is_finite(transform)) {
    return Result<ComponentInstance>::failure(
        Error::validation(object_id, "component instance transform values must be finite"));
  }
  return Result<ComponentInstance>::success(
      ComponentInstance(std::move(id), std::move(name), std::move(referenced_part_document),
                        visibility, suppression_state, grounding_state, transform));
}

Result<ComponentInstance> ComponentInstance::with_transform(RigidTransform transform) const {
  return create(id_, name_, referenced_part_document_, visibility_, suppression_state_,
                grounding_state_, transform);
}

Result<ComponentInstance> ComponentInstance::with_visibility(ComponentVisibility visibility) const {
  return create(id_, name_, referenced_part_document_, visibility, suppression_state_,
                grounding_state_, transform_);
}

Result<ComponentInstance>
ComponentInstance::with_suppression_state(ComponentSuppressionState suppression_state) const {
  return create(id_, name_, referenced_part_document_, visibility_, suppression_state,
                grounding_state_, transform_);
}

Result<ComponentInstance>
ComponentInstance::with_grounding_state(ComponentGroundingState grounding_state) const {
  return create(id_, name_, referenced_part_document_, visibility_, suppression_state_,
                grounding_state, transform_);
}

ComponentInstance::ComponentInstance(ComponentInstanceId id, std::string name,
                                     DocumentId referenced_part_document,
                                     ComponentVisibility visibility,
                                     ComponentSuppressionState suppression_state,
                                     ComponentGroundingState grounding_state,
                                     RigidTransform transform)
    : id_(std::move(id)), name_(std::move(name)),
      referenced_part_document_(std::move(referenced_part_document)), visibility_(visibility),
      suppression_state_(suppression_state), grounding_state_(grounding_state),
      transform_(transform) {}

const ComponentInstanceId& ComponentInstance::id() const noexcept {
  return id_;
}
const std::string& ComponentInstance::name() const noexcept {
  return name_;
}
const DocumentId& ComponentInstance::referenced_part_document() const noexcept {
  return referenced_part_document_;
}
ComponentVisibility ComponentInstance::visibility() const noexcept {
  return visibility_;
}
ComponentSuppressionState ComponentInstance::suppression_state() const noexcept {
  return suppression_state_;
}
ComponentGroundingState ComponentInstance::grounding_state() const noexcept {
  return grounding_state_;
}
const RigidTransform& ComponentInstance::transform() const noexcept {
  return transform_;
}

Result<AssemblyDocument> AssemblyDocument::create(DocumentId id, std::string name) {
  const auto object_id = id.empty() ? std::string("assembly_document") : id.value();
  if (id.empty()) {
    return Result<AssemblyDocument>::failure(
        Error::validation(object_id, "assembly document id must not be empty"));
  }
  if (name.empty()) {
    return Result<AssemblyDocument>::failure(
        Error::validation(object_id, "assembly document name must not be empty"));
  }
  return Result<AssemblyDocument>::success(AssemblyDocument(std::move(id), std::move(name)));
}

AssemblyDocument::AssemblyDocument(DocumentId id, std::string name)
    : id_(std::move(id)), name_(std::move(name)) {}

Result<std::size_t> AssemblyDocument::add_parameter(Parameter parameter) {
  if (parameter.scope() != ParameterScope::Assembly) {
    return Result<std::size_t>::failure(Error::validation(
        parameter.id().value(), "assembly document parameters must use assembly scope"));
  }
  if (has_parameter_id(parameter.id())) {
    return Result<std::size_t>::failure(Error::validation(
        parameter.id().value(), "parameter id must be unique within assembly document"));
  }
  if (has_parameter_name(parameter.name())) {
    return Result<std::size_t>::failure(Error::validation(
        parameter.id().value(), "parameter name must be unique within assembly document"));
  }
  parameters_.push_back(std::move(parameter));
  return Result<std::size_t>::success(parameters_.size() - 1U);
}

Result<std::size_t> AssemblyDocument::add_member_part(DocumentId part_document) {
  if (part_document.empty()) {
    return Result<std::size_t>::failure(
        Error::validation(id_.value(), "member part document id must not be empty"));
  }
  if (part_document == id_) {
    return Result<std::size_t>::failure(
        Error::validation(id_.value(), "assembly document cannot contain itself"));
  }
  if (has_member_part(part_document)) {
    return Result<std::size_t>::failure(Error::validation(
        part_document.value(), "member part must be unique within assembly document"));
  }
  member_parts_.push_back(std::move(part_document));
  return Result<std::size_t>::success(member_parts_.size() - 1U);
}

Result<std::size_t> AssemblyDocument::add_binding(ParameterBinding binding) {
  if (has_binding_id(binding.id())) {
    return Result<std::size_t>::failure(Error::validation(
        binding.id().value(), "parameter binding id must be unique within assembly document"));
  }
  if (!has_member_part(binding.part_document())) {
    return Result<std::size_t>::failure(Error::validation(
        binding.id().value(), "parameter binding part must be a member of the assembly document"));
  }
  if (find_parameter(binding.assembly_parameter()) == nullptr) {
    return Result<std::size_t>::failure(
        Error::validation(binding.id().value(), "parameter binding assembly parameter must exist"));
  }
  const auto duplicate = std::find_if(
      bindings_.begin(), bindings_.end(), [&binding](const ParameterBinding& existing) {
        return existing.part_document() == binding.part_document() &&
               existing.part_parameter() == binding.part_parameter();
      });
  if (duplicate != bindings_.end()) {
    return Result<std::size_t>::failure(Error::validation(
        binding.id().value(), "part parameter is already bound to an assembly parameter"));
  }
  bindings_.push_back(std::move(binding));
  return Result<std::size_t>::success(bindings_.size() - 1U);
}

Result<std::size_t> AssemblyDocument::add_component_instance(ComponentInstance instance) {
  if (has_component_instance_id(instance.id())) {
    return Result<std::size_t>::failure(Error::validation(
        instance.id().value(), "component instance id must be unique within assembly document"));
  }
  if (!has_member_part(instance.referenced_part_document())) {
    return Result<std::size_t>::failure(
        Error::validation(instance.id().value(),
                          "component instance referenced part must be an assembly member part"));
  }
  component_instances_.push_back(std::move(instance));
  return Result<std::size_t>::success(component_instances_.size() - 1U);
}

Result<std::size_t> AssemblyDocument::add_constraint(AssemblyConstraint constraint) {
  if (has_constraint_id(constraint.id())) {
    return Result<std::size_t>::failure(Error::validation(
        constraint.id().value(), "assembly constraint id must be unique within assembly document"));
  }
  if (!has_component_instance_id(constraint.target_a().component_instance())) {
    return Result<std::size_t>::failure(Error::validation(
        constraint.id().value(), "assembly constraint target A component instance must exist"));
  }
  if (!has_component_instance_id(constraint.target_b().component_instance())) {
    return Result<std::size_t>::failure(Error::validation(
        constraint.id().value(), "assembly constraint target B component instance must exist"));
  }

  constraints_.push_back(std::move(constraint));
  return Result<std::size_t>::success(constraints_.size() - 1U);
}

Result<std::size_t> AssemblyDocument::add_joint(AssemblyJoint joint) {
  if (has_joint_id(joint.id())) {
    return Result<std::size_t>::failure(Error::validation(
        joint.id().value(), "assembly joint id must be unique within assembly document"));
  }
  if (!has_component_instance_id(joint.target_a().component_instance())) {
    return Result<std::size_t>::failure(Error::validation(
        joint.id().value(), "assembly joint target A component instance must exist"));
  }
  if (!has_component_instance_id(joint.target_b().component_instance())) {
    return Result<std::size_t>::failure(Error::validation(
        joint.id().value(), "assembly joint target B component instance must exist"));
  }

  joints_.push_back(std::move(joint));
  return Result<std::size_t>::success(joints_.size() - 1U);
}

Result<std::size_t> AssemblyDocument::set_component_instance_transform(ComponentInstanceId id,
                                                                       RigidTransform transform) {
  return update_component_instance(component_instances_, std::move(id),
                                   [transform](const ComponentInstance& instance) {
                                     return instance.with_transform(transform);
                                   });
}

Result<std::size_t>
AssemblyDocument::set_component_instance_visibility(ComponentInstanceId id,
                                                    ComponentVisibility visibility) {
  return update_component_instance(component_instances_, std::move(id),
                                   [visibility](const ComponentInstance& instance) {
                                     return instance.with_visibility(visibility);
                                   });
}

Result<std::size_t> AssemblyDocument::set_component_instance_suppression_state(
    ComponentInstanceId id, ComponentSuppressionState suppression_state) {
  return update_component_instance(component_instances_, std::move(id),
                                   [suppression_state](const ComponentInstance& instance) {
                                     return instance.with_suppression_state(suppression_state);
                                   });
}

Result<std::size_t>
AssemblyDocument::set_component_instance_grounding_state(ComponentInstanceId id,
                                                         ComponentGroundingState grounding_state) {
  return update_component_instance(component_instances_, std::move(id),
                                   [grounding_state](const ComponentInstance& instance) {
                                     return instance.with_grounding_state(grounding_state);
                                   });
}

Result<std::size_t> AssemblyDocument::set_joint_coordinate(AssemblyJointId id,
                                                           Quantity coordinate) {
  return set_joint_coordinate_value(id, AssemblyJointCoordinateRole::Rotation,
                                    std::move(coordinate));
}

Result<std::size_t> AssemblyDocument::set_joint_coordinate_value(AssemblyJointId id,
                                                                 AssemblyJointCoordinateRole role,
                                                                 Quantity coordinate) {
  if (id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation("assembly_joint", "assembly joint id must not be empty"));
  }
  for (std::size_t index = 0U; index < joints_.size(); ++index) {
    if (joints_[index].id() == id) {
      auto updated = joints_[index].with_coordinate_value(role, std::move(coordinate));
      if (updated.has_error()) {
        return Result<std::size_t>::failure(updated.error());
      }
      joints_[index] = std::move(updated.value());
      return Result<std::size_t>::success(index);
    }
  }
  return Result<std::size_t>::failure(
      Error::validation(id.value(), "assembly joint must exist in assembly document"));
}

Result<std::size_t> AssemblyDocument::set_parameter_value(ParameterId id, Quantity value) {
  if (id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation("parameter", "parameter id must not be empty"));
  }
  for (std::size_t index = 0U; index < parameters_.size(); ++index) {
    if (parameters_[index].id() == id) {
      auto updated = parameters_[index].with_value(value);
      if (updated.has_error()) {
        return Result<std::size_t>::failure(updated.error());
      }
      parameters_[index] = std::move(updated.value());
      return Result<std::size_t>::success(index);
    }
  }
  return Result<std::size_t>::failure(
      Error::validation(id.value(), "parameter must exist in assembly document"));
}

Result<BindingApplication> AssemblyDocument::apply_bindings_to(PartDocument& part) const {
  if (!has_member_part(part.id())) {
    return Result<BindingApplication>::failure(Error::validation(
        part.id().value(), "part document must be a member of the assembly document"));
  }

  BindingApplication application;
  for (const ParameterBinding& binding : bindings_) {
    if (binding.part_document() != part.id()) {
      continue;
    }

    const Parameter* assembly_parameter = find_parameter(binding.assembly_parameter());
    if (assembly_parameter == nullptr) {
      return Result<BindingApplication>::failure(Error::validation(
          binding.id().value(), "parameter binding assembly parameter must exist"));
    }

    const Parameter* part_parameter = part.find_parameter(binding.part_parameter());
    if (part_parameter == nullptr) {
      return Result<BindingApplication>::failure(Error::validation(
          binding.id().value(), "parameter binding part parameter must exist in part document"));
    }
    if (part_parameter->type() != assembly_parameter->type()) {
      return Result<BindingApplication>::failure(Error::validation(
          binding.id().value(),
          "parameter binding requires matching parameter types in assembly and part"));
    }

    auto affected = part.set_parameter_value(binding.part_parameter(), assembly_parameter->value());
    if (affected.has_error()) {
      return Result<BindingApplication>::failure(affected.error());
    }

    ++application.applied_binding_count;
    for (auto& node : affected.value()) {
      application.affected_part_nodes.push_back(std::move(node));
    }
  }

  return Result<BindingApplication>::success(std::move(application));
}

const DocumentId& AssemblyDocument::id() const noexcept {
  return id_;
}
const std::string& AssemblyDocument::name() const noexcept {
  return name_;
}
const std::vector<Parameter>& AssemblyDocument::parameters() const noexcept {
  return parameters_;
}
const std::vector<DocumentId>& AssemblyDocument::member_parts() const noexcept {
  return member_parts_;
}
const std::vector<ParameterBinding>& AssemblyDocument::bindings() const noexcept {
  return bindings_;
}
const std::vector<ComponentInstance>& AssemblyDocument::component_instances() const noexcept {
  return component_instances_;
}
const std::vector<AssemblyConstraint>& AssemblyDocument::constraints() const noexcept {
  return constraints_;
}
const std::vector<AssemblyJoint>& AssemblyDocument::joints() const noexcept {
  return joints_;
}
std::size_t AssemblyDocument::parameter_count() const noexcept {
  return parameters_.size();
}
std::size_t AssemblyDocument::member_part_count() const noexcept {
  return member_parts_.size();
}
std::size_t AssemblyDocument::binding_count() const noexcept {
  return bindings_.size();
}
std::size_t AssemblyDocument::component_instance_count() const noexcept {
  return component_instances_.size();
}
std::size_t AssemblyDocument::constraint_count() const noexcept {
  return constraints_.size();
}
std::size_t AssemblyDocument::joint_count() const noexcept {
  return joints_.size();
}

const Parameter* AssemblyDocument::find_parameter(ParameterId id) const noexcept {
  for (const auto& parameter : parameters_) {
    if (parameter.id() == id) {
      return &parameter;
    }
  }
  return nullptr;
}

const ParameterBinding* AssemblyDocument::find_binding(ParameterBindingId id) const noexcept {
  for (const auto& binding : bindings_) {
    if (binding.id() == id) {
      return &binding;
    }
  }
  return nullptr;
}

const ComponentInstance*
AssemblyDocument::find_component_instance(ComponentInstanceId id) const noexcept {
  for (const auto& instance : component_instances_) {
    if (instance.id() == id) {
      return &instance;
    }
  }
  return nullptr;
}

const AssemblyConstraint*
AssemblyDocument::find_constraint(AssemblyConstraintId id) const noexcept {
  for (const auto& constraint : constraints_) {
    if (constraint.id() == id) {
      return &constraint;
    }
  }
  return nullptr;
}

const AssemblyJoint* AssemblyDocument::find_joint(AssemblyJointId id) const noexcept {
  for (const auto& joint : joints_) {
    if (joint.id() == id) {
      return &joint;
    }
  }
  return nullptr;
}

bool AssemblyDocument::has_member_part(const DocumentId& part_document) const noexcept {
  return std::find(member_parts_.begin(), member_parts_.end(), part_document) !=
         member_parts_.end();
}

bool AssemblyDocument::has_parameter_id(const ParameterId& id) const noexcept {
  return find_parameter(id) != nullptr;
}

bool AssemblyDocument::has_parameter_name(std::string_view name) const noexcept {
  for (const auto& parameter : parameters_) {
    if (parameter.name() == name) {
      return true;
    }
  }
  return false;
}

bool AssemblyDocument::has_binding_id(const ParameterBindingId& id) const noexcept {
  return find_binding(id) != nullptr;
}

bool AssemblyDocument::has_component_instance_id(const ComponentInstanceId& id) const noexcept {
  return find_component_instance(id) != nullptr;
}

bool AssemblyDocument::has_constraint_id(const AssemblyConstraintId& id) const noexcept {
  return find_constraint(id) != nullptr;
}

bool AssemblyDocument::has_joint_id(const AssemblyJointId& id) const noexcept {
  return find_joint(id) != nullptr;
}

} // namespace blcad
