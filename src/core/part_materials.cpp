#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <utility>

namespace blcad {
Result<std::size_t> PartDocument::add_engineering_resource(EngineeringResource resource) {
  if (is_reference_part())
    return Result<std::size_t>::failure(reference_part_edit_error());
  return engineering_resources_.insert(std::move(resource));
}
Result<std::size_t>
PartDocument::adopt_engineering_resource_revision(const EngineeringResourceKey& source,
                                                  EngineeringResource replacement) {
  if (is_reference_part())
    return Result<std::size_t>::failure(reference_part_edit_error());
  const auto next = engineering_resource_key(replacement);
  if (!engineering_resources_.find(source) || source.id() != next.id())
    return Result<std::size_t>::failure(Error::validation(
        id_.value(), "Revision adoption requires an existing source with the same resource id"));
  auto candidate = *this;
  const auto added = candidate.engineering_resources_.insert(std::move(replacement));
  if (!added)
    return added;
  std::size_t changed = added.value();
  if (source != next && candidate.engineering_resources_.find_material(next)) {
    if (candidate.default_material_ == source) {
      candidate.default_material_ = next;
      ++changed;
    }
    for (auto& entry : candidate.body_materials_)
      if (entry.material == source) {
        entry.material = next;
        ++changed;
      }
  }
  if (source != next && candidate.engineering_resources_.find_table(next)) {
    // Each update uses the existing modifier/history transaction. Work on a
    // copy so one missing row, wrong cell type or later invalid consumer rolls
    // back the catalog insertion and all previously remapped consumers.
    const auto holes = candidate.hole_features_;
    for (const auto& hole : holes) {
      const auto* selected = std::get_if<HoleStandardSelection>(&hole.diameter());
      if (!selected || selected->table != source)
        continue;
      auto selection = *selected;
      selection.table = next;
      auto replacement_hole = HoleFeature::create(
          hole.id(), hole.name(), hole.kind(), hole.target_body(), hole.support(),
          hole.placements(), std::move(selection), hole.extent(), hole.direction(), hole.tip());
      if (!replacement_hole)
        return Result<std::size_t>::failure(replacement_hole.error());
      auto updated = candidate.update_hole_feature(std::move(replacement_hole.value()));
      if (!updated)
        return updated;
      ++changed;
    }
  }
  *this = std::move(candidate);
  return Result<std::size_t>::success(changed);
}
Result<std::size_t> PartDocument::remove_engineering_resource(const EngineeringResourceKey& key) {
  if (is_reference_part())
    return Result<std::size_t>::failure(reference_part_edit_error());
  if (default_material_ == key ||
      std::any_of(body_materials_.begin(), body_materials_.end(),
                  [&](const auto& entry) { return entry.material == key; }))
    return Result<std::size_t>::failure(Error::validation(
        id_.value(), "Material revision is still assigned to the Part or a Body"));
  for (const auto& hole : hole_features_)
    if (const auto* selected = std::get_if<HoleStandardSelection>(&hole.diameter());
        selected && selected->table == key)
      return Result<std::size_t>::failure(Error::dependency(
          hole.id().value(), "Standard revision is still referenced by Hole intent"));
  auto candidate = *this;
  auto removed = candidate.engineering_resources_.remove(key);
  if (!removed)
    return removed;
  const auto node = engineering_resource_node_id(key);
  if (candidate.dependency_graph_.has_node(node) && !candidate.has_authored_node_identity(node) &&
      !candidate.is_imported_authority_node(node)) {
    if (!candidate.dependency_graph_.direct_dependents(node).empty())
      return Result<std::size_t>::failure(
          Error::dependency(node, "Standard revision still has model consumers"));
    auto erased = candidate.dependency_graph_.remove_node(node);
    if (!erased)
      return erased;
    candidate.invalidation_state_.untrack_node(node);
  }
  *this = std::move(candidate);
  return removed;
}
void PartDocument::prune_unused_engineering_resource_nodes() {
  for (const auto& resource : engineering_resources_.entries()) {
    if (!std::holds_alternative<EngineeringStandardTable>(resource))
      continue;
    const auto node = engineering_resource_node_id(engineering_resource_key(resource));
    if (dependency_graph_.has_node(node) && !has_authored_node_identity(node) &&
        !is_imported_authority_node(node) && dependency_graph_.direct_dependents(node).empty()) {
      (void)dependency_graph_.remove_node(node);
      invalidation_state_.untrack_node(node);
    }
  }
}
Result<std::size_t> PartDocument::set_default_material(std::optional<EngineeringResourceKey> key) {
  if (is_reference_part())
    return Result<std::size_t>::failure(reference_part_edit_error());
  if (key && !engineering_resources_.find_material(*key))
    return Result<std::size_t>::failure(Error::validation(
        id_.value(), "Default material requires an existing Material resource revision"));
  if (key == default_material_)
    return Result<std::size_t>::success(0);
  default_material_ = std::move(key);
  return Result<std::size_t>::success(1);
}
Result<std::size_t> PartDocument::set_body_material(BodyId body,
                                                    std::optional<EngineeringResourceKey> key) {
  if (is_reference_part())
    return Result<std::size_t>::failure(reference_part_edit_error());
  if (!find_body(body))
    return Result<std::size_t>::failure(
        Error::validation(body.value(), "Material override requires an existing Body"));
  if (key && !engineering_resources_.find_material(*key))
    return Result<std::size_t>::failure(Error::validation(
        body.value(), "Body material requires an existing Material resource revision"));
  const auto found =
      std::lower_bound(body_materials_.begin(), body_materials_.end(), body.value(),
                       [](const auto& entry, const auto& id) { return entry.body.value() < id; });
  if (found != body_materials_.end() && found->body == body) {
    if (key && *key == found->material)
      return Result<std::size_t>::success(0);
    if (key)
      found->material = std::move(*key);
    else
      body_materials_.erase(found);
  } else if (key)
    body_materials_.insert(found, {std::move(body), std::move(*key)});
  else
    return Result<std::size_t>::success(0);
  return Result<std::size_t>::success(1);
}
const EngineeringMaterial* PartDocument::effective_material_for_body(BodyId body) const noexcept {
  if (!find_body(body))
    return nullptr;
  const auto found = std::find_if(body_materials_.begin(), body_materials_.end(),
                                  [&](const auto& entry) { return entry.body == body; });
  const auto* key = found == body_materials_.end()
                        ? (default_material_ ? &*default_material_ : nullptr)
                        : &found->material;
  return key ? engineering_resources_.find_material(*key) : nullptr;
}
} // namespace blcad
