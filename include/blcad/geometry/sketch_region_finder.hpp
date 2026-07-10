#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/spatial.hpp"

#include <vector>

namespace blcad::geometry {

struct ResolvedSketchRegionLine {
  SketchEntityId id;
  Point2 start;
  Point2 end;
};

struct GeneratedClosedProfileCandidate {
  ProfileId id;
  std::vector<SketchEntityId> line_segments;
  std::vector<Point2> vertices;
};

class SketchRegionFinder {
public:
  [[nodiscard]] Result<GeneratedClosedProfileCandidate>
  find_single_region(const PartDocument& document, const Sketch& sketch) const;

  [[nodiscard]] Result<ClosedProfile>
  make_closed_profile(const GeneratedClosedProfileCandidate& candidate) const;
};

} // namespace blcad::geometry
