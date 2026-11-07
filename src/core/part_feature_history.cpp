#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace blcad {
namespace {
template <typename Record> std::string executable_id(const Record& record) {
  if constexpr (requires { record.id(); })
    return record.id().value();
  else
    return surface_feature_id(record).value();
}

// Creator availability is a hard prerequisite. Sequential edges between modifiers
// are instead rebuilt for the proposed order by the typed adders.
bool creates_body_identity(const FeatureBodyResultContext& context) {
  return !context.target_body() || context.effective_produced_body() != *context.target_body();
}
template <typename Record> bool creates_output(const Record& record) {
  if constexpr (std::is_same_v<Record, ImportedBodyFeature> ||
                std::is_same_v<Record, SurfaceFeature>)
    return true;
  else if constexpr (std::is_same_v<Record, Feature>)
    return record.body_result_context() ? creates_body_identity(*record.body_result_context())
                                        : record.type() == FeatureType::AdditiveExtrude;
  else if constexpr (std::is_same_v<Record, BodyBooleanFeature>)
    return record.result_mode() == BodyBooleanResultMode::NewBody;
  else if constexpr (requires { record.body_result_context(); })
    return creates_body_identity(record.body_result_context());
  else
    return false;
}
} // namespace

template <typename Visitor> void PartDocument::visit_executable_families(Visitor&& visitor) const {
  visitor(&PartDocument::imported_body_features_, &PartDocument::add_imported_body_feature,
          PartFeatureFamily::ImportedBody);
  visitor(&PartDocument::features_, &PartDocument::add_feature, PartFeatureFamily::Extrude);
  visitor(&PartDocument::revolve_features_, &PartDocument::add_revolve_feature,
          PartFeatureFamily::Revolve);
  visitor(&PartDocument::sweep_features_, &PartDocument::add_sweep_feature,
          PartFeatureFamily::Sweep);
  visitor(&PartDocument::loft_features_, &PartDocument::add_loft_feature, PartFeatureFamily::Loft);
  visitor(&PartDocument::surface_features_, &PartDocument::add_surface_feature,
          PartFeatureFamily::Surface);
  visitor(&PartDocument::linear_pattern_features_, &PartDocument::add_linear_pattern_feature,
          PartFeatureFamily::LinearPattern);
  visitor(&PartDocument::circular_pattern_features_, &PartDocument::add_circular_pattern_feature,
          PartFeatureFamily::CircularPattern);
  visitor(&PartDocument::mirror_features_, &PartDocument::add_mirror_feature,
          PartFeatureFamily::Mirror);
  visitor(&PartDocument::fillet_features_, &PartDocument::add_fillet_feature,
          PartFeatureFamily::Fillet);
  visitor(&PartDocument::chamfer_features_, &PartDocument::add_chamfer_feature,
          PartFeatureFamily::Chamfer);
  visitor(&PartDocument::shell_features_, &PartDocument::add_shell_feature,
          PartFeatureFamily::Shell);
  visitor(&PartDocument::draft_features_, &PartDocument::add_draft_feature,
          PartFeatureFamily::Draft);
  visitor(&PartDocument::body_boolean_features_, &PartDocument::add_body_boolean_feature,
          PartFeatureFamily::BodyBoolean);
  visitor(&PartDocument::body_transforms_, &PartDocument::add_body_transform,
          PartFeatureFamily::BodyTransform);
}

Result<std::vector<PartFeatureHistoryEntry>> PartDocument::executable_history() const {
  using History = std::vector<PartFeatureHistoryEntry>;
  std::unordered_map<std::string, PartFeatureFamily> registered;
  visit_executable_families([&](auto member, auto, PartFeatureFamily family) {
    for (const auto& record : this->*member)
      registered.emplace(executable_id(record), family);
  });
  const auto order = dependency_graph_.topological_order();
  if (!order)
    return Result<History>::failure(order.error());
  History result;
  result.reserve(registered.size());
  for (const auto& node : order.value())
    if (const auto found = registered.find(node); found != registered.end()) {
      result.push_back({node, found->second});
      registered.erase(found);
    }
  if (!registered.empty())
    return Result<History>::failure(Error::dependency(
        "part.history", "An executable record is absent from the dependency graph."));
  if (history_intent_) {
    if (history_intent_->order.size() != result.size())
      return Result<History>::failure(
          Error::dependency("part.history", "Authored history is incomplete."));
    History authored;
    std::unordered_set<std::string> seen;
    for (const auto& id : history_intent_->order) {
      const auto found = std::find_if(result.begin(), result.end(),
                                      [&](const auto& entry) { return entry.node_id == id; });
      if (found == result.end() || !seen.insert(id).second)
        return Result<History>::failure(Error::dependency(
            "part.history", "Authored history contains an unknown or duplicate executable."));
      auto entry = *found;
      if (std::find(history_intent_->suppressed.begin(), history_intent_->suppressed.end(), id) !=
          history_intent_->suppressed.end())
        entry.state = PartFeatureState::Suppressed;
      authored.push_back(std::move(entry));
    }
    const auto valid = validate_history_graph(dependency_graph_);
    if (!valid)
      return Result<History>::failure(valid.error());
    return Result<History>::success(std::move(authored));
  }
  return Result<History>::success(std::move(result));
}

