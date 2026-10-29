#pragma once

#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <vector>

namespace blcad::geometry {

class StepImportTopologyContext;

// Validates selected imported bodies and active geometric inputs without solving or moving
// components. An empty filter covers every owned Part; solve/apply may restrict it to the exact
// participating Part set. The context observes source bytes again before publication.
[[nodiscard]] Result<std::size_t>
validate_imported_assembly_inputs(const Project& project, StepImportTopologyContext* context,
                                  const std::vector<DocumentId>& participating_parts = {});

[[nodiscard]] inline Result<std::size_t>
validate_imported_assembly_inputs(const Project& project, StepImportTopologyContext& context,
                                  const std::vector<DocumentId>& participating_parts = {}) {
  return validate_imported_assembly_inputs(project, &context, participating_parts);
}

} // namespace blcad::geometry
