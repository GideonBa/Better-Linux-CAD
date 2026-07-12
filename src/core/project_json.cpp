#include "blcad/core/project_json.hpp"

#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/part_document_json.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace blcad {
namespace {

using json = nlohmann::json;
constexpr std::string_view k_schema = "blcad.project.mvp4";
constexpr int k_version = 1;

[[nodiscard]] Error json_error(std::string message) {
  return Error::validation("project_json", std::move(message));
}

[[nodiscard]] Error json_file_error(const std::filesystem::path& path, std::string message) {
  const auto object_id = path.empty() ? std::string("project_json_file") : path.string();
  return Error::validation(object_id, std::move(message));
}

[[nodiscard]] Result<json> parse_json_document(std::string_view content) {
  try {
    return Result<json>::success(json::parse(content));
  } catch (const std::exception& parse_error) {
    return Result<json>::failure(
        json_error(std::string("invalid project json: ") + parse_error.what()));
  }
}

[[nodiscard]] Result<json> assembly_to_json(const AssemblyDocument& assembly) {
  auto serialized = serialize_assembly_document_to_json(assembly);
  if (serialized.has_error()) return Result<json>::failure(serialized.error());
  auto parsed = parse_json_document(serialized.value());
  if (parsed.has_error()) return Result<json>::failure(parsed.error());
  return parsed;
}

[[nodiscard]] Result<json> part_to_json(const PartDocument& part) {
  auto serialized = serialize_part_document_to_json(part);
  if (serialized.has_error()) return Result<json>::failure(serialized.error());
  auto parsed = parse_json_document(serialized.value());
  if (parsed.has_error()) return Result<json>::failure(parsed.error());
  return parsed;
}

[[nodiscard]] Result<AssemblyDocument> assembly_from_json(const json& assembly_json) {
  return deserialize_assembly_document_from_json(assembly_json.dump());
}

[[nodiscard]] Result<PartDocument> part_from_json(const json& part_json) {
  return deserialize_part_document_from_json(part_json.dump());
}

[[nodiscard]] Result<AssemblyConstraintType> constraint_type_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "mate")
    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Mate);
  if (text == "concentric")
    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Concentric);
  if (text == "distance")
    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Distance);
  if (text == "insert")
    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Insert);
  if (text == "angle")
    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Angle);
  return Result<AssemblyConstraintType>::failure(
      json_error("unsupported cross-hierarchy assembly constraint type"));
}

[[nodiscard]] Result<AssemblyConstraintState> constraint_state_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "active")
    return Result<AssemblyConstraintState>::success(AssemblyConstraintState::Active);
  if (text == "inactive")
    return Result<AssemblyConstraintState>::success(AssemblyConstraintState::Inactive);
  return Result<AssemblyConstraintState>::failure(
      json_error("unsupported cross-hierarchy assembly constraint state"));
}

[[nodiscard]] Result<AssemblyJointType> joint_type_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "revolute")
    return Result<AssemblyJointType>::success(AssemblyJointType::Revolute);
  return Result<AssemblyJointType>::failure(
      json_error("unsupported cross-hierarchy assembly joint type"));
}

[[nodiscard]] Result<AssemblyJointState> joint_state_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "active")
    return Result<AssemblyJointState>::success(AssemblyJointState::Active);
  if (text == "inactive")
    return Result<AssemblyJointState>::success(AssemblyJointState::Inactive);
  return Result<AssemblyJointState>::failure(
      json_error("unsupported cross-hierarchy assembly joint state"));
}

[[nodiscard]] json angle_quantity_to_json(double degrees) {
  return json{{"unit", "deg"}, {"value", degrees}};
}

[[nodiscard]] Result<Quantity> angle_quantity_from_json(const json& value,
                                                        const std::string& object_id,
                                                        std::string_view context) {
  if (value.at("unit").get<std::string>() != "deg") {
    return Result<Quantity>::failure(json_error(std::string(context) + " must use degrees"));
  }
  return Quantity::angle_deg(value.at("value").get<double>(), object_id);
}

