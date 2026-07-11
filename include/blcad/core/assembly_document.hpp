#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

// Declares that a part parameter follows an assembly parameter. The binding is
// model intent stored in the assembly document; value propagation happens
// through AssemblyDocument::apply_bindings_to.
class ParameterBinding {
public:
  [[nodiscard]] static Result<ParameterBinding> create(ParameterBindingId id,
                                                       DocumentId part_document,
                                                       ParameterId part_parameter,
                                                       ParameterId assembly_parameter);

  [[nodiscard]] const ParameterBindingId& id() const noexcept;
  [[nodiscard]] const DocumentId& part_document() const noexcept;
  [[nodiscard]] const ParameterId& part_parameter() const noexcept;
  [[nodiscard]] const ParameterId& assembly_parameter() const noexcept;

private:
  ParameterBinding(ParameterBindingId id, DocumentId part_document, ParameterId part_parameter,
                   ParameterId assembly_parameter);

  ParameterBindingId id_;
  DocumentId part_document_;
  ParameterId part_parameter_;
  ParameterId assembly_parameter_;
};

// Result of applying assembly bindings to one part document: the bindings that
// changed a part parameter and the affected dependency-graph nodes reported by
// the part.
struct BindingApplication {
  std::size_t applied_binding_count = 0U;
  std::vector<std::string> affected_part_nodes;
};

struct RigidTransform {
  Vector3 translation_mm;
  Vector3 rotation_deg;

  friend bool operator==(const RigidTransform&, const RigidTransform&) = default;
};

[[nodiscard]] RigidTransform identity_rigid_transform() noexcept;

enum class ComponentVisibility { Visible, Hidden };
[[nodiscard]] std::string_view to_string(ComponentVisibility visibility) noexcept;

enum class ComponentSuppressionState { Active, Suppressed };
[[nodiscard]] std::string_view to_string(ComponentSuppressionState state) noexcept;

enum class ComponentGroundingState { Free, Grounded };
[[nodiscard]] std::string_view to_string(ComponentGroundingState state) noexcept;

class ComponentInstance {
public:
  [[nodiscard]] static Result<ComponentInstance> create(
      ComponentInstanceId id,
      std::string name,
      DocumentId referenced_part_document,
      ComponentVisibility visibility = ComponentVisibility::Visible,
      ComponentSuppressionState suppression_state = ComponentSuppressionState::Active,
      ComponentGroundingState grounding_state = ComponentGroundingState::Free,
      RigidTransform transform = identity_rigid_transform());

  // Copy-style updates preserve component identity and referenced part model
  // intent while replacing one explicit free-placement/state field.
  [[nodiscard]] Result<ComponentInstance> with_transform(RigidTransform transform) const;
  [[nodiscard]] Result<ComponentInstance> with_visibility(ComponentVisibility visibility) const;
  [[nodiscard]] Result<ComponentInstance>
  with_suppression_state(ComponentSuppressionState suppression_state) const;
  [[nodiscard]] Result<ComponentInstance>
  with_grounding_state(ComponentGroundingState grounding_state) const;

  [[nodiscard]] const ComponentInstanceId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const DocumentId& referenced_part_document() const noexcept;
  [[nodiscard]] ComponentVisibility visibility() const noexcept;
  [[nodiscard]] ComponentSuppressionState suppression_state() const noexcept;
  [[nodiscard]] ComponentGroundingState grounding_state() const noexcept;
  [[nodiscard]] const RigidTransform& transform() const noexcept;

private:
  ComponentInstance(ComponentInstanceId id,
                    std::string name,
                    DocumentId referenced_part_document,
                    ComponentVisibility visibility,
                    ComponentSuppressionState suppression_state,
                    ComponentGroundingState grounding_state,
                    RigidTransform transform);

  ComponentInstanceId id_;
  std::string name_;
  DocumentId referenced_part_document_;
  ComponentVisibility visibility_;
  ComponentSuppressionState suppression_state_;
  ComponentGroundingState grounding_state_;
  RigidTransform transform_;
};

// MVP-4/5 assembly document: owns assembly-scoped parameters, member parts,
// parameter bindings, component occurrence placement/state intent, geometric
// assembly constraints, and solver-independent joint/limit intent. Storage does
// not resolve semantic geometry or run placement/motion solving.
class AssemblyDocument {
public:
  [[nodiscard]] static Result<AssemblyDocument> create(DocumentId id, std::string name);

