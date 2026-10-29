#pragma once

#include "blcad/geometry/step_reference_commands.hpp"

namespace blcad::geometry {

using StepEditablePartCandidate = StepReferencePartCandidate;
using StepEditableProjectCandidate = StepReferenceProjectCandidate;

// Explicit EditableBody import and shared-asset refresh. All geometry work is
// completed on disposable candidates, including every downstream Part feature.
class StepEditableCommands {
public:
  [[nodiscard]] Result<PartDocument>
  create_definition(const StepImportSource&, const ImportedTopologyCatalog&, DocumentId,
                    std::string name, FeatureId base_feature,
                    const std::filesystem::path& asset_root) const;
  // A direct editable import creates a new explicitly identified definition;
  // an existing, possibly modeled definition is never silently reused.
  [[nodiscard]] Result<StepEditableProjectCandidate>
  insert(const Project&, ImportedTopologyCatalogId, DocumentId new_part, std::string name,
         FeatureId base_feature, DocumentId target_assembly, ComponentInstanceId, RigidTransform,
         const std::filesystem::path& asset_root) const;
  // An empty source selects only an unambiguous source actually consumed by
  // this owner's imported base(s) or Reference binding, never opaque metadata.
  [[nodiscard]] Result<StepEditablePartCandidate>
  refresh(const PartDocument&, StepImportSourceId, const std::filesystem::path& asset_root) const;
  [[nodiscard]] Result<StepEditableProjectCandidate>
  refresh(const Project&, DocumentId imported_part, StepImportSourceId,
          const std::filesystem::path& asset_root) const;
};

} // namespace blcad::geometry
