#include "blcad/core/assembly_document_json.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace blcad {
namespace {

using json = nlohmann::json;
constexpr std::string_view k_schema = "blcad.assembly_document.mvp4";
constexpr int k_version = 1;

[[nodiscard]] Error json_error(std::string message) {
  return Error::validation("assembly_document_json", std::move(message));
}

[[nodiscard]] json vector3_to_json(Vector3 vector) {
  return json{{"x", vector.x}, {"y", vector.y}, {"z", vector.z}};
}

[[nodiscard]] Vector3 vector3_from_json(const json& value) {
  return Vector3{value.at("x").get<double>(), value.at("y").get<double>(),
                 value.at("z").get<double>()};
}

[[nodiscard]] json transform_to_json(const RigidTransform& transform) {
  return json{{"translation_mm", vector3_to_json(transform.translation_mm)},
              {"rotation_deg", vector3_to_json(transform.rotation_deg)}};
}

[[nodiscard]] RigidTransform transform_from_json(const json& value) {
  return RigidTransform{vector3_from_json(value.at("translation_mm")),
                        vector3_from_json(value.at("rotation_deg"))};
}

[[nodiscard]] Result<ComponentVisibility> visibility_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "visible")
    return Result<ComponentVisibility>::success(ComponentVisibility::Visible);
  if (text == "hidden")
    return Result<ComponentVisibility>::success(ComponentVisibility::Hidden);
  return Result<ComponentVisibility>::failure(json_error("unsupported component visibility"));
}

[[nodiscard]] Result<ComponentSuppressionState> suppression_state_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "active") {
    return Result<ComponentSuppressionState>::success(ComponentSuppressionState::Active);
  }
  if (text == "suppressed") {
    return Result<ComponentSuppressionState>::success(ComponentSuppressionState::Suppressed);
  }
  return Result<ComponentSuppressionState>::failure(
      json_error("unsupported component suppression state"));
}

[[nodiscard]] Result<ComponentGroundingState> grounding_state_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "free")
    return Result<ComponentGroundingState>::success(ComponentGroundingState::Free);
  if (text == "grounded") {
    return Result<ComponentGroundingState>::success(ComponentGroundingState::Grounded);
  }
  return Result<ComponentGroundingState>::failure(
      json_error("unsupported component grounding state"));
}

[[nodiscard]] json component_instance_to_json(const ComponentInstance& instance) {
  return json{{"id", instance.id().value()},
              {"name", instance.name()},
              {"referenced_part_document", instance.referenced_part_document().value()},
              {"visibility", std::string(to_string(instance.visibility()))},
              {"suppression_state", std::string(to_string(instance.suppression_state()))},
              {"grounding_state", std::string(to_string(instance.grounding_state()))},
              {"transform", transform_to_json(instance.transform())}};
}

[[nodiscard]] Result<ComponentInstance> component_instance_from_json(const json& instance_json) {
  auto visibility = visibility_from_json(instance_json.at("visibility"));
  if (visibility.has_error())
    return Result<ComponentInstance>::failure(visibility.error());

  auto suppression_state = suppression_state_from_json(instance_json.at("suppression_state"));
  if (suppression_state.has_error()) {
    return Result<ComponentInstance>::failure(suppression_state.error());
  }

  auto grounding_state = grounding_state_from_json(instance_json.at("grounding_state"));
  if (grounding_state.has_error())
    return Result<ComponentInstance>::failure(grounding_state.error());

  return ComponentInstance::create(
      ComponentInstanceId(instance_json.at("id").get<std::string>()),
      instance_json.at("name").get<std::string>(),
      DocumentId(instance_json.at("referenced_part_document").get<std::string>()),
      visibility.value(), suppression_state.value(), grounding_state.value(),
      transform_from_json(instance_json.at("transform")));
}

[[nodiscard]] json subassembly_instance_to_json(const SubassemblyInstance& instance) {
  return json{{"id", instance.id().value()},
              {"name", instance.name()},
              {"referenced_assembly_document", instance.referenced_assembly_document().value()},
              {"visibility", std::string(to_string(instance.visibility()))},
              {"suppression_state", std::string(to_string(instance.suppression_state()))},
              {"transform", transform_to_json(instance.transform())}};
}

[[nodiscard]] Result<SubassemblyInstance>
subassembly_instance_from_json(const json& instance_json) {
  auto visibility = visibility_from_json(instance_json.at("visibility"));
  if (visibility.has_error())
    return Result<SubassemblyInstance>::failure(visibility.error());

  auto suppression_state = suppression_state_from_json(instance_json.at("suppression_state"));
  if (suppression_state.has_error())
    return Result<SubassemblyInstance>::failure(suppression_state.error());

  return SubassemblyInstance::create(
      SubassemblyInstanceId(instance_json.at("id").get<std::string>()),
      instance_json.at("name").get<std::string>(),
      DocumentId(instance_json.at("referenced_assembly_document").get<std::string>()),
      visibility.value(), suppression_state.value(),
      transform_from_json(instance_json.at("transform")));
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
  return Result<AssemblyConstraintType>::failure(json_error("unsupported assembly constraint type"));
}

