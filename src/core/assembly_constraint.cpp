#include "blcad/core/assembly_constraint.hpp"

#include "blcad/core/project.hpp"

#include <algorithm>
#include <utility>

namespace blcad {

std::string_view to_string(AssemblyConstraintType type) noexcept {
  switch (type) {
  case AssemblyConstraintType::Mate:
    return "mate";
  case AssemblyConstraintType::Concentric:
    return "concentric";
  case AssemblyConstraintType::Distance:
    return "distance";
  case AssemblyConstraintType::Insert:
    return "insert";
  case AssemblyConstraintType::Angle:
    return "angle";
  }
  return "mate";
}

std::string_view to_string(AssemblyConstraintState state) noexcept {
  switch (state) {
  case AssemblyConstraintState::Active:
    return "active";
  case AssemblyConstraintState::Inactive:
    return "inactive";
  }
  return "active";
}

Result<AssemblyConstraintTarget>
AssemblyConstraintTarget::create(ComponentInstanceId component_instance,
                                 std::string semantic_reference) {
  const auto object_id = component_instance.empty() ? std::string("assembly_constraint_target")
                                                    : component_instance.value();
  if (component_instance.empty()) {
    return Result<AssemblyConstraintTarget>::failure(Error::validation(
        object_id, "assembly constraint target component instance id must not be empty"));
  }
  if (semantic_reference.empty()) {
    return Result<AssemblyConstraintTarget>::failure(Error::validation(
        object_id, "assembly constraint target semantic reference must not be empty"));
  }
  return Result<AssemblyConstraintTarget>::success(
      AssemblyConstraintTarget(std::move(component_instance), std::move(semantic_reference)));
}

AssemblyConstraintTarget::AssemblyConstraintTarget(ComponentInstanceId component_instance,
                                                   std::string semantic_reference)
    : component_instance_(std::move(component_instance)),
      semantic_reference_(std::move(semantic_reference)) {}

const ComponentInstanceId& AssemblyConstraintTarget::component_instance() const noexcept {
  return component_instance_;
}

const std::string& AssemblyConstraintTarget::semantic_reference() const noexcept {
  return semantic_reference_;
}

Result<AssemblyHierarchyConstraintEndpoint> AssemblyHierarchyConstraintEndpoint::create(
    std::vector<SubassemblyInstanceId> occurrence_path,
    ComponentInstanceId component_instance,
    std::string semantic_reference) {
  for (const SubassemblyInstanceId& occurrence : occurrence_path) {
    if (occurrence.empty()) {
      return Result<AssemblyHierarchyConstraintEndpoint>::failure(Error::validation(
          "assembly_hierarchy_constraint_endpoint",
          "cross-hierarchy relationship occurrence path ids must not be empty"));
    }
  }

  auto local_target = AssemblyConstraintTarget::create(component_instance, semantic_reference);
  if (local_target.has_error()) {
    return Result<AssemblyHierarchyConstraintEndpoint>::failure(local_target.error());
  }

  return Result<AssemblyHierarchyConstraintEndpoint>::success(
      AssemblyHierarchyConstraintEndpoint(std::move(occurrence_path),
                                          std::move(component_instance),
                                          std::move(semantic_reference)));
}

AssemblyHierarchyConstraintEndpoint::AssemblyHierarchyConstraintEndpoint(
    std::vector<SubassemblyInstanceId> occurrence_path,
    ComponentInstanceId component_instance,
    std::string semantic_reference)
    : occurrence_path_(std::move(occurrence_path)),
      component_instance_(std::move(component_instance)),
      semantic_reference_(std::move(semantic_reference)) {}

const std::vector<SubassemblyInstanceId>&
AssemblyHierarchyConstraintEndpoint::occurrence_path() const noexcept {
  return occurrence_path_;
}

const ComponentInstanceId&
AssemblyHierarchyConstraintEndpoint::component_instance() const noexcept {
  return component_instance_;
}

const std::string& AssemblyHierarchyConstraintEndpoint::semantic_reference() const noexcept {
  return semantic_reference_;
}

Result<AssemblyConstraint>
AssemblyConstraint::create(AssemblyConstraintId id, std::string name, AssemblyConstraintType type,
                           AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
                           AssemblyConstraintState state, std::optional<Quantity> distance,
                           std::optional<Quantity> angle) {
  const auto object_id = id.empty() ? std::string("assembly_constraint") : id.value();
  if (id.empty()) {
    return Result<AssemblyConstraint>::failure(
        Error::validation(object_id, "assembly constraint id must not be empty"));
  }
  if (name.empty()) {
    return Result<AssemblyConstraint>::failure(
        Error::validation(object_id, "assembly constraint name must not be empty"));
  }

  if (type == AssemblyConstraintType::Distance) {
    if (!distance.has_value()) {
      return Result<AssemblyConstraint>::failure(
          Error::validation(object_id, "distance assembly constraint requires a distance value"));
    }
    if (distance->kind() != QuantityKind::LengthMm) {
      return Result<AssemblyConstraint>::failure(
          Error::validation(object_id, "distance assembly constraint requires a length value"));
    }
  } else if (distance.has_value()) {
    return Result<AssemblyConstraint>::failure(Error::validation(
        object_id, "only distance assembly constraints may store a distance value"));
  }

  if (type == AssemblyConstraintType::Angle) {
    if (!angle.has_value()) {
      return Result<AssemblyConstraint>::failure(
          Error::validation(object_id, "angle assembly constraint requires an angle value"));
    }
    if (angle->kind() != QuantityKind::AngleDeg) {
      return Result<AssemblyConstraint>::failure(Error::validation(
          object_id, "angle assembly constraint requires an angle value in degrees"));
    }
  } else if (angle.has_value()) {
    return Result<AssemblyConstraint>::failure(
        Error::validation(object_id, "only angle assembly constraints may store an angle value"));
  }

  return Result<AssemblyConstraint>::success(
      AssemblyConstraint(std::move(id), std::move(name), type, std::move(target_a),
                         std::move(target_b), state, std::move(distance), std::move(angle)));
}

AssemblyConstraint::AssemblyConstraint(
    AssemblyConstraintId id, std::string name, AssemblyConstraintType type,
    AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
    AssemblyConstraintState state, std::optional<Quantity> distance, std::optional<Quantity> angle)
    : id_(std::move(id)), name_(std::move(name)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), state_(state), distance_(std::move(distance)),
      angle_(std::move(angle)) {}

const AssemblyConstraintId& AssemblyConstraint::id() const noexcept {
  return id_;
}

const std::string& AssemblyConstraint::name() const noexcept {
  return name_;
}

AssemblyConstraintType AssemblyConstraint::type() const noexcept {
  return type_;
}

const AssemblyConstraintTarget& AssemblyConstraint::target_a() const noexcept {
  return target_a_;
}

const AssemblyConstraintTarget& AssemblyConstraint::target_b() const noexcept {
  return target_b_;
}

AssemblyConstraintState AssemblyConstraint::state() const noexcept {
  return state_;
}

const std::optional<Quantity>& AssemblyConstraint::distance() const noexcept {
  return distance_;
}

const std::optional<Quantity>& AssemblyConstraint::angle() const noexcept {
  return angle_;
}

Result<AssemblyHierarchyConstraint> AssemblyHierarchyConstraint::create(
    AssemblyConstraintId id, std::string name, AssemblyConstraintType type,
    AssemblyHierarchyConstraintEndpoint target_a,
    AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyConstraintState state,
    std::optional<Quantity> distance,
    std::optional<Quantity> angle) {
  auto local_target_a =
      AssemblyConstraintTarget::create(target_a.component_instance(), target_a.semantic_reference());
  if (local_target_a.has_error()) {
    return Result<AssemblyHierarchyConstraint>::failure(local_target_a.error());
  }
  auto local_target_b =
      AssemblyConstraintTarget::create(target_b.component_instance(), target_b.semantic_reference());
  if (local_target_b.has_error()) {
    return Result<AssemblyHierarchyConstraint>::failure(local_target_b.error());
  }

  auto validated = AssemblyConstraint::create(id, name, type, std::move(local_target_a.value()),
                                              std::move(local_target_b.value()), state, distance,
                                              angle);
  if (validated.has_error()) {
    return Result<AssemblyHierarchyConstraint>::failure(validated.error());
  }

  return Result<AssemblyHierarchyConstraint>::success(AssemblyHierarchyConstraint(
      std::move(id), std::move(name), type, std::move(target_a), std::move(target_b), state,
      std::move(distance), std::move(angle)));
}

AssemblyHierarchyConstraint::AssemblyHierarchyConstraint(
    AssemblyConstraintId id, std::string name, AssemblyConstraintType type,
    AssemblyHierarchyConstraintEndpoint target_a,
    AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyConstraintState state,
    std::optional<Quantity> distance,
    std::optional<Quantity> angle)
    : id_(std::move(id)), name_(std::move(name)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), state_(state), distance_(std::move(distance)),
      angle_(std::move(angle)) {}

const AssemblyConstraintId& AssemblyHierarchyConstraint::id() const noexcept {
  return id_;
}

const std::string& AssemblyHierarchyConstraint::name() const noexcept {
  return name_;
}

AssemblyConstraintType AssemblyHierarchyConstraint::type() const noexcept {
  return type_;
}

const AssemblyHierarchyConstraintEndpoint& AssemblyHierarchyConstraint::target_a() const noexcept {
  return target_a_;
}

const AssemblyHierarchyConstraintEndpoint& AssemblyHierarchyConstraint::target_b() const noexcept {
  return target_b_;
}

AssemblyConstraintState AssemblyHierarchyConstraint::state() const noexcept {
  return state_;
}

const std::optional<Quantity>& AssemblyHierarchyConstraint::distance() const noexcept {
  return distance_;
}

const std::optional<Quantity>& AssemblyHierarchyConstraint::angle() const noexcept {
  return angle_;
}

Result<std::size_t>
Project::add_cross_hierarchy_constraint(AssemblyHierarchyConstraint constraint) {
  if (find_cross_hierarchy_constraint(constraint.id()) != nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        constraint.id().value(),
        "cross-hierarchy assembly constraint id must be unique within project"));
  }
  cross_hierarchy_constraints_.push_back(std::move(constraint));
  return Result<std::size_t>::success(cross_hierarchy_constraints_.size() - 1U);
}

