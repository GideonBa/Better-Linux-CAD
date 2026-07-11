#include "blcad/core/project_json.hpp"

#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/part_document_json.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

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
