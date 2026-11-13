#include "blcad/geometry/sketch_curve_contour.hpp"
#include "blcad/geometry/equation_curve_geometry.hpp"
#include <algorithm>

namespace blcad::geometry {
Result<std::vector<SketchContourCurve>>
resolve_sketch_curve_contour(const PartDocument& document, const Sketch& sketch,
                             const std::vector<SketchEntityId>& ids) {
  if (std::none_of(ids.begin(), ids.end(),
                   [&](const auto& id) { return sketch.find_equation_curve(id) != nullptr; }))
    return resolve_sketch_contour(sketch, ids);
  std::vector<SketchContourCurve> curves;
  for (const auto& id : ids) {
    if (const auto* line = sketch.find_line_segment(id))
      curves.push_back({id, line->start(), line->end()});
    else if (const auto* arc = sketch.find_arc_segment(id))
      curves.push_back({id, arc->start(), arc->end()});
    else if (const auto* spline = sketch.find_spline_segment(id))
      curves.push_back({id, spline->start(), spline->end()});
    else if (const auto* equation = sketch.find_equation_curve(id)) {
      auto resolved = EquationCurveGeometryAdapter{}.convert(document, *equation);
      if (!resolved)
        return Result<std::vector<SketchContourCurve>>::failure(resolved.error());
      const auto& endpoints = resolved.value().endpoints;
      curves.push_back({id,
                        {endpoints[0].position.x, endpoints[0].position.y},
                        {endpoints[1].position.x, endpoints[1].position.y}});
    } else
      return Result<std::vector<SketchContourCurve>>::failure(
          Error::validation(id.value(), "Profile curve source must exist"));
  }
  return orient_sketch_contour(std::move(curves), sketch.id(), ids.size() == 1);
}
} // namespace blcad::geometry
