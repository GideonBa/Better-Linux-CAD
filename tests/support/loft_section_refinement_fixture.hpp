#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace blcad::test::loft_refinement {
template <typename T> T checked(Result<T> value) {
  if (!value)
    FAIL(value.error().message());
  return std::move(value.value());
}
inline std::array<Point2, 4> poles(std::size_t station, bool upper) {
  const double x = 1 - .1 * station, y = 1 - .15 * station;
  if (upper)
    return {{{-20 * x, 0}, {-12 * x, 15 * y}, {15 * x, 13 * y}, {30 * x, 0}}};
  return {{{-20 * x, 0}, {-10 * x, -8 * y}, {15 * x, -5 * y}, {30 * x, 0}}};
}
// Bernstein/derivative oracle, independent of Geometry's OCCT subdivision.
inline Point2 at(const std::array<Point2, 4>& p, double t) {
  const double u = 1 - t;
  return {u * u * u * p[0].x + 3 * u * u * t * p[1].x + 3 * u * t * t * p[2].x + t * t * t * p[3].x,
          u * u * u * p[0].y + 3 * u * u * t * p[1].y + 3 * u * t * t * p[2].y +
              t * t * t * p[3].y};
}
inline Point2 derivative(const std::array<Point2, 4>& p, double t) {
  const double u = 1 - t;
  return {
      3 * (u * u * (p[1].x - p[0].x) + 2 * u * t * (p[2].x - p[1].x) + t * t * (p[3].x - p[2].x)),
      3 * (u * u * (p[1].y - p[0].y) + 2 * u * t * (p[2].y - p[1].y) + t * t * (p[3].y - p[2].y))};
}
inline std::array<Point2, 4> piece(std::size_t station, bool upper, std::size_t i,
                                   std::size_t count) {
  const auto p = poles(station, upper);
  const double a = double(i) / count, b = double(i + 1) / count;
  const auto first = at(p, a), last = at(p, b), da = derivative(p, a), db = derivative(p, b);
  const double d = (b - a) / 3;
  return {{first,
           {first.x + d * da.x, first.y + d * da.y},
           {last.x - d * db.x, last.y - d * db.y},
           last}};
}
inline Point3 model(std::size_t station, Point2 p) {
  const double angle = station == 1 ? .1 : station == 2 ? -.08 : 0;
  return {3 * double(station) + std::cos(angle) * p.x, 2 * double(station) + p.y,
          20 * double(station) + std::sin(angle) * p.x};
}
inline const std::array<std::array<std::size_t, 2>, 3> counts{{{4, 4}, {5, 4}, {3, 3}}};
inline std::vector<std::vector<geometry::ClosedProfileCurveSegment>> sections() {
  std::vector<std::vector<geometry::ClosedProfileCurveSegment>> result;
  for (std::size_t station = 0; station < 3; ++station) {
    result.emplace_back();
    for (int side = 0; side < 2; ++side) {
      const auto n = counts[station][side];
      for (std::size_t k = 0; k < n; ++k) {
        auto p = piece(station, side == 0, side == 0 ? k : n - 1 - k, n);
        if (side == 1)
          std::reverse(p.begin(), p.end());
        result.back().push_back({geometry::ClosedProfileCurveKind::CubicBezierSpline,
                                 model(station, p[0]), model(station, p[1]), model(station, p[3]),
                                 model(station, p[2])});
      }
    }
  }
  return result;
}
inline PartDocument inputs() {
  auto part =
      checked(PartDocument::create(DocumentId("part.unequal.loft"), "Unequal spline sections"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  for (std::size_t station = 0; station < 3; ++station) {
    const auto suffix = std::to_string(station);
    const auto plane = station == 0 ? std::string("datum.xy") : "plane." + suffix;
    if (station) {
      const double angle = station == 1 ? .1 : -.08;
      const Vector3 x{std::cos(angle), 0, std::sin(angle)}, y{0, 1, 0};
      REQUIRE(part.add_construction_plane(
          checked(ConstructionPlane::create_explicit(ConstructionPlaneId(plane), "Plane " + suffix,
                                                     model(station, {0, 0}), x, y, cross(x, y)))));
    }
    const std::array names{"Root profile", "Mid profile", "Tip profile"};
    auto sketch =
        checked(Sketch::create(SketchId("section." + suffix), names[station], DatumPlaneId(plane)));
    std::array<std::vector<SketchEntityId>, 2> ids;
    for (int side = 0; side < 2; ++side)
      for (std::size_t k = 0; k < counts[station][side]; ++k) {
        const SketchEntityId id(std::string(side == 0 ? "upper." : "lower.") + std::to_string(k));
        const auto p = piece(station, side == 0, k, counts[station][side]);
        REQUIRE(sketch.add_entity(
            checked(SplineSegment::create_cubic_bezier(id, p[0], p[1], p[2], p[3]))));
        ids[side].push_back(id);
      }
    auto boundary = ids[0];
    boundary.insert(boundary.end(), ids[1].rbegin(), ids[1].rend());
    REQUIRE(sketch.add_profile(checked(ArcClosedProfile::create(ProfileId("outline"), boundary))));
    REQUIRE(part.add_sketch(std::move(sketch)));
  }
  return part;
}
inline void add_loft(PartDocument& part) {
  std::vector<ProfileSectionReference> refs;
  for (int i = 0; i < 3; ++i)
    refs.push_back(checked(ProfileSectionReference::create_closed_region(checked(
        ProfileRegionReference::create(SketchId("section." + std::to_string(i)),
                                       ProfileId("outline"), PartFeatureInputRole::LoftSection)))));
  REQUIRE(part.add_body(checked(Body::create(BodyId("body.loft"), "Loft body", BodyKind::Solid))));
  const auto context = checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                std::nullopt, BodyId("body.loft")));
  REQUIRE(part.add_loft_feature(checked(LoftFeature::create_loft(
      FeatureId("loft.unequal"), "Automatic correspondence", refs, context))));
}
} // namespace blcad::test::loft_refinement
