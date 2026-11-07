#include "blcad/core/part_document.hpp"

#include <functional>
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
  return Result<History>::success(std::move(result));
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
