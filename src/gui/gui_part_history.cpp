#include "blcad/gui/gui_part_history.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_step_import_context.hpp"

#include <algorithm>
#include <utility>

namespace blcad::gui {
namespace {
Error history_error(std::string message) {
  return Error::validation("gui.part.history", std::move(message));
}
} // namespace

Result<std::size_t> GuiPartHistoryController::begin(const GuiDocumentSession& session) {
  const auto* part = session.part_document();
  if (active_ || !part || part->is_reference_part() || session.task().active() ||
      session.part_workflow_active() ||
      (!session.derived_results_fresh() && !session.reference_repair_mode()))
    return Result<std::size_t>::failure(
        history_error("History requires an editable Part without another active command."));
  if (session.derived_results_fresh() &&
      (!session.part_shape_cache() ||
       !session.part_shape_cache()->validate_complete_history_result(*part)))
    return Result<std::size_t>::failure(
        history_error("Recompute the current Part before editing history."));
  const auto history = part->executable_history();
  if (!history)
    return Result<std::size_t>::failure(history.error());
  if (history.value().empty())
    return Result<std::size_t>::failure(history_error("The Part has no executable features."));
  const auto source = serialize_part_document_to_json(*part);
  if (!source)
    return Result<std::size_t>::failure(source.error());
  intent_ = {};
  for (const auto& entry : history.value()) {
    intent_.order.push_back(entry.node_id);
    if (entry.state == PartFeatureState::Suppressed)
      intent_.suppressed.push_back(entry.node_id);
  }
  source_json_ = source.value();
  initial_ = intent_;
  generation_ = part->executable_generation();
  active_ = true;
  return Result<std::size_t>::success(intent_.order.size());
}

bool GuiPartHistoryController::current(const GuiDocumentSession& session) const {
  if (!active_ || !session.part_document() || session.part_workflow_active() ||
      session.part_document()->executable_generation() != generation_ ||
      (session.task().active() && session.task().command_id() != task_id))
    return false;
  const auto source = serialize_part_document_to_json(*session.part_document());
  return source && source.value() == source_json_;
}

Result<std::size_t> GuiPartHistoryController::stage_order(const GuiDocumentSession& session,
                                                          std::vector<std::string> order) {
  if (!current(session))
    return Result<std::size_t>::failure(
        history_error("The history source changed. Restart editing."));
  intent_.order = std::move(order);
  return Result<std::size_t>::success(intent_.order.size());
}

Result<std::size_t> GuiPartHistoryController::stage_suppression(const GuiDocumentSession& session,
                                                                std::string feature,
                                                                bool suppressed,
                                                                bool include_dependents) {
  if (!current(session))
    return Result<std::size_t>::failure(
        history_error("The history source changed. Restart editing."));
  if (std::find(intent_.order.begin(), intent_.order.end(), feature) == intent_.order.end())
    return Result<std::size_t>::failure(history_error("The selected feature is missing."));
  const auto baseline = session.part_document()->executable_history();
  if (!baseline)
    return Result<std::size_t>::failure(baseline.error());
  intent_.suppressed.clear();
  for (const auto& entry : baseline.value())
    if (entry.state == PartFeatureState::Suppressed)
      intent_.suppressed.push_back(entry.node_id);
  std::erase(intent_.suppressed, feature);
  if (suppressed)
    intent_.suppressed.push_back(std::move(feature));
  if (suppressed && include_dependents) {
    // This closure is requested explicitly. It never changes the source flags.
    for (;;) {
      const auto blocked = blocked_dependents(session);
      if (!blocked)
        return Result<std::size_t>::failure(blocked.error());
      if (blocked.value().empty())
        break;
      intent_.suppressed.insert(intent_.suppressed.end(), blocked.value().begin(),
                                blocked.value().end());
    }
  }
  std::vector<std::string> normalized;
  for (const auto& id : intent_.order)
    if (std::find(intent_.suppressed.begin(), intent_.suppressed.end(), id) !=
        intent_.suppressed.end())
      normalized.push_back(id);
  intent_.suppressed = std::move(normalized);
  return Result<std::size_t>::success(intent_.suppressed.size());
}

Result<PartDocument> GuiPartHistoryController::candidate(const GuiDocumentSession& session) const {
  if (!current(session))
    return Result<PartDocument>::failure(
        history_error("The history source changed. Restart editing."));
  return session.part_document()->with_history_intent(intent_);
}

Result<std::vector<std::string>>
GuiPartHistoryController::blocked_dependents(const GuiDocumentSession& session) const {
  const auto draft = candidate(session);
  if (!draft)
    return Result<std::vector<std::string>>::failure(draft.error());
  const auto plan = draft.value().create_history_evaluation_plan();
  if (!plan)
    return Result<std::vector<std::string>>::failure(plan.error());
  std::vector<std::string> blocked;
  for (const auto& entry : plan.value().entries)
    if (entry.state == PartHistoryExecutionState::Blocked)
      blocked.push_back(entry.feature.node_id);
  return Result<std::vector<std::string>>::success(std::move(blocked));
}

Result<GuiPartHistoryPreview>
GuiPartHistoryController::preview(const GuiDocumentSession& session) const {
  auto draft = candidate(session);
  if (!draft)
    return Result<GuiPartHistoryPreview>::failure(draft.error());
  auto context = make_step_import_context(session);
  if (!context)
    return Result<GuiPartHistoryPreview>::failure(context.error());
  auto shapes = geometry::ShapeCache::create(ShapeCacheId("gui.part.history.preview"));
  if (!shapes)
    return Result<GuiPartHistoryPreview>::failure(shapes.error());
  const auto computed = geometry::GeometryRecomputeExecutor{context.value().get()}.execute_document(
      draft.value(), shapes.value());
  if (!computed)
    return Result<GuiPartHistoryPreview>::failure(computed.error());
  return Result<GuiPartHistoryPreview>::success(
      {std::move(draft.value()), std::move(shapes.value())});
}

Result<std::size_t> GuiPartHistoryController::apply(GuiDocumentSession& session,
                                                    std::string label) {
  if (!current(session))
    return Result<std::size_t>::failure(
        history_error("The history source changed. Restart editing."));
  if (intent_ == initial_)
    return Result<std::size_t>::failure(history_error("No history change is staged."));
  // The existing transaction validates the complete candidate Geometry before
  // publication. Do not repeat that full rebuild in the controller and shell.
  const auto edit = intent_;
  const auto mutate = [edit](PartDocument& part) { return part.apply_history_intent(edit); };
  auto committed = session.reference_repair_mode()
                       ? session.commit_part_repair_transaction(std::move(label), mutate)
                       : session.commit_part_transaction(std::move(label), mutate);
  if (committed)
    cancel();
  return committed;
}

void GuiPartHistoryController::cancel() noexcept {
  active_ = false;
  source_json_.clear();
  intent_ = {};
  initial_ = {};
}
} // namespace blcad::gui
