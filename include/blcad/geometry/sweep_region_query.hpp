#pragma once
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include <vector>
namespace blcad {
class PartDocument;
}
namespace blcad::geometry {
// Uses the same exact native wire containment and ambiguity checks as execution.
// Returns the immediate holes of the selected material region; no model mutation.
[[nodiscard]] Result<std::vector<ProfileId>> inspect_sweep_material_holes(const PartDocument&,
                                                                          SketchId, ProfileId);
} // namespace blcad::geometry
