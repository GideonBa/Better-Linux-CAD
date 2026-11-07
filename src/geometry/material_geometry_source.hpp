#pragma once

#include "blcad/core/result.hpp"

#include <string>
#include <string_view>

namespace blcad::geometry::detail {
// Canonical typed Part JSON, with material snapshots/pins removed. Standard
// tables remain inputs: their later feature consumers must invalidate geometry.
[[nodiscard]] Result<std::string> material_independent_geometry_source(std::string_view source);
} // namespace blcad::geometry::detail
