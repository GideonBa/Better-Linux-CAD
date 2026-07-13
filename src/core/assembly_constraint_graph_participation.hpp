#pragma once

#include "blcad/core/assembly_constraint.hpp"

namespace blcad {

// Equation-enabled geometric relationship participation authority. Block 39
// enables the three generic relationship families in the same local, hierarchy,
// and motion connectivity used by the historical five families.
[[nodiscard]] inline bool
participates_in_current_assembly_constraint_graph(AssemblyConstraintType type) noexcept {
  switch (type) {
  case AssemblyConstraintType::Mate:
  case AssemblyConstraintType::Concentric:
  case AssemblyConstraintType::Distance:
  case AssemblyConstraintType::Insert:
  case AssemblyConstraintType::Angle:
  case AssemblyConstraintType::Coincident:
  case AssemblyConstraintType::Parallel:
  case AssemblyConstraintType::Perpendicular:
    return true;
  }
  return false;
}

} // namespace blcad
