#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"
#include "blcad/geometry/step_import_topology.hpp"

#include <filesystem>
#include <memory>

namespace blcad::geometry {

// One operation's derived import context. The caller supplies the containing document's asset
// root explicitly; no machine-local path is inferred from persistent Part or Assembly identity.
// Reader snapshots are shared by references to the same owned source. Changed source/catalog
// intent requires a new context, and publication must validate all observed source bytes again.
class StepImportTopologyContext {
public:
  explicit StepImportTopologyContext(std::filesystem::path root_directory);
  ~StepImportTopologyContext();
  StepImportTopologyContext(StepImportTopologyContext&&) noexcept;
  StepImportTopologyContext& operator=(StepImportTopologyContext&&) noexcept;
  StepImportTopologyContext(const StepImportTopologyContext&) = delete;
  StepImportTopologyContext& operator=(const StepImportTopologyContext&) = delete;

  [[nodiscard]] Result<const ImportedTopologyRecovery*> recover(const PartDocument& document,
                                                                ImportedTopologyCatalogId catalog);
  [[nodiscard]] Result<ImportedTopologyResolution>
  resolve(const PartDocument& document, const ImportedTopologyReference& reference);
  [[nodiscard]] Result<std::size_t> validate_sources() const;
  [[nodiscard]] const std::filesystem::path& root_directory() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Observe actual imported result authorities, including a converted editable base. Opaque source
// registration alone does not imply that external geometry participates in this operation.
[[nodiscard]] Result<std::size_t> observe_imported_part_sources(const PartDocument& document,
                                                                StepImportTopologyContext* context);

// Canonical analytic frames are derived from the recovered normalized descriptor. Generic
// topology has no analytic fallback; it cannot be approximated as a plane, line or cylinder.
[[nodiscard]] Result<AssemblyGeometricTargetDescriptor>
imported_topology_analytic_descriptor(const ImportedTopologyDescriptor& descriptor);
[[nodiscard]] Result<AssemblyGeometricTargetSourceKind>
imported_topology_assembly_source_kind(const ImportedTopologyDescriptor& descriptor);

} // namespace blcad::geometry
