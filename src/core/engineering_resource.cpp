#include "blcad/core/engineering_resource.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace blcad {
namespace {
bool label_valid(std::string_view value) {
  return !value.empty() && std::none_of(value.begin(), value.end(), [](unsigned char c) {
    return c < 32 || c == 127;
  }) && std::any_of(value.begin(), value.end(), [](unsigned char c) { return !std::isspace(c); });
}
Error invalid(std::string id, std::string message) {
  return Error::validation(std::move(id), std::move(message));
}
bool provenance_valid(const EngineeringProvenance& value) {
  return bool(EngineeringProvenance::create(value.origin(), value.source(), value.reference(),
                                            value.uri(), value.license()));
}
bool header_valid(const EngineeringResourceKey& key, std::string_view name,
                  const EngineeringProvenance& provenance) {
  return bool(EngineeringResourceKey::create(key.id(), key.revision())) && label_valid(name) &&
         provenance_valid(provenance);
}
bool known_kind(EngineeringTableValueKind kind) {
  switch (kind) {
  case EngineeringTableValueKind::Length:
  case EngineeringTableValueKind::LinearDisplacement:
  case EngineeringTableValueKind::Angle:
  case EngineeringTableValueKind::Count:
  case EngineeringTableValueKind::Density:
  case EngineeringTableValueKind::Pressure:
  case EngineeringTableValueKind::Scalar:
  case EngineeringTableValueKind::Text:
  case EngineeringTableValueKind::Boolean:
    return true;
  }
  return false;
}
bool value_matches(EngineeringTableValueKind kind, const EngineeringTableValue& value) {
  const auto* quantity = std::get_if<Quantity>(&value);
  switch (kind) {
  case EngineeringTableValueKind::Length:
    return quantity && quantity->is_positive_length();
  case EngineeringTableValueKind::LinearDisplacement:
    return quantity && quantity->is_valid_linear_displacement();
  case EngineeringTableValueKind::Angle:
    return quantity && quantity->is_valid_angle();
  case EngineeringTableValueKind::Count:
    // The existing count accessor converts to size_t. Validate before any such
    // conversion, including a rounded double at that integer type's upper bound.
    return quantity && quantity->is_valid_count() &&
           quantity->millimeters() < std::ldexp(1.0, std::numeric_limits<std::size_t>::digits);
  case EngineeringTableValueKind::Density:
    return std::holds_alternative<MassDensity>(value);
  case EngineeringTableValueKind::Pressure:
    return std::holds_alternative<EngineeringPressure>(value);
  case EngineeringTableValueKind::Scalar:
    return std::holds_alternative<double>(value) && std::isfinite(std::get<double>(value));
  case EngineeringTableValueKind::Text:
    return std::holds_alternative<std::string>(value);
  case EngineeringTableValueKind::Boolean:
    return std::holds_alternative<bool>(value);
  }
  return false;
}
} // namespace

EngineeringResourceKey::EngineeringResourceKey(EngineeringResourceId id, std::string revision)
    : id_(std::move(id)), revision_(std::move(revision)) {}
Result<EngineeringResourceKey> EngineeringResourceKey::create(EngineeringResourceId id,
                                                              std::string revision) {
  if (!label_valid(id.value()) || !label_valid(revision))
    return Result<EngineeringResourceKey>::failure(invalid(
        "engineering.resource", "Resource id and revision must be nonempty single-line values"));
  return Result<EngineeringResourceKey>::success(
      EngineeringResourceKey(std::move(id), std::move(revision)));
}

EngineeringProvenance::EngineeringProvenance(EngineeringDataOrigin origin, std::string source,
                                             std::optional<std::string> reference,
                                             std::optional<std::string> uri,
                                             std::optional<std::string> license)
    : origin_(origin), source_(std::move(source)), reference_(std::move(reference)),
      uri_(std::move(uri)), license_(std::move(license)) {}
