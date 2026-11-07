#pragma once

#include "blcad/core/engineering_resource.hpp"
#include <initializer_list>
#include <nlohmann/json.hpp>

namespace blcad::detail {
void require_engineering_json_fields(const nlohmann::json& value,
                                     std::initializer_list<std::string_view> required,
                                     std::initializer_list<std::string_view> optional = {});
[[nodiscard]] nlohmann::json engineering_resource_key_to_json(const EngineeringResourceKey& key);
[[nodiscard]] Result<EngineeringResourceKey>
engineering_resource_key_from_json(const nlohmann::json& value);
[[nodiscard]] nlohmann::json
engineering_resources_to_json(const EngineeringResourceCatalog& catalog);
[[nodiscard]] Result<EngineeringResourceCatalog>
engineering_resources_from_json(const nlohmann::json& value);
} // namespace blcad::detail
