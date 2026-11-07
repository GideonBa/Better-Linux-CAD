#pragma once

#include "blcad/core/engineering_quantity.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace blcad {

struct EngineeringResourceIdTag;
using EngineeringResourceId = TypedId<EngineeringResourceIdTag>;

class EngineeringResourceKey {
public:
  [[nodiscard]] static Result<EngineeringResourceKey> create(EngineeringResourceId id,
                                                             std::string revision);
  [[nodiscard]] const EngineeringResourceId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& revision() const noexcept {
    return revision_;
  }
  friend bool operator==(const EngineeringResourceKey&, const EngineeringResourceKey&) = default;

private:
  EngineeringResourceKey(EngineeringResourceId id, std::string revision);
  EngineeringResourceId id_;
  std::string revision_;
};

enum class EngineeringDataOrigin { UserDefined, Manufacturer, Normative };

class EngineeringProvenance {
public:
  [[nodiscard]] static Result<EngineeringProvenance>
  create(EngineeringDataOrigin origin, std::string source,
         std::optional<std::string> reference = {}, std::optional<std::string> uri = {},
         std::optional<std::string> license = {});
  [[nodiscard]] EngineeringDataOrigin origin() const noexcept {
    return origin_;
  }
  [[nodiscard]] const std::string& source() const noexcept {
    return source_;
  }
  [[nodiscard]] const std::optional<std::string>& reference() const noexcept {
    return reference_;
  }
  [[nodiscard]] const std::optional<std::string>& uri() const noexcept {
    return uri_;
  }
  [[nodiscard]] const std::optional<std::string>& license() const noexcept {
    return license_;
  }
  friend bool operator==(const EngineeringProvenance&, const EngineeringProvenance&) = default;

private:
  EngineeringProvenance(EngineeringDataOrigin origin, std::string source,
                        std::optional<std::string> reference, std::optional<std::string> uri,
                        std::optional<std::string> license);
  EngineeringDataOrigin origin_;
  std::string source_;
  std::optional<std::string> reference_, uri_, license_;
};

template <typename T> struct SourcedEngineeringValue {
  T value;
  EngineeringProvenance provenance;
  friend bool operator==(const SourcedEngineeringValue&, const SourcedEngineeringValue&) = default;
};

struct EngineeringMaterialProperties {
  std::optional<SourcedEngineeringValue<MassDensity>> density;
  std::optional<SourcedEngineeringValue<EngineeringPressure>> young_modulus;
  std::optional<SourcedEngineeringValue<EngineeringPressure>> yield_strength;
  std::optional<SourcedEngineeringValue<EngineeringPressure>> tensile_strength;
  friend bool operator==(const EngineeringMaterialProperties&,
                         const EngineeringMaterialProperties&) = default;
};

class EngineeringMaterial {
public:
  [[nodiscard]] static Result<EngineeringMaterial>
  create(EngineeringResourceKey key, std::string name, EngineeringProvenance provenance,
         EngineeringMaterialProperties properties = {});
  [[nodiscard]] const EngineeringResourceKey& key() const noexcept {
    return key_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const EngineeringProvenance& provenance() const noexcept {
    return provenance_;
  }
  [[nodiscard]] const EngineeringMaterialProperties& properties() const noexcept {
    return properties_;
  }
  friend bool operator==(const EngineeringMaterial&, const EngineeringMaterial&) = default;

private:
  EngineeringMaterial(EngineeringResourceKey key, std::string name,
                      EngineeringProvenance provenance, EngineeringMaterialProperties properties);
  EngineeringResourceKey key_;
  std::string name_;
  EngineeringProvenance provenance_;
  EngineeringMaterialProperties properties_;
};

enum class EngineeringTableValueKind {
  Length,
  LinearDisplacement,
  Angle,
  Count,
  Density,
  Pressure,
  Scalar,
  Text,
  Boolean
};
using EngineeringTableValue =
    std::variant<Quantity, MassDensity, EngineeringPressure, double, std::string, bool>;
struct EngineeringTableColumn {
  std::string key;
  std::string name;
  EngineeringTableValueKind kind;
  bool required = true;
  friend bool operator==(const EngineeringTableColumn&, const EngineeringTableColumn&) = default;
};
struct EngineeringTableCell {
  std::string column;
  EngineeringTableValue value;
  EngineeringProvenance provenance;
  friend bool operator==(const EngineeringTableCell&, const EngineeringTableCell&) = default;
};
struct EngineeringTableRow {
  std::string key;
  std::string name;
  std::vector<EngineeringTableCell> cells;
  friend bool operator==(const EngineeringTableRow&, const EngineeringTableRow&) = default;
};
class EngineeringStandardTable {
public:
  [[nodiscard]] static Result<EngineeringStandardTable>
  create(EngineeringResourceKey key, std::string name, EngineeringProvenance provenance,
         std::vector<EngineeringTableColumn> columns, std::vector<EngineeringTableRow> rows);
  [[nodiscard]] const EngineeringResourceKey& key() const noexcept {
    return key_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const EngineeringProvenance& provenance() const noexcept {
    return provenance_;
  }
  [[nodiscard]] const std::vector<EngineeringTableColumn>& columns() const noexcept {
    return columns_;
  }
  [[nodiscard]] const std::vector<EngineeringTableRow>& rows() const noexcept {
    return rows_;
  }
  [[nodiscard]] const EngineeringTableRow* find_row(std::string_view key) const noexcept;
  friend bool operator==(const EngineeringStandardTable&,
                         const EngineeringStandardTable&) = default;

private:
  EngineeringStandardTable(EngineeringResourceKey key, std::string name,
                           EngineeringProvenance provenance,
                           std::vector<EngineeringTableColumn> columns,
                           std::vector<EngineeringTableRow> rows);
  EngineeringResourceKey key_;
  std::string name_;
  EngineeringProvenance provenance_;
  std::vector<EngineeringTableColumn> columns_;
  std::vector<EngineeringTableRow> rows_;
};

using EngineeringResource = std::variant<EngineeringMaterial, EngineeringStandardTable>;
[[nodiscard]] const EngineeringResourceKey&
engineering_resource_key(const EngineeringResource&) noexcept;

class EngineeringResourceCatalog {
public:
  [[nodiscard]] const std::vector<EngineeringResource>& entries() const noexcept {
    return entries_;
  }
  [[nodiscard]] const EngineeringResource* find(const EngineeringResourceKey& key) const noexcept;
  [[nodiscard]] const EngineeringMaterial*
  find_material(const EngineeringResourceKey& key) const noexcept;
  [[nodiscard]] const EngineeringStandardTable*
  find_table(const EngineeringResourceKey& key) const noexcept;
  // Returns 0 for an identical existing revision, 1 for a new revision. A
  // conflicting key or resource kind leaves the catalog unchanged.
  [[nodiscard]] Result<std::size_t> insert(EngineeringResource resource);
  [[nodiscard]] Result<std::size_t> merge_from(const EngineeringResourceCatalog& source);
  friend bool operator==(const EngineeringResourceCatalog&,
                         const EngineeringResourceCatalog&) = default;

private:
  std::vector<EngineeringResource> entries_;
};

} // namespace blcad
