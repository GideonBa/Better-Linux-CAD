#pragma once
#include "blcad/core/hole_feature.hpp"
#include <nlohmann/json.hpp>
namespace blcad::detail {
[[nodiscard]] nlohmann::json shared_hole_to_json(const HoleFeature&);
[[nodiscard]] Result<HoleFeature> shared_hole_from_json(const nlohmann::json&);
} // namespace blcad::detail