[[nodiscard]] Result<AssemblyConstraintState> constraint_state_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "active")
    return Result<AssemblyConstraintState>::success(AssemblyConstraintState::Active);
  if (text == "inactive")
    return Result<AssemblyConstraintState>::success(AssemblyConstraintState::Inactive);
  return Result<AssemblyConstraintState>::failure(json_error("unsupported assembly constraint state"));
}

[[nodiscard]] json constraint_target_to_json(const AssemblyConstraintTarget& target) {
  return json{{"component_instance", target.component_instance().value()},
              {"semantic_reference", target.semantic_reference()}};
}

[[nodiscard]] Result<AssemblyConstraintTarget>
constraint_target_from_json(const json& target_json) {
  return AssemblyConstraintTarget::create(
      ComponentInstanceId(target_json.at("component_instance").get<std::string>()),
      target_json.at("semantic_reference").get<std::string>());
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

[[nodiscard]] json assembly_constraint_to_json(const AssemblyConstraint& constraint) {
  json constraint_json{{"id", constraint.id().value()},
                       {"name", constraint.name()},
                       {"type", std::string(to_string(constraint.type()))},
                       {"target_a", constraint_target_to_json(constraint.target_a())},
                       {"target_b", constraint_target_to_json(constraint.target_b())},
                       {"state", std::string(to_string(constraint.state()))}};
  if (constraint.distance().has_value()) {
    constraint_json["distance"] = json{{"unit", std::string(constraint.distance()->unit())},
                                       {"value", constraint.distance()->millimeters()}};
  }
  if (constraint.angle().has_value()) {
    constraint_json["angle"] = angle_quantity_to_json(constraint.angle()->degrees());
  }
  return constraint_json;
}

[[nodiscard]] Result<AssemblyConstraint>
assembly_constraint_from_json(const json& constraint_json) {
  auto type = constraint_type_from_json(constraint_json.at("type"));
  if (type.has_error())
    return Result<AssemblyConstraint>::failure(type.error());

  auto state = constraint_state_from_json(constraint_json.at("state"));
  if (state.has_error())
    return Result<AssemblyConstraint>::failure(state.error());

  auto target_a = constraint_target_from_json(constraint_json.at("target_a"));
  if (target_a.has_error())
    return Result<AssemblyConstraint>::failure(target_a.error());

  auto target_b = constraint_target_from_json(constraint_json.at("target_b"));
  if (target_b.has_error())
    return Result<AssemblyConstraint>::failure(target_b.error());

  const std::string id = constraint_json.at("id").get<std::string>();
  std::optional<Quantity> distance;
  if (constraint_json.contains("distance")) {
    const json& distance_json = constraint_json.at("distance");
    if (distance_json.at("unit").get<std::string>() != "mm") {
      return Result<AssemblyConstraint>::failure(
          json_error("assembly constraint distance must use millimeters"));
    }
    auto quantity = Quantity::length_mm(distance_json.at("value").get<double>(), id);
    if (quantity.has_error())
      return Result<AssemblyConstraint>::failure(quantity.error());
    distance = quantity.value();
  }

  std::optional<Quantity> angle;
  if (constraint_json.contains("angle")) {
    auto quantity = angle_quantity_from_json(constraint_json.at("angle"), id,
                                             "assembly constraint angle");
    if (quantity.has_error())
      return Result<AssemblyConstraint>::failure(quantity.error());
    angle = quantity.value();
  }

  return AssemblyConstraint::create(AssemblyConstraintId(id),
                                    constraint_json.at("name").get<std::string>(), type.value(),
                                    std::move(target_a.value()), std::move(target_b.value()),
                                    state.value(), std::move(distance), std::move(angle));
}

[[nodiscard]] Result<AssemblyJointType> joint_type_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "revolute")
    return Result<AssemblyJointType>::success(AssemblyJointType::Revolute);
  return Result<AssemblyJointType>::failure(json_error("unsupported assembly joint type"));
}

[[nodiscard]] Result<AssemblyJointState> joint_state_from_json(const json& value) {
  const auto text = value.get<std::string>();
  if (text == "active")
    return Result<AssemblyJointState>::success(AssemblyJointState::Active);
  if (text == "inactive")
    return Result<AssemblyJointState>::success(AssemblyJointState::Inactive);
  return Result<AssemblyJointState>::failure(json_error("unsupported assembly joint state"));
}

