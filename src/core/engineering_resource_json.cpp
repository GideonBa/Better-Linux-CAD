#include "blcad/core/engineering_resource_json.hpp"
#include "engineering_resource_json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace blcad {
namespace {
using json = nlohmann::json;
Error error(std::string message) {
  return Error::validation("engineering_resource_json", std::move(message));
}
[[noreturn]] void fail(std::string message) {
  throw std::runtime_error(std::move(message));
}
template <typename T> T take(Result<T> value) {
  if (!value)
    fail(value.error().object_id() + ": " + value.error().message());
  return std::move(value.value());
}
void fields(const json& value, std::initializer_list<std::string_view> required,
            std::initializer_list<std::string_view> optional = {}) {
  if (!value.is_object())
    fail("Resource record must be an object");
  for (auto key : required)
    if (!value.contains(std::string(key)))
      fail("Missing resource field " + std::string(key));
  for (const auto& [key, unused] : value.items())
    if (std::find(required.begin(), required.end(), key) == required.end() &&
        std::find(optional.begin(), optional.end(), key) == optional.end())
      fail("Unknown resource field " + key);
}
std::string string(const json& value) {
  if (!value.is_string())
    fail("Resource string field has the wrong primitive type");
  return value.get<std::string>();
}
double number(const json& value) {
  if (!value.is_number())
    fail("Resource numeric field has the wrong primitive type");
  const auto result = value.get<double>();
  if (!std::isfinite(result))
    fail("Resource number must be finite");
  return result;
}
std::optional<std::string> optional_string(const json& value, const char* key) {
  return value.contains(key) ? std::optional(string(value.at(key))) : std::nullopt;
}
json provenance_json(const EngineeringProvenance& value) {
  const auto origin = value.origin() == EngineeringDataOrigin::UserDefined    ? "user_defined"
                      : value.origin() == EngineeringDataOrigin::Manufacturer ? "manufacturer"
                                                                              : "normative";
  json result{{"origin", origin}, {"source", value.source()}};
  if (value.reference())
    result["reference"] = *value.reference();
  if (value.uri())
    result["uri"] = *value.uri();
  if (value.license())
    result["license"] = *value.license();
  return result;
}
EngineeringProvenance provenance(const json& value) {
  fields(value, {"origin", "source"}, {"reference", "uri", "license"});
  const auto origin = string(value.at("origin"));
  EngineeringDataOrigin kind;
  if (origin == "user_defined")
    kind = EngineeringDataOrigin::UserDefined;
  else if (origin == "manufacturer")
    kind = EngineeringDataOrigin::Manufacturer;
  else if (origin == "normative")
    kind = EngineeringDataOrigin::Normative;
  else
    fail("Unknown engineering data origin " + origin);
  return take(EngineeringProvenance::create(
      kind, string(value.at("source")), optional_string(value, "reference"),
      optional_string(value, "uri"), optional_string(value, "license")));
}
using K = EngineeringTableValueKind;
const char* kind_name(K kind) {
  switch (kind) {
  case K::Length:
    return "length";
  case K::LinearDisplacement:
    return "linear_displacement";
  case K::Angle:
    return "angle";
  case K::Count:
    return "count";
  case K::Density:
    return "density";
  case K::Pressure:
    return "pressure";
  case K::Scalar:
    return "scalar";
  case K::Text:
    return "text";
  case K::Boolean:
    return "boolean";
  }
  fail("Unknown table value kind");
}
K table_kind(const json& value) {
  const auto name = string(value);
  for (auto kind : {K::Length, K::LinearDisplacement, K::Angle, K::Count, K::Density, K::Pressure,
                    K::Scalar, K::Text, K::Boolean})
    if (name == kind_name(kind))
      return kind;
  fail("Unknown table value kind " + name);
}
const char* unit(K kind) {
  switch (kind) {
  case K::Length:
  case K::LinearDisplacement:
    return "mm";
  case K::Angle:
    return "deg";
  case K::Count:
  case K::Scalar:
    return "1";
  case K::Density:
    return "kg/m3";
  case K::Pressure:
    return "Pa";
  case K::Text:
  case K::Boolean:
    return nullptr;
  }
  fail("Unknown table unit kind");
}
void require_unit(const json& value, const char* expected) {
  if (!value.contains("unit") || string(value.at("unit")) != expected)
    fail("Resource value requires canonical unit " + std::string(expected));
}
template <typename T> json property_json(const SourcedEngineeringValue<T>& value) {
  if constexpr (std::is_same_v<T, MassDensity>)
    return {{"value", value.value.kg_per_m3()},
            {"unit", "kg/m3"},
            {"provenance", provenance_json(value.provenance)}};
  else
    return {{"value", value.value.pascals()},
            {"unit", "Pa"},
            {"provenance", provenance_json(value.provenance)}};
}
std::optional<SourcedEngineeringValue<EngineeringPressure>> pressure(const json& props,
                                                                     const char* key) {
  if (!props.contains(key))
    return {};
  const auto& value = props.at(key);
  fields(value, {"value", "unit", "provenance"});
  require_unit(value, "Pa");
  return SourcedEngineeringValue{take(EngineeringPressure::from_pa(number(value.at("value")))),
                                 provenance(value.at("provenance"))};
}
EngineeringMaterial material(const json& value) {
  fields(value, {"kind", "id", "revision", "name", "provenance", "properties"});
  const auto& props = value.at("properties");
  fields(props, {}, {"density", "young_modulus", "yield_strength", "tensile_strength"});
  EngineeringMaterialProperties properties;
  if (props.contains("density")) {
    const auto& d = props.at("density");
    fields(d, {"value", "unit", "provenance"});
    require_unit(d, "kg/m3");
    properties.density = SourcedEngineeringValue{
        take(MassDensity::from_kg_per_m3(number(d.at("value")))), provenance(d.at("provenance"))};
  }
  properties.young_modulus = pressure(props, "young_modulus");
  properties.yield_strength = pressure(props, "yield_strength");
  properties.tensile_strength = pressure(props, "tensile_strength");
  return take(EngineeringMaterial::create(
      take(EngineeringResourceKey::create(EngineeringResourceId(string(value.at("id"))),
                                          string(value.at("revision")))),
      string(value.at("name")), provenance(value.at("provenance")), properties));
}
EngineeringTableValue cell_value(const json& value, K kind) {
  switch (kind) {
  case K::Length:
    return take(Quantity::length_mm(number(value)));
  case K::LinearDisplacement:
    return take(Quantity::linear_displacement_mm(number(value)));
  case K::Angle:
    return take(Quantity::angle_deg(number(value)));
  case K::Count: {
    if (!value.is_number_integer())
      fail("Table count must be a positive JSON integer");
    std::uint64_t integer = 0;
    if (value.is_number_unsigned())
      integer = value.get<std::uint64_t>();
    else {
      const auto signed_value = value.get<std::int64_t>();
      if (signed_value < 1)
        fail("Table count must be positive");
      integer = static_cast<std::uint64_t>(signed_value);
    }
    const double n = static_cast<double>(integer);
    if (n < 1 ||
        n >= std::ldexp(1.0, std::min(std::numeric_limits<std::size_t>::digits,
                                      std::numeric_limits<std::uint64_t>::digits)) ||
        static_cast<std::uint64_t>(n) != integer)
      fail("Table count cannot be represented exactly by the count API");
    return take(Quantity::count(n));
  }
  case K::Density:
    return take(MassDensity::from_kg_per_m3(number(value)));
  case K::Pressure:
    return take(EngineeringPressure::from_pa(number(value)));
  case K::Scalar:
    return number(value);
  case K::Text:
    return string(value);
  case K::Boolean:
    if (!value.is_boolean())
      fail("Table boolean has the wrong primitive type");
    return value.get<bool>();
  }
  fail("Unknown table value kind");
}
json cell_json(const EngineeringTableValue& value, K kind) {
  switch (kind) {
  case K::Length:
  case K::LinearDisplacement:
    return std::get<Quantity>(value).millimeters();
  case K::Angle:
    return std::get<Quantity>(value).degrees();
  case K::Count:
    return std::get<Quantity>(value).count_value();
  case K::Density:
    return std::get<MassDensity>(value).kg_per_m3();
  case K::Pressure:
    return std::get<EngineeringPressure>(value).pascals();
  case K::Scalar:
    return std::get<double>(value);
  case K::Text:
    return std::get<std::string>(value);
  case K::Boolean:
    return std::get<bool>(value);
  }
  fail("Unknown table cell kind");
}
EngineeringStandardTable table(const json& value) {
  fields(value, {"kind", "id", "revision", "name", "provenance", "columns", "rows"});
  if (!value.at("columns").is_array() || !value.at("rows").is_array())
    fail("Table columns and rows must be arrays");
  std::vector<EngineeringTableColumn> columns;
  for (const auto& c : value.at("columns")) {
    fields(c, {"key", "name", "kind", "required"}, {"unit"});
    const auto kind = table_kind(c.at("kind"));
    if (const auto* expected = unit(kind))
      require_unit(c, expected);
    else if (c.contains("unit"))
      fail("Text and boolean table columns do not have units");
    if (!c.at("required").is_boolean())
      fail("Column required flag must be boolean");
    columns.push_back(
        {string(c.at("key")), string(c.at("name")), kind, c.at("required").get<bool>()});
  }
  std::vector<EngineeringTableRow> rows;
  for (const auto& r : value.at("rows")) {
    fields(r, {"key", "name", "cells"});
    if (!r.at("cells").is_array())
      fail("Table cells must be an array");
    EngineeringTableRow row{string(r.at("key")), string(r.at("name")), {}};
    for (const auto& cell : r.at("cells")) {
      fields(cell, {"column", "value", "provenance"});
      const auto column = string(cell.at("column"));
      const auto found = std::find_if(columns.begin(), columns.end(),
                                      [&](const auto& c) { return c.key == column; });
      if (found == columns.end())
        fail("Unknown table cell column " + column);
      row.cells.push_back(
          {column, cell_value(cell.at("value"), found->kind), provenance(cell.at("provenance"))});
    }
    rows.push_back(std::move(row));
  }
  return take(EngineeringStandardTable::create(
      take(EngineeringResourceKey::create(EngineeringResourceId(string(value.at("id"))),
                                          string(value.at("revision")))),
      string(value.at("name")), provenance(value.at("provenance")), std::move(columns),
      std::move(rows)));
}
} // namespace

