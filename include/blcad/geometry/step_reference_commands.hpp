#pragma once

#include "blcad/core/project.hpp"
#include <filesystem>

namespace blcad::geometry {

// Candidates are Core values, suitable for the same transaction in a widget or
// a headless client. Source snapshots and kernel geometry remain disposable.
struct StepReferenceProjectCandidate {
  Project project;
  DocumentId part;
  std::vector<DocumentId> affected_owners;
  std::vector<std::string> diagnostics;
};
struct StepReferencePartCandidate {
  PartDocument part;
  std::vector<DocumentId> affected_owners;
  std::vector<std::string> diagnostics;
};

class StepReferenceCommands {
public:
  [[nodiscard]] Result<PartDocument>
  create_definition(const StepImportSource&, const ImportedTopologyCatalog&, DocumentId,
                    std::string name, const std::filesystem::path& asset_root) const;
  // Reuse requires the exact BLCAD source/catalog binding, never equal geometry
  // or an equal product name. Membership and the occurrence are added atomically.
  [[nodiscard]] Result<StepReferenceProjectCandidate>
  insert(const Project&, ImportedTopologyCatalogId, DocumentId new_part, std::string name,
         DocumentId target_assembly, ComponentInstanceId, RigidTransform,
         const std::filesystem::path& asset_root) const;
  [[nodiscard]] Result<StepReferenceProjectCandidate>
  repeat(const Project&, DocumentId reference_part, DocumentId target_assembly, ComponentInstanceId,
         RigidTransform, const std::filesystem::path& asset_root) const;
  // Refresh every explicit same-asset binding, including project registry and
  // copied owned Parts, preserving modes, catalogs, output IDs and occurrences.
  [[nodiscard]] Result<StepReferenceProjectCandidate>
  refresh(const Project&, DocumentId reference_part, const std::filesystem::path& asset_root) const;
  [[nodiscard]] Result<StepReferencePartCandidate>
  refresh(const PartDocument&, const std::filesystem::path& asset_root) const;
  [[nodiscard]] Result<StepReferencePartCandidate>
  convert(const PartDocument&, DocumentId new_part, std::string name, FeatureId base_feature,
          const std::filesystem::path& asset_root) const;
  // Conversion adds a new uninstantiated editable definition. Existing assembly
  // occurrences continue to reference the original immutable definition.
  [[nodiscard]] Result<StepReferenceProjectCandidate>
  convert(const Project&, DocumentId reference_part, DocumentId new_part, std::string name,
          FeatureId base_feature, const std::filesystem::path& asset_root) const;
};
} // namespace blcad::geometry