[[nodiscard]] json assembly_joint_to_json(const AssemblyJoint& joint) {
  return json{{"id", joint.id().value()},
              {"name", joint.name()},
              {"type", std::string(to_string(joint.type()))},
              {"target_a", constraint_target_to_json(joint.target_a())},
              {"target_b", constraint_target_to_json(joint.target_b())},
              {"state", std::string(to_string(joint.state()))},
              {"limits",
               json{{"lower", angle_quantity_to_json(joint.limits().lower_deg)},
                    {"upper", angle_quantity_to_json(joint.limits().upper_deg)}}},
              {"coordinate", angle_quantity_to_json(joint.coordinate_deg())}};
}

[[nodiscard]] Result<AssemblyJoint> assembly_joint_from_json(const json& joint_json) {
  auto type = joint_type_from_json(joint_json.at("type"));
  if (type.has_error())
    return Result<AssemblyJoint>::failure(type.error());
  auto state = joint_state_from_json(joint_json.at("state"));
  if (state.has_error())
    return Result<AssemblyJoint>::failure(state.error());
  auto target_a = constraint_target_from_json(joint_json.at("target_a"));
  if (target_a.has_error())
    return Result<AssemblyJoint>::failure(target_a.error());
  auto target_b = constraint_target_from_json(joint_json.at("target_b"));
  if (target_b.has_error())
    return Result<AssemblyJoint>::failure(target_b.error());

  const std::string id = joint_json.at("id").get<std::string>();
  const json& limits = joint_json.at("limits");
  auto lower = angle_quantity_from_json(limits.at("lower"), id, "assembly joint lower limit");
  if (lower.has_error())
    return Result<AssemblyJoint>::failure(lower.error());
  auto upper = angle_quantity_from_json(limits.at("upper"), id, "assembly joint upper limit");
  if (upper.has_error())
    return Result<AssemblyJoint>::failure(upper.error());
  auto coordinate = angle_quantity_from_json(joint_json.at("coordinate"), id,
                                              "assembly joint coordinate");
  if (coordinate.has_error())
    return Result<AssemblyJoint>::failure(coordinate.error());

  return AssemblyJoint::create(AssemblyJointId(id), joint_json.at("name").get<std::string>(),
                               type.value(), std::move(target_a.value()),
                               std::move(target_b.value()), state.value(), lower.value(),
                               upper.value(), coordinate.value());
}

[[nodiscard]] Result<Parameter> assembly_parameter_from_json(const json& parameter_json) {
  const auto id = parameter_json.at("id").get<std::string>();
  const auto type = parameter_json.at("type").get<std::string>();
  if (type != "length" && type != "count") {
    return Result<Parameter>::failure(json_error("only length and count parameters are supported"));
  }
  if (parameter_json.at("scope").get<std::string>() != "assembly") {
    return Result<Parameter>::failure(
        json_error("assembly documents support only assembly-scope parameters"));
  }
  if (type == "count") {
    if (parameter_json.at("unit").get<std::string>() != "1") {
      return Result<Parameter>::failure(json_error("count parameters must use unit \"1\""));
    }
    auto quantity = Quantity::count(parameter_json.at("value").get<double>(), id);
    if (quantity.has_error())
      return Result<Parameter>::failure(quantity.error());
    return Parameter::create_count(ParameterId(id), parameter_json.at("name").get<std::string>(),
                                   quantity.value(), ParameterScope::Assembly);
  }
  if (parameter_json.at("unit").get<std::string>() != "mm") {
    return Result<Parameter>::failure(json_error("only millimeter length parameters are supported"));
  }
  auto quantity = Quantity::length_mm(parameter_json.at("value").get<double>(), id);
  if (quantity.has_error())
    return Result<Parameter>::failure(quantity.error());
  return Parameter::create_length(ParameterId(id), parameter_json.at("name").get<std::string>(),
                                  quantity.value(), ParameterScope::Assembly);
}

} // namespace

Result<std::string> serialize_assembly_document_to_json(const AssemblyDocument& document) {
  json root;
  root["schema"] = k_schema;
  root["version"] = k_version;
  root["document"] = json{{"id", document.id().value()}, {"name", document.name()}};
  root["parameters"] = json::array();
  for (const auto& parameter : document.parameters()) {
    root["parameters"].push_back(json{{"id", parameter.id().value()},
                                      {"name", parameter.name()},
                                      {"type", std::string(to_string(parameter.type()))},
                                      {"scope", std::string(to_string(parameter.scope()))},
                                      {"unit", std::string(parameter.value().unit())},
                                      {"value", parameter.value().millimeters()}});
  }
  root["member_parts"] = json::array();
  for (const auto& part : document.member_parts()) {
    root["member_parts"].push_back(part.value());
  }
  root["parameter_bindings"] = json::array();
  for (const auto& binding : document.bindings()) {
    root["parameter_bindings"].push_back(
        json{{"id", binding.id().value()},
             {"part_document", binding.part_document().value()},
             {"part_parameter", binding.part_parameter().value()},
             {"assembly_parameter", binding.assembly_parameter().value()}});
  }
  root["component_instances"] = json::array();
  for (const auto& instance : document.component_instances()) {
    root["component_instances"].push_back(component_instance_to_json(instance));
  }
  root["subassembly_instances"] = json::array();
  for (const auto& instance : document.subassembly_instances()) {
    root["subassembly_instances"].push_back(subassembly_instance_to_json(instance));
  }
  root["assembly_constraints"] = json::array();
  for (const auto& constraint : document.constraints()) {
    root["assembly_constraints"].push_back(assembly_constraint_to_json(constraint));
  }
  root["assembly_joints"] = json::array();
  for (const auto& joint : document.joints()) {
    root["assembly_joints"].push_back(assembly_joint_to_json(joint));
  }
  return Result<std::string>::success(root.dump(2));
}

