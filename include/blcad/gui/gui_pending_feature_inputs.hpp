#pragma once

#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <algorithm>
#include <optional>
#include <vector>
#include <unordered_set>
#include <type_traits>

namespace blcad::gui {
namespace detail {
inline Result<std::size_t> validate_new_feature_parameters(
    const GuiDocumentSession& session, const std::vector<Parameter>& pending,
    const std::vector<std::pair<ParameterId, ParameterType>>& expected) {
  const auto* part = session.part_document();
  if (!part)
    return Result<std::size_t>::failure(Error::validation("feature", "no active Part"));
  std::vector<ParameterId> seen;
  std::vector<std::string> names;
  for (const auto& parameter : pending) {
    const bool accepted = std::any_of(expected.begin(), expected.end(), [&](const auto& value) {
      return value.first == parameter.id() && value.second == parameter.type();
    });
    if (!accepted || part->find_parameter(parameter.id()) ||
        std::find(seen.begin(), seen.end(), parameter.id()) != seen.end() ||
        std::find(names.begin(), names.end(), parameter.name()) != names.end() ||
        std::any_of(part->parameters().begin(), part->parameters().end(),
                    [&](const auto& existing) { return existing.name() == parameter.name(); }))
      return Result<std::size_t>::failure(Error::validation(
          parameter.id().value(),
          "new feature parameter has an incompatible type or colliding identity/name"));
    seen.push_back(parameter.id());
    names.push_back(parameter.name());
  }
  return Result<std::size_t>::success(pending.size());
}
inline Result<std::size_t> provision_new_feature_parameters(PartDocument& part,
                                                            const std::vector<Parameter>& pending) {
  for (const auto& parameter : pending) {
    auto added = part.add_parameter(parameter);
    if (!added)
      return added;
  }
  return Result<std::size_t>::success(pending.size());
}
inline Result<geometry::ShapeCache>
recompute_preview_geometry(const GuiDocumentSession& session, const PartDocument& candidate,
                           const std::vector<std::string>& changed_features) {
  const auto* committed = session.part_document();
  const auto* existing = session.part_shape_cache();
  const auto backings_match = [](const PartDocument& part, const geometry::ShapeCache& cache) {
    std::unordered_set<std::string> producers;
    producers.reserve(cache.feature_shape_count());
    const auto collect = [&](const auto& records) {
      for (const auto& record : records) {
        if constexpr (std::is_same_v<std::decay_t<decltype(record)>, SurfaceFeature>)
          producers.insert(surface_feature_id(record).value());
        else
          producers.insert(record.id().value());
      }
    };
    collect(part.features());
    collect(part.revolve_features());
    collect(part.sweep_features());
    collect(part.loft_features());
    collect(part.surface_features());
    collect(part.linear_pattern_features());
    collect(part.circular_pattern_features());
    collect(part.mirror_features());
    collect(part.fillet_features());
    collect(part.chamfer_features());
    collect(part.shell_features());
    collect(part.draft_features());
    collect(part.body_boolean_features());
    collect(part.body_transforms());
    return producers.size() == cache.feature_shape_count() &&
           std::all_of(
               cache.feature_shapes().begin(), cache.feature_shapes().end(),
               [&](const auto& shape) { return producers.contains(shape.feature_id.value()); });
  };
  const bool reuse =
      committed && existing && committed->id() == candidate.id() && !candidate.bodies().empty() &&
      session.recompute_status() == GuiRecomputeStatus::Fresh &&
      backings_match(*committed, *existing) &&
      std::all_of(committed->invalidation_state().entries().begin(),
                  committed->invalidation_state().entries().end(),
                  [](const auto& entry) { return entry.status == InvalidationStatus::Clean; });
  auto cache = reuse ? Result<geometry::ShapeCache>::success(*existing)
                     : geometry::ShapeCache::create(ShapeCacheId("interactive.preview"));
  if (!cache)
    return cache;
  if (!reuse) {
    auto recomputed =
        geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache.value());
    if (!recomputed)
      return Result<geometry::ShapeCache>::failure(recomputed.error());
    return cache;
  }
  auto valid = candidate.validate_reference_sources();
  if (!valid)
    return Result<geometry::ShapeCache>::failure(valid.error());
  auto roots = changed_features;
  const auto& committed_nodes = committed->dependency_graph().nodes();
  const std::unordered_set<std::string> existing_nodes(committed_nodes.begin(),
                                                       committed_nodes.end());
  for (const auto& node : candidate.dependency_graph().nodes())
    if (!existing_nodes.contains(node))
      roots.push_back(node);
  for (const auto& entry : candidate.invalidation_state().entries())
    if (entry.status != InvalidationStatus::Clean)
      roots.push_back(entry.node_id);
  auto plan = RecomputePlan::from_graph_and_changed_nodes(candidate.dependency_graph(), roots);
  if (!plan)
    return Result<geometry::ShapeCache>::failure(plan.error());
  auto recomputed =
      geometry::GeometryRecomputeExecutor{}.execute_plan(candidate, plan.value(), cache.value());
  if (!recomputed)
    return Result<geometry::ShapeCache>::failure(recomputed.error());
  return cache;
}
inline Result<geometry::GeometryShape> preview_feature_body(const GuiDocumentSession& session,
                                                            const PartDocument& candidate,
                                                            const FeatureId& feature,
                                                            const BodyId& body) {
  auto cache = recompute_preview_geometry(session, candidate, {feature.value()});
  if (!cache)
    return Result<geometry::GeometryShape>::failure(cache.error());
  const auto* shape = cache.value().find_body_shape(body);
  if (!shape || shape->empty())
    return Result<geometry::GeometryShape>::failure(
        Error::geometry(feature.value(), "feature preview produced no Body shape"));
  return Result<geometry::GeometryShape>::success(*shape);
}

inline Result<std::size_t> validate_new_feature_body(const GuiDocumentSession& session,
                                                     const BodyId& id,
                                                     const std::optional<Body>& pending) {
  const auto* part = session.part_document();
  if (!part)
    return Result<std::size_t>::failure(Error::validation(id.value(), "no active Part"));
  if (pending) {
    if (pending->id() != id || part->find_body(id) ||
        std::any_of(part->bodies().begin(), part->bodies().end(),
                    [&](const auto& existing) { return existing.name() == pending->name(); }))
      return Result<std::size_t>::failure(Error::validation(
          id.value(), "new result Body has a colliding or incompatible identity"));
  } else if (!part->find_body(id))
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "result requires an existing Body"));
  return Result<std::size_t>::success(1);
}
} // namespace detail
} // namespace blcad::gui
