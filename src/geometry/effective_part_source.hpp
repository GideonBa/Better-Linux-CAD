#pragma once

#include "blcad/core/part_document_json.hpp"
#include <nlohmann/json.hpp>

namespace blcad::geometry::detail {
// Derived freshness token only. Active state is session/evaluation context and
// must never become an authored Primary override in the save format.
inline Result<std::string> effective_part_source(const PartDocument& p) {
  auto source = serialize_part_document_to_json(p);
  if (!source || p.effective_model_state() == primary_model_state_id())
    return source;
  auto value = nlohmann::json::parse(source.value());
  value["_effective_model_state"] = p.effective_model_state().value();
  return Result<std::string>::success(value.dump());
}
} // namespace blcad::geometry::detail