Result<AssemblyDocument> deserialize_assembly_document_from_json(std::string_view content) {
  json root;
  try {
    root = json::parse(content);
  } catch (const std::exception& parse_error) {
    return Result<AssemblyDocument>::failure(
        json_error(std::string("invalid assembly document json: ") + parse_error.what()));
  }

  try {
    if (root.at("schema").get<std::string>() != k_schema) {
      return Result<AssemblyDocument>::failure(json_error("unsupported assembly document schema"));
    }
    if (root.at("version").get<int>() != k_version) {
      return Result<AssemblyDocument>::failure(json_error("unsupported assembly document version"));
    }

    auto document =
        AssemblyDocument::create(DocumentId(root.at("document").at("id").get<std::string>()),
                                 root.at("document").at("name").get<std::string>());
    if (document.has_error())
      return document;

    for (const auto& parameter_json : root.value("parameters", json::array())) {
      auto parameter = assembly_parameter_from_json(parameter_json);
      if (parameter.has_error())
        return Result<AssemblyDocument>::failure(parameter.error());
      auto added = document.value().add_parameter(std::move(parameter.value()));
      if (added.has_error())
        return Result<AssemblyDocument>::failure(added.error());
    }

    for (const auto& part_json : root.value("member_parts", json::array())) {
      auto added = document.value().add_member_part(DocumentId(part_json.get<std::string>()));
      if (added.has_error())
        return Result<AssemblyDocument>::failure(added.error());
    }

    for (const auto& binding_json : root.value("parameter_bindings", json::array())) {
      auto binding = ParameterBinding::create(
          ParameterBindingId(binding_json.at("id").get<std::string>()),
          DocumentId(binding_json.at("part_document").get<std::string>()),
          ParameterId(binding_json.at("part_parameter").get<std::string>()),
          ParameterId(binding_json.at("assembly_parameter").get<std::string>()));
      if (binding.has_error())
        return Result<AssemblyDocument>::failure(binding.error());
      auto added = document.value().add_binding(std::move(binding.value()));
      if (added.has_error())
        return Result<AssemblyDocument>::failure(added.error());
    }

    for (const auto& instance_json : root.value("component_instances", json::array())) {
      auto instance = component_instance_from_json(instance_json);
      if (instance.has_error())
        return Result<AssemblyDocument>::failure(instance.error());
      auto added = document.value().add_component_instance(std::move(instance.value()));
      if (added.has_error())
        return Result<AssemblyDocument>::failure(added.error());
    }

    for (const auto& instance_json : root.value("subassembly_instances", json::array())) {
      auto instance = subassembly_instance_from_json(instance_json);
      if (instance.has_error())
        return Result<AssemblyDocument>::failure(instance.error());
      auto added = document.value().add_subassembly_instance(std::move(instance.value()));
      if (added.has_error())
        return Result<AssemblyDocument>::failure(added.error());
    }

    for (const auto& constraint_json : root.value("assembly_constraints", json::array())) {
      auto constraint = assembly_constraint_from_json(constraint_json);
      if (constraint.has_error())
        return Result<AssemblyDocument>::failure(constraint.error());
      auto added = document.value().add_constraint(std::move(constraint.value()));
      if (added.has_error())
        return Result<AssemblyDocument>::failure(added.error());
    }

    for (const auto& joint_json : root.value("assembly_joints", json::array())) {
      auto joint = assembly_joint_from_json(joint_json);
      if (joint.has_error())
        return Result<AssemblyDocument>::failure(joint.error());
      auto added = document.value().add_joint(std::move(joint.value()));
      if (added.has_error())
        return Result<AssemblyDocument>::failure(added.error());
    }

    return document;
  } catch (const std::exception& content_error) {
    return Result<AssemblyDocument>::failure(
        json_error(std::string("invalid assembly document content: ") + content_error.what()));
  }
}

} // namespace blcad
