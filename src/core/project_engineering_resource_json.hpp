#pragma once

#include "blcad/core/part_document.hpp"
#include <nlohmann/json.hpp>

namespace blcad::detail {
[[nodiscard]] Result<nlohmann::json>
resource_part_to_project_json(const PartDocument& part, const EngineeringResourceCatalog& catalog);
[[nodiscard]] Result<PartDocument>
resource_part_from_project_json(const nlohmann::json& value,
                                const EngineeringResourceCatalog& catalog);
} // namespace blcad::detail
