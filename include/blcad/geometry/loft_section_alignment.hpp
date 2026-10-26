#pragma once

#include "blcad/core/loft_feature.hpp"

#include <vector>

namespace blcad {
class PartDocument;
}

namespace blcad::geometry {
class ShapeCache;

// Authoring suggestion only; recompute never calls this function. The first
// section anchors the seam. Existing explicit controls and open sections opt
// the complete list out. Manual flips are applied after automatic alignment.
// Only existing alignment/flip intent is returned; Sketch geometry is unchanged.
[[nodiscard]] Result<std::vector<ProfileSectionReference>>
suggest_loft_section_alignment(const PartDocument& document,
                               const std::vector<ProfileSectionReference>& sections,
                               const ShapeCache& cache);

} // namespace blcad::geometry
