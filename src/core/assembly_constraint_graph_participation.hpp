#pragma once

#include "blcad/core/assembly_constraint.hpp"

namespace blcad {

// Block 38 adds persistent generic relationship intent only. Keep the existing
// five equation-enabled families in solve/motion connectivity until Block 39
// implements generic relationship equations and shared graph integration.
[[nodiscard]] inline bool
participates_in_current_assembly_constraint_graph(AssemblyConstraintType type) noexcept {
  switch (type) {
  case AssemblyConstraintType::Mate:
  case AssemblyConstraintType::Concentric:
  case AssemblyConstraintType::Distance:
  case AssemblyConstraintType::Insert:
  case AssemblyConstraintType::Angle:
    return true;
  case AssemblyConstraintType::Coincident:
  case AssemblyConstraintType::Parallel:
  case AssemblyConstraintType::Perpendicular:
    return false;
  }
  return false;
}

} // namespace blcad
