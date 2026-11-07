#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <utility>

namespace blcad {
Result<std::size_t> PartDocument::add_engineering_resource(EngineeringResource resource) {
  if (is_reference_part())
    return Result<std::size_t>::failure(reference_part_edit_error());
  return engineering_resources_.insert(std::move(resource));
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
