#include "blcad/core/sketch_modify.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace blcad;

namespace {

Sketch make_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.modify"), "Modify", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  return std::move(sketch.value());
}

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId(id), start, end).value()));
}

const LineSegment* line(const Sketch& sketch, const char* id) {
  return sketch.find_line_segment(SketchEntityId(id));
}

bool same(Point2 first, Point2 second) {
  return std::hypot(first.x - second.x, first.y - second.y) <= 1.0e-6;
}

} // namespace

TEST_CASE("Block 116 trim shortens a line to the picked bounding intersection", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.v", {4.0, -5.0}, {4.0, 5.0});

  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.h"), {8.0, 0.0});
  REQUIRE(result);
  CHECK(result.value().invalidated_ids.empty());
  const auto* trimmed = line(result.value().sketch, "line.h");
  REQUIRE(trimmed != nullptr);
  CHECK(same(trimmed->start(), {0.0, 0.0}));
  CHECK(same(trimmed->end(), {4.0, 0.0}));
  CHECK(result.value().sketch.line_segments().size() == 2U);
}

TEST_CASE("Block 116 trim removes the middle span between two intersections as a split",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {12.0, 0.0});
  add_line(sketch, "line.a", {3.0, -2.0}, {3.0, 2.0});
  add_line(sketch, "line.b", {9.0, -2.0}, {9.0, 2.0});

  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.h"), {6.0, 0.0});
  REQUIRE(result);
  CHECK(std::find(result.value().invalidated_ids.begin(), result.value().invalidated_ids.end(),
                  "line.h") != result.value().invalidated_ids.end());
  const auto* first = line(result.value().sketch, "line.h");
  const auto* second = line(result.value().sketch, "line.h.split.1");
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(same(first->end(), {3.0, 0.0}));
  CHECK(same(second->start(), {9.0, 0.0}));
}

TEST_CASE("Block 116 trim removes a line with no bounding intersection", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.h"), {5.0, 0.0});
  REQUIRE(result);
  CHECK(result.value().sketch.line_segments().empty());
}

TEST_CASE("Block 116 extend lengthens the picked end to the next intersection",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {4.0, 0.0});
  add_line(sketch, "line.v", {8.0, -3.0}, {8.0, 3.0});

  auto result = SketchModifyService::extend(sketch, SketchEntityId("line.h"), {4.0, 0.0});
  REQUIRE(result);
  const auto* extended = line(result.value().sketch, "line.h");
  REQUIRE(extended != nullptr);
  CHECK(same(extended->start(), {0.0, 0.0}));
  CHECK(same(extended->end(), {8.0, 0.0}));
}

TEST_CASE("Block 116 split divides a line into two entities at the projected point",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  auto result = SketchModifyService::split(sketch, SketchEntityId("line.h"), {6.0, 1.0});
  REQUIRE(result);
  const auto* first = line(result.value().sketch, "line.h");
  const auto* second = line(result.value().sketch, "line.h.split.1");
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(same(first->end(), {6.0, 0.0}));
  CHECK(same(second->start(), {6.0, 0.0}));
  CHECK(same(second->end(), {10.0, 0.0}));
}

TEST_CASE("Block 116 fillet trims two lines and inserts a tangent arc", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.v", {0.0, 0.0}, {0.0, 10.0});

  auto result = SketchModifyService::fillet(sketch, SketchEntityId("line.h"),
                                            SketchEntityId("line.v"), 2.0);
  REQUIRE(result);
  const Sketch& modified = result.value().sketch;
  CHECK(modified.arc_segments().size() == 1U);
  const auto* trimmed_h = line(modified, "line.h");
  const auto* trimmed_v = line(modified, "line.v");
  REQUIRE(trimmed_h != nullptr);
  REQUIRE(trimmed_v != nullptr);
  CHECK(same(trimmed_h->start(), {2.0, 0.0}));
  CHECK(same(trimmed_v->end(), {0.0, 10.0}));
  CHECK(same(trimmed_v->start(), {0.0, 2.0}));
  const auto& arc = modified.arc_segments().front();
  CHECK((same(arc.start(), {2.0, 0.0}) || same(arc.start(), {0.0, 2.0})));
}

TEST_CASE("Block 116 chamfer trims two lines and inserts a connecting line",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.v", {0.0, 0.0}, {0.0, 10.0});

  auto result = SketchModifyService::chamfer(sketch, SketchEntityId("line.h"),
                                             SketchEntityId("line.v"), 3.0);
  REQUIRE(result);
  const Sketch& modified = result.value().sketch;
  CHECK(modified.line_segments().size() == 3U);
  const auto* chamfer = line(modified, "line.chamfer.1");
  REQUIRE(chamfer != nullptr);
  CHECK((same(chamfer->start(), {3.0, 0.0}) || same(chamfer->start(), {0.0, 3.0})));
  CHECK((same(chamfer->end(), {3.0, 0.0}) || same(chamfer->end(), {0.0, 3.0})));
}

TEST_CASE("Block 116 preserves an orientation constraint on an in-place trimmed line",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.v", {4.0, -5.0}, {4.0, 5.0});
  REQUIRE(sketch.add_constraint(
      SketchGeometricConstraint::create_horizontal(
          SketchConstraintId("horizontal.h"),
          SketchReferenceTarget::create_line_segment(SketchEntityId("line.h")).value())
          .value()));

  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.h"), {8.0, 0.0});
  REQUIRE(result);
  CHECK(result.value().sketch.geometric_constraints().size() == 1U);
}

TEST_CASE("Block 116 rejects splitting a line referenced by a dimension", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  REQUIRE(sketch.add_dimension(
      SketchDrivingDimension::create_point_to_point_distance(
          SketchDimensionId("length.h"),
          SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.h")).value(),
          SketchReferenceTarget::create_line_segment_end(SketchEntityId("line.h")).value(),
          ParameterId("length"))
          .value()));

  auto result = SketchModifyService::split(sketch, SketchEntityId("line.h"), {5.0, 0.0});
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("length.h") != std::string::npos);
}

TEST_CASE("Block 116 rejects trimming an entity used by a profile", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.a", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.b", {10.0, 0.0}, {10.0, 10.0});
  add_line(sketch, "line.c", {10.0, 10.0}, {0.0, 10.0});
  add_line(sketch, "line.d", {0.0, 10.0}, {0.0, 0.0});
  REQUIRE(sketch.add_profile(
      ClosedProfile::create(ProfileId("profile.rect"),
                            {SketchEntityId("line.a"), SketchEntityId("line.b"),
                             SketchEntityId("line.c"), SketchEntityId("line.d")})
          .value()));

  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.a"), {5.0, 0.0});
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("profile") != std::string::npos);
}

TEST_CASE("Block 116 rejects degenerate and unknown modify requests", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.p", {0.0, 2.0}, {10.0, 2.0});

  CHECK_FALSE(SketchModifyService::trim(sketch, SketchEntityId("missing"), {1.0, 0.0}));
  CHECK_FALSE(SketchModifyService::fillet(sketch, SketchEntityId("line.h"),
                                          SketchEntityId("line.p"), 1.0));
  CHECK_FALSE(SketchModifyService::fillet(sketch, SketchEntityId("line.h"),
                                          SketchEntityId("line.h"), 1.0));
  CHECK_FALSE(SketchModifyService::chamfer(sketch, SketchEntityId("line.h"),
                                           SketchEntityId("line.p"), -1.0));
}
