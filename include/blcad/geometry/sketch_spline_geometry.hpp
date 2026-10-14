#pragma once

#include "blcad/core/sketch_spline_edit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {

struct SketchSplineSample {
  double parameter{0.0};
  Point2 position;
  Point2 tangent;
  double curvature{0.0};
};

struct SketchSplineContinuityDiagnostic {
  std::size_t junction_index{0U};
  bool position_continuous{false};
  bool tangent_continuous{false};
  std::string message;
};

struct SketchSplineGeometryResult {
  std::vector<std::vector<SketchSplineSample>> sampled_segments;
  std::vector<std::vector<Point2>> control_polygons;
  std::vector<SketchSplineContinuityDiagnostic> continuity;
};

class SketchSplineGeometryEvaluator {
public:
  [[nodiscard]] Result<SketchSplineGeometryResult>
  evaluate(const std::vector<SplineSegment>& segments, std::size_t samples_per_segment = 32U) const {
    if (segments.empty())
      return Result<SketchSplineGeometryResult>::failure(
          Error::validation("sketch_spline_geometry", "spline geometry requires at least one segment"));
    if (samples_per_segment < 2U || samples_per_segment > 4096U)
      return Result<SketchSplineGeometryResult>::failure(Error::validation(
          "sketch_spline_geometry", "spline samples per segment must be between 2 and 4096"));

    SketchSplineGeometryResult result;
    result.sampled_segments.reserve(segments.size());
    result.control_polygons.reserve(segments.size());
    for (const auto& segment : segments) {
      result.control_polygons.push_back(
          {segment.start(), segment.control1(), segment.control2(), segment.end()});
      std::vector<SketchSplineSample> samples;
      samples.reserve(samples_per_segment + 1U);
      for (std::size_t index = 0U; index <= samples_per_segment; ++index) {
        const double parameter =
            static_cast<double>(index) / static_cast<double>(samples_per_segment);
        const Point2 first = derivative(segment, parameter);
        const Point2 second = second_derivative(segment, parameter);
        const double speed = std::hypot(first.x, first.y);
        const double curvature = speed <= kTolerance
                                     ? 0.0
                                     : (first.x * second.y - first.y * second.x) /
                                           (speed * speed * speed);
        samples.push_back({parameter, evaluate_point(segment, parameter), first, curvature});
      }
      result.sampled_segments.push_back(std::move(samples));
    }

    for (std::size_t index = 1U; index < segments.size(); ++index) {
      const auto& first = segments[index - 1U];
      const auto& second = segments[index];
      const Point2 incoming = derivative(first, 1.0);
      const Point2 outgoing = derivative(second, 0.0);
      const double tangent_scale =
          std::hypot(incoming.x, incoming.y) * std::hypot(outgoing.x, outgoing.y);
      const bool position = distance(first.end(), second.start()) <= kTolerance;
      const bool tangent = tangent_scale > kTolerance &&
                           std::abs(incoming.x * outgoing.y - incoming.y * outgoing.x) <=
                               kTolerance * tangent_scale &&
                           incoming.x * outgoing.x + incoming.y * outgoing.y > 0.0;
      result.continuity.push_back(
          {index - 1U, position, tangent,
           !position ? "spline chain is not position-continuous"
                     : tangent ? "spline chain is tangent-continuous"
                               : "spline chain is position-continuous but not tangent-continuous"});
    }
    return Result<SketchSplineGeometryResult>::success(std::move(result));
  }

  [[nodiscard]] static Point2 evaluate_point(const SplineSegment& segment, double parameter) noexcept {
    return segment.curve().point(std::clamp(parameter, 0.0, 1.0));
  }
  [[nodiscard]] static Point2 derivative(const SplineSegment& segment, double parameter) noexcept {
    return segment.curve().derivative(std::clamp(parameter, 0.0, 1.0));
  }
  [[nodiscard]] static Point2 second_derivative(const SplineSegment& segment, double parameter) noexcept {
    return segment.curve().second_derivative(std::clamp(parameter, 0.0, 1.0));
  }

private:
  static constexpr double kTolerance = 1.0e-9;
  [[nodiscard]] static double distance(Point2 first, Point2 second) noexcept {
    return std::hypot(second.x - first.x, second.y - first.y);
  }
};

} // namespace blcad::geometry
