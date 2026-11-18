#pragma once
#include "blcad/core/project_json.hpp"
#include <nlohmann/json.hpp>
namespace blcad::geometry::detail {
inline Result<std::string> effective_project_source(const Project& p) {
  auto source = serialize_project_to_json(p);
  if (!source || (p.assembly().effective_model_state() == primary_model_state_id() &&
                  p.effective_position() == master_position_id() &&
                  p.effective_design_view() == default_design_view_id()))
    return source;
  auto value = nlohmann::json::parse(source.value());
  value["_effective_root_model_state"] = p.assembly().effective_model_state().value();
  value["_effective_position"] = p.effective_position().value();
  value["_effective_design_view"] = p.effective_design_view().value();
  return Result<std::string>::success(value.dump());
}
} // namespace blcad::geometry::detail
