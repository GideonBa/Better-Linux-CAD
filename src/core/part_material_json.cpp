#include "part_material_json.hpp"
#include "engineering_resource_json.hpp"

#include <stdexcept>
#include <unordered_set>

namespace blcad::detail {
using json = nlohmann::json;
json part_material_assignments_to_json(const PartDocument& part) {
  json overrides = json::array();
  for (const auto& entry : part.body_materials())
    overrides.push_back({{"body", entry.body.value()},
                         {"material", engineering_resource_key_to_json(entry.material)}});
  json result{{"body_overrides", std::move(overrides)}};
  if (part.default_material())
    result["default"] = engineering_resource_key_to_json(*part.default_material());
  return result;
}
Result<std::size_t> load_part_engineering_resources(const json& root, PartDocument& part) {
  const auto catalog = engineering_resources_from_json(root.at("engineering_resources"));
  if (!catalog)
    return Result<std::size_t>::failure(catalog.error());
  for (const auto& entry : catalog.value().entries()) {
    const auto added = part.add_engineering_resource(entry);
    if (!added)
      return added;
  }
  return Result<std::size_t>::success(catalog.value().entries().size());
}
Result<std::size_t> load_part_material_assignments(const json& root, PartDocument& part) {
  try {
    const auto& values = root.at("material_assignments");
    require_engineering_json_fields(values, {"body_overrides"}, {"default"});
    if (values.contains("default")) {
      auto key = engineering_resource_key_from_json(values.at("default"));
      if (!key)
        return Result<std::size_t>::failure(key.error());
      const auto set = part.set_default_material(std::move(key.value()));
      if (!set)
        return set;
    }
    if (!values.at("body_overrides").is_array())
      throw std::runtime_error("Body material overrides must be an array");
    std::unordered_set<std::string> seen;
    for (const auto& entry : values.at("body_overrides")) {
      require_engineering_json_fields(entry, {"body", "material"});
      if (!entry.at("body").is_string())
        throw std::runtime_error("Body material identity must be a string");
      const auto body = entry.at("body").get<std::string>();
      if (!seen.insert(body).second)
        throw std::runtime_error("Duplicate Body material override");
      auto key = engineering_resource_key_from_json(entry.at("material"));
      if (!key)
        return Result<std::size_t>::failure(key.error());
      const auto set = part.set_body_material(BodyId(body), std::move(key.value()));
      if (!set)
        return set;
    }
    return Result<std::size_t>::success(seen.size());
  } catch (const std::exception& e) {
    return Result<std::size_t>::failure(Error::validation("part.materials", e.what()));
  }
}
} // namespace blcad::detail
