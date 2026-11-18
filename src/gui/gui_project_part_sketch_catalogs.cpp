#include "blcad/gui/gui_document_session.hpp"

#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <set>

namespace blcad::gui {
namespace {
using json = nlohmann::json;
Error error(std::string message) {
  return Error::validation("gui.project_sketch_catalogs", std::move(message));
}
bool exact(const json& value, std::initializer_list<const char*> fields) {
  if (!value.is_object() || value.size() != fields.size())
    return false;
  return std::all_of(fields.begin(), fields.end(),
                     [&](const auto* field) { return value.contains(field); });
}
} // namespace
bool project_sketch_catalog_target_is_step_asset(
    const Project& project, const std::filesystem::path& source_document,
    const std::filesystem::path& destination_document) {
  std::error_code access;
  const auto target =
      std::filesystem::absolute(project_part_sketch_catalog_sidecar(destination_document), access)
          .lexically_normal();
  if (access)
    return false;
  std::vector<std::filesystem::path> roots;
  for (const auto& document : {source_document, destination_document}) {
    if (document.empty())
      continue;
    auto absolute = std::filesystem::absolute(document, access).lexically_normal();
    if (!access && std::find(roots.begin(), roots.end(), absolute.parent_path()) == roots.end())
      roots.push_back(absolute.parent_path());
  }
  const auto same = [](const auto& a, const auto& b) {
    if (a.lexically_normal() == b.lexically_normal())
      return true;
    std::error_code error;
    const bool identical = std::filesystem::equivalent(a, b, error);
    return !error && identical;
  };
  const auto owns = [&](const auto& sources) {
    for (const auto& source : sources)
      for (const auto& directory : roots)
        if (same(directory / source.project_relative_asset_path(), target))
          return true;
    return false;
  };
  if (owns(project.step_import_sources()))
    return true;
  return std::any_of(project.part_documents().begin(), project.part_documents().end(),
                     [&](const auto& part) { return owns(part.step_import_sources()); });
}
Result<std::size_t> GuiDocumentSession::validate_project_part_sketch_catalogs(
    const Project& project, const std::vector<GuiProjectPartSketchCatalogs>& catalogs) const {
  std::set<std::string> seen;
  for (const auto& group : catalogs) {
    const auto* part = project.find_part_document(group.part);
    if (!part || part->is_reference_part() || !seen.insert(group.part.value()).second ||
        (group.constraints.empty() && group.dimensions.empty()))
      return Result<std::size_t>::failure(error(
          "Child Sketch catalogs require a unique editable Part owner and nonempty catalogs"));
    auto constraints = validate_constraint_catalogs(*part, group.constraints);
    if (!constraints)
      return constraints;
    auto dimensions = validate_dimension_catalogs(*part, group.dimensions);
    if (!dimensions)
      return dimensions;
  }
  return Result<std::size_t>::success(catalogs.size());
}
Result<std::string>
GuiDocumentSession::serialize_project_part_sketch_catalogs(const Project& project) const {
  auto valid = validate_project_part_sketch_catalogs(project, project_part_sketch_catalogs_);
  if (!valid)
    return Result<std::string>::failure(valid.error());
  json parts = json::array();
  auto ordered = project_part_sketch_catalogs_;
  std::sort(ordered.begin(), ordered.end(),
            [](const auto& a, const auto& b) { return a.part.value() < b.part.value(); });
  for (const auto& group : ordered) {
    auto constraints = serialize_sketch_constraint_catalogs_to_json(group.part, group.constraints);
    if (!constraints)
      return Result<std::string>::failure(constraints.error());
    auto dimensions = serialize_sketch_dimension_catalogs_to_json(group.part, group.dimensions);
    if (!dimensions)
      return Result<std::string>::failure(dimensions.error());
    parts.push_back({{"part", group.part.value()},
                     {"constraints", json::parse(constraints.value())},
                     {"dimensions", json::parse(dimensions.value())}});
  }
  return Result<std::string>::success(json{
      {"schema", "blcad.project_part_sketch_catalogs.mvp9"},
      {"version", 1},
      {"project", project.id().value()},
      {"parts", std::move(parts)}}.dump(2));
}
Result<std::vector<GuiProjectPartSketchCatalogs>>
GuiDocumentSession::read_project_part_sketch_catalogs(
    const Project& project, const std::filesystem::path& document_path) const {
  using R = Result<std::vector<GuiProjectPartSketchCatalogs>>;
  if (project_sketch_catalog_target_is_step_asset(project, document_path, document_path))
    return R::success({});
  const auto path = project_part_sketch_catalog_sidecar(document_path);
  std::error_code access;
  const bool exists = std::filesystem::exists(path, access);
  if (access)
    return R::failure(error("Cannot inspect Project child-Sketch sidecar: " + access.message()));
  if (!exists)
    return R::success({});
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    return R::failure(error("Cannot read Project child-Sketch sidecar"));
  try {
    const json root = json::parse(stream);
    if (stream.bad() || !exact(root, {"schema", "version", "project", "parts"}) ||
        root.at("schema") != "blcad.project_part_sketch_catalogs.mvp9" ||
        !root.at("version").is_number_integer() || root.at("version") != 1 ||
        !root.at("project").is_string() || root.at("project") != project.id().value() ||
        !root.at("parts").is_array() || root.at("parts").empty())
      return R::failure(error("Unsupported or unnecessary Project child-Sketch sidecar envelope"));
    std::vector<GuiProjectPartSketchCatalogs> groups;
    for (const auto& entry : root.at("parts")) {
      if (!exact(entry, {"part", "constraints", "dimensions"}) || !entry.at("part").is_string())
        return R::failure(error("Malformed Project child-Sketch owner record"));
      const DocumentId owner(entry.at("part").get<std::string>());
      auto constraints =
          deserialize_sketch_constraint_catalogs_from_json(entry.at("constraints").dump());
      if (!constraints)
        return R::failure(constraints.error());
      auto dimensions =
          deserialize_sketch_dimension_catalogs_from_json(entry.at("dimensions").dump());
      if (!dimensions)
        return R::failure(dimensions.error());
      auto canonical_constraints =
          serialize_sketch_constraint_catalogs_to_json(owner, constraints.value());
      if (!canonical_constraints)
        return R::failure(canonical_constraints.error());
      auto canonical_dimensions =
          serialize_sketch_dimension_catalogs_to_json(owner, dimensions.value());
      if (!canonical_dimensions)
        return R::failure(canonical_dimensions.error());
      if (json::parse(canonical_constraints.value()) != entry.at("constraints") ||
          json::parse(canonical_dimensions.value()) != entry.at("dimensions"))
        return R::failure(
            error("Child-Sketch catalogs require exact canonical fields and Part ownership"));
      groups.push_back({owner, std::move(constraints.value()), std::move(dimensions.value())});
    }
    auto valid = validate_project_part_sketch_catalogs(project, groups);
    if (!valid)
      return R::failure(valid.error());
    std::sort(groups.begin(), groups.end(),
              [](const auto& a, const auto& b) { return a.part.value() < b.part.value(); });
    return R::success(std::move(groups));
  } catch (const std::exception& e) {
    return R::failure(error(e.what()));
  }
}
} // namespace blcad::gui
