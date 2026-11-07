#pragma once

#include "blcad/core/engineering_resource.hpp"

#include <string_view>

namespace blcad {
[[nodiscard]] Result<std::string>
serialize_engineering_resource_catalog_to_json(const EngineeringResourceCatalog& catalog);
[[nodiscard]] Result<EngineeringResourceCatalog>
deserialize_engineering_resource_catalog_from_json(std::string_view content);
} // namespace blcad
