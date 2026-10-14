#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_contour.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"

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

TEST_CASE("Sketch stores trim and extend metadata for explicit curve entities",
          "[core][sketch][arc]") {
  Sketch sketch = make_arc_sketch();
  auto trim = SketchTrimExtendOperation::create_trim(SketchTrimOperationId("trim.arc.top"),
                                                     SketchEntityId("arc.top"), Point2{8.0, 1.0});
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

TEST_CASE("Sketch rejects self-intersecting line and arc closed profiles", "[core][sketch][arc]") {
  auto sketch = Sketch::create(SketchId("sketch.arc.cross"), "ArcCross", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.bottom", Point2{0.0, 0.0}, Point2{10.0, 0.0});
  add_line(sketch.value(), "line.right", Point2{10.0, 0.0}, Point2{10.0, 10.0});
  add_arc(sketch.value(), "arc.crossing", Point2{10.0, 10.0}, Point2{5.0, -1.0}, Point2{0.0, 10.0});
  add_line(sketch.value(), "line.left", Point2{0.0, 10.0}, Point2{0.0, 0.0});

  auto profile = ArcClosedProfile::create(
      ProfileId("profile.crossing"), {SketchEntityId("line.bottom"), SketchEntityId("line.right"),
                                      SketchEntityId("arc.crossing"), SketchEntityId("line.left")});
  REQUIRE(profile);
  auto added = sketch.value().add_profile(profile.value());
  REQUIRE_FALSE(added);
  CHECK(added.error().message() == "arc closed profile curve segments must not self-intersect");
}

TEST_CASE("Curved profiles traverse reversed source curves without changing endpoint identity",
          "[core][sketch][arc][sketch-curved-regions]") {
  auto sketch = Sketch::create(SketchId("semicircle"), "Semicircle", DatumPlaneId("datum.xy")).value();
  // Both authored edges have the same endpoint direction. The profile needs
  // the arc in reverse; reversing the authored arc would invalidate references.
  add_line(sketch, "base", {-10, 0}, {10, 0});
  add_arc(sketch, "top", {-10, 0}, {0, 10}, {10, 0});
  const auto original = *sketch.find_arc_segment(SketchEntityId("top"));
  auto profile = ArcClosedProfile::create(ProfileId("profile"), {SketchEntityId("base"), SketchEntityId("top")});
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
  auto traversal = resolve_sketch_contour(sketch, profile.value().curve_segments());
  REQUIRE(traversal);
  CHECK_FALSE(traversal.value()[0].reversed);
  CHECK(traversal.value()[1].reversed);
  auto topology = SketchTopology::migrate_legacy(sketch);
  REQUIRE(topology);
  const auto* base = topology.value().find_entity("entity/base");
  const auto* top = topology.value().find_entity("entity/top");
  REQUIRE(base);
  REQUIRE(top);
  CHECK(base->points().front() == top->points().front());
  CHECK(base->points().back() == top->points().back());
  CHECK(topology.value().points().size() == 3U);
  auto restored = SketchTopologyLegacyMaterializer{}.materialize(sketch, topology.value());
  REQUIRE(restored);
  CHECK(restored.value().find_arc_segment(SketchEntityId("top"))->start().x == original.start().x);
  CHECK(restored.value().find_arc_segment(SketchEntityId("top"))->end().x == original.end().x);
  CHECK(SketchTopology::migrate_legacy(restored.value()).value() == topology.value());
}

TEST_CASE("Two circular arcs can bound a full circle and open contours still fail",
          "[core][sketch][arc][sketch-curved-regions]") {
  auto sketch = Sketch::create(SketchId("circle"), "Circle", DatumPlaneId("datum.xy")).value();
  add_arc(sketch, "top", {-10, 0}, {0, 10}, {10, 0});
  add_arc(sketch, "bottom", {10, 0}, {0, -10}, {-10, 0});
  auto profile = ArcClosedProfile::create(ProfileId("profile"), {SketchEntityId("top"), SketchEntityId("bottom")});
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
  CHECK(sketch.arc_closed_profile_vertices(profile.value()).value().size() == 2U);
  auto open = Sketch::create(SketchId("open"), "Open", DatumPlaneId("datum.xy")).value();
  add_arc(open, "top", {-10, 0}, {0, 10}, {10, 0});
  add_line(open, "base", {-10, 0}, {9, 0});
  auto gap = ArcClosedProfile::create(ProfileId("gap"), {SketchEntityId("top"), SketchEntityId("base")});
  REQUIRE(gap);
  CHECK_FALSE(open.add_profile(gap.value()));
  add_line(open, "back", {9, 0}, {-10, 0});
  auto flat = ArcClosedProfile::create(ProfileId("flat"), {SketchEntityId("base"), SketchEntityId("back")});
  REQUIRE(flat);
  CHECK_FALSE(open.add_profile(flat.value()));
  auto overlap = Sketch::create(SketchId("overlap"), "Overlap", DatumPlaneId("datum.xy")).value();
  add_arc(overlap, "a", {-10, 0}, {0, 10}, {10, 0});
  add_arc(overlap, "b", {10, 0}, {0, 10}, {-10, 0});
  auto doubled = ArcClosedProfile::create(ProfileId("overlap"), {SketchEntityId("a"), SketchEntityId("b")});
  REQUIRE(doubled);
  CHECK_FALSE(overlap.add_profile(doubled.value()));
}
