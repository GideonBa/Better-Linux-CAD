#pragma once
#include "blcad/core/imported_topology.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace blcad::detail {
inline Error imported_json_error() {
  return Error::validation(
      "imported_topology_json",
      "imported topology requires exact typed canonical catalog/reference records");
}
inline bool imported_fields(const nlohmann::json& value,
                            std::initializer_list<const char*> fields) {
  return value.is_object() && value.size() == fields.size() &&
         std::all_of(fields.begin(), fields.end(),
                     [&](const char* key) { return value.contains(key); });
}
template <typename E>
std::optional<E> imported_enum(const nlohmann::json& value, std::initializer_list<E> values) {
  if (!value.is_string())
    return {};
  for (const auto candidate : values)
    if (value.get<std::string>() == to_string(candidate))
      return candidate;
  return {};
}
inline nlohmann::json imported_point(Point3 p) {
  return {{"x", p.x}, {"y", p.y}, {"z", p.z}};
}
inline bool imported_point_valid(const nlohmann::json& p) {
  return imported_fields(p, {"x", "y", "z"}) && p.at("x").is_number() && p.at("y").is_number() &&
         p.at("z").is_number();
}
inline Point3 imported_point_value(const nlohmann::json& p) {
  return {p.at("x").get<double>(), p.at("y").get<double>(), p.at("z").get<double>()};
}
inline nlohmann::json imported_reference_to_json(const ImportedTopologyReference& ref) {
  return {{"catalog", ref.catalog().value()},
          {"entry", ref.entry().value()},
          {"capability", to_string(ref.capability())}};
}
inline Result<ImportedTopologyReference> imported_reference_from_json(const nlohmann::json& value) {
  if (!imported_fields(value, {"catalog", "entry", "capability"}) ||
      !value.at("catalog").is_string() || !value.at("entry").is_string())
    return Result<ImportedTopologyReference>::failure(imported_json_error());
  const auto capability = imported_enum<ImportedTopologyCapability>(
      value.at("capability"),
      {ImportedTopologyCapability::Body, ImportedTopologyCapability::Face,
       ImportedTopologyCapability::Edge, ImportedTopologyCapability::Plane,
       ImportedTopologyCapability::Cylinder, ImportedTopologyCapability::Axis,
       ImportedTopologyCapability::Line, ImportedTopologyCapability::Circle,
       ImportedTopologyCapability::Point});
  if (!capability)
    return Result<ImportedTopologyReference>::failure(imported_json_error());
  return ImportedTopologyReference::create(
      ImportedTopologyCatalogId(value.at("catalog").get<std::string>()),
      ImportedTopologyId(value.at("entry").get<std::string>()), *capability);
}
inline nlohmann::json imported_descriptor_to_json(const ImportedTopologyDescriptor& d) {
  auto samples = nlohmann::json::array();
  for (auto p : d.samples)
    samples.push_back(imported_point(p));
  return {{"kind", to_string(d.kind)},
          {"geometry", to_string(d.geometry)},
          {"body_kind",
           d.body_kind ? nlohmann::json(to_string(*d.body_kind)) : nlohmann::json(nullptr)},
          {"centroid", imported_point(d.centroid)},
          {"bounds_mm", d.bounds_mm},
          {"measure", d.measure},
          {"origin", imported_point(d.origin)},
          {"direction", imported_point({d.direction.x, d.direction.y, d.direction.z})},
          {"radius_mm", d.radius_mm},
          {"samples", std::move(samples)},
          {"boundary_keys", d.boundary_keys},
          {"geometry_key", d.geometry_key}};
}
inline Result<ImportedTopologyDescriptor>
imported_descriptor_from_json(const nlohmann::json& value) {
  const auto fail = [] {
    return Result<ImportedTopologyDescriptor>::failure(imported_json_error());
  };
  if (!imported_fields(value, {"kind", "geometry", "body_kind", "centroid", "bounds_mm", "measure",
                               "origin", "direction", "radius_mm", "samples", "boundary_keys",
                               "geometry_key"}) ||
      !imported_point_valid(value.at("centroid")) || !imported_point_valid(value.at("origin")) ||
      !imported_point_valid(value.at("direction")) || !value.at("bounds_mm").is_array() ||
      value.at("bounds_mm").size() != 6 || !value.at("measure").is_number() ||
      !value.at("radius_mm").is_number() || !value.at("samples").is_array() ||
      !value.at("boundary_keys").is_array() || !value.at("geometry_key").is_string())
    return fail();
  const auto kind = imported_enum<ImportedTopologyKind>(
      value.at("kind"), {ImportedTopologyKind::Body, ImportedTopologyKind::Face,
                         ImportedTopologyKind::Edge, ImportedTopologyKind::Vertex});
  const auto geometry = imported_enum<ImportedGeometryClass>(
      value.at("geometry"),
      {ImportedGeometryClass::Opaque, ImportedGeometryClass::Plane, ImportedGeometryClass::Cylinder,
       ImportedGeometryClass::Line, ImportedGeometryClass::Circle, ImportedGeometryClass::Point});
  if (!kind || !geometry)
    return fail();
  ImportedTopologyDescriptor result;
  result.kind = *kind;
  result.geometry = *geometry;
  if (!value.at("body_kind").is_null()) {
    auto body =
        imported_enum<BodyKind>(value.at("body_kind"), {BodyKind::Solid, BodyKind::Surface});
    if (!body)
      return fail();
    result.body_kind = *body;
  }
  result.centroid = imported_point_value(value.at("centroid"));
  result.origin = imported_point_value(value.at("origin"));
  const auto direction = imported_point_value(value.at("direction"));
  result.direction = {direction.x, direction.y, direction.z};
  for (std::size_t i = 0; i < result.bounds_mm.size(); ++i) {
    if (!value.at("bounds_mm").at(i).is_number())
      return fail();
    result.bounds_mm[i] = value.at("bounds_mm").at(i).get<double>();
  }
  result.measure = value.at("measure").get<double>();
  result.radius_mm = value.at("radius_mm").get<double>();
  for (const auto& p : value.at("samples")) {
    if (!imported_point_valid(p))
      return fail();
    result.samples.push_back(imported_point_value(p));
  }
  for (const auto& key : value.at("boundary_keys")) {
    if (!key.is_string())
      return fail();
    result.boundary_keys.push_back(key.get<std::string>());
  }
  result.geometry_key = value.at("geometry_key").get<std::string>();
  return Result<ImportedTopologyDescriptor>::success(std::move(result));
}
inline nlohmann::json
imported_catalogs_to_json(const std::vector<ImportedTopologyCatalog>& catalogs) {
  auto values = nlohmann::json::array();
  for (const auto& catalog : catalogs) {
    auto entries = nlohmann::json::array();
    for (const auto& entry : catalog.entries())
      entries.push_back(
          {{"id", entry.id.value()},
           {"parent_body", entry.parent_body ? nlohmann::json(entry.parent_body->value())
                                             : nlohmann::json(nullptr)},
           {"descriptor", imported_descriptor_to_json(entry.descriptor)}});
    values.push_back({{"id", catalog.id().value()},
                      {"source", catalog.source().value()},
                      {"captured_sha256_digest", catalog.captured_sha256_digest()},
                      {"product_key", catalog.product_key()},
                      {"descriptor_version", ImportedTopologyCatalog::descriptor_version},
                      {"entries", std::move(entries)}});
  }
  return values;
}
template <typename Host>
Result<std::size_t> load_imported_catalogs(const nlohmann::json& root, Host& host) {
  if (!root.contains("imported_topology_catalogs"))
    return Result<std::size_t>::success(0);
  const auto fail = [] { return Result<std::size_t>::failure(imported_json_error()); };
  const auto& catalogs = root.at("imported_topology_catalogs");
  if (!catalogs.is_array())
    return fail();
  for (const auto& value : catalogs) {
    if (!imported_fields(value, {"id", "source", "captured_sha256_digest", "product_key",
                                 "descriptor_version", "entries"}) ||
        !value.at("id").is_string() || !value.at("source").is_string() ||
        !value.at("captured_sha256_digest").is_string() || !value.at("product_key").is_string() ||
        !value.at("descriptor_version").is_number_integer() ||
        value.at("descriptor_version") != ImportedTopologyCatalog::descriptor_version ||
        !value.at("entries").is_array())
      return fail();
    std::vector<ImportedTopologyEntry> entries;
    for (const auto& entry : value.at("entries")) {
      if (!imported_fields(entry, {"id", "parent_body", "descriptor"}) ||
          !entry.at("id").is_string() ||
          (!entry.at("parent_body").is_null() && !entry.at("parent_body").is_string()))
        return fail();
      auto descriptor = imported_descriptor_from_json(entry.at("descriptor"));
      if (!descriptor)
        return Result<std::size_t>::failure(descriptor.error());
      entries.push_back(
          {ImportedTopologyId(entry.at("id").get<std::string>()),
           entry.at("parent_body").is_null()
               ? std::nullopt
               : std::optional(ImportedTopologyId(entry.at("parent_body").get<std::string>())),
           std::move(descriptor.value())});
    }
    auto catalog = ImportedTopologyCatalog::create(
        ImportedTopologyCatalogId(value.at("id").get<std::string>()),
        StepImportSourceId(value.at("source").get<std::string>()),
        value.at("captured_sha256_digest").get<std::string>(),
        value.at("product_key").get<std::string>(), std::move(entries));
    if (!catalog)
      return Result<std::size_t>::failure(catalog.error());
    auto added = host.add_imported_topology_catalog(std::move(catalog.value()));
    if (!added)
      return added;
  }
  return Result<std::size_t>::success(catalogs.size());
}
} // namespace blcad::detail