Result<EngineeringProvenance> EngineeringProvenance::create(EngineeringDataOrigin origin,
                                                            std::string source,
                                                            std::optional<std::string> reference,
                                                            std::optional<std::string> uri,
                                                            std::optional<std::string> license) {
  switch (origin) {
  case EngineeringDataOrigin::UserDefined:
  case EngineeringDataOrigin::Manufacturer:
  case EngineeringDataOrigin::Normative:
    break;
  default:
    return Result<EngineeringProvenance>::failure(
        invalid("engineering.provenance", "Unknown engineering data origin"));
  }
  if (!label_valid(source) || (reference && !label_valid(*reference)) ||
      (uri && !label_valid(*uri)) || (license && !label_valid(*license)))
    return Result<EngineeringProvenance>::failure(
        invalid("engineering.provenance",
                "Source and supplied provenance fields must be nonempty single-line values"));
  if (origin == EngineeringDataOrigin::Normative && !reference)
    return Result<EngineeringProvenance>::failure(
        invalid("engineering.provenance",
                "Normative data requires an explicit source reference or edition"));
  return Result<EngineeringProvenance>::success(EngineeringProvenance(
      origin, std::move(source), std::move(reference), std::move(uri), std::move(license)));
}

EngineeringMaterial::EngineeringMaterial(EngineeringResourceKey key, std::string name,
                                         EngineeringProvenance provenance,
                                         EngineeringMaterialProperties properties)
    : key_(std::move(key)), name_(std::move(name)), provenance_(std::move(provenance)),
      properties_(std::move(properties)) {}
Result<EngineeringMaterial> EngineeringMaterial::create(EngineeringResourceKey key,
                                                        std::string name,
                                                        EngineeringProvenance provenance,
                                                        EngineeringMaterialProperties properties) {
  const auto sourced_valid = [](const auto& value) {
    return !value || provenance_valid(value->provenance);
  };
  if (!header_valid(key, name, provenance) || !sourced_valid(properties.density) ||
      !sourced_valid(properties.young_modulus) || !sourced_valid(properties.yield_strength) ||
      !sourced_valid(properties.tensile_strength))
    return Result<EngineeringMaterial>::failure(invalid(
        key.id().value(), "Material requires a valid key, name and property/source provenance"));
  return Result<EngineeringMaterial>::success(EngineeringMaterial(
      std::move(key), std::move(name), std::move(provenance), std::move(properties)));
}

EngineeringStandardTable::EngineeringStandardTable(EngineeringResourceKey key, std::string name,
                                                   EngineeringProvenance provenance,
                                                   std::vector<EngineeringTableColumn> columns,
                                                   std::vector<EngineeringTableRow> rows)
    : key_(std::move(key)), name_(std::move(name)), provenance_(std::move(provenance)),
      columns_(std::move(columns)), rows_(std::move(rows)) {}
