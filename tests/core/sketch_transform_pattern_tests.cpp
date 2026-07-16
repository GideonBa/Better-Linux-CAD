#include "blcad/core/sketch_transform_pattern.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {
Sketch make_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.transform-pattern"), "Transform Pattern",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  return std::move(sketch.value());
}
void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId(id), start, end).value()));
}
} // namespace

TEST_CASE("Block 118 move rotate and scale preserve selected entity identity",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_sketch();
  add_line(sketch, "line.a", {0.0, 0.0}, {2.0, 0.0});

  auto moved = SketchTransformPatternService::move(
      sketch, {SketchEntityId("line.a")}, {3.0, 4.0});
  REQUIRE(moved);
  const auto* moved_line = moved.value().sketch.find_line_segment(SketchEntityId("line.a"));
  REQUIRE(moved_line != nullptr);
  CHECK(moved_line->start().x == Catch::Approx(3.0));
  CHECK(moved_line->start().y == Catch::Approx(4.0));

  auto rotated = SketchTransformPatternService::rotate(
      sketch, {SketchEntityId("line.a")}, {0.0, 0.0}, 3.14159265358979323846 / 2.0);
  REQUIRE(rotated);
  const auto* rotated_line = rotated.value().sketch.find_line_segment(SketchEntityId("line.a"));
  REQUIRE(rotated_line != nullptr);
  CHECK(rotated_line->end().x == Catch::Approx(0.0).margin(1.0e-9));
  CHECK(rotated_line->end().y == Catch::Approx(2.0));

  auto scaled = SketchTransformPatternService::scale(
      sketch, {SketchEntityId("line.a")}, {0.0, 0.0}, 2.5);
  REQUIRE(scaled);
  const auto* scaled_line = scaled.value().sketch.find_line_segment(SketchEntityId("line.a"));
  REQUIRE(scaled_line != nullptr);
  CHECK(scaled_line->end().x == Catch::Approx(5.0));
}

TEST_CASE("Block 118 copy and mirror create deterministic ordinary curves",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_sketch();
  add_line(sketch, "line.a", {1.0, 1.0}, {3.0, 1.0});

  auto copied = SketchTransformPatternService::copy(
      sketch, {SketchEntityId("line.a")}, {0.0, 5.0});
  REQUIRE(copied);
  REQUIRE(copied.value().created_entities.size() == 1U);
  CHECK(copied.value().created_entities.front().value() == "line.a.copy.1");
  const auto* copied_line = copied.value().sketch.find_line_segment(
      copied.value().created_entities.front());
  REQUIRE(copied_line != nullptr);
  CHECK(copied_line->start().y == Catch::Approx(6.0));

  auto mirrored = SketchTransformPatternService::mirror(
      sketch, {SketchEntityId("line.a")}, {0.0, 0.0}, {0.0, 1.0});
  REQUIRE(mirrored);
  const auto* mirrored_line = mirrored.value().sketch.find_line_segment(
      mirrored.value().created_entities.front());
  REQUIRE(mirrored_line != nullptr);
  CHECK(mirrored_line->start().x == Catch::Approx(-1.0));
  CHECK(mirrored_line->end().x == Catch::Approx(-3.0));
}

TEST_CASE("Block 118 rectangular and circular patterns distinguish associative and exploded modes",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_sketch();
  add_line(sketch, "line.seed", {1.0, 0.0}, {2.0, 0.0});

  auto rectangular = SketchTransformPatternService::rectangular_pattern(
      sketch, {SketchEntityId("line.seed")}, {4.0, 0.0}, 3U, {0.0, 5.0}, 2U,
      SketchPatternMode::Associative, "pattern.grid");
  REQUIRE(rectangular);
  CHECK(rectangular.value().created_entities.size() == 5U);
  REQUIRE(rectangular.value().pattern_intent.has_value());
  CHECK(rectangular.value().pattern_intent->id == "pattern.grid");
  CHECK(rectangular.value().pattern_intent->first_count == 3U);
  CHECK(rectangular.value().pattern_intent->second_count == 2U);

  auto circular = SketchTransformPatternService::circular_pattern(
      sketch, {SketchEntityId("line.seed")}, {0.0, 0.0}, 4U,
      3.14159265358979323846 * 1.5, SketchPatternMode::Exploded);
  REQUIRE(circular);
  CHECK(circular.value().created_entities.size() == 3U);
  CHECK_FALSE(circular.value().pattern_intent.has_value());
}

TEST_CASE("Block 118 rejects invalid selections and degenerate transforms",
          "[core][sketch-transform-pattern]") {
  auto sketch = make_sketch();
  add_line(sketch, "line.a", {0.0, 0.0}, {1.0, 0.0});
  CHECK_FALSE(SketchTransformPatternService::move(sketch, {}, {1.0, 0.0}));
  CHECK_FALSE(SketchTransformPatternService::scale(
      sketch, {SketchEntityId("line.a")}, {0.0, 0.0}, 0.0));
  CHECK_FALSE(SketchTransformPatternService::mirror(
      sketch, {SketchEntityId("line.a")}, {0.0, 0.0}, {0.0, 0.0}));
  CHECK_FALSE(SketchTransformPatternService::copy(
      sketch, {SketchEntityId("missing")}, {1.0, 0.0}));
}
