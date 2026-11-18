#include "blcad/geometry/recompute_executor.hpp"
#include "effective_part_source.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace blcad::geometry {
namespace {
// Compare canonical intent, rather than trusting a mutation callback's dirty
// flags. Unknown fields, removals and changed existing input edges fall back
// to full execution. A Body result alias follows its new producing feature.
std::optional<std::vector<std::string>> transition_roots(const PartDocument& before,
                                                         const PartDocument& after,
                                                         const std::string& before_json,
                                                         const std::string& after_json) {
  const auto& previous_graph = before.dependency_graph();
  const auto& next_graph = after.dependency_graph();
  const std::unordered_set<std::string> previous_nodes(previous_graph.nodes().begin(),
                                                       previous_graph.nodes().end());
  for (const auto& node : previous_nodes)
    if (!next_graph.has_node(node))
      return std::nullopt;
  auto previous_edges = previous_graph.dependencies();
  auto next_edges = next_graph.dependencies();
  std::vector<std::string> roots;
  std::unordered_set<std::string> body_aliases;
  for (const auto& body : after.bodies()) {
    const auto alias = "body:" + body.id().value();
    body_aliases.insert(alias);
    const auto producers = [&](const auto& edges) {
      std::vector<std::string> result;
      for (const auto& [source, target] : edges)
        if (target == alias)
          result.push_back(source);
      std::sort(result.begin(), result.end());
      return result;
    };
    const auto old_producers = producers(previous_edges);
    const auto new_producers = producers(next_edges);
    if (old_producers != new_producers) {
      roots.push_back(alias);
      // Recompute the new final producer even when it already existed (for
      // example, reordered intent); an alias is not itself an executable node.
      roots.insert(roots.end(), new_producers.begin(), new_producers.end());
    }
  }
  std::erase_if(previous_edges,
                [&](const auto& edge) { return body_aliases.contains(edge.second); });
  std::erase_if(next_edges, [&](const auto& edge) {
    return body_aliases.contains(edge.second) || !previous_nodes.contains(edge.first) ||
           !previous_nodes.contains(edge.second);
  });
  std::sort(previous_edges.begin(), previous_edges.end());
  std::sort(next_edges.begin(), next_edges.end());
  if (previous_edges != next_edges)
    return std::nullopt;

  using Json = nlohmann::json;
  const auto previous = Json::parse(before_json, nullptr, false);
  const auto next = Json::parse(after_json, nullptr, false);
  if (!previous.is_object() || !next.is_object())
    return std::nullopt;
  const std::unordered_set<std::string> records{"parameters",
                                                "datum_planes",
                                                "datum_axes",
                                                "construction_points",
                                                "construction_lines",
                                                "construction_planes",
                                                "derived_workplanes",
                                                "result_face_workplanes",
                                                "sketches",
                                                "sketches_3d",
                                                "path_curves",
                                                "features",
                                                "revolve_features",
                                                "sweep_features",
                                                "loft_features",
                                                "surface_features",
                                                "sketch_driven_features",
                                                "direct_edit_features",
                                                "part_patterns",
                                                "mirror_features",
                                                "edge_treatments",
                                                "shell_features",
                                                "draft_features",
                                                "body_booleans",
                                                "body_transforms"};
  std::unordered_set<std::string> keys;
  for (const auto& [key, value] : previous.items())
    keys.insert(key);
  for (const auto& [key, value] : next.items())
    keys.insert(key);
  for (const auto& key : keys) {
    if (previous.contains(key) && next.contains(key) && previous.at(key) == next.at(key))
      continue;
    // Core reconstructs feature ordering as dependency edges. Existing edges
    // were checked above; newly inserted producers are included below.
    if (key == "feature_order")
      continue;
    if (!records.contains(key))
      return std::nullopt;
    const auto old_records = previous.value(key, Json::array());
    const auto new_records = next.value(key, Json::array());
    if (!old_records.is_array() || !new_records.is_array())
      return std::nullopt;
    std::unordered_map<std::string, Json> old_by_id;
    for (const auto& record : old_records) {
      if (!record.is_object() || !record.contains("id") || !record.at("id").is_string() ||
          !old_by_id.emplace(record.at("id").get<std::string>(), record).second)
        return std::nullopt;
    }
    std::unordered_set<std::string> seen;
    for (const auto& record : new_records) {
      if (!record.is_object() || !record.contains("id") || !record.at("id").is_string())
        return std::nullopt;
      const auto id = record.at("id").get<std::string>();
      if (!next_graph.has_node(id) || !seen.insert(id).second)
        return std::nullopt;
      const auto old = old_by_id.find(id);
      if (old == old_by_id.end() || old->second != record)
        roots.push_back(id);
    }
    for (const auto& [id, record] : old_by_id)
      if (!seen.contains(id))
        return std::nullopt;
  }
  return roots;
}

} // namespace

