#include "blcad/core/model_state.hpp"

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace blcad {
namespace {
Error error(std::string id, std::string message) {
  return Error::validation(std::move(id), std::move(message));
}
bool blank(std::string_view value) {
  return value.empty() || std::all_of(value.begin(), value.end(),
                                      [](unsigned char c) { return std::isspace(c) != 0; });
}
template <typename Entries, typename Key> bool unique_keys(const Entries& entries, Key key) {
  std::unordered_set<std::string> seen;
  for (const auto& entry : entries) {
    const auto id = key(entry);
    if (blank(id) || !seen.insert(id).second)
      return false;
  }
  return true;
}
const ModelState* find_state(const std::vector<ModelState>& states, const ModelStateId& id) {
  const auto found =
      std::find_if(states.begin(), states.end(), [&](const auto& state) { return state.id == id; });
  return found == states.end() ? nullptr : &*found;
}
Result<std::size_t> validate_parameter_override(const Parameter* parameter,
                                                const ModelStateParameterOverride& change) {
  if (!parameter)
    return Result<std::size_t>::failure(
        error(change.parameter.value(), "Model-state parameter must resolve in Primary"));
  if (const auto* literal = std::get_if<Quantity>(&change.value)) {
    if (parameter->is_expression())
      return Result<std::size_t>::failure(error(
          change.parameter.value(), "Expression parameters require an explicit formula override"));
    auto valid = parameter->with_value(*literal);
    if (!valid)
      return Result<std::size_t>::failure(valid.error());
  } else if (!parameter->is_expression() || blank(std::get<std::string>(change.value))) {
    return Result<std::size_t>::failure(error(
        change.parameter.value(), "Formula overrides require an existing expression and formula"));
  }
  return Result<std::size_t>::success(1);
}
} // namespace

Result<std::size_t> validate_model_state_catalog(const std::vector<ModelState>& states) {
  std::unordered_set<std::string> ids{"primary"};
  std::unordered_set<std::string> names{"Primary"};
  for (const auto& state : states) {
    if (blank(state.id.value()) || blank(state.name) || !ids.insert(state.id.value()).second ||
        !names.insert(state.name).second)
      return Result<std::size_t>::failure(
          error("model_states",
                "Named states require unique nonempty identities/names; Primary is reserved"));
    if (!unique_keys(state.parameters, [](const auto& x) { return x.parameter.value(); }) ||
        !unique_keys(state.features, [](const auto& x) { return x.executable; }) ||
        !unique_keys(state.materials,
                     [](const auto& x) {
                       return x.body ? "body/" + x.body->value() : std::string("default");
                     }) ||
        !unique_keys(state.components, [](const auto& x) { return x.component.value(); }) ||
        !unique_keys(state.subassemblies, [](const auto& x) { return x.subassembly.value(); }))
      return Result<std::size_t>::failure(
          error(state.id.value(), "State override targets must be unique and nonempty"));
    for (const auto& material : state.materials)
      if (material.body && material.body->empty())
        return Result<std::size_t>::failure(
            error(state.id.value(), "Body identity must not be empty"));
    const auto invalid_child = [](const auto& child) {
      return (!child.suppressed && !child.selected_state) ||
             (child.selected_state && child.selected_state->empty());
    };
    if (std::any_of(state.components.begin(), state.components.end(), invalid_child) ||
        std::any_of(state.subassemblies.begin(), state.subassemblies.end(), invalid_child))
      return Result<std::size_t>::failure(error(
          state.id.value(), "Child overrides require suppression or a nonempty selected state"));
  }
  return Result<std::size_t>::success(states.size());
}

bool has_model_state(const std::vector<ModelState>& states, const ModelStateId& id) noexcept {
  return id == primary_model_state_id() || find_state(states, id) != nullptr;
}

Result<std::size_t> PartDocument::validate_model_states() const {
  auto valid = validate_model_state_catalog(model_states_);
  if (!valid)
    return valid;
  for (const auto& state : model_states_) {
    auto evaluated = evaluate_model_state(state.id);
    if (!evaluated)
      return Result<std::size_t>::failure(evaluated.error());
  }
  return Result<std::size_t>::success(model_states_.size());
}

Result<std::size_t> PartDocument::replace_model_states(std::vector<ModelState> states) {
  if (is_reference_part() && !states.empty())
    return Result<std::size_t>::failure(
        error(id().value(),
              "Reference Parts require explicit editable conversion before state authoring"));
  auto valid = validate_model_state_catalog(states);
  if (!valid)
    return valid;
  auto candidate = *this;
  candidate.model_states_ = std::move(states);
  for (const auto& state : candidate.model_states_) {
    auto evaluated = candidate.evaluate_model_state(state.id);
    if (!evaluated)
      return Result<std::size_t>::failure(evaluated.error());
  }
  model_states_ = std::move(candidate.model_states_);
  return Result<std::size_t>::success(model_states_.size());
}

Result<PartDocument> PartDocument::evaluate_model_state(ModelStateId selected) const {
  if (effective_model_state_ != primary_model_state_id())
    return Result<PartDocument>::failure(
        error(id().value(),
              "Select a state from the authored Primary document, not an effective result"));
  if (selected == primary_model_state_id()) {
    auto result = *this;
    result.effective_model_state_ = std::move(selected);
    return Result<PartDocument>::success(std::move(result));
  }
  const auto* state = find_state(model_states_, selected);
  if (!state)
    return Result<PartDocument>::failure(error(selected.value(), "Model state does not exist"));
  if (!state->components.empty() || !state->subassemblies.empty())
    return Result<PartDocument>::failure(
        error(selected.value(), "Part states cannot override occurrences"));
  for (const auto& change : state->parameters) {
    const auto valid = validate_parameter_override(find_parameter(change.parameter), change);
    if (!valid)
      return Result<PartDocument>::failure(valid.error());
  }
  // Rebuild the complete final parameter assignment through the canonical Core
  // loader. This permits simultaneous formula rewiring and avoids rejecting a
  // valid final table because a transient sequential edit would contain a cycle.
  auto base = *this;
  base.model_states_.clear();
  auto serialized = serialize_part_document_to_json(base);
  if (!serialized)
    return Result<PartDocument>::failure(serialized.error());
  auto json = nlohmann::json::parse(serialized.value());
  for (const auto& change : state->parameters)
    for (auto& parameter : json.at("parameters"))
      if (parameter.at("id") == change.parameter.value()) {
        if (const auto* literal = std::get_if<Quantity>(&change.value))
          parameter["value"] = literal->kind() == QuantityKind::AngleDeg ? literal->degrees()
                               : literal->kind() == QuantityKind::Count
                                   ? static_cast<double>(literal->count_value())
                                   : literal->millimeters();
        else
          parameter["formula"] = std::get<std::string>(change.value);
      }
  auto result = deserialize_part_document_from_json(json.dump());
  if (!result)
    return result;
  // Loading follows final expression dependencies; retain the authored table
  // order even when a state changes those dependencies.
  auto parameters = result.value().parameters_;
  result.value().parameters_.clear();
  for (const auto& original : parameters_)
    for (const auto& evaluated : parameters)
      if (original.id() == evaluated.id())
        result.value().parameters_.push_back(evaluated);
  if (!state->features.empty()) {
    auto history = result.value().executable_history();
    if (!history)
      return Result<PartDocument>::failure(history.error());
    PartFeatureHistoryIntent intent;
    for (const auto& row : history.value()) {
      intent.order.push_back(row.node_id);
      if (row.state == PartFeatureState::Suppressed)
        intent.suppressed.push_back(row.node_id);
    }
    for (const auto& change : state->features) {
      if (std::find(intent.order.begin(), intent.order.end(), change.executable) ==
          intent.order.end())
        return Result<PartDocument>::failure(
            error(change.executable, "State executable must exist"));
      std::erase(intent.suppressed, change.executable);
      if (change.suppressed)
        intent.suppressed.push_back(change.executable);
    }
    auto applied = result.value().apply_history_intent(std::move(intent));
    if (!applied)
      return Result<PartDocument>::failure(applied.error());
  }
  for (const auto& change : state->materials) {
    auto applied = change.body ? result.value().set_body_material(*change.body, change.material)
                               : result.value().set_default_material(change.material);
    if (!applied)
      return Result<PartDocument>::failure(applied.error());
  }
  // This result is a derived effective definition, not a new Primary authority.
  // Sibling definitions continue to belong to the original Primary document.
  result.value().model_states_.clear();
  result.value().effective_model_state_ = std::move(selected);
  return result;
}

Result<std::string> model_state_source_key(const PartDocument& document, ModelStateId state) {
  if (document.effective_model_state() != primary_model_state_id())
    return Result<std::string>::failure(
        error(document.id().value(), "State source keys require the authored Primary definition"));
  if (!has_model_state(document.model_states(), state))
    return Result<std::string>::failure(error(state.value(), "Model state does not exist"));
  auto source = serialize_part_document_to_json(document);
  if (!source)
    return source;
  return Result<std::string>::success(
      nlohmann::json{{"source", source.value()}, {"state", state.value()}}.dump());
}

Result<ComponentInstance> ComponentInstance::with_model_state(ModelStateId state) const {
  return create(id_, name_, referenced_part_document_, visibility_, suppression_state_,
                grounding_state_, transform_, std::move(state));
}
Result<SubassemblyInstance> SubassemblyInstance::with_model_state(ModelStateId state) const {
  return create(id_, name_, referenced_assembly_document_, visibility_, suppression_state_,
                transform_, std::move(state));
}
Result<std::size_t> AssemblyDocument::set_component_model_state(ComponentInstanceId id,
                                                                ModelStateId state) {
  for (auto& component : component_instances_)
    if (component.id() == id) {
      auto updated = component.with_model_state(std::move(state));
      if (!updated)
        return Result<std::size_t>::failure(updated.error());
      component = std::move(updated.value());
      return Result<std::size_t>::success(1);
    }
  return Result<std::size_t>::failure(error(id.value(), "State component must exist"));
}
Result<std::size_t> AssemblyDocument::set_subassembly_model_state(SubassemblyInstanceId id,
                                                                  ModelStateId state) {
  for (auto& subassembly : subassembly_instances_)
    if (subassembly.id() == id) {
      auto updated = subassembly.with_model_state(std::move(state));
      if (!updated)
        return Result<std::size_t>::failure(updated.error());
      subassembly = std::move(updated.value());
      return Result<std::size_t>::success(1);
    }
  return Result<std::size_t>::failure(error(id.value(), "State subassembly must exist"));
}

Result<std::size_t> AssemblyDocument::replace_model_states(std::vector<ModelState> states) {
  auto valid = validate_model_state_catalog(states);
  if (!valid)
    return valid;
  auto candidate = *this;
  candidate.model_states_ = std::move(states);
  for (const auto& state : candidate.model_states_) {
    auto evaluated = candidate.evaluate_model_state(state.id);
    if (!evaluated)
      return Result<std::size_t>::failure(evaluated.error());
  }
  model_states_ = std::move(candidate.model_states_);
  return Result<std::size_t>::success(model_states_.size());
}

Result<AssemblyDocument> AssemblyDocument::evaluate_model_state(ModelStateId selected) const {
  if (effective_model_state_ != primary_model_state_id()) {
    if (selected == effective_model_state_)
      return Result<AssemblyDocument>::success(*this);
    return Result<AssemblyDocument>::failure(
        error(id().value(), "Select a state from the authored Primary Assembly"));
  }
  if (selected == primary_model_state_id())
    return Result<AssemblyDocument>::success(*this);
  const auto* state = find_state(model_states_, selected);
  if (!state)
    return Result<AssemblyDocument>::failure(
        error(selected.value(), "Assembly model state does not exist"));
  if (!state->features.empty() || !state->materials.empty())
    return Result<AssemblyDocument>::failure(
        error(selected.value(), "Assembly states require Part-owned feature/material overrides"));
  auto result = *this;
  for (const auto& change : state->parameters) {
    auto valid = validate_parameter_override(find_parameter(change.parameter), change);
    if (!valid)
      return Result<AssemblyDocument>::failure(valid.error());
    if (!std::holds_alternative<Quantity>(change.value))
      return Result<AssemblyDocument>::failure(
          error(change.parameter.value(),
                "Assembly formulas are not supported by the existing parameter authority"));
    auto applied = result.set_parameter_value(change.parameter, std::get<Quantity>(change.value));
    if (!applied)
      return Result<AssemblyDocument>::failure(applied.error());
  }
  for (const auto& change : state->components) {
    if (!result.find_component_instance(change.component))
      return Result<AssemblyDocument>::failure(
          error(change.component.value(), "State component must exist"));
    if (change.suppressed) {
      auto applied = result.set_component_instance_suppression_state(
          change.component, *change.suppressed ? ComponentSuppressionState::Suppressed
                                               : ComponentSuppressionState::Active);
      if (!applied)
        return Result<AssemblyDocument>::failure(applied.error());
    }
    if (change.selected_state) {
      auto applied = result.set_component_model_state(change.component, *change.selected_state);
      if (!applied)
        return Result<AssemblyDocument>::failure(applied.error());
    }
  }
  for (const auto& change : state->subassemblies) {
    if (!result.find_subassembly_instance(change.subassembly))
      return Result<AssemblyDocument>::failure(
          error(change.subassembly.value(), "State subassembly must exist"));
    if (change.suppressed) {
      auto applied = result.set_subassembly_instance_suppression_state(
          change.subassembly, *change.suppressed ? ComponentSuppressionState::Suppressed
                                                 : ComponentSuppressionState::Active);
      if (!applied)
        return Result<AssemblyDocument>::failure(applied.error());
    }
    if (change.selected_state) {
      auto applied = result.set_subassembly_model_state(change.subassembly, *change.selected_state);
      if (!applied)
        return Result<AssemblyDocument>::failure(applied.error());
    }
  }
  result.model_states_.clear();
  result.effective_model_state_ = std::move(selected);
  return Result<AssemblyDocument>::success(std::move(result));
}

Result<std::size_t> validate_project_model_states(const Project& project) {
  std::vector<const AssemblyDocument*> assemblies{&project.assembly()};
  for (const auto& child : project.child_assembly_documents())
    assemblies.push_back(&child);
  std::size_t count = 0;
  for (const auto* assembly : assemblies) {
    std::vector<ModelStateId> states{assembly->effective_model_state()};
    for (const auto& state : assembly->model_states())
      states.push_back(state.id);
    for (const auto& state : states) {
      auto effective = assembly->evaluate_model_state(state);
      if (!effective)
        return Result<std::size_t>::failure(effective.error());
      for (const auto& component : effective.value().component_instances()) {
        const auto* part = project.find_part_document(component.referenced_part_document());
        if (!part || !has_model_state(part->model_states(), component.selected_model_state()))
          return Result<std::size_t>::failure(
              error(component.id().value(),
                    "Selected Part model state must resolve, including suppressed occurrences"));
      }
      for (const auto& occurrence : effective.value().subassembly_instances()) {
        const auto* child =
            project.find_assembly_document(occurrence.referenced_assembly_document());
        if (!child || !has_model_state(child->model_states(), occurrence.selected_model_state()))
          return Result<std::size_t>::failure(
              error(occurrence.id().value(), "Selected child Assembly model state must resolve"));
      }
      ++count;
    }
  }
  return Result<std::size_t>::success(count);
}

} // namespace blcad
