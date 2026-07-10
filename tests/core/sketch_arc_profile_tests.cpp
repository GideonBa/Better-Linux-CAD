#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

void add_arc(Sketch& sketch, const char* id, Point2 start, Point2 mid, Point2 end) {
  auto arc = ArcSegment::create_three_point(SketchEntityId(id), start, mid, end);
  REQUIRE(arc);
  REQUIRE(sketch.add_entity(arc.value()));
}

Sketch make_arc_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.arc"), "Arc", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.left", Point2{-10.0, 0.0}, Point2{-10.0, -10.0});
  add_line(sketch.value(), "line.bottom", Point2{-10.0, -10.0}, Point2{10.0, -10.0});
  add_line(sketch.value(), "line.right", Point2{10.0, -10.0}, Point2{10.0, 0.0});
  add_arc(sketch.value(), "arc.top", Point2{10.0, 0.0}, Point2{0.0, 10.0}, Point2{-10.0, 0.0});
  return sketch.value();
}

} // namespace

TEST_CASE("ArcSegment rejects collinear three-point arcs", "[core][sketch][arc]") {
  auto arc = ArcSegment::create_three_point(SketchEntityId("arc.bad"), Point2{0.0, 0.0},
                                            Point2{1.0, 0.0}, Point2{2.0, 0.0});
  REQUIRE_FALSE(arc);
  CHECK(arc.error().message() == "arc segment three-point definition must not be collinear");
}

TEST_CASE("Sketch stores trim and extend metadata for explicit curve entities", "[core][sketch][arc]") {
  Sketch sketch = make_arc_sketch();
  auto trim = SketchTrimExtendOperation::create_trim(
      SketchTrimOperationId("trim.arc.top"), SketchEntityId("arc.top"), Point2{8.0, 1.0});
  REQUIRE(trim);
  REQUIRE(sketch.add_trim_extend_operation(trim.value()));
  CHECK(sketch.trim_extend_operations().size() == 1U);
  REQUIRE(sketch.find_trim_extend_operation(SketchTrimOperationId("trim.arc.top")) != nullptr);
}

TEST_CASE("Sketch validates ordered line and arc closed profiles", "[core][sketch][arc]") {
  Sketch sketch = make_arc_sketch();
  auto profile = ArcClosedProfile::create(
      ProfileId("profile.arc"), {SketchEntityId("line.left"), SketchEntityId("line.bottom"),
                                  SketchEntityId("line.right"), SketchEntityId("arc.top")});
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
  CHECK(sketch.arc_closed_profiles().size() == 1U);
  auto vertices = sketch.arc_closed_profile_vertices(sketch.arc_closed_profiles().front());
  REQUIRE(vertices);
  CHECK(vertices.value().size() == 4U);
}
