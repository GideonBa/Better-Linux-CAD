#pragma once

#include "blcad/core/project.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace blcad::geometry {

struct StepStructuredImportCandidate {
  Project project;
  std::vector<DocumentId> affected_owners;
  std::vector<std::string> diagnostics;
};

// Structured import builds a new disposable Project from its own immutable,
// digest-verified STEP read. Inspection previews never supply geometry authority.
class StepStructuredImportCommands {
public:
  [[nodiscard]] Result<StepStructuredImportCandidate>
  create_project(const StepImportSource& source, DocumentId project_id, std::string name,
                 const std::filesystem::path& asset_root) const;
};

} // namespace blcad::geometry
