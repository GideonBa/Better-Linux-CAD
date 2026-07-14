#include "blcad/gui/gui_part_foundation_workbench.hpp"

#include <utility>

namespace blcad::gui {
namespace {
Result<std::size_t> require_part(const GuiDocumentSession& session, std::string_view operation) {
  return session.part_document()
             ? Result<std::size_t>::success(0)
             : Result<std::size_t>::failure(Error::validation("gui.part_foundation",
                                                              std::string(operation) + " requires an active Part document"));
}

template <typename Object, typename Add>
Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session, Object object, Add add,
                                      FeatureId id, std::string summary) {
  if (!session.part_document())
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.part_foundation", "preview requires an active Part document"));
  PartDocument candidate = *session.part_document();
  auto added = (candidate.*add)(std::move(object));
  if (added.has_error()) return Result<GuiPartFeaturePreview>::failure(added.error());
  const auto plan = candidate.create_recompute_plan();
  if (plan.has_error()) return Result<GuiPartFeaturePreview>::failure(plan.error());
  return Result<GuiPartFeaturePreview>::success(
      {std::move(id), std::move(summary), plan.value().step_count()});
}
} // namespace

GuiPartSelectionPrompt GuiPartFoundationWorkbench::prompt_for(GuiPartFoundationCommand command) {
  switch (command) {
  case GuiPartFoundationCommand::Profile: return {"Select one closed sketch profile", GuiSelectionKind::SketchEntity, "profile_region"};
  case GuiPartFoundationCommand::Extrude: return {"Select a closed profile and, for modifying modes, a target Body", GuiSelectionKind::SketchEntity, "profile_and_body_operation"};
  case GuiPartFoundationCommand::Revolve: return {"Select a closed profile and an axis-capable datum, line, or edge", GuiSelectionKind::SketchEntity, "profile_and_axis"};
  case GuiPartFoundationCommand::Hole: return {"Select a sketch and target solid Body for the bolt-circle cut", GuiSelectionKind::SketchEntity, "hole_profile_and_target_body"};
  case GuiPartFoundationCommand::Body: return {"Select or create a Solid or Surface Body", GuiSelectionKind::Body, "body"};
  case GuiPartFoundationCommand::InspectBody: return {"Select one Body", GuiSelectionKind::Body, "body_result"};
  case GuiPartFoundationCommand::Suppress: return {"Select one Part feature", std::nullopt, "persistent_feature_suppression"};
  case GuiPartFoundationCommand::Parameter: return {"Enter a typed value with mm, deg, or count semantics", std::nullopt, "typed_parameter"};
  }
  return {"Select compatible Part input", std::nullopt, "part_input"};
}

Result<std::size_t> GuiPartFoundationWorkbench::create_parameter(GuiDocumentSession& session, Parameter value) const {
  if (auto valid = require_part(session, "parameter creation"); valid.has_error()) return valid;
  return session.commit_part_transaction("Create parameter", [value = std::move(value)](PartDocument& part) mutable { return part.add_parameter(std::move(value)); });
}
Result<std::size_t> GuiPartFoundationWorkbench::create_expression_parameter(GuiDocumentSession& session, ParameterId id, std::string name, ParameterType type, std::string formula) const {
  if (auto valid = require_part(session, "expression creation"); valid.has_error()) return valid;
  return session.commit_part_transaction("Create expression parameter", [id, name = std::move(name), type, formula = std::move(formula)](PartDocument& part) mutable { return part.add_expression_parameter(id, std::move(name), type, std::move(formula)); });
}
Result<std::size_t> GuiPartFoundationWorkbench::set_parameter_value(GuiDocumentSession& session, ParameterId id, Quantity value) const {
  if (auto valid = require_part(session, "parameter editing"); valid.has_error()) return valid;
  return session.commit_part_transaction("Edit parameter", [id, value](PartDocument& part) { auto changed = part.set_parameter_value(id, value); return changed.has_error() ? Result<std::size_t>::failure(changed.error()) : Result<std::size_t>::success(changed.value().size()); });
}
Result<std::size_t> GuiPartFoundationWorkbench::set_parameter_formula(GuiDocumentSession& session, ParameterId id, std::string formula) const {
  if (auto valid = require_part(session, "formula editing"); valid.has_error()) return valid;
  return session.commit_part_transaction("Edit parameter formula", [id, formula = std::move(formula)](PartDocument& part) mutable { auto changed = part.set_parameter_formula(id, std::move(formula)); return changed.has_error() ? Result<std::size_t>::failure(changed.error()) : Result<std::size_t>::success(changed.value().size()); });
}
Result<std::size_t> GuiPartFoundationWorkbench::create_body(GuiDocumentSession& session, Body body) const {
  if (auto valid = require_part(session, "Body creation"); valid.has_error()) return valid;
  return session.commit_part_transaction("Create Body", [body = std::move(body)](PartDocument& part) mutable { return part.add_body(std::move(body)); });
}
Result<std::size_t> GuiPartFoundationWorkbench::activate_body(const GuiDocumentSession& session, BodyId id) {
  const auto* part = session.part_document();
  if (!part || !part->find_body(id)) return Result<std::size_t>::failure(Error::validation(id.value(), "active Body must exist"));
  active_body_ = std::move(id);
  return Result<std::size_t>::success(1);
}
const std::optional<BodyId>& GuiPartFoundationWorkbench::active_body() const noexcept { return active_body_; }

