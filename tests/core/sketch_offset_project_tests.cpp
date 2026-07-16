#include "blcad/core/sketch_offset_project.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {
Sketch make_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.offset-project"), "Offset Project",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  return std::move(sketch.value());
}

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId(id), start, end).value()));
}
} // namespace

TEST_CASE("Block 117 offsets an ordered line chain with deterministic miter joints",
          "[core][sketch-offset-project]") {
  auto sketch = make_sketch();
  add_line(sketch, "line.a", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.b", {10.0, 0.0}, {10.0, 8.0});

  auto result = SketchOffsetProjectService::offset_lines(
      sketch, {SketchEntityId("line.a"), SketchEntityId("line.b")}, 2.0);
  REQUIRE(result);
  REQUIRE(result.value().created_entities.size() == 2U);
  const auto* first = result.value().sketch.find_line_segment(result.value().created_entities[0]);
  const auto* second = result.value().sketch.find_line_segment(result.value().created_entities[1]);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first->start().x == Catch::Approx(0.0));
  CHECK(first->start().y == Catch::Approx(2.0));
  CHECK(first->end().x == Catch::Approx(8.0));
  CHECK(first->end().y == Catch::Approx(2.0));
  CHECK(second->start().x == Catch::Approx(8.0));
  CHECK(second->start().y == Catch::Approx(2.0));
  CHECK(second->end().x == Catch::Approx(8.0));
  CHECK(second->end().y == Catch::Approx(8.0));
}

TEST_CASE("Block 117 rejects disconnected and open-loop offset selections",
          "[core][sketch-offset-project]") {
  auto sketch = make_sketch();
  add_line(sketch, "line.a", {0.0, 0.0}, {4.0, 0.0});
  add_line(sketch, "line.b", {8.0, 0.0}, {8.0, 4.0});
  CHECK_FALSE(SketchOffsetProjectService::offset_lines(
      sketch, {SketchEntityId("line.a"), SketchEntityId("line.b")}, 1.0));

  auto connected = make_sketch();
  add_line(connected, "line.a", {0.0, 0.0}, {4.0, 0.0});
  add_line(connected, "line.b", {4.0, 0.0}, {4.0, 4.0});
  CHECK_FALSE(SketchOffsetProjectService::offset_lines(
      connected, {SketchEntityId("line.a"), SketchEntityId("line.b")}, 1.0, true));
}

TEST_CASE("Block 117 keeps construction projection associative until explicit break-link",
          "[core][sketch-offset-project]") {
  auto sketch = make_sketch();
  auto projected = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("axis.projected"), ConstructionLineId("axis.x"));
  REQUIRE(projected);

  auto included = SketchOffsetProjectService::project_line(sketch, projected.value());
  REQUIRE(included);
  REQUIRE(included.value().sketch.find_projected_line(SketchEntityId("axis.projected")) != nullptr);
  CHECK(included.value().sketch.find_line_segment(SketchEntityId("axis.projected")) == nullptr);

  auto detached = SketchOffsetProjectService::break_link_line(
      included.value().sketch, SketchEntityId("axis.projected"), {-5.0, 0.0}, {5.0, 0.0});
  REQUIRE(detached);
  CHECK(detached.value().sketch.find_projected_line(SketchEntityId("axis.projected")) == nullptr);
  const auto* line = detached.value().sketch.find_line_segment(SketchEntityId("axis.projected"));
  REQUIRE(line != nullptr);
  CHECK(line->start().x == Catch::Approx(-5.0));
  CHECK(line->end().x == Catch::Approx(5.0));
}
