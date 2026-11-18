#pragma once
#include "assembly_occurrence_pose_json.hpp"
#include "project_design_reference_json.hpp"
#include <nlohmann/json.hpp>

namespace blcad::detail {
inline nlohmann::json representations_to_json(const Project& project) {
  using J = nlohmann::json;
  auto vector = [](Vector3 v) { return J::array({v.x, v.y, v.z}); };
  auto coordinate = [](const AssemblyPositionCoordinate& c) {
    const bool angle = c.value.kind() == QuantityKind::AngleDeg;
    return J{{"joint", c.joint.value()},
             {"role", to_string(c.role)},
             {"unit", angle ? "deg" : "mm"},
             {"value", angle ? c.value.degrees() : c.value.millimeters()}};
  };
  J positions = J::array(), views = J::array();
  for (const auto& position : project.assembly_positions()) {
    J components = J::array(), subassemblies = J::array(), joints = J::array(), cross = J::array();
    for (const auto& c : position.components)
      components.push_back({{"component", c.component.value()},
                            {"part", c.part.value()},
                            {"translation_mm", vector(c.transform.translation_mm)},
                            {"rotation_deg", vector(c.transform.rotation_deg)}});
    for (const auto& c : position.subassemblies)
      subassemblies.push_back({{"subassembly", c.subassembly.value()},
                               {"assembly_document", c.assembly_document.value()},
                               {"translation_mm", vector(c.transform.translation_mm)},
                               {"rotation_deg", vector(c.transform.rotation_deg)}});
    for (const auto& c : position.joints)
      joints.push_back(coordinate(c));
    for (const auto& c : position.cross_joints)
      cross.push_back(coordinate(c));
    auto pose_project = project;
    const auto existing = pose_project.occurrence_poses();
    for (const auto& pose : existing)
      (void)pose_project.remove_occurrence_pose(pose.path());
    for (const auto& pose : position.poses)
      (void)pose_project.set_occurrence_pose(pose);
    positions.push_back({{"version", 1},
                         {"id", position.id.value()},
                         {"name", position.name},
                         {"components", std::move(components)},
                         {"subassemblies", std::move(subassemblies)},
                         {"poses", occurrence_poses_to_json(pose_project)},
                         {"joints", std::move(joints)},
                         {"cross_joints", std::move(cross)}});
  }
  for (const auto& view : project.assembly_design_views()) {
    J components = J::array(), subassemblies = J::array();
    for (const auto& c : view.components)
      components.push_back(
          {{"occurrence", design_occurrence_json(c.occurrence)}, {"visible", c.visible}});
    for (const auto& c : view.subassemblies) {
      J path = J::array();
      for (const auto& id : c.path)
        path.push_back(id.value());
      subassemblies.push_back({{"path", std::move(path)},
                               {"assembly_document", c.assembly_document.value()},
                               {"visible", c.visible}});
    }
    views.push_back({{"version", 1},
                     {"id", view.id.value()},
                     {"name", view.name},
                     {"components", std::move(components)},
                     {"subassemblies", std::move(subassemblies)}});
  }
  return {{"positions", std::move(positions)}, {"design_views", std::move(views)}};
}
inline Result<std::size_t> load_representations(const nlohmann::json& root, Project& project) {
  using J = nlohmann::json;
  auto fail = [] {
    return Result<std::size_t>::failure(
        Error::validation("project.representations", "Invalid strict position/design-view record"));
  };
  auto exact = [](const J& value, const std::set<std::string>& keys) {
    if (!value.is_object() || value.size() != keys.size())
      return false;
    for (const auto& key : keys)
      if (!value.contains(key))
        return false;
    return true;
  };
  auto vector = [](const J& value) -> std::optional<Vector3> {
    if (!value.is_array() || value.size() != 3 ||
        !std::all_of(value.begin(), value.end(), [](const J& n) { return n.is_number(); }))
      return {};
    return Vector3{value[0].get<double>(), value[1].get<double>(), value[2].get<double>()};
  };
  auto coordinate = [&](const J& value) -> Result<AssemblyPositionCoordinate> {
    if (!exact(value, {"joint", "role", "unit", "value"}) || !value.at("value").is_number())
      return Result<AssemblyPositionCoordinate>::failure(fail().error());
    std::optional<AssemblyJointCoordinateRole> role;
    for (auto candidate :
         {AssemblyJointCoordinateRole::Rotation, AssemblyJointCoordinateRole::Translation,
          AssemblyJointCoordinateRole::TranslationU, AssemblyJointCoordinateRole::TranslationV,
          AssemblyJointCoordinateRole::RotationNormal})
      if (value.at("role") == to_string(candidate))
        role = candidate;
    if (!role || (value.at("unit") != "deg" && value.at("unit") != "mm"))
      return Result<AssemblyPositionCoordinate>::failure(fail().error());
    auto q = value.at("unit") == "deg"
                 ? Quantity::angle_deg(value.at("value").get<double>())
                 : Quantity::linear_displacement_mm(value.at("value").get<double>());
    if (!q)
      return Result<AssemblyPositionCoordinate>::failure(q.error());
    return Result<AssemblyPositionCoordinate>::success(
        {AssemblyJointId(value.at("joint").get<std::string>()), *role, q.value()});
  };
  if (!exact(root, {"positions", "design_views"}) || !root.at("positions").is_array() ||
      !root.at("design_views").is_array() ||
      (root.at("positions").empty() && root.at("design_views").empty()))
    return fail();
  std::vector<AssemblyPosition> positions;
  for (const auto& value : root.at("positions")) {
    if (!exact(value, {"version", "id", "name", "components", "subassemblies", "poses", "joints",
                       "cross_joints"}) ||
        !value.at("version").is_number_integer() || value.at("version") != 1)
      return fail();
    for (const char* key : {"components", "subassemblies", "poses", "joints", "cross_joints"})
      if (!value.at(key).is_array())
        return fail();
    AssemblyPosition position{AssemblyPositionId(value.at("id").get<std::string>()),
                              value.at("name").get<std::string>(),
                              {},
                              {},
                              {},
                              {},
                              {}};
    for (const auto& c : value.at("components")) {
      if (!exact(c, {"component", "part", "translation_mm", "rotation_deg"}))
        return fail();
      auto t = vector(c.at("translation_mm")), r = vector(c.at("rotation_deg"));
      if (!t || !r)
        return fail();
      position.components.push_back({ComponentInstanceId(c.at("component").get<std::string>()),
                                     DocumentId(c.at("part").get<std::string>()),
                                     {*t, *r}});
    }
    for (const auto& c : value.at("subassemblies")) {
      if (!exact(c, {"subassembly", "assembly_document", "translation_mm", "rotation_deg"}))
        return fail();
      auto t = vector(c.at("translation_mm")), r = vector(c.at("rotation_deg"));
      if (!t || !r)
        return fail();
      position.subassemblies.push_back(
          {SubassemblyInstanceId(c.at("subassembly").get<std::string>()),
           DocumentId(c.at("assembly_document").get<std::string>()),
           {*t, *r}});
    }
    if (!value.at("poses").empty()) {
      auto pose_project = project;
      const auto existing = pose_project.occurrence_poses();
      for (const auto& pose : existing)
        (void)pose_project.remove_occurrence_pose(pose.path());
      auto loaded = load_occurrence_poses(value.at("poses"), pose_project);
      if (!loaded)
        return loaded;
      position.poses = pose_project.occurrence_poses();
    }
    for (const auto& c : value.at("joints")) {
      auto q = coordinate(c);
      if (!q)
        return Result<std::size_t>::failure(q.error());
      position.joints.push_back(q.value());
    }
    for (const auto& c : value.at("cross_joints")) {
      auto q = coordinate(c);
      if (!q)
        return Result<std::size_t>::failure(q.error());
      position.cross_joints.push_back(q.value());
    }
    positions.push_back(std::move(position));
  }
  std::vector<AssemblyDesignView> views;
  for (const auto& value : root.at("design_views")) {
    if (!exact(value, {"version", "id", "name", "components", "subassemblies"}) ||
        !value.at("version").is_number_integer() || value.at("version") != 1 ||
        !value.at("components").is_array() || !value.at("subassemblies").is_array())
      return fail();
    AssemblyDesignView view{AssemblyDesignViewId(value.at("id").get<std::string>()),
                            value.at("name").get<std::string>(),
                            {},
                            {}};
    for (const auto& c : value.at("components")) {
      if (!exact(c, {"occurrence", "visible"}) || !c.at("visible").is_boolean() ||
          !exact(c.at("occurrence"), {"path", "component", "part"}) ||
          !c.at("occurrence").at("path").is_array())
        return fail();
      const auto& occurrence = c.at("occurrence");
      std::vector<SubassemblyInstanceId> path;
      for (const auto& id : occurrence.at("path"))
        path.emplace_back(id.get<std::string>());
      view.components.push_back(
          {{std::move(path), ComponentInstanceId(occurrence.at("component").get<std::string>()),
            DocumentId(occurrence.at("part").get<std::string>())},
           c.at("visible").get<bool>()});
    }
    for (const auto& c : value.at("subassemblies")) {
      if (!exact(c, {"path", "assembly_document", "visible"}) || !c.at("path").is_array() ||
          !c.at("visible").is_boolean())
        return fail();
      std::vector<SubassemblyInstanceId> path;
      for (const auto& id : c.at("path"))
        path.emplace_back(id.get<std::string>());
      view.subassemblies.push_back({std::move(path),
                                    DocumentId(c.at("assembly_document").get<std::string>()),
                                    c.at("visible").get<bool>()});
    }
    views.push_back(std::move(view));
  }
  auto stored = project.replace_assembly_positions(std::move(positions));
  if (!stored)
    return stored;
  return project.replace_assembly_design_views(std::move(views));
}
} // namespace blcad::detail