namespace {
std::vector<PartHistoryDiagnostic> order_diagnostics(const DependencyGraph& graph,
                                                     const std::vector<std::string>& order) {
  std::unordered_map<std::string, std::size_t> positions;
  for (std::size_t i = 0; i < order.size(); ++i)
    positions.emplace(order[i], i);
  std::vector<PartHistoryDiagnostic> diagnostics;
  for (const auto& source : order)
    for (const auto& dependent : graph.transitive_dependents(source)) {
      const auto found = positions.find(dependent);
      if (found != positions.end() && positions.at(source) >= found->second)
        diagnostics.push_back(
            {PartHistoryDiagnosticCode::DependencyAfterConsumer, source, dependent,
             "Executable " + source + " must precede dependent " + dependent + "."});
    }
  return diagnostics;
}
Error history_error(const PartHistoryDiagnostic& diagnostic) {
  const auto id = diagnostic.consumer_id.empty() ? "part.history" : diagnostic.consumer_id;
  return diagnostic.code == PartHistoryDiagnosticCode::DependencyAfterConsumer
             ? Error::dependency(id, diagnostic.message)
             : Error::validation(id, diagnostic.message);
}
} // namespace

const std::optional<PartFeatureHistoryIntent>&
PartDocument::authored_history_intent() const noexcept {
  return history_intent_;
}

Result<std::size_t> PartDocument::validate_history_graph(const DependencyGraph& graph,
                                                         std::string_view appended) const {
  if (!history_intent_)
    return Result<std::size_t>::success(0);
  auto order = history_intent_->order;
  if (!appended.empty() && std::find(order.begin(), order.end(), appended) == order.end())
    order.emplace_back(appended);
  const auto diagnostics = order_diagnostics(graph, order);
  if (!diagnostics.empty())
    return Result<std::size_t>::failure(history_error(diagnostics.front()));
  return Result<std::size_t>::success(order.size());
}

Result<std::size_t> PartDocument::sync_graph(DependencyGraph graph, InvalidationState& invalidation,
                                             DependencyGraph& destination,
                                             std::string_view appended) const {
  const auto valid = validate_history_graph(graph, appended);
  if (!valid)
    return valid;
  const auto synced = invalidation.sync_from_graph(graph);
  if (!synced)
    return synced;
  destination = std::move(graph);
  return Result<std::size_t>::success(destination.node_count());
}

void PartDocument::append_history_executable(const std::string& id) {
  if (history_intent_ && std::find(history_intent_->order.begin(), history_intent_->order.end(),
                                   id) == history_intent_->order.end())
    history_intent_->order.push_back(id);
}
void PartDocument::erase_history_executable(const std::string& id) {
  if (history_intent_) {
    std::erase(history_intent_->order, id);
    std::erase(history_intent_->suppressed, id);
  }
}

Result<PartDocument>
PartDocument::build_history_candidate(PartFeatureHistoryIntent intent,
                                      std::vector<PartHistoryDiagnostic>& diagnostics) const {
  if (is_reference_part()) {
    diagnostics.push_back({PartHistoryDiagnosticCode::ReferencePartReadOnly,
                           {},
                           {},
                           "Reference Part history is read-only."});
    return Result<PartDocument>::failure(history_error(diagnostics.front()));
  }
  std::unordered_map<std::string, PartFeatureFamily> registered;
  visit_executable_families([&](auto member, auto, PartFeatureFamily family) {
    for (const auto& record : this->*member)
      registered.emplace(executable_id(record), family);
  });
  std::unordered_set<std::string> seen;
  for (const auto& id : intent.order) {
    if (!registered.contains(id))
      diagnostics.push_back({PartHistoryDiagnosticCode::UnknownExecutable,
                             {},
                             id,
                             "Unknown executable in authored history: " + id});
    if (!seen.insert(id).second)
      diagnostics.push_back({PartHistoryDiagnosticCode::DuplicateExecutable,
                             {},
                             id,
                             "Duplicate executable in authored history: " + id});
  }
  // Report missing identities in stable graph-node order, never hash-map order.
  for (const auto& node : dependency_graph_.nodes())
    if (registered.contains(node) && !seen.contains(node))
      diagnostics.push_back({PartHistoryDiagnosticCode::MissingExecutable,
                             {},
                             node,
                             "Authored history omits executable: " + node});
  std::unordered_set<std::string> suppressed;
  for (const auto& id : intent.suppressed) {
    const auto found = registered.find(id);
    if (found == registered.end())
      diagnostics.push_back({PartHistoryDiagnosticCode::UnknownSuppressedExecutable,
                             {},
                             id,
                             "Unknown suppressed executable: " + id});
    else if (found->second == PartFeatureFamily::ImportedBody)
      diagnostics.push_back(
          {PartHistoryDiagnosticCode::ImportedBaseRequired,
           id,
           {},
           "Imported Body base " + id + " is indispensable and cannot be suppressed."});
    if (!suppressed.insert(id).second)
      diagnostics.push_back({PartHistoryDiagnosticCode::DuplicateSuppression,
                             {},
                             id,
                             "Duplicate suppression intent: " + id});
  }
  if (!diagnostics.empty())
    return Result<PartDocument>::failure(history_error(diagnostics.front()));
  std::unordered_set<std::string> creators;
  visit_executable_families([&](auto member, auto, PartFeatureFamily) {
    for (const auto& record : this->*member)
      if (creates_output(record))
        creators.insert(executable_id(record));
  });
  for (auto& diagnostic : order_diagnostics(dependency_graph_, intent.order))
    if (creators.contains(diagnostic.producer_id))
      diagnostics.push_back(std::move(diagnostic));
  if (!diagnostics.empty())
    return Result<PartDocument>::failure(history_error(diagnostics.front()));
  PartDocument candidate = *this;
  candidate.history_intent_.reset();
  const auto replayed = candidate.replay_feature_order(intent.order);
  if (!replayed) {
    diagnostics.push_back({PartHistoryDiagnosticCode::RebuildRejected,
                           {},
                           replayed.error().object_id(),
                           replayed.error().message()});
    return Result<PartDocument>::failure(replayed.error());
  }
  diagnostics = order_diagnostics(candidate.dependency_graph_, intent.order);
  if (!diagnostics.empty())
    return Result<PartDocument>::failure(history_error(diagnostics.front()));
  intent.suppressed.clear();
  for (const auto& id : intent.order)
    if (suppressed.contains(id))
      intent.suppressed.push_back(id);
  candidate.history_intent_ = std::move(intent);
  for (const auto& id : candidate.history_intent_->order) {
    const auto changed =
        candidate.invalidation_state_.mark_changed(candidate.dependency_graph_, id);
    if (!changed)
      return Result<PartDocument>::failure(changed.error());
  }
  return Result<PartDocument>::success(std::move(candidate));
}