  // Only assembly-scoped parameters are allowed.
  [[nodiscard]] Result<std::size_t> add_parameter(Parameter parameter);
  [[nodiscard]] Result<std::size_t> add_member_part(DocumentId part_document);
  // The binding target part must be a registered member and the assembly
  // parameter must exist. The part parameter is validated on application.
  [[nodiscard]] Result<std::size_t> add_binding(ParameterBinding binding);
  // The component instance must reference an already registered member part.
  [[nodiscard]] Result<std::size_t> add_component_instance(ComponentInstance instance);
  // Constraint and joint endpoints must reference component instances already
  // owned by this assembly. Adding either record never changes transforms.
  [[nodiscard]] Result<std::size_t> add_constraint(AssemblyConstraint constraint);
  [[nodiscard]] Result<std::size_t> add_joint(AssemblyJoint joint);
  // Explicit free-placement/state edits. These do not infer constraints, solve
  // transforms, enforce grounding, or trigger part recompute.
  [[nodiscard]] Result<std::size_t>
  set_component_instance_transform(ComponentInstanceId id, RigidTransform transform);
  [[nodiscard]] Result<std::size_t>
  set_component_instance_visibility(ComponentInstanceId id, ComponentVisibility visibility);
  [[nodiscard]] Result<std::size_t> set_component_instance_suppression_state(
      ComponentInstanceId id, ComponentSuppressionState suppression_state);
  [[nodiscard]] Result<std::size_t>
  set_component_instance_grounding_state(ComponentInstanceId id,
                                         ComponentGroundingState grounding_state);
  // Explicit authored joint-coordinate update; geometry movement remains an
  // application-layer operation through the motion solve/application boundary.
  [[nodiscard]] Result<std::size_t> set_joint_coordinate(AssemblyJointId id, Quantity coordinate);
  // Sets an assembly parameter value with the same validation as creation.
  [[nodiscard]] Result<std::size_t> set_parameter_value(ParameterId id, Quantity value);

  // Pushes bound assembly values into the given part document. Bound part
  // parameters must exist and have the same parameter type as the assembly
  // parameter. Bindings for other parts are skipped. Returns which bindings
  // were applied and the affected part graph nodes.
  [[nodiscard]] Result<BindingApplication> apply_bindings_to(PartDocument& part) const;

  [[nodiscard]] const DocumentId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<Parameter>& parameters() const noexcept;
  [[nodiscard]] const std::vector<DocumentId>& member_parts() const noexcept;
  [[nodiscard]] const std::vector<ParameterBinding>& bindings() const noexcept;
  [[nodiscard]] const std::vector<ComponentInstance>& component_instances() const noexcept;
  [[nodiscard]] const std::vector<AssemblyConstraint>& constraints() const noexcept;
  [[nodiscard]] const std::vector<AssemblyJoint>& joints() const noexcept;
  [[nodiscard]] std::size_t parameter_count() const noexcept;
  [[nodiscard]] std::size_t member_part_count() const noexcept;
  [[nodiscard]] std::size_t binding_count() const noexcept;
  [[nodiscard]] std::size_t component_instance_count() const noexcept;
  [[nodiscard]] std::size_t constraint_count() const noexcept;
  [[nodiscard]] std::size_t joint_count() const noexcept;
  [[nodiscard]] const Parameter* find_parameter(ParameterId id) const noexcept;
  [[nodiscard]] const ParameterBinding* find_binding(ParameterBindingId id) const noexcept;
  [[nodiscard]] const ComponentInstance*
  find_component_instance(ComponentInstanceId id) const noexcept;
  [[nodiscard]] const AssemblyConstraint*
  find_constraint(AssemblyConstraintId id) const noexcept;
  [[nodiscard]] const AssemblyJoint* find_joint(AssemblyJointId id) const noexcept;
  [[nodiscard]] bool has_member_part(const DocumentId& part_document) const noexcept;

private:
  AssemblyDocument(DocumentId id, std::string name);

  [[nodiscard]] bool has_parameter_id(const ParameterId& id) const noexcept;
  [[nodiscard]] bool has_parameter_name(std::string_view name) const noexcept;
  [[nodiscard]] bool has_binding_id(const ParameterBindingId& id) const noexcept;
  [[nodiscard]] bool has_component_instance_id(const ComponentInstanceId& id) const noexcept;
  [[nodiscard]] bool has_constraint_id(const AssemblyConstraintId& id) const noexcept;
  [[nodiscard]] bool has_joint_id(const AssemblyJointId& id) const noexcept;

  DocumentId id_;
  std::string name_;
  std::vector<Parameter> parameters_;
  std::vector<DocumentId> member_parts_;
  std::vector<ParameterBinding> bindings_;
  std::vector<ComponentInstance> component_instances_;
  std::vector<AssemblyConstraint> constraints_;
  std::vector<AssemblyJoint> joints_;
};

} // namespace blcad
