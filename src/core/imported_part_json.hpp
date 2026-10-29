#pragma once

#include "blcad/core/imported_part.hpp"

#include <nlohmann/json.hpp>

#include <initializer_list>
#include <utility>

namespace blcad::detail {
inline bool imported_part_fields(const nlohmann::json& value,
                                 std::initializer_list<std::string_view> fields) {
  if (!value.is_object() || value.size() != fields.size())
    return false;
  for (const auto field : fields)
    if (!value.contains(std::string(field)))
      return false;
  return true;
}
inline Error imported_part_json_error() {
  return Error::validation("imported_part_json",
                           "invalid imported Part definition or base feature");
}
inline nlohmann::json imported_part_to_json(const ImportedPartDefinition& definition) {
  nlohmann::json outputs = nlohmann::json::array();
  for (const auto& output : definition.outputs())
    outputs.push_back({{"imported_body", output.imported_body.value()},
                       {"body", output.body.value()},
                       {"kind", to_string(output.kind)}});
  return {{"source", definition.source().value()},
          {"catalog", definition.catalog().value()},
          {"outputs", std::move(outputs)}};
}
inline Result<ImportedPartDefinition> imported_part_from_json(const nlohmann::json& value) {
  if (!imported_part_fields(value, {"source", "catalog", "outputs"}) ||
      !value.at("source").is_string() || !value.at("catalog").is_string() ||
      !value.at("outputs").is_array())
    return Result<ImportedPartDefinition>::failure(imported_part_json_error());
  std::vector<ImportedBodyOutput> outputs;
  for (const auto& output : value.at("outputs")) {
    if (!imported_part_fields(output, {"imported_body", "body", "kind"}) ||
        !output.at("imported_body").is_string() || !output.at("body").is_string() ||
        !output.at("kind").is_string() ||
        (output.at("kind") != "solid" && output.at("kind") != "surface"))
      return Result<ImportedPartDefinition>::failure(imported_part_json_error());
    outputs.push_back({ImportedTopologyId(output.at("imported_body").get<std::string>()),
                       BodyId(output.at("body").get<std::string>()),
                       output.at("kind") == "solid" ? BodyKind::Solid : BodyKind::Surface});
  }
  return ImportedPartDefinition::create(
      StepImportSourceId(value.at("source").get<std::string>()),
      ImportedTopologyCatalogId(value.at("catalog").get<std::string>()), std::move(outputs));
}
inline nlohmann::json imported_body_feature_to_json(const ImportedBodyFeature& feature) {
  return {{"id", feature.id().value()},
          {"name", feature.name()},
          {"definition", imported_part_to_json(feature.definition())}};
}
inline Result<ImportedBodyFeature> imported_body_feature_from_json(const nlohmann::json& value) {
  if (!imported_part_fields(value, {"id", "name", "definition"}) || !value.at("id").is_string() ||
      !value.at("name").is_string())
    return Result<ImportedBodyFeature>::failure(imported_part_json_error());
  auto definition = imported_part_from_json(value.at("definition"));
  if (!definition)
    return Result<ImportedBodyFeature>::failure(definition.error());
  return ImportedBodyFeature::create(FeatureId(value.at("id").get<std::string>()),
                                     value.at("name").get<std::string>(),
                                     std::move(definition.value()));
}
} // namespace blcad::detail
