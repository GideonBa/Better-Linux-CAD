#include "blcad/core/assembly_document_json.hpp"

#include <nlohmann/json.hpp>

#include <exception>
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
  if (text == "visible") return Result<ComponentVisibility>::success(ComponentVisibility::Visible);
  if (text == "hidden") return Result<ComponentVisibility>::success(ComponentVisibility::Hidden);
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
  if (text == "free") return Result<ComponentGroundingState>::success(ComponentGroundingState::Free);
  if (text == "grounded") {
    return Result<ComponentGroundingState>::success(ComponentGroundingState::Grounded);
  }
  return Result<ComponentGroundingState>::failure(json_error("unsupported component grounding state"));
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
  if (visibility.has_error()) return Result<ComponentInstance>::failure(visibility.error());

  auto suppression_state = suppression_state_from_json(instance_json.at("suppression_state"));
  if (suppression_state.has_error()) {
    return Result<ComponentInstance>::failure(suppression_state.error());
  }

  auto grounding_state = grounding_state_from_json(instance_json.at("grounding_state"));
  if (grounding_state.has_error()) return Result<ComponentInstance>::failure(grounding_state.error());

  return ComponentInstance::create(
      ComponentInstanceId(instance_json.at("id").get<std::string>()),
      instance_json.at("name").get<std::string>(),
      DocumentId(instance_json.at("referenced_part_document").get<std::string>()),
      visibility.value(), suppression_state.value(), grounding_state.value(),
      transform_from_json(instance_json.at("transform")));
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
    if (quantity.has_error()) {
      return Result<Parameter>::failure(quantity.error());
    }
    return Parameter::create_count(ParameterId(id), parameter_json.at("name").get<std::string>(),
                                   quantity.value(), ParameterScope::Assembly);
  }
  if (parameter_json.at("unit").get<std::string>() != "mm") {
    return Result<Parameter>::failure(
        json_error("only millimeter length parameters are supported"));
  }
  auto quantity = Quantity::length_mm(parameter_json.at("value").get<double>(), id);
  if (quantity.has_error()) {
    return Result<Parameter>::failure(quantity.error());
  }
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
    if (document.has_error()) {
      return document;
    }

    for (const auto& parameter_json : root.value("parameters", json::array())) {
      auto parameter = assembly_parameter_from_json(parameter_json);
      if (parameter.has_error()) {
        return Result<AssemblyDocument>::failure(parameter.error());
      }
      auto added = document.value().add_parameter(std::move(parameter.value()));
      if (added.has_error()) {
        return Result<AssemblyDocument>::failure(added.error());
      }
    }

    for (const auto& part_json : root.value("member_parts", json::array())) {
      auto added = document.value().add_member_part(DocumentId(part_json.get<std::string>()));
      if (added.has_error()) {
        return Result<AssemblyDocument>::failure(added.error());
      }
    }

    for (const auto& binding_json : root.value("parameter_bindings", json::array())) {
      auto binding = ParameterBinding::create(
          ParameterBindingId(binding_json.at("id").get<std::string>()),
          DocumentId(binding_json.at("part_document").get<std::string>()),
          ParameterId(binding_json.at("part_parameter").get<std::string>()),
          ParameterId(binding_json.at("assembly_parameter").get<std::string>()));
      if (binding.has_error()) {
        return Result<AssemblyDocument>::failure(binding.error());
      }
      auto added = document.value().add_binding(std::move(binding.value()));
      if (added.has_error()) {
        return Result<AssemblyDocument>::failure(added.error());
      }
    }

    for (const auto& instance_json : root.value("component_instances", json::array())) {
      auto instance = component_instance_from_json(instance_json);
      if (instance.has_error()) {
        return Result<AssemblyDocument>::failure(instance.error());
      }
      auto added = document.value().add_component_instance(std::move(instance.value()));
      if (added.has_error()) {
        return Result<AssemblyDocument>::failure(added.error());
      }
    }

    return document;
  } catch (const std::exception& content_error) {
    return Result<AssemblyDocument>::failure(
        json_error(std::string("invalid assembly document content: ") + content_error.what()));
  }
}

} // namespace blcad