namespace detail {
void require_engineering_json_fields(const json& value,
                                     std::initializer_list<std::string_view> required,
                                     std::initializer_list<std::string_view> optional) {
  fields(value, required, optional);
}
json engineering_resource_key_to_json(const EngineeringResourceKey& key) {
  return {{"id", key.id().value()}, {"revision", key.revision()}};
}
Result<EngineeringResourceKey> engineering_resource_key_from_json(const json& value) {
  try {
    fields(value, {"id", "revision"});
    return EngineeringResourceKey::create(EngineeringResourceId(string(value.at("id"))),
                                          string(value.at("revision")));
  } catch (const std::exception& e) {
    return Result<EngineeringResourceKey>::failure(error(e.what()));
  }
}
json engineering_resources_to_json(const EngineeringResourceCatalog& catalog) {
  json entries = json::array();
  for (const auto& entry : catalog.entries()) {
    std::visit(
        [&](const auto& resource) {
          json result{{"id", resource.key().id().value()},
                      {"revision", resource.key().revision()},
                      {"name", resource.name()},
                      {"provenance", provenance_json(resource.provenance())}};
          if constexpr (std::is_same_v<std::decay_t<decltype(resource)>, EngineeringMaterial>) {
            result["kind"] = "material";
            auto props = json::object();
            const auto& p = resource.properties();
            if (p.density)
              props["density"] = property_json(*p.density);
            if (p.young_modulus)
              props["young_modulus"] = property_json(*p.young_modulus);
            if (p.yield_strength)
              props["yield_strength"] = property_json(*p.yield_strength);
            if (p.tensile_strength)
              props["tensile_strength"] = property_json(*p.tensile_strength);
            result["properties"] = std::move(props);
          } else {
            result["kind"] = "standard_table";
            result["columns"] = json::array();
            for (const auto& c : resource.columns()) {
              json col{{"key", c.key},
                       {"name", c.name},
                       {"kind", kind_name(c.kind)},
                       {"required", c.required}};
              if (const auto* u = unit(c.kind))
                col["unit"] = u;
              result["columns"].push_back(std::move(col));
            }
            result["rows"] = json::array();
            for (const auto& r : resource.rows()) {
              json cells = json::array();
              for (const auto& c : r.cells) {
                const auto column =
                    std::find_if(resource.columns().begin(), resource.columns().end(),
                                 [&](const auto& col) { return col.key == c.column; });
                cells.push_back({{"column", c.column},
                                 {"value", cell_json(c.value, column->kind)},
                                 {"provenance", provenance_json(c.provenance)}});
              }
              result["rows"].push_back(
                  {{"key", r.key}, {"name", r.name}, {"cells", std::move(cells)}});
            }
          }
          entries.push_back(std::move(result));
        },
        entry);
  }
  return {{"schema", "blcad.engineering_resources.mvp1"},
          {"version", 1},
          {"entries", std::move(entries)}};
}
Result<EngineeringResourceCatalog> engineering_resources_from_json(const json& value) {
  try {
    fields(value, {"schema", "version", "entries"});
    if (string(value.at("schema")) != "blcad.engineering_resources.mvp1" ||
        !value.at("version").is_number_integer() || value.at("version") != 1)
      fail("Unsupported engineering resource schema or version");
    if (!value.at("entries").is_array())
      fail("Engineering resource entries must be an array");
    EngineeringResourceCatalog catalog;
    for (const auto& e : value.at("entries")) {
      const auto kind = string(e.at("kind"));
      EngineeringResource resource =
          kind == "material" ? EngineeringResource(material(e))
          : kind == "standard_table"
              ? EngineeringResource(table(e))
              : throw std::runtime_error("Unknown engineering resource kind " + kind);
      if (take(catalog.insert(std::move(resource))) != 1)
        fail("Duplicate resource revision in catalog JSON");
    }
    return Result<EngineeringResourceCatalog>::success(std::move(catalog));
  } catch (const std::exception& e) {
    return Result<EngineeringResourceCatalog>::failure(error(e.what()));
  }
}
} // namespace detail
Result<std::string>
serialize_engineering_resource_catalog_to_json(const EngineeringResourceCatalog& catalog) {
  try {
    return Result<std::string>::success(detail::engineering_resources_to_json(catalog).dump(2));
  } catch (const std::exception& e) {
    return Result<std::string>::failure(error(e.what()));
  }
}
Result<EngineeringResourceCatalog>
deserialize_engineering_resource_catalog_from_json(std::string_view content) {
  try {
    return detail::engineering_resources_from_json(json::parse(content.begin(), content.end()));
  } catch (const std::exception& e) {
    return Result<EngineeringResourceCatalog>::failure(error(e.what()));
  }
}
} // namespace blcad
