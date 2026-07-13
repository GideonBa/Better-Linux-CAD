#pragma once

#include "blcad/core/assembly_joint.hpp"

#include <nlohmann/json_fwd.hpp>

#include <string_view>
#include <vector>

namespace blcad::detail {

[[nodiscard]] nlohmann::json
assembly_joint_coordinate_slots_to_json(const std::vector<AssemblyJointCoordinateSlot>& slots);

[[nodiscard]] Result<std::vector<AssemblyJointCoordinateSlot>>
assembly_joint_coordinate_slots_from_json(const nlohmann::json& slots_json,
                                          std::string_view object_id);

} // namespace blcad::detail
