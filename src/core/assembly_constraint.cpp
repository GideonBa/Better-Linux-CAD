#include "blcad/core/assembly_document.hpp"

#include <utility>

namespace blcad {

std::string_view to_string(AssemblyConstraintType type) noexcept {
  switch (type) {
  case AssemblyConstraintType::Mate: return "mate";
  case AssemblyConstraintType::Concentric: return "concentric";
  case AssemblyConstraintType::Distance: return "distance";
  }
  return "mate";
}

std::string_view to_string(AssemblyConstraintState state) noexcept {
  switch (state) {
  case AssemblyConstraintState::Active: return "active";
  case AssemblyConstraintState::Inactive: return "inactive";
  }
  return "active";
}

Result<AssemblyConstraintTarget>
AssemblyConstraintTarget::create(ComponentInstanceId component_instance,
                                 std::string semantic_reference) {
  const auto object_id =
      component_instance.empty() ? std::string("assembly_constraint_target")
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

Result<AssemblyConstraint> AssemblyConstraint::create(
    AssemblyConstraintId id,
    std::string name,
    AssemblyConstraintType type,
    AssemblyConstraintTarget target_a,
    AssemblyConstraintTarget target_b,
    AssemblyConstraintState state,
    std::optional<Quantity> distance) {
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

  return Result<AssemblyConstraint>::success(
      AssemblyConstraint(std::move(id), std::move(name), type, std::move(target_a),
                         std::move(target_b), state, std::move(distance)));
}

AssemblyConstraint::AssemblyConstraint(AssemblyConstraintId id,
                                       std::string name,
                                       AssemblyConstraintType type,
                                       AssemblyConstraintTarget target_a,
                                       AssemblyConstraintTarget target_b,
                                       AssemblyConstraintState state,
                                       std::optional<Quantity> distance)
    : id_(std::move(id)), name_(std::move(name)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), state_(state), distance_(std::move(distance)) {}

const AssemblyConstraintId& AssemblyConstraint::id() const noexcept { return id_; }

const std::string& AssemblyConstraint::name() const noexcept { return name_; }

AssemblyConstraintType AssemblyConstraint::type() const noexcept { return type_; }

const AssemblyConstraintTarget& AssemblyConstraint::target_a() const noexcept { return target_a_; }

const AssemblyConstraintTarget& AssemblyConstraint::target_b() const noexcept { return target_b_; }

AssemblyConstraintState AssemblyConstraint::state() const noexcept { return state_; }

const std::optional<Quantity>& AssemblyConstraint::distance() const noexcept { return distance_; }

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

const std::vector<AssemblyConstraint>& AssemblyDocument::constraints() const noexcept {
  return constraints_;
}

std::size_t AssemblyDocument::constraint_count() const noexcept { return constraints_.size(); }

const AssemblyConstraint*
AssemblyDocument::find_constraint(AssemblyConstraintId id) const noexcept {
  for (const auto& constraint : constraints_) {
    if (constraint.id() == id) {
      return &constraint;
    }
  }
  return nullptr;
}

bool AssemblyDocument::has_constraint_id(const AssemblyConstraintId& id) const noexcept {
  return find_constraint(id) != nullptr;
}

} // namespace blcad
