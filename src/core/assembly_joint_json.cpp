#include "assembly_joint_json.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace blcad::detail {
namespace {

using json = nlohmann::json;

[[nodiscard]] Error coordinate_json_error(std::string_view object_id, std::string message) {
  return Error::validation(std::string(object_id), std::move(message));
}

[[nodiscard]] Result<AssemblyJointCoordinateRole> role_from_json(const json& value,
                                                                 std::string_view object_id) {
  const auto text = value.get<std::string>();
  if (text == "rotation")
    return Result<AssemblyJointCoordinateRole>::success(AssemblyJointCoordinateRole::Rotation);
  if (text == "translation")
    return Result<AssemblyJointCoordinateRole>::success(AssemblyJointCoordinateRole::Translation);
  if (text == "translation_u")
    return Result<AssemblyJointCoordinateRole>::success(AssemblyJointCoordinateRole::TranslationU);
  if (text == "translation_v")
    return Result<AssemblyJointCoordinateRole>::success(AssemblyJointCoordinateRole::TranslationV);
  if (text == "rotation_normal") {
    return Result<AssemblyJointCoordinateRole>::success(
        AssemblyJointCoordinateRole::RotationNormal);
  }
  return Result<AssemblyJointCoordinateRole>::failure(
      coordinate_json_error(object_id, "unknown assembly joint coordinate role"));
}

[[nodiscard]] Result<AssemblyJointCoordinateKind> kind_from_json(const json& value,
                                                                 std::string_view object_id) {
  const auto text = value.get<std::string>();
  if (text == "angular")
    return Result<AssemblyJointCoordinateKind>::success(AssemblyJointCoordinateKind::Angular);
  if (text == "linear")
    return Result<AssemblyJointCoordinateKind>::success(AssemblyJointCoordinateKind::Linear);
  return Result<AssemblyJointCoordinateKind>::failure(
      coordinate_json_error(object_id, "unknown assembly joint coordinate kind"));
}

[[nodiscard]] json quantity_to_json(const Quantity& quantity) {
  return json{{"value", quantity.kind() == QuantityKind::AngleDeg ? quantity.degrees()
                                                                  : quantity.millimeters()},
              {"unit", std::string(quantity.unit())}};
}

[[nodiscard]] Result<Quantity> quantity_from_json(const json& value,
                                                  AssemblyJointCoordinateKind kind,
                                                  std::string_view object_id) {
  const auto unit = value.at("unit").get<std::string>();
  const double authored_value = value.at("value").get<double>();
  if (kind == AssemblyJointCoordinateKind::Angular) {
    if (unit != "deg") {
      return Result<Quantity>::failure(
          coordinate_json_error(object_id, "angular joint coordinates must use unit \"deg\""));
    }
    return Quantity::angle_deg(authored_value, object_id);
  }
  if (unit != "mm") {
    return Result<Quantity>::failure(
        coordinate_json_error(object_id, "linear joint coordinates must use unit \"mm\""));
  }
  return Quantity::linear_displacement_mm(authored_value, object_id);
}

} // namespace

json assembly_joint_coordinate_slots_to_json(
    const std::vector<AssemblyJointCoordinateSlot>& slots) {
  json result = json::array();
  for (const auto& slot : slots) {
    json slot_json{{"role", std::string(to_string(slot.role()))},
                   {"kind", std::string(to_string(slot.kind()))},
                   {"value", quantity_to_json(slot.value())}};
    if (slot.lower_limit())
      slot_json["lower_limit"] = quantity_to_json(*slot.lower_limit());
    if (slot.upper_limit())
      slot_json["upper_limit"] = quantity_to_json(*slot.upper_limit());
    result.push_back(std::move(slot_json));
  }
  return result;
}

Result<std::vector<AssemblyJointCoordinateSlot>>
assembly_joint_coordinate_slots_from_json(const json& slots_json, std::string_view object_id) {
  if (!slots_json.is_array()) {
    return Result<std::vector<AssemblyJointCoordinateSlot>>::failure(
        coordinate_json_error(object_id, "assembly joint coordinates must be an array"));
  }

  std::vector<AssemblyJointCoordinateSlot> slots;
  slots.reserve(slots_json.size());
  for (const auto& slot_json : slots_json) {
    auto role = role_from_json(slot_json.at("role"), object_id);
    if (role.has_error())
      return Result<std::vector<AssemblyJointCoordinateSlot>>::failure(role.error());
    if (std::any_of(slots.begin(), slots.end(),
                    [&](const auto& slot) { return slot.role() == role.value(); })) {
      return Result<std::vector<AssemblyJointCoordinateSlot>>::failure(
          coordinate_json_error(object_id, "duplicate assembly joint coordinate role"));
    }
    auto kind = kind_from_json(slot_json.at("kind"), object_id);
    if (kind.has_error())
      return Result<std::vector<AssemblyJointCoordinateSlot>>::failure(kind.error());
    auto value = quantity_from_json(slot_json.at("value"), kind.value(), object_id);
    if (value.has_error())
      return Result<std::vector<AssemblyJointCoordinateSlot>>::failure(value.error());

    std::optional<Quantity> lower_limit;
    if (slot_json.contains("lower_limit")) {
      auto parsed = quantity_from_json(slot_json.at("lower_limit"), kind.value(), object_id);
      if (parsed.has_error())
        return Result<std::vector<AssemblyJointCoordinateSlot>>::failure(parsed.error());
      lower_limit = parsed.value();
    }
    std::optional<Quantity> upper_limit;
    if (slot_json.contains("upper_limit")) {
      auto parsed = quantity_from_json(slot_json.at("upper_limit"), kind.value(), object_id);
      if (parsed.has_error())
        return Result<std::vector<AssemblyJointCoordinateSlot>>::failure(parsed.error());
      upper_limit = parsed.value();
    }

    auto slot = AssemblyJointCoordinateSlot::create(role.value(), kind.value(), value.value(),
                                                    std::move(lower_limit), std::move(upper_limit),
                                                    object_id);
    if (slot.has_error())
      return Result<std::vector<AssemblyJointCoordinateSlot>>::failure(slot.error());
    slots.push_back(slot.value());
  }
  return Result<std::vector<AssemblyJointCoordinateSlot>>::success(std::move(slots));
}

} // namespace blcad::detail
