#pragma once
#include "blcad/core/step_import_source.hpp"
#include <nlohmann/json.hpp>

namespace blcad::detail {
inline nlohmann::json step_import_sources_to_json(const std::vector<StepImportSource>& sources) {
  auto values = nlohmann::json::array();
  for (const auto& source : sources)
    values.push_back({{"id", source.id().value()},
                      {"name", source.name()},
                      {"project_relative_asset_path", source.project_relative_asset_path()},
                      {"sha256_digest", source.sha256_digest()},
                      {"mode", to_string(source.mode())}});
  return values;
}
template <typename Host>
Result<std::size_t> load_step_import_sources(const nlohmann::json& root, Host& host) {
  if (!root.contains("step_import_sources"))
    return Result<std::size_t>::success(0);
  const auto& sources = root.at("step_import_sources");
  const auto invalid = [] {
    return Result<std::size_t>::failure(Error::validation(
        "step_import_sources_json", "STEP sources require an array of exact "
                                    "id/name/project_relative_asset_path/sha256_digest/mode string "
                                    "records with canonical spelling"));
  };
  if (!sources.is_array())
    return invalid();
  for (const auto& value : sources) {
    if (!value.is_object() || value.size() != 5)
      return invalid();
    for (const char* field : {"id", "name", "project_relative_asset_path", "sha256_digest", "mode"})
      if (!value.contains(field) || !value.at(field).is_string())
        return invalid();
    const auto mode = value.at("mode").get<std::string>();
    if (mode != "reference" && mode != "editable_body")
      return invalid();
    auto source = StepImportSource::create(
        StepImportSourceId(value.at("id").get<std::string>()), value.at("name").get<std::string>(),
        value.at("project_relative_asset_path").get<std::string>(),
        value.at("sha256_digest").get<std::string>(),
        mode == "reference" ? StepImportMode::Reference : StepImportMode::EditableBody);
    if (!source)
      return Result<std::size_t>::failure(source.error());
    if (source.value().project_relative_asset_path() !=
            value.at("project_relative_asset_path").get<std::string>() ||
        source.value().sha256_digest() != value.at("sha256_digest").get<std::string>())
      return invalid();
    auto added = host.add_step_import_source(std::move(source.value()));
    if (!added)
      return added;
  }
  return Result<std::size_t>::success(sources.size());
}
} // namespace blcad::detail
