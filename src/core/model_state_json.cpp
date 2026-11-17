#include "model_state_json.hpp"

#include "blcad/core/part_document.hpp"
#include "engineering_resource_json.hpp"

#include <stdexcept>

namespace blcad::detail {
namespace {
using json = nlohmann::json;
void fields(const json& value, std::initializer_list<std::string_view> required) {
  require_engineering_json_fields(value, required);
}
void string_field(const json& value, std::string_view key) {
  if (!value.at(std::string(key)).is_string())
    throw std::runtime_error("Model-state identities/names/formulas must be strings");
}
void boolean_field(const json& value, std::string_view key) {
  if (!value.at(std::string(key)).is_boolean())
    throw std::runtime_error("Model-state suppression must be a boolean");
}
template <typename Child>
json child_to_json(const Child& child, std::string_view field, const std::string& id) {
  json value{{std::string(field), id}, {"suppressed", nullptr}, {"selected_state", nullptr}};
  if (child.suppressed)
    value["suppressed"] = *child.suppressed;
  if (child.selected_state)
    value["selected_state"] = child.selected_state->value();
  return value;
}
template <typename Child> void parse_child(const json& value, Child& child) {
  if (!value.at("suppressed").is_null()) {
    boolean_field(value, "suppressed");
    child.suppressed = value.at("suppressed").get<bool>();
  }
  if (!value.at("selected_state").is_null()) {
    string_field(value, "selected_state");
    child.selected_state = ModelStateId(value.at("selected_state").get<std::string>());
  }
}
} // namespace

nlohmann::json model_states_to_json(const std::vector<ModelState>& states) {
  auto result = json::array();
  for (const auto& state : states) {
    json value{{"version", 1},
               {"id", state.id.value()},
               {"name", state.name},
               {"parameters", json::array()},
               {"features", json::array()},
               {"materials", json::array()},
               {"components", json::array()},
               {"subassemblies", json::array()}};
    for (const auto& change : state.parameters) {
      if (const auto* literal = std::get_if<Quantity>(&change.value)) {
        value["parameters"].push_back({{"parameter", change.parameter.value()},
                                       {"kind", "literal"},
                                       {"unit", std::string(literal->unit())},
                                       {"value", literal->millimeters()}});
      } else {
        value["parameters"].push_back({{"parameter", change.parameter.value()},
                                       {"kind", "formula"},
                                       {"formula", std::get<std::string>(change.value)}});
      }
    }
    for (const auto& change : state.features)
      value["features"].push_back(
          {{"executable", change.executable}, {"suppressed", change.suppressed}});
    for (const auto& change : state.materials) {
      json material{{"body", nullptr}, {"material", nullptr}};
      if (change.body)
        material["body"] = change.body->value();
      if (change.material)
        material["material"] = engineering_resource_key_to_json(*change.material);
      value["materials"].push_back(std::move(material));
    }
    for (const auto& change : state.components)
      value["components"].push_back(child_to_json(change, "component", change.component.value()));
    for (const auto& change : state.subassemblies)
      value["subassemblies"].push_back(
          child_to_json(change, "subassembly", change.subassembly.value()));
    result.push_back(std::move(value));
  }
  return result;
}

Result<std::vector<ModelState>> model_states_from_json(const nlohmann::json& values) {
  try {
    if (!values.is_array())
      throw std::runtime_error("Model states must be an array");
    std::vector<ModelState> result;
    for (const auto& value : values) {
      fields(value, {"version", "id", "name", "parameters", "features", "materials", "components",
                     "subassemblies"});
      if (!value.at("version").is_number_integer() || value.at("version") != 1)
        throw std::runtime_error("Unsupported model-state operation version");
      string_field(value, "id");
      string_field(value, "name");
      ModelState state{ModelStateId(value.at("id").get<std::string>()),
                       value.at("name").get<std::string>(),
                       {},
                       {},
                       {},
                       {},
                       {}};
      for (const auto* field :
           {"parameters", "features", "materials", "components", "subassemblies"})
        if (!value.at(field).is_array())
          throw std::runtime_error("Model-state override fields must be arrays");
      for (const auto& change : value.at("parameters")) {
        string_field(change, "kind");
        string_field(change, "parameter");
        const ParameterId id(change.at("parameter").get<std::string>());
        if (change.at("kind") == "literal") {
          fields(change, {"parameter", "kind", "unit", "value"});
          string_field(change, "unit");
          if (!change.at("value").is_number())
            throw std::runtime_error("Literal override values must be numbers");
          const auto amount = change.at("value").get<double>();
          const auto unit = change.at("unit").get<std::string>();
          auto quantity = unit == "mm"    ? Quantity::length_mm(amount, id.value())
                          : unit == "deg" ? Quantity::angle_deg(amount, id.value())
                          : unit == "1"   ? Quantity::count(amount, id.value())
                                          : Result<Quantity>::failure(Error::validation(
                                              id.value(), "Unsupported state literal unit"));
          if (!quantity)
            return Result<std::vector<ModelState>>::failure(quantity.error());
          state.parameters.push_back({id, quantity.value()});
        } else if (change.at("kind") == "formula") {
          fields(change, {"parameter", "kind", "formula"});
          string_field(change, "formula");
          state.parameters.push_back({id, change.at("formula").get<std::string>()});
        } else {
          throw std::runtime_error("Unknown model-state parameter override kind");
        }
      }
      for (const auto& change : value.at("features")) {
        fields(change, {"executable", "suppressed"});
        string_field(change, "executable");
        boolean_field(change, "suppressed");
        state.features.push_back(
            {change.at("executable").get<std::string>(), change.at("suppressed").get<bool>()});
      }
      for (const auto& change : value.at("materials")) {
        fields(change, {"body", "material"});
        ModelStateMaterialOverride material;
        if (!change.at("body").is_null()) {
          string_field(change, "body");
          material.body = BodyId(change.at("body").get<std::string>());
        }
        if (!change.at("material").is_null()) {
          auto key = engineering_resource_key_from_json(change.at("material"));
          if (!key)
            return Result<std::vector<ModelState>>::failure(key.error());
          material.material = std::move(key.value());
        }
        state.materials.push_back(std::move(material));
      }
      for (const auto& change : value.at("components")) {
        fields(change, {"component", "suppressed", "selected_state"});
        string_field(change, "component");
        ModelStateComponentOverride child{
            ComponentInstanceId(change.at("component").get<std::string>()), {}, {}};
        parse_child(change, child);
        state.components.push_back(std::move(child));
      }
      for (const auto& change : value.at("subassemblies")) {
        fields(change, {"subassembly", "suppressed", "selected_state"});
        string_field(change, "subassembly");
        ModelStateSubassemblyOverride child{
            SubassemblyInstanceId(change.at("subassembly").get<std::string>()), {}, {}};
        parse_child(change, child);
        state.subassemblies.push_back(std::move(child));
      }
      result.push_back(std::move(state));
    }
    auto valid = validate_model_state_catalog(result);
    if (!valid)
      return Result<std::vector<ModelState>>::failure(valid.error());
    return Result<std::vector<ModelState>>::success(std::move(result));
  } catch (const std::exception& e) {
    return Result<std::vector<ModelState>>::failure(
        Error::validation("model_states_json", e.what()));
  }
}

Result<std::size_t> load_part_model_states(const nlohmann::json& root, PartDocument& part) {
  if (!root.contains("model_states"))
    return Result<std::size_t>::success(0);
  auto states = model_states_from_json(root.at("model_states"));
  if (!states)
    return Result<std::size_t>::failure(states.error());
  return part.replace_model_states(std::move(states.value()));
}
} // namespace blcad::detail
