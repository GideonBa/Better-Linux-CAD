#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

Sketch make_profile_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.composite"), "Composite", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "outer.bottom", Point2{-20.0, -10.0}, Point2{20.0, -10.0});
  add_line(sketch.value(), "outer.right", Point2{20.0, -10.0}, Point2{20.0, 10.0});
  add_line(sketch.value(), "outer.top", Point2{20.0, 10.0}, Point2{-20.0, 10.0});
  add_line(sketch.value(), "outer.left", Point2{-20.0, 10.0}, Point2{-20.0, -10.0});
  add_line(sketch.value(), "inner.bottom", Point2{-5.0, -3.0}, Point2{5.0, -3.0});
  add_line(sketch.value(), "inner.right", Point2{5.0, -3.0}, Point2{5.0, 3.0});
  add_line(sketch.value(), "inner.top", Point2{5.0, 3.0}, Point2{-5.0, 3.0});
  add_line(sketch.value(), "inner.left", Point2{-5.0, 3.0}, Point2{-5.0, -3.0});
  return sketch.value();
}

} // namespace

TEST_CASE("CompositeClosedProfile stores outer and inner contour intent", "[core][composite-profile]") {
  auto profile = CompositeClosedProfile::create(
      ProfileId("profile.composite"),
      {SketchEntityId("outer.bottom"), SketchEntityId("outer.right"), SketchEntityId("outer.top"),
       SketchEntityId("outer.left")},
      {{SketchEntityId("inner.bottom"), SketchEntityId("inner.right"), SketchEntityId("inner.top"),
        SketchEntityId("inner.left")}});
  REQUIRE(profile);
  CHECK(profile.value().outer_contour().size() == 4U);
  REQUIRE(profile.value().inner_contours().size() == 1U);
  CHECK(profile.value().inner_contours().front().size() == 4U);
}

TEST_CASE("Sketch accepts a valid composite closed profile", "[core][composite-profile]") {
  Sketch sketch = make_profile_sketch();
  auto profile = CompositeClosedProfile::create(
      ProfileId("profile.composite"),
      {SketchEntityId("outer.bottom"), SketchEntityId("outer.right"), SketchEntityId("outer.top"),
       SketchEntityId("outer.left")},
      {{SketchEntityId("inner.bottom"), SketchEntityId("inner.right"), SketchEntityId("inner.top"),
        SketchEntityId("inner.left")}});
  REQUIRE(profile);
  auto added = sketch.add_profile(profile.value());
  REQUIRE(added);
  REQUIRE(sketch.find_composite_closed_profile(ProfileId("profile.composite")) != nullptr);
  CHECK(sketch.composite_closed_profiles().size() == 1U);
}

TEST_CASE("CompositeClosedProfile rejects shared contour line ids", "[core][composite-profile]") {
  auto profile = CompositeClosedProfile::create(
      ProfileId("profile.invalid"),
      {SketchEntityId("outer.bottom"), SketchEntityId("outer.right"), SketchEntityId("outer.top")},
      {{SketchEntityId("outer.bottom"), SketchEntityId("inner.right"), SketchEntityId("inner.top")}});
  REQUIRE_FALSE(profile);
  CHECK(profile.error().message() == "composite closed profile contours must not share line segment ids");
}