Result<EngineeringStandardTable> EngineeringStandardTable::create(
    EngineeringResourceKey key, std::string name, EngineeringProvenance provenance,
    std::vector<EngineeringTableColumn> columns, std::vector<EngineeringTableRow> rows) {
  const auto fail = [&](std::string message) {
    return Result<EngineeringStandardTable>::failure(invalid(key.id().value(), std::move(message)));
  };
  if (!header_valid(key, name, provenance) || columns.empty())
    return fail("A standards table requires a valid key, name, provenance and declared columns");
  std::unordered_set<std::string> column_keys;
  for (const auto& column : columns)
    if (!label_valid(column.key) || !label_valid(column.name) || !known_kind(column.kind) ||
        !column_keys.insert(column.key).second)
      return fail("Table columns require unique nonempty keys, names and known value kinds");
  std::unordered_set<std::string> row_keys;
  for (auto& row : rows) {
    if (!label_valid(row.key) || !label_valid(row.name) || !row_keys.insert(row.key).second)
      return fail("Table rows require unique nonempty keys and names");
    std::unordered_set<std::string> cell_keys;
    for (const auto& cell : row.cells) {
      const auto column = std::find_if(columns.begin(), columns.end(),
                                       [&](const auto& c) { return c.key == cell.column; });
      if (column == columns.end() || !cell_keys.insert(cell.column).second)
        return fail("Row " + row.key + " has an unknown or duplicate column " + cell.column);
      if (!value_matches(column->kind, cell.value) || !provenance_valid(cell.provenance))
        return fail("Row " + row.key + " column " + cell.column +
                    " has an incompatible value or unit");
    }
    for (const auto& column : columns)
      if (column.required && !cell_keys.contains(column.key))
        return fail("Row " + row.key + " is missing required column " + column.key);
    const auto position = [&](const auto& cell) {
      return std::find_if(columns.begin(), columns.end(),
                          [&](const auto& c) { return c.key == cell.column; }) -
             columns.begin();
    };
    std::sort(row.cells.begin(), row.cells.end(),
              [&](const auto& a, const auto& b) { return position(a) < position(b); });
  }
  return Result<EngineeringStandardTable>::success(EngineeringStandardTable(
      std::move(key), std::move(name), std::move(provenance), std::move(columns), std::move(rows)));
}
const EngineeringTableRow* EngineeringStandardTable::find_row(std::string_view key) const noexcept {
  const auto found =
      std::find_if(rows_.begin(), rows_.end(), [&](const auto& r) { return r.key == key; });
  return found == rows_.end() ? nullptr : &*found;
}
const EngineeringResourceKey&
engineering_resource_key(const EngineeringResource& resource) noexcept {
  return std::visit([](const auto& value) -> const EngineeringResourceKey& { return value.key(); },
                    resource);
}
const EngineeringResource*
EngineeringResourceCatalog::find(const EngineeringResourceKey& key) const noexcept {
  const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const auto& r) {
    return engineering_resource_key(r) == key;
  });
  return found == entries_.end() ? nullptr : &*found;
}
const EngineeringMaterial*
EngineeringResourceCatalog::find_material(const EngineeringResourceKey& key) const noexcept {
  const auto* entry = find(key);
  return entry ? std::get_if<EngineeringMaterial>(entry) : nullptr;
}
const EngineeringStandardTable*
EngineeringResourceCatalog::find_table(const EngineeringResourceKey& key) const noexcept {
  const auto* entry = find(key);
  return entry ? std::get_if<EngineeringStandardTable>(entry) : nullptr;
}
Result<std::size_t> EngineeringResourceCatalog::insert(EngineeringResource resource) {
  const bool valid = std::visit(
      [](const auto& value) {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, EngineeringMaterial>)
          return bool(EngineeringMaterial::create(value.key(), value.name(), value.provenance(),
                                                  value.properties()));
        else
          return bool(EngineeringStandardTable::create(
              value.key(), value.name(), value.provenance(), value.columns(), value.rows()));
      },
      resource);
  if (!valid)
    return Result<std::size_t>::failure(
        invalid("engineering.resource", "Invalid engineering resource snapshot"));
  const auto& key = engineering_resource_key(resource);
  for (const auto& entry : entries_) {
    const auto& existing = engineering_resource_key(entry);
    if (existing.id() == key.id() && entry.index() != resource.index())
      return Result<std::size_t>::failure(
          invalid(key.id().value(), "A resource id cannot change kind across revisions"));
    if (existing == key) {
      if (entry == resource)
        return Result<std::size_t>::success(0);
      return Result<std::size_t>::failure(
          invalid(key.id().value(),
                  "Conflicting immutable engineering resource revision " + key.revision()));
    }
  }
  entries_.push_back(std::move(resource));
  return Result<std::size_t>::success(1);
}
Result<std::size_t>
EngineeringResourceCatalog::merge_from(const EngineeringResourceCatalog& source) {
  auto candidate = *this;
  std::size_t added = 0;
  for (const auto& entry : source.entries_) {
    const auto inserted = candidate.insert(entry);
    if (!inserted)
      return Result<std::size_t>::failure(inserted.error());
    added += inserted.value();
  }
  *this = std::move(candidate);
  return Result<std::size_t>::success(added);
}
Result<std::size_t> EngineeringResourceCatalog::remove(const EngineeringResourceKey& key) {
  const auto valid = EngineeringResourceKey::create(key.id(), key.revision());
  if (!valid)
    return Result<std::size_t>::failure(valid.error());
  const auto owned_key = valid.value();
  return Result<std::size_t>::success(std::erase_if(
      entries_, [&](const auto& entry) { return engineering_resource_key(entry) == owned_key; }));
}
} // namespace blcad