[[nodiscard]] json endpoint_to_json(const AssemblyHierarchyConstraintEndpoint& endpoint) {
  json occurrence_path = json::array();
  for (const SubassemblyInstanceId& occurrence : endpoint.occurrence_path()) {
    occurrence_path.push_back(occurrence.value());
  }
  return json{{"occurrence_path", std::move(occurrence_path)},
              {"component_instance", endpoint.component_instance().value()},
              {"semantic_reference", endpoint.semantic_reference()}};
}

[[nodiscard]] Result<AssemblyHierarchyConstraintEndpoint>
endpoint_from_json(const json& endpoint_json) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const auto& occurrence_json : endpoint_json.at("occurrence_path")) {
    occurrence_path.emplace_back(occurrence_json.get<std::string>());
  }
  return AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path),
      ComponentInstanceId(endpoint_json.at("component_instance").get<std::string>()),
      endpoint_json.at("semantic_reference").get<std::string>());
}

[[nodiscard]] json cross_hierarchy_constraint_to_json(
    const AssemblyHierarchyConstraint& constraint) {
  json constraint_json{{"id", constraint.id().value()},
                       {"name", constraint.name()},
                       {"type", std::string(to_string(constraint.type()))},
                       {"target_a", endpoint_to_json(constraint.target_a())},
                       {"target_b", endpoint_to_json(constraint.target_b())},
                       {"state", std::string(to_string(constraint.state()))}};
  if (constraint.distance().has_value()) {
    constraint_json["distance"] =
        json{{"unit", std::string(constraint.distance()->unit())},
             {"value", constraint.distance()->millimeters()}};
  }
  if (constraint.angle().has_value()) {
    constraint_json["angle"] =
        json{{"unit", std::string(constraint.angle()->unit())},
             {"value", constraint.angle()->degrees()}};
  }
  return constraint_json;
}

[[nodiscard]] Result<AssemblyHierarchyConstraint>
cross_hierarchy_constraint_from_json(const json& constraint_json) {
  auto type = constraint_type_from_json(constraint_json.at("type"));
  if (type.has_error()) {
    return Result<AssemblyHierarchyConstraint>::failure(type.error());
  }
  auto state = constraint_state_from_json(constraint_json.at("state"));
  if (state.has_error()) {
    return Result<AssemblyHierarchyConstraint>::failure(state.error());
  }
  auto target_a = endpoint_from_json(constraint_json.at("target_a"));
  if (target_a.has_error()) {
    return Result<AssemblyHierarchyConstraint>::failure(target_a.error());
  }
  auto target_b = endpoint_from_json(constraint_json.at("target_b"));
  if (target_b.has_error()) {
    return Result<AssemblyHierarchyConstraint>::failure(target_b.error());
  }

  const std::string id = constraint_json.at("id").get<std::string>();
  std::optional<Quantity> distance;
  if (constraint_json.contains("distance")) {
    const json& distance_json = constraint_json.at("distance");
    if (distance_json.at("unit").get<std::string>() != "mm") {
      return Result<AssemblyHierarchyConstraint>::failure(
          json_error("cross-hierarchy assembly constraint distance must use millimeters"));
    }
    auto quantity = Quantity::length_mm(distance_json.at("value").get<double>(), id);
    if (quantity.has_error()) {
      return Result<AssemblyHierarchyConstraint>::failure(quantity.error());
    }
    distance = quantity.value();
  }

  std::optional<Quantity> angle;
  if (constraint_json.contains("angle")) {
    const json& angle_json = constraint_json.at("angle");
    if (angle_json.at("unit").get<std::string>() != "deg") {
      return Result<AssemblyHierarchyConstraint>::failure(
          json_error("cross-hierarchy assembly constraint angle must use degrees"));
    }
    auto quantity = Quantity::angle_deg(angle_json.at("value").get<double>(), id);
    if (quantity.has_error()) {
      return Result<AssemblyHierarchyConstraint>::failure(quantity.error());
    }
    angle = quantity.value();
  }

  return AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), constraint_json.at("name").get<std::string>(), type.value(),
      std::move(target_a.value()), std::move(target_b.value()), state.value(),
      std::move(distance), std::move(angle));
}

