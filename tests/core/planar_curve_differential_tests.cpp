#include "blcad/core/planar_curve_differential.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

using namespace blcad;
namespace {
void vector_near(Point2 actual, Point2 expected) {
  CHECK(actual.x == Catch::Approx(expected.x).margin(1e-9 * (1 + std::abs(expected.x))));
  CHECK(actual.y == Catch::Approx(expected.y).margin(1e-9 * (1 + std::abs(expected.y))));
}
void differential_near(const Result<PlanarEndpointDifferential>& result, Point2 tangent,
                       Point2 curvature) {
  INFO((result ? "regular endpoint" : result.error().message()));
  REQUIRE(result);
  vector_near(result.value().inward_tangent, tangent);
  vector_near(result.value().curvature_vector, curvature);
}
} // namespace

TEST_CASE("Planar endpoint differentials agree with an independently differentiated parabola",
          "[core][parity-b159][smooth-differential]") {
  // x=6t, y=6t^2, independently K=(-y' y'',y'')/(1+y'^2)^2 in x coordinates.
  const RationalBezier curve{{Point2{0, 0}, Point2{2, 0}, Point2{4, 2}, Point2{6, 6}}};
  differential_near(PlanarCurveDifferential::spline(curve, false, 10, "parabola"), {1, 0},
                    {0, 1.0 / 3});
  differential_near(PlanarCurveDifferential::spline(curve, true, 10, "parabola"),
                    {-1 / std::sqrt(5.0), -2 / std::sqrt(5.0)}, {-2.0 / 75, 1.0 / 75});
}

TEST_CASE("Rational conic curvature survives orientation scale and parameter speed changes",
          "[core][parity-b159][smooth-differential]") {
  // Degree elevation of the homogeneous quadratic quarter circle, independent
  // radial curvature oracle. Multiplying weight i by a^i is a Mobius change of
  // parameter and preserves the geometric conic with different endpoint speeds.
  const double q = std::sqrt(.5), w = (1 + 2 * q) / 3;
  for (double scale : {.001, 1.0, 1000.0})
    for (double speed : {.25, 1.0, 4.0})
      for (double weight_scale : {1e-100, 1.0, 1e100})
        for (bool reversed : {false, true}) {
          CAPTURE(scale, speed, weight_scale, reversed);
          const double radius = 5 * scale;
          const Point2 center{123 * scale, -24 * scale};
          const double middle = 2 * q * radius / (1 + 2 * q);
          RationalBezier curve{{Point2{radius, 0}, Point2{radius, middle}, Point2{middle, radius},
                                Point2{0, radius}},
                               {1, w, w, 1}};
          for (std::size_t i = 0; i < 4; ++i) {
            curve.poles[i].x += center.x;
            curve.poles[i].y += center.y;
            curve.weights[i] *= weight_scale * std::pow(speed, static_cast<int>(i));
          }
          if (reversed) {
            std::reverse(curve.poles.begin(), curve.poles.end());
            std::reverse(curve.weights.begin(), curve.weights.end());
          }
          for (bool at_end : {false, true}) {
            const bool at_top = at_end != reversed;
            differential_near(PlanarCurveDifferential::spline(curve, at_end, 10 * scale, "conic"),
                              at_top ? Point2{1, 0} : Point2{0, 1},
                              at_top ? Point2{0, -1 / radius} : Point2{-1 / radius, 0});
          }
        }
}

TEST_CASE("Circular endpoint curvature has the radial oracle under reversal and rigid placement",
          "[core][parity-b159][smooth-differential]") {
  for (double scale : {.001, 1.0, 1000.0})
    for (bool reversed : {false, true}) {
      CAPTURE(scale, reversed);
      std::array<Point2, 3> points{
          {{8 * scale, -2 * scale}, {3 * scale, 3 * scale}, {-2 * scale, -2 * scale}}};
      for (auto& point : points) {
        point.x += 17 * scale;
        point.y -= 9 * scale;
      }
      if (reversed)
        std::reverse(points.begin(), points.end());
      for (bool at_end : {false, true}) {
        const bool at_left = at_end != reversed;
        differential_near(PlanarCurveDifferential::arc(points, at_end, 10 * scale, "arc"), {0, 1},
                          {(at_left ? 1 : -1) / (5 * scale), 0});
      }
    }
}

TEST_CASE("Smooth endpoint regularity distinguishes zero speed from zero curvature",
          "[core][parity-b159][smooth-differential]") {
  CHECK_FALSE(PlanarCurveDifferential::line({1, 2}, {1, 2}, 1, "collapsed"));
  CHECK_FALSE(
      PlanarCurveDifferential::arc({Point2{0, 0}, Point2{1, 0}, Point2{2, 0}}, false, 2, "flat"));
  RationalBezier zero_start{{Point2{0, 0}, Point2{0, 0}, Point2{2, 1}, Point2{3, 2}}};
  CHECK_FALSE(PlanarCurveDifferential::spline(zero_start, false, 4, "zero speed"));
  std::reverse(zero_start.poles.begin(), zero_start.poles.end());
  CHECK_FALSE(PlanarCurveDifferential::spline(zero_start, true, 4, "zero speed"));
  for (double end_y : {-1.0, 0.0, 1.0}) {
    const RationalBezier inflection{{Point2{0, 0}, Point2{1, 0}, Point2{2, 0}, Point2{3, end_y}}};
    differential_near(PlanarCurveDifferential::spline(inflection, false, 4, "inflection"), {1, 0},
                      {0, 0});
  }
}
