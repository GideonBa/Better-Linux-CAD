#pragma once

#include "blcad/core/thread_feature.hpp"
#include <nlohmann/json_fwd.hpp>

namespace blcad::detail {
[[nodiscard]] nlohmann::json thread_definition_to_json(const ThreadDefinition& definition);
[[nodiscard]] Result<ThreadDefinition> thread_definition_from_json(const nlohmann::json& value);
} // namespace blcad::detail
