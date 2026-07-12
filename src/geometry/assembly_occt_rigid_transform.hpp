#pragma once

#include "blcad/core/assembly_document.hpp"

#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>

namespace blcad::geometry::detail {

// Converts one authored BLCAD RigidTransform to the exact OCCT transform used by
// posed assembly consumers: active fixed-axis X, then Y, then Z, then translation.
[[nodiscard]] gp_Trsf to_occt_rigid_transform(const RigidTransform& transform);

[[nodiscard]] TopLoc_Location to_occt_location(const RigidTransform& transform);

} // namespace blcad::geometry::detail
