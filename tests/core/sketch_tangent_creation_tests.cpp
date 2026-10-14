#include "blcad/core/sketch_tangent_creation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("Tangent construction continues line, arc and spline endpoints through a chosen point",
          "[core][sketch-conics][sketch-tangent-create]") {
  auto sketch = Sketch::create(SketchId("tangent"), "Tangent", DatumPlaneId("datum.xy")).value();
  const SketchEntityId id("source");
  Point2 endpoint{0, 0};
  SECTION("line") {
    REQUIRE(sketch.add_entity(LineSegment::create(id, {-10, 0}, endpoint).value()));
  }
  SECTION("arc") {
    REQUIRE(
        sketch.add_entity(ArcSegment::create_three_point(
                              id, {-10, -10}, {-7.0710678118654755, -2.9289321881345245}, endpoint)
                              .value()));
  }
  SECTION("polynomial spline") {
    REQUIRE(sketch.add_entity(
        SplineSegment::create_cubic_bezier(id, {-10, -10}, {-10, 0}, {-5, 0}, endpoint).value()));
  }
  SECTION("rational ellipse") {
    auto ellipse = SplineSegment::create_elliptical_arc(id, {}, {10, 0}, 5, 0, std::acos(-1.0) / 2);
    REQUIRE(ellipse);
    endpoint = ellipse.value().end();
    REQUIRE(sketch.add_entity(ellipse.value()));
  }
  auto anchor = SketchTangentCreation::anchor(sketch, id, endpoint, true);
  REQUIRE(anchor);
  REQUIRE(anchor.value().endpoint);
  const auto p = anchor.value().point;
  const auto t = anchor.value().direction;
  const Point2 rim{p.x + 5 * t.x - 5 * t.y, p.y + 5 * t.y + 5 * t.x};
  auto circle = SketchTangentCreation::circle(anchor.value(), rim);
  REQUIRE(circle);
  const auto c = circle.value().center;
  CHECK(std::hypot(p.x - c.x, p.y - c.y) == Catch::Approx(circle.value().radius));
  CHECK(std::hypot(rim.x - c.x, rim.y - c.y) == Catch::Approx(circle.value().radius));
  CHECK((p.x - c.x) * t.x + (p.y - c.y) * t.y == Catch::Approx(0).margin(1e-10));
  auto arc = SketchTangentCreation::arc(SketchEntityId("new.arc"), anchor.value(), rim);
  REQUIRE(arc);
  const auto sampler = detail::arc_sampler(arc.value());
  REQUIRE(sampler);
  const auto tangent = detail::sampler_derivative(*sampler, 0);
  CHECK(tangent.x * t.y - tangent.y * t.x == Catch::Approx(0).margin(1e-10));
  CHECK(tangent.x * t.x + tangent.y * t.y > 0); // oriented continuation, including source start
  CHECK(arc.value().end() == rim);
}

TEST_CASE("Tangent creation rejects missing sources and degenerate or ambiguous contact",
          "[core][sketch-conics][sketch-tangent-create]") {
  auto sketch = Sketch::create(SketchId("tangent"), "Tangent", DatumPlaneId("datum.xy")).value();
  const SketchEntityId id("source");
  REQUIRE(sketch.add_entity(LineSegment::create(id, {-10, 0}, {10, 0}).value()));
  CHECK_FALSE(SketchTangentCreation::anchor(sketch, SketchEntityId("missing"), {}, true));
  CHECK_FALSE(SketchTangentCreation::anchor(sketch, id, {}, true));
  auto contact = SketchTangentCreation::anchor(sketch, id, {}, false);
  REQUIRE(contact); // circle can touch the interior of a line
  CHECK_FALSE(contact.value().endpoint);
  CHECK_FALSE(SketchTangentCreation::circle(contact.value(), {}));
  CHECK_FALSE(SketchTangentCreation::circle(contact.value(), {20, 0}));
  REQUIRE(SketchTangentCreation::circle(contact.value(), {10, 10}));
}