std::vector<PartHistoryDiagnostic>
PartDocument::diagnose_history_intent(const PartFeatureHistoryIntent& intent) const {
  std::vector<PartHistoryDiagnostic> diagnostics;
  (void)build_history_candidate(intent, diagnostics);
  return diagnostics;
}
Result<PartDocument> PartDocument::with_history_intent(PartFeatureHistoryIntent intent) const {
  std::vector<PartHistoryDiagnostic> diagnostics;
  return build_history_candidate(std::move(intent), diagnostics);
}
Result<std::size_t> PartDocument::apply_history_intent(PartFeatureHistoryIntent intent) {
  auto candidate = with_history_intent(std::move(intent));
  if (!candidate)
    return Result<std::size_t>::failure(candidate.error());
  const auto count = candidate.value().history_intent_->order.size();
  *this = std::move(candidate.value());
  return Result<std::size_t>::success(count);
}

Result<std::size_t> PartDocument::replay_feature_order(const std::vector<std::string>& order) {
  const PartDocument& saved = *this;
  PartDocument candidate = saved;
  std::unordered_map<std::string, std::function<Result<std::size_t>()>> replay;
  std::vector<std::function<void()>> restore_collections;
  saved.visit_executable_families([&](auto member, auto adder, PartFeatureFamily) {
    for (const auto& record : saved.*member)
      replay.emplace(executable_id(record),
                     [&, record, adder] { return (candidate.*adder)(record); });
    (candidate.*member).clear();
    restore_collections.emplace_back([&, member] { candidate.*member = saved.*member; });
  });
  if (order.size() != replay.size())
    return Result<std::size_t>::failure(Error::validation(
        "part_document_json", "feature_order must name every executable exactly once"));
  std::unordered_set<std::string> seen;
  for (const auto& id : order)
    if (!replay.contains(id) || !seen.insert(id).second)
      return Result<std::size_t>::failure(Error::validation(
          "part_document_json", "feature_order contains an invalid or duplicate executable id"));

  std::vector<DependencyGraph::DependencyEdge> consumers;
  for (const auto& edge : saved.dependency_graph_.dependencies())
    if (replay.contains(edge.first) && !replay.contains(edge.second) &&
        !edge.second.starts_with("body:"))
      consumers.push_back(edge);
  for (const auto& [id, unused] : replay) {
    const auto removed = candidate.dependency_graph_.remove_node(id);
    if (!removed)
      return removed;
    candidate.invalidation_state_.untrack_node(id);
  }
  for (const auto& id : order) {
    const auto added = replay.at(id)();
    if (!added)
      return added;
  }
  for (const auto& [source, dependent] : consumers)
    if (!candidate.dependency_graph_.has_dependency(source, dependent)) {
      const auto restored = candidate.dependency_graph_.add_dependency(source, dependent);
      if (!restored)
        return restored;
    }
  if (candidate.dependency_graph_.has_cycle())
    return Result<std::size_t>::failure(
        Error::validation("part_document_json", "feature_order creates a dependency cycle"));
  const auto synced = candidate.invalidation_state_.sync_from_graph(candidate.dependency_graph_);
  if (!synced)
    return synced;
  for (const auto& restore : restore_collections)
    restore();
  *this = std::move(candidate);
  return Result<std::size_t>::success(order.size());
}

} // namespace blcad
