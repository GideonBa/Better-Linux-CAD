#pragma once

#include "blcad/core/project.hpp"
#include <nlohmann/json.hpp>
#include <set>

namespace blcad::detail {
inline nlohmann::json occurrence_poses_to_json(const Project& project) {
  using J = nlohmann::json;
  J poses = J::array();
  for (const auto& pose : project.occurrence_poses()) {
    J path = J::array(), components = J::array(), joints = J::array();
    for (const auto& id : pose.path())
      path.push_back(id.value());
    for (const auto& component : pose.components()) {
      const auto& t = component.transform;
      components.push_back(
          {{"component", component.component.value()},
           {"translation_mm", {t.translation_mm.x, t.translation_mm.y, t.translation_mm.z}},
           {"rotation_deg", {t.rotation_deg.x, t.rotation_deg.y, t.rotation_deg.z}}});
    }
    for (const auto& coordinate : pose.joints()) {
      const bool angular = coordinate.value.kind() == QuantityKind::AngleDeg;
      joints.push_back(
          {{"joint", coordinate.joint.value()},
           {"role", to_string(coordinate.role)},
           {"unit", angular ? "deg" : "mm"},
           {"value", angular ? coordinate.value.degrees() : coordinate.value.millimeters()}});
    }
    poses.push_back(
        {{"version", 1},
         {"path", std::move(path)},
         {"assembly_document", pose.assembly_document().value()},
         {"rigidity",
          pose.rigidity() == AssemblyOccurrenceRigidity::Flexible ? "flexible" : "rigid"},
         {"components", std::move(components)},
         {"joints", std::move(joints)}});
  }
  return poses;
}
inline Result<std::size_t> load_occurrence_poses(const nlohmann::json& values, Project& project) {
  using J = nlohmann::json;
  auto fail = [] {
    return Result<std::size_t>::failure(Error::validation(
        "project.occurrence_poses", "Invalid versioned occurrence pose fields or duplicate path"));
  };
  auto exact = [](const J& value, const std::set<std::string>& keys) {
    if (!value.is_object() || value.size() != keys.size())
      return false;
    for (const auto& key : keys)
      if (!value.contains(key))
        return false;
    return true;
  };
  auto triple = [](const J& value) {
    return value.is_array() && value.size() == 3 &&
           std::all_of(value.begin(), value.end(), [](const J& n) { return n.is_number(); });
  };
  if (!values.is_array() || values.empty())
    return fail();
  for (const auto& value : values) {
    if (!exact(value,
               {"version", "path", "assembly_document", "rigidity", "components", "joints"}) ||
        !value.at("version").is_number_integer() || value.at("version") != 1 ||
        !value.at("path").is_array() || !value.at("components").is_array() ||
        !value.at("joints").is_array() ||
        (value.at("rigidity") != "flexible" && value.at("rigidity") != "rigid"))
      return fail();
    std::vector<SubassemblyInstanceId> path;
    for (const auto& id : value.at("path"))
      path.emplace_back(id.get<std::string>());
    if (project.find_occurrence_pose(path))
      return fail();
    std::vector<AssemblyComponentPoseOverride> components;
    for (const auto& item : value.at("components")) {
      if (!exact(item, {"component", "translation_mm", "rotation_deg"}) ||
          !triple(item.at("translation_mm")) || !triple(item.at("rotation_deg")))
        return fail();
      const auto& t = item.at("translation_mm");
      const auto& r = item.at("rotation_deg");
      components.push_back({ComponentInstanceId(item.at("component").get<std::string>()),
                            {{t[0].get<double>(), t[1].get<double>(), t[2].get<double>()},
                             {r[0].get<double>(), r[1].get<double>(), r[2].get<double>()}}});
    }
    std::vector<AssemblyOccurrenceJointCoordinate> joints;
    for (const auto& item : value.at("joints")) {
      if (!exact(item, {"joint", "role", "unit", "value"}) || !item.at("value").is_number())
        return fail();
      std::optional<AssemblyJointCoordinateRole> role;
      for (const auto candidate :
           {AssemblyJointCoordinateRole::Rotation, AssemblyJointCoordinateRole::Translation,
            AssemblyJointCoordinateRole::TranslationU, AssemblyJointCoordinateRole::TranslationV,
            AssemblyJointCoordinateRole::RotationNormal})
        if (item.at("role") == to_string(candidate))
          role = candidate;
      if (!role || (item.at("unit") != "deg" && item.at("unit") != "mm"))
        return fail();
      auto quantity = item.at("unit") == "deg"
                          ? Quantity::angle_deg(item.at("value").get<double>())
                          : Quantity::linear_displacement_mm(item.at("value").get<double>());
      if (!quantity)
        return Result<std::size_t>::failure(quantity.error());
      joints.push_back(
          {AssemblyJointId(item.at("joint").get<std::string>()), *role, quantity.value()});
    }
    auto pose = AssemblyOccurrencePose::create(
        std::move(path), DocumentId(value.at("assembly_document").get<std::string>()),
        value.at("rigidity") == "flexible" ? AssemblyOccurrenceRigidity::Flexible
                                           : AssemblyOccurrenceRigidity::Rigid,
        std::move(components), std::move(joints));
    if (!pose)
      return Result<std::size_t>::failure(pose.error());
    auto added = project.set_occurrence_pose(std::move(pose.value()));
    if (!added)
      return added;
  }
  return Result<std::size_t>::success(values.size());
}
} // namespace blcad::detail
