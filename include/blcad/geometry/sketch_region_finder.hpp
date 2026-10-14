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
  // Curved contours retain exact source ids. Vertices are a transient pick
  // polygon; they must never replace the curves in a materialized profile.
  std::vector<SketchEntityId> curve_segments;

  [[nodiscard]] const std::vector<SketchEntityId>& source_entities() const noexcept {
    return curve_segments.empty() ? line_segments : curve_segments;
  }
};

class SketchRegionFinder {
public:
  [[nodiscard]] Result<GeneratedClosedProfileCandidate>
  find_single_region(const PartDocument& document, const Sketch& sketch) const;

  [[nodiscard]] Result<GeneratedClosedProfileCandidate>
  find_single_curve_region(const PartDocument& document, const Sketch& sketch) const;

  [[nodiscard]] Result<ClosedProfile>
  make_closed_profile(const GeneratedClosedProfileCandidate& candidate) const;

  [[nodiscard]] Result<std::size_t>
  materialize_profile(Sketch& sketch, const GeneratedClosedProfileCandidate& candidate) const;

private:
  [[nodiscard]] static Result<std::vector<ResolvedSketchRegionLine>>
  resolved_lines(const PartDocument& document, const Sketch& sketch);
};

} // namespace blcad::geometry
