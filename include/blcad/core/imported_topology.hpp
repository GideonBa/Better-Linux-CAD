#pragma once

#include "blcad/core/body.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

class PartDocument;

enum class ImportedTopologyKind { Body, Face, Edge, Vertex };
enum class ImportedGeometryClass { Opaque, Plane, Cylinder, Line, Circle, Point };
enum class ImportedTopologyCapability {
  Body,
  Face,
  Edge,
  Plane,
  Cylinder,
  Axis,
  Line,
  Circle,
  Point
};
enum class ImportedTopologyRecoveryStatus { Resolved, Lost, Ambiguous };

[[nodiscard]] std::string_view to_string(ImportedTopologyKind value) noexcept;
[[nodiscard]] std::string_view to_string(ImportedGeometryClass value) noexcept;
[[nodiscard]] std::string_view to_string(ImportedTopologyCapability value) noexcept;
[[nodiscard]] std::string_view to_string(ImportedTopologyRecoveryStatus value) noexcept;

// Version-1 recovery evidence in product-local millimeters. No kernel object, traversal ordinal,
// STEP entity number, XDE label or source payload is model intent. Body measure is volume for a
// solid and area for a surface; face/edge measures are area/length; vertices have measure zero.
// Canonical directions have unit length and their first nonzero component positive. Plane/line/
// cylinder origins use the closest point to the coordinate origin; circles retain their center.
// Boundary keys encode normalized complete trimmed boundaries/incidence, never neighboring ids.
struct ImportedTopologyDescriptor {
  ImportedTopologyKind kind{ImportedTopologyKind::Vertex};
  ImportedGeometryClass geometry{ImportedGeometryClass::Point};
  std::optional<BodyKind> body_kind;
  Point3 centroid;
  std::array<double, 6> bounds_mm{};
  double measure{};
  Point3 origin;
  Vector3 direction;
  double radius_mm{};
  std::vector<Point3> samples;
  std::vector<std::string> boundary_keys;
  // Canonical normalized curve/surface/topology equation witness, lowercase SHA-256.
  // It is recovery evidence, never the persistent selection identity.
  std::string geometry_key;
  friend bool operator==(const ImportedTopologyDescriptor&,
                         const ImportedTopologyDescriptor&) = default;
};

struct ImportedTopologyEntry {
  ImportedTopologyId id;
  // Required for Face/Edge/Vertex; absent for Body. Always a Body id in this same catalog.
  std::optional<ImportedTopologyId> parent_body;
  ImportedTopologyDescriptor descriptor;
  friend bool operator==(const ImportedTopologyEntry&, const ImportedTopologyEntry&) = default;
};

class ImportedTopologyCatalog {
public:
  static constexpr int descriptor_version = 1;
  // One catalog owns exactly one selected source product definition. The baseline product key
  // and digest describe capture evidence; explicit source refresh preserves these and all ids.
  [[nodiscard]] static Result<ImportedTopologyCatalog>
  create(ImportedTopologyCatalogId id, StepImportSourceId source,
         std::string captured_sha256_digest, std::string product_key,
         std::vector<ImportedTopologyEntry> entries);
  [[nodiscard]] const ImportedTopologyCatalogId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const StepImportSourceId& source() const noexcept {
    return source_;
  }
  [[nodiscard]] const std::string& captured_sha256_digest() const noexcept {
    return digest_;
  }
  [[nodiscard]] const std::string& product_key() const noexcept {
    return product_key_;
  }
  [[nodiscard]] const std::vector<ImportedTopologyEntry>& entries() const noexcept {
    return entries_;
  }
  [[nodiscard]] const ImportedTopologyEntry* find(ImportedTopologyId id) const noexcept;
  [[nodiscard]] std::string node_id() const;
  friend bool operator==(const ImportedTopologyCatalog&, const ImportedTopologyCatalog&) = default;

private:
  ImportedTopologyCatalog(ImportedTopologyCatalogId id, StepImportSourceId source,
                          std::string captured_sha256_digest, std::string product_key,
                          std::vector<ImportedTopologyEntry> entries);
  ImportedTopologyCatalogId id_;
  StepImportSourceId source_;
  std::string digest_, product_key_;
  std::vector<ImportedTopologyEntry> entries_;
};

class ImportedTopologyReference {
public:
  [[nodiscard]] static Result<ImportedTopologyReference>
  create(ImportedTopologyCatalogId catalog, ImportedTopologyId entry,
         ImportedTopologyCapability capability);
  [[nodiscard]] const ImportedTopologyCatalogId& catalog() const noexcept {
    return catalog_;
  }
  [[nodiscard]] const ImportedTopologyId& entry() const noexcept {
    return entry_;
  }
  [[nodiscard]] ImportedTopologyCapability capability() const noexcept {
    return capability_;
  }
  [[nodiscard]] std::string source_node_id() const;
  friend bool operator==(const ImportedTopologyReference&,
                         const ImportedTopologyReference&) = default;

private:
  ImportedTopologyReference(ImportedTopologyCatalogId catalog, ImportedTopologyId entry,
                            ImportedTopologyCapability capability);
  ImportedTopologyCatalogId catalog_;
  ImportedTopologyId entry_;
  ImportedTopologyCapability capability_;
};

[[nodiscard]] std::vector<ImportedTopologyCapability>
imported_topology_capabilities(const ImportedTopologyDescriptor& descriptor);
[[nodiscard]] Result<std::size_t>
validate_imported_topology_reference(const PartDocument& document,
                                     const ImportedTopologyReference& reference);

// Disjoint canonical Assembly target grammar with uppercase percent escapes for arbitrary id
// bytes: imported:<catalog-id>:<entry-id>:<capability>. Literal '.', ':' and '%' bytes are escaped.
[[nodiscard]] Result<std::string>
make_imported_topology_target_spelling(const ImportedTopologyReference& reference);
[[nodiscard]] Result<ImportedTopologyReference>
parse_imported_topology_target_spelling(std::string_view spelling);
[[nodiscard]] bool is_imported_topology_target_spelling(std::string_view spelling) noexcept;

} // namespace blcad
