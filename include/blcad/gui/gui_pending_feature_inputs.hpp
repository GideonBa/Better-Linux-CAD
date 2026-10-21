#pragma once

#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <algorithm>
#include <optional>
#include <vector>

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
inline Result<geometry::GeometryShape>
preview_feature_body(const PartDocument& candidate, const FeatureId& feature, const BodyId& body) {
  auto cache = geometry::ShapeCache::create(ShapeCacheId(feature.value() + ".interactive_preview"));
  if (!cache)
    return Result<geometry::GeometryShape>::failure(cache.error());
  auto recomputed =
      geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache.value());
  if (!recomputed)
    return Result<geometry::GeometryShape>::failure(recomputed.error());
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
