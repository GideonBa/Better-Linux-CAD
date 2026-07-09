#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/spatial.hpp"

#include <vector>

namespace blcad::geometry {

class DimensionDrivenProfileResolver {
public:
  [[nodiscard]] Result<std::vector<Point2>> resolve_closed_profile_vertices(
      const PartDocument& document, const Sketch& sketch, const ClosedProfile& profile) const;
};

} // namespace blcad::geometry
