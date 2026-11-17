#include "blcad/gui/gui_fillet_draft.hpp"
#include <algorithm>
#include <cmath>
#include <type_traits>
namespace blcad::gui {
namespace {
Result<FilletFeature> rebind(const FilletFeature& source,
                             const std::map<std::string, ParameterId>& bindings) {
  auto parameter = [&](const ParameterId& old) {
    auto found = bindings.find(old.value());
    return found == bindings.end() ? old : found->second;
  };
  if (!source.advanced_intent())
    return FilletFeature::create(source.id(), source.name(), source.target_body(), source.edges(),
                                 parameter(source.radius_parameter()));
  return std::visit(
      [&](auto intent) -> Result<FilletFeature> {
        using T = std::decay_t<decltype(intent)>;
        if constexpr (std::is_same_v<T, VariableFilletIntent>) {
          for (auto& group : intent.groups)
            for (auto& station : group.stations)
              station.radius = parameter(station.radius);
          if (intent.setback)
            intent.setback->distance = parameter(intent.setback->distance);
          return FilletFeature::create_variable(source.id(), source.name(), source.target_body(),
                                                intent);
        } else if constexpr (std::is_same_v<T, FaceBlendIntent>) {
          intent.size = parameter(intent.size);
          return FilletFeature::create_face_blend(source.id(), source.name(), source.target_body(),
                                                  intent);
        } else
          return FilletFeature::create_full_round(source.id(), source.name(), source.target_body(),
                                                  intent);
      },
      source.advanced_intent()->definition);
}
} // namespace
double GuiFilletDraft::value(const PartDocument& document, const ParameterId& id) const {
  if (auto it = values.find(id.value()); it != values.end())
    return it->second;
  for (const auto& p : pending)
    if (p.id() == id)
      return p.value().millimeters();
  if (auto p = document.find_parameter(id))
    return p->value().millimeters();
  return 0;
}
bool GuiFilletDraft::move(const GuiViewportManipulatorCandidate& candidate) {
  auto binding = handle_bindings.find(candidate.handle_id);
  if (binding == handle_bindings.end() ||
      candidate.value_kind != GuiViewportManipulatorValueKind::Distance ||
      !std::isfinite(candidate.scalar_value) || candidate.scalar_value <= 0)
    return false;
  values[binding->second.value()] = candidate.scalar_value;
  for (auto& handle : handles) {
    const auto other = handle_bindings.find(handle.id);
    if (other != handle_bindings.end() && other->second == binding->second)
      handle.reference_value = candidate.scalar_value;
  }
  return true;
}
Result<std::size_t> GuiFilletDraft::apply(PartDocument& document) const {
  const auto dependencies = record.parameter_dependencies();
  for (const auto& p : pending) {
    if (p.type() != ParameterType::Length ||
        std::find(dependencies.begin(), dependencies.end(), p.id()) == dependencies.end())
      return Result<std::size_t>::failure(
          Error::validation(p.id().value(), "Pending value does not belong to this Fillet mode"));
    auto added = document.add_parameter(p);
    if (!added)
      return added;
  }
  std::map<std::string, ParameterId> bindings;
  for (const auto& [name, v] : values) {
    const ParameterId id(name);
    auto source = document.find_parameter(id);
    if (!source || std::find(dependencies.begin(), dependencies.end(), id) == dependencies.end())
      return Result<std::size_t>::failure(
          Error::validation(name, "Value is not bound to this Fillet mode"));
    if (v == source->value().millimeters())
      continue;
    auto q = Quantity::length_mm(v);
    if (!q)
      return Result<std::size_t>::failure(q.error());
    const bool is_new =
        std::any_of(pending.begin(), pending.end(), [&](const auto& p) { return p.id() == id; });
    if (independent_values && !is_new) {
      ParameterId clone("");
      std::string label;
      for (std::size_t i = 1;; ++i) {
        clone = ParameterId(record.id().value() + ".independent." + std::to_string(i));
        label = record.name() + " Independent Size " + std::to_string(i);
        if (!document.find_parameter(clone) &&
            std::none_of(document.parameters().begin(), document.parameters().end(),
                         [&](const auto& p) { return p.name() == label; }))
          break;
      }
      auto p = Parameter::create_length(clone, label, q.value());
      if (!p)
        return Result<std::size_t>::failure(p.error());
      auto added = document.add_parameter(p.value());
      if (!added)
        return added;
      bindings.emplace(name, clone);
    } else {
      auto changed = document.set_parameter_value(id, q.value());
      if (!changed)
        return Result<std::size_t>::failure(changed.error());
    }
  }
  auto candidate = rebind(record, bindings);
  if (!candidate)
    return Result<std::size_t>::failure(candidate.error());
  return editing ? document.update_fillet_feature(candidate.value())
                 : document.add_fillet_feature(candidate.value());
}
} // namespace blcad::gui
