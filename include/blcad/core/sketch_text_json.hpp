#pragma once

#include "blcad/core/sketch_text.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

// Canonical sidecar persistence for Block-113 annotation intent. It is kept
// independent from historical blcad.part_document.mvp1 so old Part files keep
// their exact schema while text remains deterministic and headless.
[[nodiscard]] inline Result<std::string>
serialize_sketch_text_catalog_to_json(const SketchTextCatalog& catalog) {
  using json = nlohmann::json;
  json root{{"schema", "blcad.sketch_text.mvp8"},
            {"version", 1},
            {"document", catalog.document().value()},
            {"texts", json::array()}};
  for (const auto& text : catalog.texts()) {
    json bindings = json::array();
    for (const auto& binding : text.bindings())
      bindings.push_back(json{{"token", binding.token},
                              {"parameter", binding.parameter.value()},
                              {"precision", binding.precision}});
    json value{{"id", text.id().value()},
               {"sketch", text.sketch().value()},
               {"template", text.template_text()},
               {"requested_font", text.requested_font()},
               {"height_parameter", text.height_parameter().value()},
               {"anchor", json{{"x", text.anchor().x}, {"y", text.anchor().y}}},
               {"rotation_parameter", nullptr},
               {"bindings", std::move(bindings)}};
    if (text.rotation_parameter().has_value())
      value["rotation_parameter"] = text.rotation_parameter()->value();
    root["texts"].push_back(std::move(value));
  }
  return Result<std::string>::success(root.dump(2));
}

[[nodiscard]] inline Result<SketchTextCatalog>
deserialize_sketch_text_catalog_from_json(std::string_view content) {
  using json = nlohmann::json;
  try {
    const json root = json::parse(content.begin(), content.end());
    if (!root.is_object() || root.value("schema", "") != "blcad.sketch_text.mvp8" ||
        root.value("version", 0) != 1 || !root.contains("document") ||
        !root.at("document").is_string() || !root.contains("texts") ||
        !root.at("texts").is_array())
      return Result<SketchTextCatalog>::failure(
          Error::validation("sketch_text_json", "unsupported or malformed Sketch text catalog"));

    std::vector<SketchText> texts;
    for (const auto& value : root.at("texts")) {
      if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
          !value.contains("sketch") || !value.at("sketch").is_string() ||
          !value.contains("template") || !value.at("template").is_string() ||
          !value.contains("requested_font") || !value.at("requested_font").is_string() ||
          !value.contains("height_parameter") || !value.at("height_parameter").is_string() ||
          !value.contains("anchor") || !value.at("anchor").is_object() ||
          !value.at("anchor").contains("x") || !value.at("anchor").at("x").is_number() ||
          !value.at("anchor").contains("y") || !value.at("anchor").at("y").is_number() ||
          !value.contains("rotation_parameter") ||
          (!value.at("rotation_parameter").is_null() &&
           !value.at("rotation_parameter").is_string()) ||
          !value.contains("bindings") || !value.at("bindings").is_array())
        return Result<SketchTextCatalog>::failure(
            Error::validation("sketch_text_json", "Sketch text record has invalid fields"));

      std::vector<SketchTextBinding> bindings;
      for (const auto& binding : value.at("bindings")) {
        if (!binding.is_object() || !binding.contains("token") ||
            !binding.at("token").is_string() || !binding.contains("parameter") ||
            !binding.at("parameter").is_string() || !binding.contains("precision") ||
            !binding.at("precision").is_number_unsigned())
          return Result<SketchTextCatalog>::failure(
              Error::validation("sketch_text_json", "Sketch text binding has invalid fields"));
        bindings.push_back({binding.at("token").get<std::string>(),
                            ParameterId(binding.at("parameter").get<std::string>()),
                            binding.at("precision").get<std::size_t>()});
      }
      std::optional<ParameterId> rotation;
      if (!value.at("rotation_parameter").is_null())
        rotation = ParameterId(value.at("rotation_parameter").get<std::string>());
      auto text = SketchText::create(
          SketchTextId(value.at("id").get<std::string>()),
          SketchId(value.at("sketch").get<std::string>()),
          value.at("template").get<std::string>(),
          value.at("requested_font").get<std::string>(),
          ParameterId(value.at("height_parameter").get<std::string>()),
          Point2{value.at("anchor").at("x").get<double>(),
                 value.at("anchor").at("y").get<double>()},
          rotation, std::move(bindings));
      if (text.has_error()) return Result<SketchTextCatalog>::failure(text.error());
      texts.push_back(std::move(text.value()));
    }
    return SketchTextCatalog::create(DocumentId(root.at("document").get<std::string>()),
                                     std::move(texts));
  } catch (const std::exception& exception) {
    return Result<SketchTextCatalog>::failure(
        Error::validation("sketch_text_json", exception.what()));
  }
}

} // namespace blcad
