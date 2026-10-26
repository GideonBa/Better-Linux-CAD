#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/sketch_region_finder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace blcad::test::loft_alignment {
enum class MiddleAuthoring { Matching, Reversed, ShiftedStart };

template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}

inline ProfileId profile(std::size_t station) {
  return ProfileId("generated.region.section." + std::to_string(station) + ".0");
}

inline PartDocument inputs(MiddleAuthoring middle, bool provision_body = true) {
  auto part = checked(
      PartDocument::create(DocumentId("part.loft.alignment"), "Independent profile authoring"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  constexpr std::array<Point2, 4> outline{{{0, 0}, {12, 0}, {10, 6}, {0, 4}}};
  for (std::size_t station = 0; station < 3; ++station) {
    const auto suffix = std::to_string(station);
    const auto plane = station == 0 ? std::string("datum.xy") : "plane." + suffix;
    if (station != 0)
      REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_explicit(
          ConstructionPlaneId(plane), "Section plane " + suffix, {0, 0, 20.0 * station}, {1, 0, 0},
          {0, 1, 0}, {0, 0, 1}))));
    auto sketch = checked(
        Sketch::create(SketchId("section." + suffix), "Section " + suffix, DatumPlaneId(plane)));
    std::array<std::size_t, 4> order{0, 1, 2, 3};
    if (station == 1 && middle != MiddleAuthoring::Matching)
      order = middle == MiddleAuthoring::Reversed ? std::array<std::size_t, 4>{0, 3, 2, 1}
                                                  : std::array<std::size_t, 4>{1, 2, 3, 0};
    for (std::size_t edge = 0; edge < order.size(); ++edge)
      REQUIRE(sketch.add_entity(checked(
          LineSegment::create(SketchEntityId("edge." + std::to_string(edge)), outline[order[edge]],
                              outline[order[(edge + 1) % order.size()]]))));
    // Finish Sketch materializes this same automatically discovered contour.
    const geometry::SketchRegionFinder finder;
    const auto region = checked(finder.find_single_curve_region(part, sketch));
    REQUIRE(finder.materialize_profile(sketch, region));
    REQUIRE(part.add_sketch(std::move(sketch)));
  }
  if (provision_body)
    REQUIRE(part.add_body(
        checked(Body::create(BodyId("body.aligned"), "Aligned Loft", BodyKind::Solid))));
  return part;
}

inline std::vector<ProfileSectionReference> references() {
  std::vector<ProfileSectionReference> result;
  for (std::size_t station = 0; station < 3; ++station)
    result.push_back(checked(ProfileSectionReference::create_closed_region(checked(
        ProfileRegionReference::create(SketchId("section." + std::to_string(station)),
                                       profile(station), PartFeatureInputRole::LoftSection)))));
  return result;
}

inline void verify_prism(const geometry::GeometryShape& shape) {
  // Independent shoelace area 56 mm2 times the 40 mm station span.
  const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(shape);
  CHECK(summary.solid_count == 1);
  CHECK(summary.volume_mm3 == Catch::Approx(2240.0).margin(1e-5));
  struct Ray {
    Point2 origin;
    Vector3 direction;
    double expected_distance;
  };
  // Probe all four sides halfway between authored stations, where a twisted
  // loft differs despite interpolating every original contour exactly.
  for (const double z : {10.0, 30.0})
    for (const auto& ray : std::array<Ray, 4>{{{{6, -100}, {0, 1, 0}, 100},
                                               {{100, 3}, {-1, 0, 0}, 89},
                                               {{5, 100}, {0, -1, 0}, 95},
                                               {{-100, 2}, {1, 0, 0}, 100}}}) {
      CAPTURE(z, ray.origin.x, ray.origin.y);
      const auto distance = geometry::ClosedProfileAdapter{}.first_intersection_distance(
          shape, {ray.origin.x, ray.origin.y, z}, ray.direction);
      INFO((distance ? "Forward face intersection found" : distance.error().message()));
      REQUIRE(distance);
      CHECK(distance.value() == Catch::Approx(ray.expected_distance).margin(2e-6));
    }
}
} // namespace blcad::test::loft_alignment
