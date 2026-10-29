#pragma once

#include "blcad/core/imported_topology.hpp"
#include "blcad/geometry/step_import_reader.hpp"

class TopoDS_Shape;

namespace blcad::geometry {

struct GeometryAffineTransform;

// Derived current-source evidence. Runtime product/body indices only locate the recovered
// definition for inspection/occurrence presentation; neither index is persistent identity.
struct ImportedTopologyResolution {
  ImportedTopologyId id;
  ImportedTopologyRecoveryStatus status{ImportedTopologyRecoveryStatus::Lost};
  std::size_t candidate_count{};
  std::string diagnostic;
  std::optional<std::size_t> product_index;
  std::optional<std::size_t> body_index;
  GeometryShape shape;
  std::optional<ImportedTopologyDescriptor> descriptor;
  std::vector<ImportedTopologyCapability> capabilities;
};

struct ImportedTopologyRecovery {
  ImportedTopologyCatalogId catalog;
  StepImportSourceId source;
  std::string current_sha256_digest;
  std::vector<ImportedTopologyResolution> entries;

  [[nodiscard]] const ImportedTopologyResolution* find(ImportedTopologyId id) const noexcept;
};

class StepImportTopology {
public:
  // Capture exactly one leaf product definition. BLCAD ids are allocated once after canonical
  // normalized descriptor sorting. Names and STEP/XDE/kernel indices never enter identity.
  [[nodiscard]] Result<ImportedTopologyCatalog> create_catalog(ImportedTopologyCatalogId id,
                                                               const StepImportGeometry& imported,
                                                               std::size_t product_index) const;

  // Current geometry must come from a verified reader snapshot for the same source id. Digest
  // changes are permitted after explicit source refresh; the catalog baseline remains unchanged.
  // Bodies recover by exact whole-body evidence or unique consistent surviving trimmed faces.
  [[nodiscard]] Result<ImportedTopologyRecovery> recover(const ImportedTopologyCatalog& catalog,
                                                         const StepImportGeometry& imported) const;

  // Require a unique current resolution and the explicitly requested capability. Unsupported
  // analytic surfaces/curves retain only their explicit generic topology capability.
  [[nodiscard]] Result<ImportedTopologyResolution>
  resolve(const ImportedTopologyRecovery& recovery,
          const ImportedTopologyReference& reference) const;

  // Match the same complete current witness on the actual downstream target body.
  [[nodiscard]] Result<ImportedTopologyResolution>
  match_subshape(const ImportedTopologyDescriptor& current, const TopoDS_Shape& target) const;
  // Recompute the full normalized witness after an explicit authored Body placement.
  // This never assigns identity by proximity or a primitive-only approximation.
  [[nodiscard]] Result<ImportedTopologyDescriptor>
  placed_descriptor(const GeometryShape& current, const GeometryAffineTransform& placement) const;
};

} // namespace blcad::geometry
