#pragma once
#include "blcad/geometry/profile_region_classifier.hpp"
namespace blcad::geometry::detail {
// Exact wire containment for the explicit material-region policy. Historical consumers retain
// their existing picking classifier; intersecting/touching wires cannot disappear here.
[[nodiscard]] Result<std::vector<ContainedProfileHole>>
resolve_sweep_material_holes(const PartDocument&, const Sketch&, const ProfileId&);
} // namespace blcad::geometry::detail
