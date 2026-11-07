#pragma once

#include "blcad/core/part_document.hpp"

namespace blcad::geometry::detail {

// Explicit intent is persistable before effective history execution is available.
// Never let its old full-model cache or analytical shortcuts stand in for evaluation.
[[nodiscard]] inline Result<std::size_t> validate_history_evaluation(const PartDocument& document) {
  if (document.authored_history_intent())
    return Result<std::size_t>::failure(Error::dependency(
        document.id().value(),
        "Feature history requires an effective evaluator unavailable in this build."));
  return Result<std::size_t>::success(0);
}

} // namespace blcad::geometry::detail
