#pragma once

#include "blcad/core/sketch.hpp"

#include <cmath>
#include <optional>
#include <vector>

namespace blcad {

struct SketchContourCurve {
  SketchEntityId id;
  Point2 start;
  Point2 end;
  bool reversed{false};
};

// Profile traversal is derived from connectivity. The source curve and its
// authored endpoint roles remain unchanged, even when a profile traverses it
// backwards. The caller supplies contour order, not entity insertion order.
[[nodiscard]] inline Result<std::vector<SketchContourCurve>>
orient_sketch_contour(std::vector<SketchContourCurve> curves, const SketchId& sketch,
                      bool allow_single = false) {
  const auto fail = [&] {
    return Result<std::vector<SketchContourCurve>>::failure(
        Error::validation(sketch.value(), "profile curve segments must be ordered and connected"));
  };
  if (curves.empty() || (curves.size() < 2 && !allow_single))
    return fail();
  const auto same = [](Point2 a, Point2 b) {
    return std::abs(a.x - b.x) <= 1e-9 && std::abs(a.y - b.y) <= 1e-9;
  };
  for (bool reverse_first : {false, true}) {
    auto traversal = curves;
    if (reverse_first) {
      std::swap(traversal.front().start, traversal.front().end);
      traversal.front().reversed = true;
    }
    bool connected = true;
    for (std::size_t i = 1; i < traversal.size(); ++i) {
      if (same(traversal[i - 1].end, traversal[i].start))
        continue;
      if (!same(traversal[i - 1].end, traversal[i].end)) {
        connected = false;
        break;
      }
      std::swap(traversal[i].start, traversal[i].end);
      traversal[i].reversed = true;
    }
    if (connected && same(traversal.back().end, traversal.front().start))
      return Result<std::vector<SketchContourCurve>>::success(std::move(traversal));
  }
  return fail();
}
[[nodiscard]] inline Result<std::vector<SketchContourCurve>>
resolve_sketch_contour(const Sketch& sketch, const std::vector<SketchEntityId>& ids) {
  std::vector<SketchContourCurve> curves;
  for (const auto& id : ids) {
    if (const auto* line = sketch.find_line_segment(id))
      curves.push_back({id, line->start(), line->end()});
    else if (const auto* arc = sketch.find_arc_segment(id))
      curves.push_back({id, arc->start(), arc->end()});
    else if (const auto* spline = sketch.find_spline_segment(id))
      curves.push_back({id, spline->start(), spline->end()});
    else
      return Result<std::vector<SketchContourCurve>>::failure(Error::validation(
          sketch.id().value(), "profile curve segments must be ordered and connected"));
  }
  return orient_sketch_contour(std::move(curves), sketch.id());
}
} // namespace blcad
