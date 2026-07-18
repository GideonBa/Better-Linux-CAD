#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/assembly_joint.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_joint_target_compatibility.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_solve_diagnostics.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::gui {

inline constexpr std::string_view kAssemblyPlacementTranslatePrefix =
    "assembly.placement.translate";
inline constexpr std::string_view kAssemblyPlacementRotatePrefix = "assembly.placement.rotate";
inline constexpr std::string_view kAssemblyMotionHandleId = "assembly.motion.coordinate";

enum class GuiAssemblyPlacementKind { Component, Subassembly };
enum class GuiAssemblyPlacementMode { Insert, Edit };

struct GuiAssemblyPlacementPreview {
  GuiAssemblyPlacementKind kind{GuiAssemblyPlacementKind::Component};
  GuiAssemblyPlacementMode mode{GuiAssemblyPlacementMode::Insert};
  RigidTransform transform;
};

// Candidate-only component/subassembly placement. Insert automatically registers
// a selected project-owned Part as a root member in the same transaction.
class GuiInteractiveAssemblyPlacementController {
public:
  [[nodiscard]] Result<std::size_t> begin_insert_component(const GuiDocumentSession& session,
                                                           ComponentInstanceId id, std::string name,
                                                           DocumentId part) {
    const auto* project = session.project();
    if (project == nullptr)
      return fail("component placement requires an active Project");
    if (project->find_part_document(part) == nullptr)
      return fail("component placement requires a project-owned Part definition");
    reset(GuiAssemblyPlacementKind::Component, GuiAssemblyPlacementMode::Insert);
    component_id_ = std::move(id);
    name_ = std::move(name);
    definition_ = std::move(part);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> begin_insert_subassembly(const GuiDocumentSession& session,
                                                             SubassemblyInstanceId id,
                                                             std::string name,
                                                             DocumentId assembly) {
    const auto* project = session.project();
    if (project == nullptr)
      return fail("subassembly placement requires an active Project");
    const auto* definition = project->find_assembly_document(assembly);
    if (definition == nullptr || definition->id() == project->assembly().id())
      return fail("subassembly placement requires a project-owned child Assembly definition");
    reset(GuiAssemblyPlacementKind::Subassembly, GuiAssemblyPlacementMode::Insert);
    subassembly_id_ = std::move(id);
    name_ = std::move(name);
    definition_ = std::move(assembly);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> begin_edit_component(const GuiDocumentSession& session,
                                                         ComponentInstanceId id) {
    const auto* project = session.project();
    if (project == nullptr)
      return fail("component placement requires an active Project");
    const auto* instance = project->assembly().find_component_instance(id);
    if (instance == nullptr)
      return fail("component occurrence to place does not exist");
    reset(GuiAssemblyPlacementKind::Component, GuiAssemblyPlacementMode::Edit);
    component_id_ = std::move(id);
    name_ = instance->name();
    definition_ = instance->referenced_part_document();
    transform_ = instance->transform();
    visibility_ = instance->visibility();
    suppression_ = instance->suppression_state();
    grounding_ = instance->grounding_state();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> begin_edit_subassembly(const GuiDocumentSession& session,
                                                           SubassemblyInstanceId id) {
    const auto* project = session.project();
    if (project == nullptr)
      return fail("subassembly placement requires an active Project");
    const auto* instance = project->assembly().find_subassembly_instance(id);
    if (instance == nullptr)
      return fail("subassembly occurrence to place does not exist");
    reset(GuiAssemblyPlacementKind::Subassembly, GuiAssemblyPlacementMode::Edit);
    subassembly_id_ = std::move(id);
    name_ = instance->name();
    definition_ = instance->referenced_assembly_document();
    transform_ = instance->transform();
    visibility_ = instance->visibility();
    suppression_ = instance->suppression_state();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept {
    return active_;
  }
  [[nodiscard]] GuiAssemblyPlacementKind kind() const noexcept {
    return kind_;
  }
  [[nodiscard]] GuiAssemblyPlacementMode mode() const noexcept {
    return mode_;
  }
  [[nodiscard]] const RigidTransform& transform() const noexcept {
    return transform_;
  }
  [[nodiscard]] ComponentVisibility visibility() const noexcept {
    return visibility_;
  }
  [[nodiscard]] ComponentSuppressionState suppression() const noexcept {
    return suppression_;
  }
  [[nodiscard]] ComponentGroundingState grounding() const noexcept {
    return grounding_;
  }

  void set_transform(RigidTransform value) noexcept {
    transform_ = value;
  }
  void set_visibility(ComponentVisibility value) noexcept {
    visibility_ = value;
  }
  void set_suppression(ComponentSuppressionState value) noexcept {
    suppression_ = value;
  }
  void set_grounding(ComponentGroundingState value) noexcept {
    grounding_ = value;
  }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    if (!active_)
      return {};
    const Point3 origin{transform_.translation_mm.x, transform_.translation_mm.y,
                        transform_.translation_mm.z};
    auto result =
        GuiViewportManipulatorLayer::translation_triad(kAssemblyPlacementTranslatePrefix, origin);
    auto rotations =
        GuiViewportManipulatorLayer::rotation_triad(kAssemblyPlacementRotatePrefix, origin);
    for (auto& handle : result) {
      if (handle.id.ends_with(".x"))
        handle.reference_value = transform_.translation_mm.x;
      else if (handle.id.ends_with(".y"))
        handle.reference_value = transform_.translation_mm.y;
      else
        handle.reference_value = transform_.translation_mm.z;
    }
    for (auto& handle : rotations) {
      if (handle.id.ends_with(".x"))
        handle.reference_value = transform_.rotation_deg.x;
      else if (handle.id.ends_with(".y"))
        handle.reference_value = transform_.rotation_deg.y;
      else
        handle.reference_value = transform_.rotation_deg.z;
      result.push_back(std::move(handle));
    }
    return result;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (!active_)
      return false;
    if (candidate.value_kind == GuiViewportManipulatorValueKind::Translation)
      return set_axis(candidate.handle_id, kAssemblyPlacementTranslatePrefix,
                      candidate.scalar_value, transform_.translation_mm);
    if (candidate.value_kind == GuiViewportManipulatorValueKind::Rotation)
      return set_axis(candidate.handle_id, kAssemblyPlacementRotatePrefix, candidate.scalar_value,
                      transform_.rotation_deg);
    return false;
  }

  [[nodiscard]] Result<GuiAssemblyPlacementPreview>
  preview(const GuiDocumentSession& session) const {
    if (!active_)
      return preview_fail("no Assembly placement is active");
    const auto* project = session.project();
    if (project == nullptr)
      return preview_fail("placement preview requires an active Project");
    Project candidate = *project;
    auto changed = mutate(candidate);
    if (changed.has_error())
      return Result<GuiAssemblyPlacementPreview>::failure(changed.error());
    auto valid = candidate.validate_assembly_structure();
    if (valid.has_error())
      return Result<GuiAssemblyPlacementPreview>::failure(valid.error());
    return Result<GuiAssemblyPlacementPreview>::success({kind_, mode_, transform_});
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto checked = preview(session);
    if (checked.has_error())
      return Result<std::size_t>::failure(checked.error());
    auto committed = session.commit_project_transaction(
        mode_ == GuiAssemblyPlacementMode::Insert ? "Insert Assembly occurrence"
                                                  : "Set Assembly occurrence placement",
        [this](Project& project) { return mutate(project); });
    if (!committed.has_error())
      active_ = false;
    return committed;
  }

  void cancel() noexcept {
    active_ = false;
  }

private:
  void reset(GuiAssemblyPlacementKind kind, GuiAssemblyPlacementMode mode) noexcept {
    kind_ = kind;
    mode_ = mode;
    transform_ = identity_rigid_transform();
    visibility_ = ComponentVisibility::Visible;
    suppression_ = ComponentSuppressionState::Active;
    grounding_ = ComponentGroundingState::Free;
    active_ = true;
  }

  [[nodiscard]] static bool set_axis(std::string_view id, std::string_view prefix, double value,
                                     Vector3& vector) noexcept {
    if (id == std::string(prefix) + ".x")
      vector.x = value;
    else if (id == std::string(prefix) + ".y")
      vector.y = value;
    else if (id == std::string(prefix) + ".z")
      vector.z = value;
    else
      return false;
    return true;
  }

  [[nodiscard]] Result<std::size_t> mutate(Project& project) const {
    auto& assembly = project.assembly();
    if (kind_ == GuiAssemblyPlacementKind::Component) {
      if (mode_ == GuiAssemblyPlacementMode::Edit) {
        std::size_t changed = 0U;
        for (auto result :
             {assembly.set_component_instance_transform(component_id_, transform_),
              assembly.set_component_instance_visibility(component_id_, visibility_),
              assembly.set_component_instance_suppression_state(component_id_, suppression_),
              assembly.set_component_instance_grounding_state(component_id_, grounding_)}) {
          if (result.has_error())
            return result;
          changed += result.value();
        }
        return Result<std::size_t>::success(changed);
      }
      std::size_t changed = 0U;
      if (!assembly.has_member_part(definition_)) {
        auto member = assembly.add_member_part(definition_);
        if (member.has_error())
          return member;
        changed += member.value();
      }
      auto instance = ComponentInstance::create(component_id_, name_, definition_, visibility_,
                                                suppression_, grounding_, transform_);
      if (instance.has_error())
        return Result<std::size_t>::failure(instance.error());
      auto added = assembly.add_component_instance(std::move(instance.value()));
      if (added.has_error())
        return added;
      return Result<std::size_t>::success(changed + added.value());
    }
    if (mode_ == GuiAssemblyPlacementMode::Edit) {
      std::size_t changed = 0U;
      for (auto result :
           {assembly.set_subassembly_instance_transform(subassembly_id_, transform_),
            assembly.set_subassembly_instance_visibility(subassembly_id_, visibility_),
            assembly.set_subassembly_instance_suppression_state(subassembly_id_, suppression_)}) {
        if (result.has_error())
          return result;
        changed += result.value();
      }
      return Result<std::size_t>::success(changed);
    }
    auto instance = SubassemblyInstance::create(subassembly_id_, name_, definition_, visibility_,
                                                suppression_, transform_);
    if (instance.has_error())
      return Result<std::size_t>::failure(instance.error());
    return assembly.add_subassembly_instance(std::move(instance.value()));
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_assembly_placement", std::move(message)));
  }
  [[nodiscard]] static Result<GuiAssemblyPlacementPreview> preview_fail(std::string message) {
    return Result<GuiAssemblyPlacementPreview>::failure(
        Error::validation("gui.interactive_assembly_placement", std::move(message)));
  }

  bool active_{false};
  GuiAssemblyPlacementKind kind_{GuiAssemblyPlacementKind::Component};
  GuiAssemblyPlacementMode mode_{GuiAssemblyPlacementMode::Insert};
  ComponentInstanceId component_id_{""};
  SubassemblyInstanceId subassembly_id_{""};
  std::string name_;
  DocumentId definition_{""};
  RigidTransform transform_{};
  ComponentVisibility visibility_{ComponentVisibility::Visible};
  ComponentSuppressionState suppression_{ComponentSuppressionState::Active};
  ComponentGroundingState grounding_{ComponentGroundingState::Free};
};

struct GuiAssemblyRelationshipPreview {
  AssemblyConstraint relationship;
  geometry::AssemblyTargetCompatibility compatibility;
  geometry::AssemblySolveDiagnostics diagnostics;
  geometry::AssemblySolveResult solved_pose;
};

// Two-stage semantic relationship picking. Every second-target query and final
// selection goes through the Block-37 capability matrix.
class GuiInteractiveAssemblyRelationshipController {
public:
  [[nodiscard]] Result<std::size_t> begin(const GuiDocumentSession& session,
                                          AssemblyConstraintId id, std::string name,
                                          AssemblyConstraintType type,
                                          std::optional<Quantity> distance = std::nullopt,
                                          std::optional<Quantity> angle = std::nullopt) {
    if (session.project() == nullptr)
      return fail("interactive relationship requires an active Project");
    id_ = std::move(id);
    name_ = std::move(name);
    type_ = type;
    distance_ = std::move(distance);
    angle_ = std::move(angle);
    target_a_.reset();
    target_b_.reset();
    resolved_a_.reset();
    compatibility_.reset();
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept {
    return active_;
  }
  [[nodiscard]] bool has_first_target() const noexcept {
    return target_a_.has_value();
  }
  [[nodiscard]] bool has_second_target() const noexcept {
    return target_b_.has_value();
  }

  [[nodiscard]] Result<std::size_t> select_first(const GuiDocumentSession& session,
                                                 AssemblyConstraintTarget target) {
    auto resolved = resolve(session, target);
    if (resolved.has_error())
      return Result<std::size_t>::failure(resolved.error());
    target_a_ = std::move(target);
    resolved_a_ = std::move(resolved.value());
    target_b_.reset();
    compatibility_.reset();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] std::vector<AssemblyConstraintTarget>
  compatible_second_targets(const GuiDocumentSession& session,
                            const std::vector<AssemblyConstraintTarget>& candidates) const {
    std::vector<AssemblyConstraintTarget> compatible;
    if (!active_ || !resolved_a_)
      return compatible;
    for (const auto& candidate : candidates) {
      if (target_a_ && candidate.component_instance() == target_a_->component_instance())
        continue;
      auto resolved = resolve(session, candidate);
      if (!resolved.has_error() && !geometry::AssemblyTargetCompatibilityResolver{}
                                        .resolve(type_, *resolved_a_, resolved.value())
                                        .has_error())
        compatible.push_back(candidate);
    }
    return compatible;
  }

  [[nodiscard]] Result<std::size_t> select_second(const GuiDocumentSession& session,
                                                  AssemblyConstraintTarget target) {
    if (!active_ || !resolved_a_)
      return fail("select the first relationship target first");
    if (target.component_instance() == target_a_->component_instance())
      return fail("relationship targets must belong to different component occurrences");
    auto resolved = resolve(session, target);
    if (resolved.has_error())
      return Result<std::size_t>::failure(resolved.error());
    auto compatibility = geometry::AssemblyTargetCompatibilityResolver{}.resolve(
        type_, *resolved_a_, resolved.value());
    if (compatibility.has_error())
      return Result<std::size_t>::failure(compatibility.error());
    target_b_ = std::move(target);
    compatibility_ = compatibility.value();
    return Result<std::size_t>::success(2);
  }

  [[nodiscard]] Result<GuiAssemblyRelationshipPreview>
  preview(const GuiDocumentSession& session) const {
    if (!active_ || !target_a_ || !target_b_ || !compatibility_)
      return preview_fail("relationship preview requires two compatible targets");
    const auto* project = session.project();
    if (project == nullptr)
      return preview_fail("relationship preview requires an active Project");
    auto relationship = build();
    if (relationship.has_error())
      return Result<GuiAssemblyRelationshipPreview>::failure(relationship.error());
    Project candidate = *project;
    auto added = candidate.assembly().add_constraint(relationship.value());
    if (added.has_error())
      return Result<GuiAssemblyRelationshipPreview>::failure(added.error());
    const std::vector<ComponentInstanceId> group{target_a_->component_instance(),
                                                 target_b_->component_instance()};
    auto diagnostics = geometry::AssemblySolveDiagnosticsAnalyzer{}.analyze(candidate, group);
    if (diagnostics.has_error())
      return Result<GuiAssemblyRelationshipPreview>::failure(diagnostics.error());
    auto solved = geometry::AssemblyRigidBodySolver{}.solve(candidate, group);
    if (solved.has_error())
      return Result<GuiAssemblyRelationshipPreview>::failure(solved.error());
    if (!solved.value().converged())
      return preview_fail("relationship candidate solve did not converge");
    return Result<GuiAssemblyRelationshipPreview>::success(
        {std::move(relationship.value()), *compatibility_, std::move(diagnostics.value()),
         std::move(solved.value())});
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto candidate = preview(session);
    if (candidate.has_error())
      return Result<std::size_t>::failure(candidate.error());
    auto committed = session.commit_project_transaction(
        "Apply Assembly relationship",
        [preview = candidate.value()](Project& project) mutable -> Result<std::size_t> {
          auto added = project.assembly().add_constraint(std::move(preview.relationship));
          if (added.has_error())
            return added;
          auto posed = geometry::AssemblySolveResultApplier{}.apply(project, preview.solved_pose);
          if (posed.has_error())
            return posed;
          return Result<std::size_t>::success(added.value() + posed.value());
        });
    if (!committed.has_error())
      active_ = false;
    return committed;
  }

  void cancel() noexcept {
    active_ = false;
  }

private:
  [[nodiscard]] Result<geometry::AssemblyResolvedGeometricTarget>
  resolve(const GuiDocumentSession& session, const AssemblyConstraintTarget& target) const {
    if (!active_)
      return Result<geometry::AssemblyResolvedGeometricTarget>::failure(
          Error::validation("gui.interactive_assembly_relationship", "no relationship is active"));
    if (session.project() == nullptr)
      return Result<geometry::AssemblyResolvedGeometricTarget>::failure(Error::validation(
          "gui.interactive_assembly_relationship", "an active Project is required"));
    return geometry::AssemblyConstraintTargetResolver{}.resolve_geometric(*session.project(),
                                                                          target);
  }

  [[nodiscard]] Result<AssemblyConstraint> build() const {
    return AssemblyConstraint::create(id_, name_, type_, *target_a_, *target_b_,
                                      AssemblyConstraintState::Active, distance_, angle_);
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_assembly_relationship", std::move(message)));
  }
  [[nodiscard]] static Result<GuiAssemblyRelationshipPreview> preview_fail(std::string message) {
    return Result<GuiAssemblyRelationshipPreview>::failure(
        Error::validation("gui.interactive_assembly_relationship", std::move(message)));
  }

  bool active_{false};
  AssemblyConstraintId id_{""};
  std::string name_;
  AssemblyConstraintType type_{AssemblyConstraintType::Mate};
  std::optional<Quantity> distance_;
  std::optional<Quantity> angle_;
  std::optional<AssemblyConstraintTarget> target_a_;
  std::optional<AssemblyConstraintTarget> target_b_;
  std::optional<geometry::AssemblyResolvedGeometricTarget> resolved_a_;
  std::optional<geometry::AssemblyTargetCompatibility> compatibility_;
};

struct GuiAssemblyJointPreview {
  AssemblyJoint joint;
  geometry::AssemblyJointTargetCompatibility compatibility;
  geometry::AssemblyResolvedGeometricTarget target_a;
  geometry::AssemblyResolvedGeometricTarget target_b;
  std::optional<geometry::AssemblyFrameTargetDescriptor> frame_a;
  std::optional<geometry::AssemblyFrameTargetDescriptor> frame_b;
};

class GuiInteractiveAssemblyJointController {
public:
  [[nodiscard]] Result<std::size_t>
  begin(const GuiDocumentSession& session, AssemblyJointId id, std::string name,
        AssemblyJointType type, std::vector<AssemblyJointCoordinateSlot> coordinate_slots) {
    if (session.project() == nullptr)
      return fail("interactive joint authoring requires an active Project");
    id_ = std::move(id);
    name_ = std::move(name);
    type_ = type;
    coordinate_slots_ = std::move(coordinate_slots);
    target_a_.reset();
    target_b_.reset();
    resolved_a_.reset();
    resolved_b_.reset();
    compatibility_.reset();
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept {
    return active_;
  }

  [[nodiscard]] Result<std::size_t> select_first(const GuiDocumentSession& session,
                                                 AssemblyConstraintTarget target) {
    auto resolved = resolve(session, target);
    if (resolved.has_error())
      return Result<std::size_t>::failure(resolved.error());
    target_a_ = std::move(target);
    resolved_a_ = std::move(resolved.value());
    target_b_.reset();
    resolved_b_.reset();
    compatibility_.reset();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] std::vector<AssemblyConstraintTarget>
  compatible_second_targets(const GuiDocumentSession& session,
                            const std::vector<AssemblyConstraintTarget>& candidates) const {
    std::vector<AssemblyConstraintTarget> compatible;
    if (!active_ || !resolved_a_)
      return compatible;
    for (const auto& candidate : candidates) {
      if (target_a_ && candidate.component_instance() == target_a_->component_instance())
        continue;
      auto resolved = resolve(session, candidate);
      if (!resolved.has_error() && !geometry::AssemblyJointTargetCompatibilityResolver{}
                                        .resolve(type_, *resolved_a_, resolved.value())
                                        .has_error())
        compatible.push_back(candidate);
    }
    return compatible;
  }

  [[nodiscard]] Result<std::size_t> select_second(const GuiDocumentSession& session,
                                                  AssemblyConstraintTarget target) {
    if (!active_ || !resolved_a_)
      return fail("select the first joint target first");
    if (target.component_instance() == target_a_->component_instance())
      return fail("joint targets must belong to different component occurrences");
    auto resolved = resolve(session, target);
    if (resolved.has_error())
      return Result<std::size_t>::failure(resolved.error());
    auto compatibility = geometry::AssemblyJointTargetCompatibilityResolver{}.resolve(
        type_, *resolved_a_, resolved.value());
    if (compatibility.has_error())
      return Result<std::size_t>::failure(compatibility.error());
    target_b_ = std::move(target);
    resolved_b_ = std::move(resolved.value());
    compatibility_ = compatibility.value();
    return Result<std::size_t>::success(2);
  }

  [[nodiscard]] Result<GuiAssemblyJointPreview> preview(const GuiDocumentSession& session) const {
    if (!active_ || !target_a_ || !target_b_ || !resolved_a_ || !resolved_b_ || !compatibility_)
      return preview_fail("joint preview requires two compatible targets");
    auto joint = AssemblyJoint::create(id_, name_, type_, *target_a_, *target_b_,
                                       AssemblyJointState::Active, coordinate_slots_);
    if (joint.has_error())
      return Result<GuiAssemblyJointPreview>::failure(joint.error());
    const auto* project = session.project();
    if (project == nullptr)
      return preview_fail("joint preview requires an active Project");
    Project candidate = *project;
    auto added = candidate.assembly().add_joint(joint.value());
    if (added.has_error())
      return Result<GuiAssemblyJointPreview>::failure(added.error());
    auto frame_a = frame(*resolved_a_);
    auto frame_b = frame(*resolved_b_);
    return Result<GuiAssemblyJointPreview>::success({std::move(joint.value()), *compatibility_,
                                                     *resolved_a_, *resolved_b_, std::move(frame_a),
                                                     std::move(frame_b)});
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto candidate = preview(session);
    if (candidate.has_error())
      return Result<std::size_t>::failure(candidate.error());
    auto committed = session.commit_project_transaction(
        "Apply Assembly joint", [joint = candidate.value().joint](Project& project) mutable {
          return project.assembly().add_joint(std::move(joint));
        });
    if (!committed.has_error())
      active_ = false;
    return committed;
  }

  void cancel() noexcept {
    active_ = false;
  }

private:
  [[nodiscard]] Result<geometry::AssemblyResolvedGeometricTarget>
  resolve(const GuiDocumentSession& session, const AssemblyConstraintTarget& target) const {
    if (!active_ || session.project() == nullptr)
      return Result<geometry::AssemblyResolvedGeometricTarget>::failure(Error::validation(
          "gui.interactive_assembly_joint", "an active joint and Project are required"));
    return geometry::AssemblyConstraintTargetResolver{}.resolve_geometric(*session.project(),
                                                                          target);
  }

  [[nodiscard]] static std::optional<geometry::AssemblyFrameTargetDescriptor>
  frame(const geometry::AssemblyResolvedGeometricTarget& target) {
    if (const auto* value =
            std::get_if<geometry::AssemblyFrameTargetDescriptor>(&target.descriptor)) {
      const geometry::AssemblyHierarchyTransformEvaluator evaluator;
      return geometry::AssemblyFrameTargetDescriptor{
          evaluator.evaluate_point(target.transforms_inner_to_outer, value->origin),
          evaluator.evaluate_vector(target.transforms_inner_to_outer, value->x_axis),
          evaluator.evaluate_vector(target.transforms_inner_to_outer, value->y_axis),
          evaluator.evaluate_vector(target.transforms_inner_to_outer, value->z_axis)};
    }
    return std::nullopt;
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_assembly_joint", std::move(message)));
  }
  [[nodiscard]] static Result<GuiAssemblyJointPreview> preview_fail(std::string message) {
    return Result<GuiAssemblyJointPreview>::failure(
        Error::validation("gui.interactive_assembly_joint", std::move(message)));
  }

  bool active_{false};
  AssemblyJointId id_{""};
  std::string name_;
  AssemblyJointType type_{AssemblyJointType::Revolute};
  std::vector<AssemblyJointCoordinateSlot> coordinate_slots_;
  std::optional<AssemblyConstraintTarget> target_a_;
  std::optional<AssemblyConstraintTarget> target_b_;
  std::optional<geometry::AssemblyResolvedGeometricTarget> resolved_a_;
  std::optional<geometry::AssemblyResolvedGeometricTarget> resolved_b_;
  std::optional<geometry::AssemblyJointTargetCompatibility> compatibility_;
};

struct GuiAssemblyJointCoordinateControl {
  AssemblyJointCoordinateRole role{AssemblyJointCoordinateRole::Rotation};
  AssemblyJointCoordinateKind kind{AssemblyJointCoordinateKind::Angular};
  double value{0.0};
  std::optional<double> lower_limit;
  std::optional<double> upper_limit;
  bool selected{false};
};

struct GuiAssemblyMotionHudStatus {
  geometry::AssemblySolveState solve_state{geometry::AssemblySolveState::NumericalFailure};
  std::size_t joint_dof{0U};
  std::size_t undriven_dof{0U};
  std::size_t proposed_component_count{0U};
  double final_rms{0.0};
};

struct GuiAssemblyMotionPreview {
  geometry::AssemblyJointMotionResult motion;
  GuiAssemblyMotionHudStatus hud;
};

// Drives exactly one typed coordinate slot. Spherical is intentionally passive.
class GuiInteractiveAssemblyMotionController {
public:
  [[nodiscard]] Result<std::size_t> begin(const GuiDocumentSession& session,
                                          AssemblyJointId joint) {
    const auto* project = session.project();
    if (project == nullptr)
      return fail("interactive joint motion requires an active Project");
    const auto* value = project->assembly().find_joint(joint);
    if (value == nullptr)
      return fail("joint to drive does not exist in the root Assembly");
    if (value->state() != AssemblyJointState::Active)
      return fail("only an active joint can be driven");
    if (value->type() == AssemblyJointType::Spherical)
      return fail("Spherical joints are passive and cannot be selected as a drive");
    if (value->coordinate_slots().empty())
      return fail("joint exposes no coordinate slots");
    joint_ = std::move(joint);
    role_ = value->coordinate_slots().front().role();
    requested_ = value->coordinate_slots().front().value();
    active_ = true;
    return Result<std::size_t>::success(value->coordinate_slots().size());
  }

  [[nodiscard]] bool active() const noexcept {
    return active_;
  }
  [[nodiscard]] AssemblyJointCoordinateRole selected_role() const noexcept {
    return role_;
  }

  [[nodiscard]] std::vector<GuiAssemblyJointCoordinateControl>
  coordinate_controls(const GuiDocumentSession& session) const {
    std::vector<GuiAssemblyJointCoordinateControl> controls;
    const auto* joint = find_joint(session);
    if (joint == nullptr)
      return controls;
    for (const auto& slot : joint->coordinate_slots()) {
      GuiAssemblyJointCoordinateControl control;
      control.role = slot.role();
      control.kind = slot.kind();
      const Quantity& value = slot.role() == role_ ? requested_ : slot.value();
      control.value = scalar(value, slot.kind());
      if (slot.lower_limit())
        control.lower_limit = scalar(*slot.lower_limit(), slot.kind());
      if (slot.upper_limit())
        control.upper_limit = scalar(*slot.upper_limit(), slot.kind());
      control.selected = slot.role() == role_;
      controls.push_back(std::move(control));
    }
    return controls;
  }

  [[nodiscard]] Result<std::size_t> select_coordinate(const GuiDocumentSession& session,
                                                      AssemblyJointCoordinateRole role) {
    const auto* joint = find_joint(session);
    if (joint == nullptr)
      return fail("no joint motion command is active");
    const auto* slot = joint->find_coordinate_slot(role);
    if (slot == nullptr)
      return fail("joint does not expose the selected coordinate role");
    role_ = role;
    requested_ = slot->value();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> set_value(const GuiDocumentSession& session, double value) {
    const auto* slot = selected_slot(session);
    if (slot == nullptr)
      return fail("no joint coordinate is selected");
    auto quantity = slot->kind() == AssemblyJointCoordinateKind::Angular
                        ? Quantity::angle_deg(value, joint_.value())
                        : Quantity::linear_displacement_mm(value, joint_.value());
    if (quantity.has_error())
      return Result<std::size_t>::failure(quantity.error());
    requested_ = quantity.value();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] std::optional<GuiViewportManipulatorHandle>
  handle(const GuiDocumentSession& session, geometry::AssemblyFrameTargetDescriptor frame) const {
    const auto* slot = selected_slot(session);
    if (slot == nullptr)
      return std::nullopt;
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(kAssemblyMotionHandleId);
    handle.origin = frame.origin;
    handle.reference_value = scalar(requested_, slot->kind());
    if (slot->kind() == AssemblyJointCoordinateKind::Angular) {
      handle.kind = GuiViewportManipulatorKind::Angular;
      handle.plane_normal = frame.z_axis;
      handle.reference_direction = frame.x_axis;
    } else {
      handle.kind = GuiViewportManipulatorKind::Linear;
      handle.axis = axis_for(role_, frame);
    }
    if (slot->lower_limit())
      handle.minimum_value = scalar(*slot->lower_limit(), slot->kind());
    if (slot->upper_limit())
      handle.maximum_value = scalar(*slot->upper_limit(), slot->kind());
    return handle;
  }

  [[nodiscard]] bool apply_manipulator(const GuiDocumentSession& session,
                                       const GuiViewportManipulatorCandidate& candidate) {
    const auto* slot = selected_slot(session);
    if (slot == nullptr || candidate.handle_id != kAssemblyMotionHandleId)
      return false;
    const bool correct_kind = (slot->kind() == AssemblyJointCoordinateKind::Angular &&
                               candidate.value_kind == GuiViewportManipulatorValueKind::Angle) ||
                              (slot->kind() == AssemblyJointCoordinateKind::Linear &&
                               candidate.value_kind == GuiViewportManipulatorValueKind::Distance);
    return correct_kind && !set_value(session, candidate.scalar_value).has_error();
  }

  [[nodiscard]] Result<GuiAssemblyMotionPreview> preview(const GuiDocumentSession& session) const {
    const auto* project = session.project();
    const auto* joint = find_joint(session);
    if (project == nullptr || joint == nullptr)
      return preview_fail("joint motion preview requires an active joint and Project");
    geometry::AssemblyJointDrive drive{joint_, {{role_, requested_}}};
    auto motion = geometry::AssemblyJointMotionSolver{}.move(*project, std::move(drive));
    if (motion.has_error())
      return Result<GuiAssemblyMotionPreview>::failure(motion.error());
    if (!motion.value().converged())
      return preview_fail("joint motion candidate did not converge");
    GuiAssemblyMotionHudStatus hud;
    hud.solve_state = motion.value().solve_result.state;
    hud.joint_dof = joint->coordinate_slots().size();
    hud.undriven_dof = hud.joint_dof > 0U ? hud.joint_dof - 1U : 0U;
    hud.proposed_component_count = motion.value().solve_result.proposed_transforms.size();
    hud.final_rms = motion.value().solve_result.residual_summary.final_rms;
    return Result<GuiAssemblyMotionPreview>::success({std::move(motion.value()), hud});
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto candidate = preview(session);
    if (candidate.has_error())
      return Result<std::size_t>::failure(candidate.error());
    auto committed = session.commit_project_transaction(
        "Apply joint motion", [motion = candidate.value().motion](Project& project) {
          return geometry::AssemblyJointMotionResultApplier{}.apply(project, motion);
        });
    if (!committed.has_error())
      active_ = false;
    return committed;
  }

  void cancel() noexcept {
    active_ = false;
  }

private:
  [[nodiscard]] const AssemblyJoint* find_joint(const GuiDocumentSession& session) const noexcept {
    return active_ && session.project() != nullptr
               ? session.project()->assembly().find_joint(joint_)
               : nullptr;
  }
  [[nodiscard]] const AssemblyJointCoordinateSlot*
  selected_slot(const GuiDocumentSession& session) const noexcept {
    const auto* joint = find_joint(session);
    return joint != nullptr ? joint->find_coordinate_slot(role_) : nullptr;
  }
  [[nodiscard]] static double scalar(const Quantity& value,
                                     AssemblyJointCoordinateKind kind) noexcept {
    return kind == AssemblyJointCoordinateKind::Angular ? value.degrees() : value.millimeters();
  }
  [[nodiscard]] static Vector3
  axis_for(AssemblyJointCoordinateRole role,
           const geometry::AssemblyFrameTargetDescriptor& frame) noexcept {
    if (role == AssemblyJointCoordinateRole::TranslationU)
      return frame.x_axis;
    if (role == AssemblyJointCoordinateRole::TranslationV)
      return frame.y_axis;
    return frame.z_axis;
  }
  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_assembly_motion", std::move(message)));
  }
  [[nodiscard]] static Result<GuiAssemblyMotionPreview> preview_fail(std::string message) {
    return Result<GuiAssemblyMotionPreview>::failure(
        Error::validation("gui.interactive_assembly_motion", std::move(message)));
  }

  bool active_{false};
  AssemblyJointId joint_{""};
  AssemblyJointCoordinateRole role_{AssemblyJointCoordinateRole::Rotation};
  Quantity requested_{Quantity::angle_deg(0.0).value()};
};

// Qt-free owner/router used by MainWindow. Command handlers begin a controller
// through the accessors, then call refresh_handles(); all candidates remain
// transient until apply().
class GuiInteractiveAssemblyCoordinator {
public:
  GuiInteractiveAssemblyCoordinator(GuiDocumentSession* session,
                                    GuiViewportManipulatorLayer* layer) noexcept
      : session_(session), layer_(layer) {}

  [[nodiscard]] GuiInteractiveAssemblyPlacementController& placement() noexcept {
    return placement_;
  }
  [[nodiscard]] GuiInteractiveAssemblyRelationshipController& relationship() noexcept {
    return relationship_;
  }
  [[nodiscard]] GuiInteractiveAssemblyJointController& joint() noexcept {
    return joint_;
  }
  [[nodiscard]] GuiInteractiveAssemblyMotionController& motion() noexcept {
    return motion_;
  }
  [[nodiscard]] bool active() const noexcept {
    return placement_.active() || relationship_.active() || joint_.active() || motion_.active();
  }

  void set_motion_frame(geometry::AssemblyFrameTargetDescriptor frame) noexcept {
    motion_frame_ = frame;
    refresh_handles();
  }

  void refresh_handles() {
    if (layer_ == nullptr)
      return;
    if (placement_.active()) {
      (void)layer_->set_handles(placement_.handles());
      return;
    }
    if (motion_.active() && session_ != nullptr && motion_frame_) {
      std::vector<GuiViewportManipulatorHandle> handles;
      if (auto handle = motion_.handle(*session_, *motion_frame_))
        handles.push_back(std::move(*handle));
      (void)layer_->set_handles(std::move(handles));
      return;
    }
    layer_->clear_handles();
  }

  void on_candidate(const GuiViewportManipulatorCandidate& candidate) {
    if (session_ == nullptr)
      return;
    bool changed = false;
    if (placement_.active())
      changed = placement_.apply_manipulator(candidate);
    else if (motion_.active())
      changed = motion_.apply_manipulator(*session_, candidate);
    if (changed && layer_ != nullptr && layer_->active_handle_id().empty())
      refresh_handles();
  }

  [[nodiscard]] Result<std::size_t> apply() {
    if (session_ == nullptr)
      return Result<std::size_t>::failure(
          Error::validation("gui.interactive_assembly", "no active session"));
    Result<std::size_t> applied = Result<std::size_t>::failure(
        Error::validation("gui.interactive_assembly", "no Assembly interaction is active"));
    if (placement_.active())
      applied = placement_.apply(*session_);
    else if (relationship_.active())
      applied = relationship_.apply(*session_);
    else if (joint_.active())
      applied = joint_.apply(*session_);
    else if (motion_.active())
      applied = motion_.apply(*session_);
    if (!applied.has_error() && layer_ != nullptr)
      layer_->clear_handles();
    return applied;
  }

  void cancel() noexcept {
    placement_.cancel();
    relationship_.cancel();
    joint_.cancel();
    motion_.cancel();
    motion_frame_.reset();
    if (layer_ != nullptr)
      layer_->clear_handles();
  }

private:
  GuiDocumentSession* session_{nullptr};
  GuiViewportManipulatorLayer* layer_{nullptr};
  GuiInteractiveAssemblyPlacementController placement_;
  GuiInteractiveAssemblyRelationshipController relationship_;
  GuiInteractiveAssemblyJointController joint_;
  GuiInteractiveAssemblyMotionController motion_;
  std::optional<geometry::AssemblyFrameTargetDescriptor> motion_frame_;
};

} // namespace blcad::gui
