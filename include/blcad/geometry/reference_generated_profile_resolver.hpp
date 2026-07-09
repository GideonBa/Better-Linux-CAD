#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/spatial.hpp"

#include <vector>

namespace blcad::geometry {

class ReferenceGeneratedProfileResolver {
public:
  [[nodiscard]] Result<LineSegment> resolve_line(const PartDocument& document, const Sketch& sketch,
                                                 const ReferenceGeneratedLine& line) const;

  [[nodiscard]] Result<std::vector<Point2>> resolve_closed_profile_vertices(
      const PartDocument& document, const Sketch& sketch, const ClosedProfile& profile,
      const std::vector<ReferenceGeneratedLine>& reference_generated_lines) const;
};

} // namespace blcad::geometry
