#pragma once

#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>

namespace blcad {

struct RectangularExtrudeProfile {
  Point2 center;
  double width;
  double height;
};

// Stable box roles come from authored plane coordinates, never kernel traversal order.
// Also accepts rectangles drawn as four lines by the replacement shell.
[[nodiscard]] inline Result<RectangularExtrudeProfile>
resolve_rectangular_extrude_profile(const PartDocument& document, const Feature& feature) {
  const auto fail = [&] {
    return Result<RectangularExtrudeProfile>::failure(
        Error::validation(feature.id().value(),
                          "semantic box roles require a single rectangular distance extrusion"));
  };
  if (feature.type() != FeatureType::AdditiveExtrude ||
      feature.direction() != ExtrudeDirection::SketchNormal ||
      feature.extrude_intent().extent().mode() != ExtrudeExtentMode::Distance ||
      feature.extrude_intent().thin().has_value() ||
      feature.extrude_intent().taper_angle_deg().value_or(0.0) != 0.0)
    return fail();
  const auto* sketch = document.find_sketch(feature.input_sketch());
  if (!sketch || (!feature.profile() && sketch->profile_count() != 1U))
    return fail();
  const RectangleProfile* rectangle =
      feature.profile()
          ? sketch->find_rectangle_profile(*feature.profile())
          : (sketch->rectangle_profiles().size() == 1U ? &sketch->rectangle_profiles().front()
                                                       : nullptr);
  if (rectangle) {
    const auto* width = document.find_parameter(rectangle->width_parameter());
    const auto* height = document.find_parameter(rectangle->height_parameter());
    if (!width || !height || width->type() != ParameterType::Length ||
        height->type() != ParameterType::Length || width->value().millimeters() <= 0 ||
        height->value().millimeters() <= 0)
      return fail();
    return Result<RectangularExtrudeProfile>::success(
        {rectangle->center(), width->value().millimeters(), height->value().millimeters()});
  }
  const ClosedProfile* profile =
      feature.profile()
          ? sketch->find_closed_profile(*feature.profile())
          : (sketch->closed_profiles().size() == 1U ? &sketch->closed_profiles().front() : nullptr);
  if (!profile || profile->line_segments().size() != 4U)
    return fail();
  auto vertices = sketch->closed_profile_vertices(*profile);
  if (!vertices || vertices.value().size() != 4U)
    return fail();
  const auto& points = vertices.value();
  double xmin = points[0].x, xmax = xmin, ymin = points[0].y, ymax = ymin;
  for (auto p : points) {
    xmin = std::min(xmin, p.x);
    xmax = std::max(xmax, p.x);
    ymin = std::min(ymin, p.y);
    ymax = std::max(ymax, p.y);
  }
  constexpr double tolerance = 1e-8;
  if (xmax - xmin <= tolerance || ymax - ymin <= tolerance)
    return fail();
  unsigned corners = 0;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto p = points[i], q = points[(i + 1) % points.size()];
    if ((std::abs(p.x - q.x) > tolerance && std::abs(p.y - q.y) > tolerance) ||
        (std::abs(p.x - xmin) > tolerance && std::abs(p.x - xmax) > tolerance) ||
        (std::abs(p.y - ymin) > tolerance && std::abs(p.y - ymax) > tolerance))
      return fail();
    const unsigned corner = (std::abs(p.x - xmax) <= tolerance ? 1U : 0U) +
                            (std::abs(p.y - ymax) <= tolerance ? 2U : 0U);
    if (corners & (1U << corner))
      return fail();
    corners |= 1U << corner;
    if (sketch->find_line_segment(profile->line_segments()[i])->construction())
      return fail();
  }
  return Result<RectangularExtrudeProfile>::success(
      {{(xmin + xmax) / 2, (ymin + ymax) / 2}, xmax - xmin, ymax - ymin});
}
} // namespace blcad