[[nodiscard]] json cross_hierarchy_joint_to_json(const AssemblyHierarchyJoint& joint) {
  return json{{"id", joint.id().value()},
              {"name", joint.name()},
              {"type", std::string(to_string(joint.type()))},
              {"target_a", endpoint_to_json(joint.target_a())},
              {"target_b", endpoint_to_json(joint.target_b())},
              {"state", std::string(to_string(joint.state()))},
              {"limits",
               json{{"lower", angle_quantity_to_json(joint.limits().lower_deg)},
                    {"upper", angle_quantity_to_json(joint.limits().upper_deg)}}},
              {"coordinate", angle_quantity_to_json(joint.coordinate_deg())}};
}

[[nodiscard]] Result<AssemblyHierarchyJoint>
cross_hierarchy_joint_from_json(const json& joint_json) {
  auto type = joint_type_from_json(joint_json.at("type"));
  if (type.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(type.error());
  }
  auto state = joint_state_from_json(joint_json.at("state"));
  if (state.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(state.error());
  }
  auto target_a = endpoint_from_json(joint_json.at("target_a"));
  if (target_a.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(target_a.error());
  }
  auto target_b = endpoint_from_json(joint_json.at("target_b"));
  if (target_b.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(target_b.error());
  }

  const std::string id = joint_json.at("id").get<std::string>();
  const json& limits = joint_json.at("limits");
  auto lower = angle_quantity_from_json(limits.at("lower"), id,
                                        "cross-hierarchy assembly joint lower limit");
  if (lower.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(lower.error());
  }
  auto upper = angle_quantity_from_json(limits.at("upper"), id,
                                        "cross-hierarchy assembly joint upper limit");
  if (upper.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(upper.error());
  }
  auto coordinate = angle_quantity_from_json(joint_json.at("coordinate"), id,
                                             "cross-hierarchy assembly joint coordinate");
  if (coordinate.has_error()) {
    return Result<AssemblyHierarchyJoint>::failure(coordinate.error());
  }

  return AssemblyHierarchyJoint::create(
      AssemblyJointId(id), joint_json.at("name").get<std::string>(), type.value(),
      std::move(target_a.value()), std::move(target_b.value()), state.value(), lower.value(),
      upper.value(), coordinate.value());
}

} // namespace

Result<std::string> serialize_project_to_json(const Project& project) {
  auto root_assembly_json = assembly_to_json(project.assembly());
  if (root_assembly_json.has_error())
    return Result<std::string>::failure(root_assembly_json.error());

  json assemblies = json::array();
  for (const AssemblyDocument& assembly : project.child_assembly_documents()) {
    auto assembly_json = assembly_to_json(assembly);
    if (assembly_json.has_error()) return Result<std::string>::failure(assembly_json.error());
    assemblies.push_back(std::move(assembly_json.value()));
  }

  json cross_hierarchy_constraints = json::array();
  for (const AssemblyHierarchyConstraint& constraint : project.cross_hierarchy_constraints()) {
    cross_hierarchy_constraints.push_back(cross_hierarchy_constraint_to_json(constraint));
  }

  json cross_hierarchy_joints = json::array();
  for (const AssemblyHierarchyJoint& joint : project.cross_hierarchy_joints()) {
    cross_hierarchy_joints.push_back(cross_hierarchy_joint_to_json(joint));
  }

  json parts = json::array();
  for (const PartDocument& part : project.part_documents()) {
    auto part_json = part_to_json(part);
    if (part_json.has_error()) return Result<std::string>::failure(part_json.error());
    parts.push_back(std::move(part_json.value()));
  }

  json root{{"schema", k_schema},
            {"version", k_version},
            {"project", json{{"id", project.id().value()}, {"name", project.name()}}},
            {"assembly", std::move(root_assembly_json.value())},
            {"assemblies", std::move(assemblies)},
            {"cross_hierarchy_constraints", std::move(cross_hierarchy_constraints)},
            {"cross_hierarchy_joints", std::move(cross_hierarchy_joints)},
            {"parts", std::move(parts)}};
  return Result<std::string>::success(root.dump(2));
}

