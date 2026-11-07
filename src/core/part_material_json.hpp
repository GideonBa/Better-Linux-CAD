#pragma once

#include "blcad/core/part_document.hpp"
#include <nlohmann/json.hpp>

namespace blcad::detail {
[[nodiscard]] nlohmann::json part_material_assignments_to_json(const PartDocument& part);
[[nodiscard]] Result<std::size_t> load_part_engineering_resources(const nlohmann::json& root,
                                                                  PartDocument& part);
[[nodiscard]] Result<std::size_t> load_part_material_assignments(const nlohmann::json& root,
                                                                 PartDocument& part);
} // namespace blcad::detail
