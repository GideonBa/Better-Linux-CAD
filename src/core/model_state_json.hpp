#pragma once

#include "blcad/core/model_state.hpp"
#include <nlohmann/json.hpp>

namespace blcad::detail {
[[nodiscard]] nlohmann::json model_states_to_json(const std::vector<ModelState>&);
[[nodiscard]] Result<std::vector<ModelState>> model_states_from_json(const nlohmann::json&);
[[nodiscard]] Result<std::size_t> load_part_model_states(const nlohmann::json&, PartDocument&);
} // namespace blcad::detail