Result<GuiPartFeaturePreview> GuiPartFoundationWorkbench::preview_extrude(const GuiDocumentSession& session, Feature feature) const {
  const auto id = feature.id();
  const std::string summary = std::string(to_string(feature.type())) + ", " + std::string(to_string(feature.extrude_intent().extent().mode())) + ", " + std::string(to_string(feature.direction()));
  return preview(session, std::move(feature), &PartDocument::add_feature, id, summary);
}
Result<std::size_t> GuiPartFoundationWorkbench::apply_extrude(GuiDocumentSession& session, Feature feature) const {
  auto checked = preview_extrude(session, feature); if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
  return session.commit_part_transaction("Apply Extrude", [feature = std::move(feature)](PartDocument& part) mutable { return part.add_feature(std::move(feature)); });
}
Result<GuiPartFeaturePreview> GuiPartFoundationWorkbench::preview_revolve(const GuiDocumentSession& session, RevolveFeature feature) const {
  const auto id = feature.id();
  const std::string summary = std::string(to_string(feature.kind())) + ", " + std::string(to_string(feature.extent().mode())) + ", " + std::string(to_string(feature.body_result_context().operation_mode()));
  return preview(session, std::move(feature), &PartDocument::add_revolve_feature, id, summary);
}
Result<std::size_t> GuiPartFoundationWorkbench::apply_revolve(GuiDocumentSession& session, RevolveFeature feature) const {
  auto checked = preview_revolve(session, feature); if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
  return session.commit_part_transaction("Apply Revolve", [feature = std::move(feature)](PartDocument& part) mutable { return part.add_revolve_feature(std::move(feature)); });
}
Result<std::size_t> GuiPartFoundationWorkbench::apply_hole_workflow(GuiDocumentSession& session, SketchId sketch_id, CircularHolePattern pattern, Feature cut) const {
  if (auto valid = require_part(session, "hole workflow"); valid.has_error()) return valid;
  return session.commit_part_transaction("Apply bolt-circle holes", [sketch_id, pattern = std::move(pattern), cut = std::move(cut)](PartDocument& part) mutable {
    const auto* source = part.find_sketch(sketch_id); if (!source) return Result<std::size_t>::failure(Error::validation(sketch_id.value(), "hole sketch must exist"));
    Sketch sketch = *source; auto added = sketch.add_profile(std::move(pattern)); if (added.has_error()) return added;
    auto updated = part.update_sketch(std::move(sketch)); if (updated.has_error()) return updated;
    return part.add_feature(std::move(cut));
  });
}
Result<GuiBodyInspection> GuiPartFoundationWorkbench::inspect_body(const GuiDocumentSession& session, BodyId id) const {
  const auto* part = session.part_document(); const auto* body = part ? part->find_body(id) : nullptr;
  if (!body) return Result<GuiBodyInspection>::failure(Error::validation(id.value(), "Body must exist"));
  const auto* cached = session.part_shape_cache() ? session.part_shape_cache()->find_body_result(id) : nullptr;
  return Result<GuiBodyInspection>::success({id, body->kind(), body->visibility(), cached != nullptr, cached ? std::optional<FeatureId>{cached->source_feature_id} : std::nullopt});
}
Result<std::size_t> GuiPartFoundationWorkbench::suppress_feature(GuiDocumentSession&, FeatureId id) const {
  return Result<std::size_t>::failure(Error::validation(id.value(), "persistent Part-feature suppression is not implemented by the Core model"));
}

} // namespace blcad::gui