Result<Project> deserialize_project_from_json(std::string_view content) {
  auto parsed = parse_json_document(content);
  if (parsed.has_error()) return Result<Project>::failure(parsed.error());

  try {
    const json& root = parsed.value();
    if (root.at("schema").get<std::string>() != k_schema) {
      return Result<Project>::failure(json_error("unsupported project schema"));
    }
    if (root.at("version").get<int>() != k_version) {
      return Result<Project>::failure(json_error("unsupported project version"));
    }

    auto assembly = assembly_from_json(root.at("assembly"));
    if (assembly.has_error()) return Result<Project>::failure(assembly.error());

    auto project = Project::create(DocumentId(root.at("project").at("id").get<std::string>()),
                                   root.at("project").at("name").get<std::string>(),
                                   std::move(assembly.value()));
    if (project.has_error()) return project;

    for (const auto& assembly_json : root.value("assemblies", json::array())) {
      auto child_assembly = assembly_from_json(assembly_json);
      if (child_assembly.has_error())
        return Result<Project>::failure(child_assembly.error());
      auto added =
          project.value().add_child_assembly_document(std::move(child_assembly.value()));
      if (added.has_error()) return Result<Project>::failure(added.error());
    }

    for (const auto& part_json : root.value("parts", json::array())) {
      auto part = part_from_json(part_json);
      if (part.has_error()) return Result<Project>::failure(part.error());
      auto added = project.value().add_part_document(std::move(part.value()));
      if (added.has_error()) return Result<Project>::failure(added.error());
    }

    for (const auto& constraint_json :
         root.value("cross_hierarchy_constraints", json::array())) {
      auto constraint = cross_hierarchy_constraint_from_json(constraint_json);
      if (constraint.has_error()) {
        return Result<Project>::failure(constraint.error());
      }
      auto added =
          project.value().add_cross_hierarchy_constraint(std::move(constraint.value()));
      if (added.has_error()) return Result<Project>::failure(added.error());
    }

    for (const auto& joint_json : root.value("cross_hierarchy_joints", json::array())) {
      auto joint = cross_hierarchy_joint_from_json(joint_json);
      if (joint.has_error()) {
        return Result<Project>::failure(joint.error());
      }
      auto added = project.value().add_cross_hierarchy_joint(std::move(joint.value()));
      if (added.has_error()) return Result<Project>::failure(added.error());
    }

    auto valid_structure = project.value().validate_assembly_structure();
    if (valid_structure.has_error()) return Result<Project>::failure(valid_structure.error());

    return project;
  } catch (const std::exception& content_error) {
    return Result<Project>::failure(
        json_error(std::string("invalid project json content: ") + content_error.what()));
  }
}

Result<std::uintmax_t> write_project_json_file(const Project& project,
                                               const std::filesystem::path& path) {
  auto serialized = serialize_project_to_json(project);
  if (serialized.has_error()) return Result<std::uintmax_t>::failure(serialized.error());

  std::ofstream output(path);
  if (!output) {
    return Result<std::uintmax_t>::failure(json_file_error(path, "could not open project file"));
  }
  output << serialized.value();
  if (!output) {
    return Result<std::uintmax_t>::failure(json_file_error(path, "could not write project file"));
  }
  return Result<std::uintmax_t>::success(static_cast<std::uintmax_t>(serialized.value().size()));
}

Result<Project> read_project_json_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return Result<Project>::failure(json_file_error(path, "could not open project file"));
  }
  std::stringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof()) {
    return Result<Project>::failure(json_file_error(path, "could not read project file"));
  }
  return deserialize_project_from_json(buffer.str());
}

} // namespace blcad
