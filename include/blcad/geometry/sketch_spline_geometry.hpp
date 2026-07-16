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
    const double t = std::clamp(parameter, 0.0, 1.0);
    const double u = 1.0 - t;
    const double b0 = u * u * u;
    const double b1 = 3.0 * u * u * t;
    const double b2 = 3.0 * u * t * t;
    const double b3 = t * t * t;
    return {b0 * segment.start().x + b1 * segment.control1().x +
                b2 * segment.control2().x + b3 * segment.end().x,
            b0 * segment.start().y + b1 * segment.control1().y +
                b2 * segment.control2().y + b3 * segment.end().y};
  }

  [[nodiscard]] static Point2 derivative(const SplineSegment& segment, double parameter) noexcept {
    const double t = std::clamp(parameter, 0.0, 1.0);
    const double u = 1.0 - t;
    return {3.0 * u * u * (segment.control1().x - segment.start().x) +
                6.0 * u * t * (segment.control2().x - segment.control1().x) +
                3.0 * t * t * (segment.end().x - segment.control2().x),
            3.0 * u * u * (segment.control1().y - segment.start().y) +
                6.0 * u * t * (segment.control2().y - segment.control1().y) +
                3.0 * t * t * (segment.end().y - segment.control2().y)};
  }

  [[nodiscard]] static Point2 second_derivative(const SplineSegment& segment,
                                                double parameter) noexcept {
    const double t = std::clamp(parameter, 0.0, 1.0);
    const double u = 1.0 - t;
    return {6.0 * u * (segment.control2().x - 2.0 * segment.control1().x +
                      segment.start().x) +
                6.0 * t * (segment.end().x - 2.0 * segment.control2().x +
                           segment.control1().x),
            6.0 * u * (segment.control2().y - 2.0 * segment.control1().y +
                      segment.start().y) +
                6.0 * t * (segment.end().y - 2.0 * segment.control2().y +
                           segment.control1().y)};
  }

private:
  static constexpr double kTolerance = 1.0e-9;
  [[nodiscard]] static double distance(Point2 first, Point2 second) noexcept {
    return std::hypot(second.x - first.x, second.y - first.y);
  }
};

} // namespace blcad::geometry
