#pragma once

#include "blcad/core/part_document.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>

namespace blcad::test::mixed_loft {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
inline double oracle_distance(Point3 a, Point3 b) {
  return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}
inline Point3 interpolate(Point3 a, Point3 b, double t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
// Independent oracle for the frozen 32-span cubic spline and an analytic quarter circle.
inline Point3 bezier(double t) {
  return {30 * t * t - 20 * t * t * t, 0, 30 + 30 * t};
}
inline double spline_length() {
  double sum = 0;
  for (int i = 1; i <= 32; ++i)
    sum += oracle_distance(bezier((i - 1) / 32.0), bezier(i / 32.0));
  return sum;
}
inline double path_length() {
  return 30 + spline_length() + 15 * std::numbers::pi;
}
inline Point3 path_point(double ratio) {
  double s = ratio * path_length();
  if (s <= 30)
    return {0, 0, s};
  s -= 30;
  for (int i = 1; i <= 32; ++i) {
    const auto a = bezier((i - 1) / 32.0), b = bezier(i / 32.0);
    const auto span = oracle_distance(a, b);
    if (s <= span)
      return interpolate(a, b, s / span);
    s -= span;
  }
  const double angle = s / 30;
  return {40 - 30 * std::cos(angle), 0, 60 + 30 * std::sin(angle)};
}
struct Station {
  Point3 center;
  Vector3 x, y, normal;
  std::array<Point2, 4> outline;
};
inline Station station(std::size_t count, std::size_t index, double width_factor = 1) {
  const double ratio = static_cast<double>(index) / (count - 1);
  const double theta = index == 0 ? 0 : std::numbers::pi / 2 * ratio + (index == 1 ? -.15 : 0);
  const double roll = .12 * index;
  const Vector3 x0{std::cos(theta), 0, -std::sin(theta)};
  const Vector3 x{x0.x * std::cos(roll), std::sin(roll), x0.z * std::cos(roll)};
  const Vector3 y{-x0.x * std::sin(roll), std::cos(roll), -x0.z * std::sin(roll)};
  const std::array widths{4., 6., 3., 5., 2.5};
  const std::array heights{3., 2., 4., 2.8, 3.5};
  const double w = widths[index] * width_factor, h = heights[index];
  std::array<Point2, 4> outline{
      {{-w / 2, -h / 2}, {w / 2, -h / 2}, {w / 2, h / 2}, {-w / 2, h / 2}}};
  if (index % 2 == 1)
    outline = {{{-w / 2, -h / 2}, {w / 2, -h / 2}, {w / 3, h / 2}, {-w / 3, h / 2}}};
  return {path_point(ratio), x, y, cross(x, y), outline};
}
inline Point3 model_point(const Station& frame, Point2 p) {
  return {frame.center.x + frame.x.x * p.x + frame.y.x * p.y,
          frame.center.y + frame.x.y * p.x + frame.y.y * p.y,
          frame.center.z + frame.x.z * p.x + frame.y.z * p.y};
}
inline std::vector<ProfileSectionReference> sections(std::size_t count) {
  std::vector<ProfileSectionReference> result;
  for (std::size_t i = 0; i < count; ++i) {
    const auto suffix = std::to_string(i);
    result.push_back(checked(ProfileSectionReference::create_closed_region(
        checked(ProfileRegionReference::create(SketchId("section." + suffix),
                                               ProfileId("profile." + suffix),
                                               PartFeatureInputRole::LoftSection)))));
  }
  return result;
}
inline PathCurve path() {
  return checked(PathCurve::create(
      PathCurveId("mixed.path"), "Line spline arc",
      {checked(PathSegmentReference::create_planar(SketchId("trajectory"), SketchEntityId("lead"),
                                                   PlanarPathCurveKind::Line)),
       checked(PathSegmentReference::create_planar(SketchId("trajectory"), SketchEntityId("spline"),
                                                   PlanarPathCurveKind::Spline)),
       checked(PathSegmentReference::create_planar(SketchId("trajectory"), SketchEntityId("arc"),
                                                   PlanarPathCurveKind::Arc, true))},
      PathClosure::Open, PathOrientationRule::MinimumTwist));
}
inline PartDocument inputs(std::size_t count = 5, bool include_path = true) {
  REQUIRE((count == 3 || count == 5));
  auto part =
      checked(PartDocument::create(DocumentId("mixed.loft.part"), "Mixed path oriented sections"));
  REQUIRE(part.add_construction_plane(checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("trajectory.plane"), "Trajectory XZ",
                                         {0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}))));
  auto trajectory = checked(
      Sketch::create(SketchId("trajectory"), "Mixed trajectory", DatumPlaneId("trajectory.plane")));
  REQUIRE(
      trajectory.add_entity(checked(LineSegment::create(SketchEntityId("lead"), {0, 0}, {0, 30}))));
  REQUIRE(trajectory.add_entity(checked(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline"), {0, 30}, {0, 40}, {10, 50}, {10, 60}))));
  const double k = 30 * std::sqrt(.5);
  REQUIRE(trajectory.add_entity(checked(ArcSegment::create_three_point(
      SketchEntityId("arc"), {40, 90}, {40 - k, 60 + k}, {10, 60}))));
  REQUIRE(part.add_sketch(std::move(trajectory)));
  for (std::size_t i = 0; i < count; ++i) {
    const auto suffix = std::to_string(i);
    const auto frame = station(count, i);
    const auto plane = "section.plane." + suffix;
    REQUIRE(part.add_construction_plane(
        checked(ConstructionPlane::create_explicit(ConstructionPlaneId(plane), "Plane " + suffix,
                                                   frame.center, frame.x, frame.y, frame.normal))));
    auto sketch = checked(
        Sketch::create(SketchId("section." + suffix), "Section " + suffix, DatumPlaneId(plane)));
    if (i % 2 == 0) {
      for (const auto& [role, value] : {std::pair{"width", frame.outline[1].x * 2},
                                        std::pair{"height", frame.outline[2].y * 2}}) {
        const auto id = "section." + suffix + "." + role;
        REQUIRE(part.add_parameter(checked(
            Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
      }
      REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
          ProfileId("profile." + suffix), ParameterId("section." + suffix + ".width"),
          ParameterId("section." + suffix + ".height")))));
    } else {
      std::vector<SketchEntityId> edges;
      for (std::size_t j = 0; j < 4; ++j) {
        edges.emplace_back("edge." + suffix + "." + std::to_string(j));
        REQUIRE(sketch.add_entity(checked(
            LineSegment::create(edges.back(), frame.outline[j], frame.outline[(j + 1) % 4]))));
      }
      REQUIRE(sketch.add_profile(
          checked(ClosedProfile::create(ProfileId("profile." + suffix), edges))));
    }
    REQUIRE(part.add_sketch(std::move(sketch)));
  }
  if (include_path)
    REQUIRE(part.add_path_curve(path()));
  return part;
}
inline void add_loft(PartDocument& part, std::size_t count = 5, bool guided = true,
                     LoftFeatureKind kind = LoftFeatureKind::Loft,
                     LoftContinuity continuity = LoftContinuity::C0) {
  REQUIRE(part.add_body(checked(
      Body::create(BodyId("mixed.body"), "Mixed loft",
                   kind == LoftFeatureKind::LoftSurface ? BodyKind::Surface : BodyKind::Solid))));
  const auto context = checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("mixed.body")));
  const auto center = guided ? std::optional(PathCurveId("mixed.path")) : std::nullopt;
  auto feature =
      kind == LoftFeatureKind::LoftSurface
          ? LoftFeature::create_loft_surface(FeatureId("mixed.loft"), "Mixed loft", sections(count),
                                             context, center, {}, continuity)
          : LoftFeature::create_loft(FeatureId("mixed.loft"), "Mixed loft", sections(count),
                                     context, center, {}, continuity);
  REQUIRE(part.add_loft_feature(checked(std::move(feature))));
}
} // namespace blcad::test::mixed_loft