const std::vector<AssemblyHierarchyConstraint>&
Project::cross_hierarchy_constraints() const noexcept {
  return cross_hierarchy_constraints_;
}

std::vector<AssemblyHierarchyConstraint>& Project::cross_hierarchy_constraints() noexcept {
  return cross_hierarchy_constraints_;
}

std::size_t Project::cross_hierarchy_constraint_count() const noexcept {
  return cross_hierarchy_constraints_.size();
}

const AssemblyHierarchyConstraint*
Project::find_cross_hierarchy_constraint(AssemblyConstraintId id) const noexcept {
  const auto found = std::find_if(
      cross_hierarchy_constraints_.begin(), cross_hierarchy_constraints_.end(),
      [&id](const AssemblyHierarchyConstraint& constraint) { return constraint.id() == id; });
  return found == cross_hierarchy_constraints_.end() ? nullptr : &*found;
}

AssemblyHierarchyConstraint*
Project::find_cross_hierarchy_constraint(AssemblyConstraintId id) noexcept {
  const auto found = std::find_if(
      cross_hierarchy_constraints_.begin(), cross_hierarchy_constraints_.end(),
      [&id](const AssemblyHierarchyConstraint& constraint) { return constraint.id() == id; });
  return found == cross_hierarchy_constraints_.end() ? nullptr : &*found;
}

} // namespace blcad
