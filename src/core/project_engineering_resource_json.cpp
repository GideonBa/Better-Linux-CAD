#include "project_engineering_resource_json.hpp"
#include "blcad/core/part_document_json.hpp"
#include "engineering_resource_json.hpp"

#include <algorithm>
#include <exception>

namespace blcad::detail {
namespace {
Error error(std::string message) {
  return Error::validation("project.resources", std::move(message));
}
} // namespace
Result<nlohmann::json> resource_part_to_project_json(const PartDocument& part,
                                                     const EngineeringResourceCatalog& catalog) {
  try {
    const auto serialized = serialize_part_document_to_json(part);
    if (!serialized)
      return Result<nlohmann::json>::failure(serialized.error());
    auto value = nlohmann::json::parse(serialized.value());
    if (value.at("version") == 7 || value.at("version") == 8 || value.at("version") == 9 ||
        value.at("version") == 10 || !part.engineering_resources().entries().empty() ||
        !part.hole_features().empty() || !part.thread_features().empty()) {
      auto refs = nlohmann::json::array();
      for (const auto& entry : part.engineering_resources().entries()) {
        const auto& key = engineering_resource_key(entry);
        const auto* owned = catalog.find(key);
        if (!owned || *owned != entry)
          return Result<nlohmann::json>::failure(error("Part resource differs from Project owner"));
        refs.push_back(engineering_resource_key_to_json(key));
      }
      value.erase("engineering_resources");
      value["engineering_resource_refs"] = std::move(refs);
    }
    return Result<nlohmann::json>::success(std::move(value));
  } catch (const std::exception& e) {
    return Result<nlohmann::json>::failure(error(e.what()));
  }
}
Result<PartDocument> resource_part_from_project_json(const nlohmann::json& value,
                                                     const EngineeringResourceCatalog& catalog) {
  try {
    if (!value.at("version").is_number_integer() ||
        (value.at("version") != 3 && value.at("version") != 4 && value.at("version") != 5 &&
         value.at("version") != 6 && value.at("version") != 7 && value.at("version") != 8 &&
         value.at("version") != 9 && value.at("version") != 10))
      return deserialize_part_document_from_json(value.dump());
    if (value.contains("engineering_resources") || !value.contains("engineering_resource_refs") ||
        !value.at("engineering_resource_refs").is_array())
      return Result<PartDocument>::failure(
          error("Project resource-aware Part requires resource references only"));
    EngineeringResourceCatalog local;
    std::vector<EngineeringResourceKey> seen;
    for (const auto& ref : value.at("engineering_resource_refs")) {
      auto key = engineering_resource_key_from_json(ref);
      if (!key)
        return Result<PartDocument>::failure(key.error());
      if (std::find(seen.begin(), seen.end(), key.value()) != seen.end())
        return Result<PartDocument>::failure(error("Duplicate Project Part resource reference"));
      const auto* owned = catalog.find(key.value());
      if (!owned)
        return Result<PartDocument>::failure(
            error("Project Part resource reference is unresolved"));
      const auto added = local.insert(*owned);
      if (!added)
        return Result<PartDocument>::failure(added.error());
      seen.push_back(std::move(key.value()));
    }
    auto standalone = value;
    standalone.erase("engineering_resource_refs");
    standalone["engineering_resources"] = engineering_resources_to_json(local);
    return deserialize_part_document_from_json(standalone.dump());
  } catch (const std::exception& e) {
    return Result<PartDocument>::failure(error(e.what()));
  }
}
} // namespace blcad::detail
