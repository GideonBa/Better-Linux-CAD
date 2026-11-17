#pragma once
#include "blcad/core/advanced_fillet_intent.hpp"
#include <functional>
#include <nlohmann/json.hpp>
namespace blcad::detail {
using FilletEdgeWriter = std::function<nlohmann::json(const EdgeReference&)>;
using FilletEdgeReader = std::function<Result<EdgeReference>(const nlohmann::json&)>;
nlohmann::json variable_fillet_to_json(const VariableFilletIntent&, const FilletEdgeWriter&);
Result<VariableFilletIntent> variable_fillet_from_json(const nlohmann::json&,
                                                       const FilletEdgeReader&);
} // namespace blcad::detail
