#pragma once

#include "blcad/core/loft_feature.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"

namespace blcad {
class PartDocument;
}

namespace blcad::geometry {
class ShapeCache;

namespace detail {
// Delegates to the same closed-section resolver used by Loft execution.
[[nodiscard]] Result<std::vector<ClosedProfileCurveSegment>>
resolve_loft_alignment_section(const PartDocument& document, const ProfileSectionReference& section,
                               const ShapeCache& cache);
} // namespace detail
} // namespace blcad::geometry