Result<GeometryRecomputeSummary>
GeometryRecomputeExecutor::execute_transition(const PartDocument& before, const PartDocument& after,
                                              ShapeCache& cache) const {
  const auto baseline = cache.validate_complete_geometry_result(before);
  if (!baseline)
    return Result<GeometryRecomputeSummary>::failure(baseline.error());
  const auto full = [&]() -> Result<GeometryRecomputeSummary> {
    auto created = ShapeCache::create(cache.id());
    if (!created)
      return Result<GeometryRecomputeSummary>::failure(created.error());
    auto computed = execute_document(after, created.value());
    if (!computed)
      return computed;
    const auto complete = created.value().validate_complete_geometry_result(after);
    if (!complete)
      return Result<GeometryRecomputeSummary>::failure(complete.error());
    cache = std::move(created.value());
    return computed;
  };
  if (before.id() != after.id())
    return Result<GeometryRecomputeSummary>::failure(Error::dependency(
        after.id().value(), "Verified geometry transition requires one Part identity"));
  // Metadata refresh owns its own stronger material/source equivalence checks.
  auto working = cache;
  auto metadata = refresh_material_metadata(before, after, working);
  if (metadata) {
    cache = std::move(working);
    return metadata;
  }
  const auto legacy_aggregate = [](const PartDocument& part) {
    return std::any_of(part.features().begin(), part.features().end(),
                       [](const auto& feature) { return !feature.body_result_context(); });
  };
  if (before.bodies().empty() || after.bodies().empty() || legacy_aggregate(before) ||
      legacy_aggregate(after) || !before.step_import_sources().empty() ||
      !after.step_import_sources().empty())
    return full();
  const auto previous_source = detail::effective_part_source(before);
  const auto next_source = detail::effective_part_source(after);
  if (!previous_source || !next_source)
    return Result<GeometryRecomputeSummary>::failure(!previous_source ? previous_source.error()
                                                                      : next_source.error());
  const auto roots = transition_roots(before, after, previous_source.value(), next_source.value());
  if (!roots)
    return full();
  const auto previous_history = before.executable_history();
  const auto next_history = after.executable_history();
  if (!previous_history || !next_history)
    return Result<GeometryRecomputeSummary>::failure(!previous_history ? previous_history.error()
                                                                       : next_history.error());
  std::unordered_set<std::string> previous_ids;
  for (const auto& entry : previous_history.value())
    previous_ids.insert(entry.node_id);
  std::size_t appended = 0;
  for (const auto& entry : next_history.value())
    appended += !previous_ids.contains(entry.node_id);
  // A global generation jump beyond known additions can hide erase/re-add of
  // an identical id/record. No retained producer can certify that new lifetime.
  if (after.executable_generation() < before.executable_generation() ||
      after.executable_generation() - before.executable_generation() != appended)
    return full();
  auto plan = RecomputePlan::from_graph_and_changed_nodes(after.dependency_graph(), *roots);
  if (!plan)
    return Result<GeometryRecomputeSummary>::failure(plan.error());
  const auto valid = after.validate_reference_sources();
  if (!valid)
    return Result<GeometryRecomputeSummary>::failure(valid.error());
  // Only this executor-owned path can certify a copied legacy cache: the old
  // certificate, canonical delta and complete dependency closure were checked.
  auto computed = execute_plan(after, plan.value(), working);
  if (!computed)
    return computed;
  record_complete_geometry_source(after, working);
  const auto complete = working.validate_complete_geometry_result(after);
  if (!complete)
    return Result<GeometryRecomputeSummary>::failure(complete.error());
  cache = std::move(working);
  return computed;
}
} // namespace blcad::geometry
