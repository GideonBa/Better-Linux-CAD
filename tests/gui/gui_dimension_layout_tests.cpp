#include "blcad/gui/gui_sketch_dimension_layout.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
using namespace blcad::geometry;
using Catch::Approx;
namespace {
template <class T> T checked(Result<T> value) {
  if (!value)
    FAIL(value.error().message());
  return std::move(value.value());
}
SketchTopology source_topology() {
  auto sketch = checked(Sketch::create(SketchId("layout"), "Layout", DatumPlaneId("datum.xy")));
  for (const auto& line : std::vector<std::pair<const char*, std::pair<Point2, Point2>>>{
           {"offset", {{0, 0}, {30, 20}}},
           {"base", {{10, 10}, {40, 10}}},
           {"angled", {{20, 0}, {40, 20}}}})
    REQUIRE(sketch.add_entity(checked(
        LineSegment::create(SketchEntityId(line.first), line.second.first, line.second.second))));
  const double h = std::sqrt(50.0);
  REQUIRE(sketch.add_entity(checked(
      ArcSegment::create_three_point(SketchEntityId("left.arc"), {-10, 0}, {-h, h}, {0, 10}))));
  return checked(SketchTopology::migrate_legacy(sketch));
}
SketchDimensionGlyph glyph(const SketchTopology& topology, SketchDimensionKind kind) {
  std::vector<SketchConstraintIntentTarget> targets;
  if (kind == SketchDimensionKind::Angle) {
    targets = {checked(SketchConstraintIntentTarget::entity("entity/base")),
               checked(SketchConstraintIntentTarget::entity("entity/angled"))};
  } else if (kind == SketchDimensionKind::Radius || kind == SketchDimensionKind::Diameter ||
             kind == SketchDimensionKind::ArcLength) {
    targets = {checked(SketchConstraintIntentTarget::entity("entity/left.arc"))};
  } else if (kind == SketchDimensionKind::Length)
    targets = {checked(SketchConstraintIntentTarget::entity("entity/offset"))};
  else {
    const auto* line = topology.find_entity("entity/offset");
    REQUIRE(line);
    targets = {checked(SketchConstraintIntentTarget::point(line->points()[0])),
               checked(SketchConstraintIntentTarget::point(line->points()[1]))};
  }
  const auto intent = checked(SketchDimensionIntent::create(
      SketchDimensionId("dimension"), kind, SketchDimensionMode::Reference, targets));
  return checked(SketchDimensionGlyphLayoutResolver{}.resolve(
      topology, intent, checked(SketchDimensionMeasurementEvaluator::measure(topology, intent))));
}
GuiSketchPlaneMapping mapping(double units_per_dip) {
  const GuiSketchPlaneView plane{
      DatumPlaneId("oblique"), {7, 11, 13}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}};
  return checked(
      GuiSketchPlaneMapping::create_orthographic(plane, 800, 600, {10, 10}, units_per_dip));
}
GuiSketchAnnotationPrimitive annotation(const SketchDimensionLayout& layout) {
  GuiSketchAnnotationPrimitive result{"sketch/layout/dimension/test", "test", layout.label_anchor,
                                      GuiSketchHitKind::Dimension, "30 mm"};
  result.dimension_layout = layout;
  result.label_bounds_dip = GuiSketchScreenRect{{-2, -2}, {2, 2}};
  return result;
}
void near(Point2 actual, Point2 expected) {
  CHECK(actual.x == Approx(expected.x).margin(1e-9));
  CHECK(actual.y == Approx(expected.y).margin(1e-9));
}
} // namespace

TEST_CASE("Projected dimensions retain true references with separate measurement and witness lines",
          "[gui][dimension-layout][dimension-layout-projection]") {
  const auto topology = source_topology();
  for (const auto kind :
       {SketchDimensionKind::HorizontalDistance, SketchDimensionKind::VerticalDistance,
        SketchDimensionKind::AlignedDistance, SketchDimensionKind::Length}) {
    const auto value = glyph(topology, kind);
    const auto placed = place_dimension_layout(value.layout.geometry, {50, 60});
    REQUIRE(placed.dimension_lines.size() == 1);
    REQUIRE(placed.extension_lines.size() == 2);
    near(placed.extension_lines[0].start, {0, 0});
    near(placed.extension_lines[1].start, {30, 20});
    const auto line = placed.dimension_lines.front();
    if (kind == SketchDimensionKind::HorizontalDistance) {
      near(line.start, {0, 60});
      near(line.end, {30, 60});
      CHECK(value.value_text == "(30.000 mm)");
    } else if (kind == SketchDimensionKind::VerticalDistance) {
      near(line.start, {50, 0});
      near(line.end, {50, 20});
      CHECK(value.value_text == "(20.000 mm)");
    } else {
      const auto span = dimension_layout_detail::subtract(line.end, line.start);
      CHECK(std::hypot(span.x, span.y) == Approx(std::sqrt(1300.0)));
      CHECK(span.x * 20 - span.y * 30 == Approx(0).margin(1e-8));
      const auto extension = dimension_layout_detail::subtract(line.start, {0, 0});
      CHECK(extension.x * 30 + extension.y * 20 == Approx(0).margin(1e-8));
    }
    CHECK(placed.geometry == value.layout.geometry);
  }
}

TEST_CASE("Angular dimensions use the real intersection and a circular measurement arc",
          "[gui][dimension-layout][dimension-layout-angle]") {
  const auto value = glyph(source_topology(), SketchDimensionKind::Angle);
  REQUIRE(value.layout.dimension_arcs.size() == 1);
  CHECK(value.layout.dimension_lines.empty());
  const auto arc = value.layout.dimension_arcs.front();
  near(arc.center, {30, 10});
  CHECK(arc.start_angle == Approx(0));
  CHECK(arc.sweep_angle == Approx(std::numbers::pi / 4));
  REQUIRE(value.layout.terminators.size() == 2);
  CHECK(value.value_text == "(45.000 deg)");
  const auto moved = place_dimension_layout(value.layout.geometry, {30, 40});
  CHECK(moved.dimension_arcs.front().radius == Approx(30));
  near(moved.dimension_arcs.front().center, {30, 10});
  for (const auto& witness : moved.extension_lines) {
    const auto a = dimension_layout_detail::subtract(witness.start, arc.center);
    const auto b = dimension_layout_detail::subtract(witness.end, arc.center);
    CHECK(a.x * b.y - a.y * b.x == Approx(0).margin(1e-8));
  }
}

TEST_CASE("Arc radius and length use actual swept geometry away from the right quadrant",
          "[gui][dimension-layout][dimension-layout-arc]") {
  const auto radius = glyph(source_topology(), SketchDimensionKind::Radius);
  const auto placed = place_dimension_layout(radius.layout.geometry, {25, 1});
  REQUIRE(placed.terminators.size() == 1);
  const auto tip = placed.terminators.front().tip;
  CHECK(tip.x <= 1e-9);
  CHECK(tip.y >= -1e-9);
  CHECK(std::hypot(tip.x, tip.y) == Approx(10));
  near(placed.dimension_lines.front().start, {0, 0});
  const auto length = glyph(source_topology(), SketchDimensionKind::ArcLength);
  CHECK(length.value_text == "(15.708 mm)");
  CHECK(GuiSketchDimensionController::annotation_label(length).starts_with("⌒ "));
  const double h = std::sqrt(200.0);
  const auto moved = place_dimension_layout(length.layout.geometry, {-h, h});
  REQUIRE(moved.dimension_arcs.size() == 1);
  CHECK(moved.dimension_lines.empty());
  CHECK(moved.dimension_arcs.front().radius == Approx(20));
  CHECK(moved.dimension_arcs.front().sweep_angle == Approx(-std::numbers::pi / 2));
  REQUIRE(moved.extension_lines.size() == 2);
  near(moved.extension_lines[0].start, {-10, 0});
  near(moved.extension_lines[1].start, {0, 10});
}

TEST_CASE("Diameter spans opposite circle boundaries and has two terminators",
          "[gui][dimension-layout][dimension-layout-diameter]") {
  SketchDimensionGeometry geometry;
  geometry.presentation = SketchDimensionPresentation::Diameter;
  geometry.center = {50, 20};
  geometry.radius = 12;
  const auto layout = place_dimension_layout(geometry, {80, 20});
  REQUIRE(layout.dimension_lines.size() == 1);
  REQUIRE(layout.terminators.size() == 2);
  near(layout.dimension_lines.front().start, {38, 20});
  near(layout.dimension_lines.front().end, {62, 20});
  CHECK(layout.origin_marks.empty());
  REQUIRE(layout.label_leaders.size() == 1);
  near(layout.label_leaders.front().start, {62, 20});
  near(layout.label_leaders.front().end, {80, 20});
}

TEST_CASE("Projected witness gaps and arrow sizes remain logical DIPs at multiple zoom levels",
          "[gui][dimension-layout][dimension-layout-zoom]") {
  const auto units = GENERATE(.2, 1.0, 5.0);
  const auto view = mapping(units);
  const auto value = glyph(source_topology(), SketchDimensionKind::HorizontalDistance);
  const auto placed = place_dimension_layout(value.layout.geometry, {15, 80});
  const auto resolved = checked(resolve_dimension_screen_layout(annotation(placed), view));
  REQUIRE(resolved.arrows.size() == 2);
  for (const auto& arrow : resolved.arrows) {
    const auto& p = arrow.polygon;
    const GuiSketchScreenPoint base{(p[1].x + p[2].x) / 2, (p[1].y + p[2].y) / 2};
    CHECK(std::hypot(base.x - p[0].x, base.y - p[0].y) == Approx(7));
    CHECK(
        hit_dimension_screen_layout(resolved, {(p[0].x + base.x) / 2, (p[0].y + base.y) / 2}, .1));
  }
  std::size_t witnesses = 0;
  for (const auto& stroke : resolved.strokes)
    if (stroke.role == GuiDimensionStrokeRole::Extension) {
      const auto original = checked(view.plane_to_screen(placed.extension_lines[witnesses].start));
      const auto end = checked(view.plane_to_screen(placed.extension_lines[witnesses].end));
      CHECK(std::hypot(original.x - stroke.points.front().x,
                       original.y - stroke.points.front().y) == Approx(3));
      CHECK(std::hypot(end.x - stroke.points.back().x, end.y - stroke.points.back().y) ==
            Approx(5));
      ++witnesses;
    }
  CHECK(witnesses == 2);
}

TEST_CASE("Moved labels reflow painted hit geometry without phantom diagonals or arc chords",
          "[gui][dimension-layout][dimension-layout-hit]") {
  const auto view = mapping(.5);
  const auto value = glyph(source_topology(), SketchDimensionKind::HorizontalDistance);
  auto display = annotation(value.layout);
  display.point = {50, 60};
  const auto resolved = checked(resolve_dimension_screen_layout(display, view));
  CHECK(hit_dimension_screen_layout(resolved, checked(view.plane_to_screen({15, 60})), 1));
  CHECK_FALSE(hit_dimension_screen_layout(resolved, checked(view.plane_to_screen({15, 10})), 1));
  const auto arc = glyph(source_topology(), SketchDimensionKind::ArcLength);
  const double h = std::sqrt(200.0);
  auto shown = annotation(place_dimension_layout(arc.layout.geometry, {-h, h}));
  const auto curved = checked(resolve_dimension_screen_layout(shown, view));
  CHECK_FALSE(hit_dimension_screen_layout(curved, checked(view.plane_to_screen({-10, 10})), 1));
  CHECK(hit_dimension_screen_layout(curved, checked(view.plane_to_screen({-20, 0})), 1));
  for (const auto& stroke : curved.strokes)
    if (stroke.role == GuiDimensionStrokeRole::Dimension) {
      for (const auto p : stroke.plane_points)
        CHECK(std::hypot(p.x, p.y) == Approx(20));
    }
  CHECK_FALSE(
      resolve_dimension_screen_layout(shown, view, GuiDimensionLayoutMetrics{-1, 3, 5, .25}));
}

TEST_CASE("Dimension value rectangles stay clear of the measured line and are the authoritative "
          "hit targets",
          "[gui][dimension-layout][dimension-layout-text]") {
  const auto kind = GENERATE(
      SketchDimensionKind::HorizontalDistance, SketchDimensionKind::VerticalDistance,
      SketchDimensionKind::AlignedDistance, SketchDimensionKind::Radius,
      SketchDimensionKind::Diameter, SketchDimensionKind::Angle, SketchDimensionKind::ArcLength);
  const auto value = glyph(source_topology(), kind);
  const auto view = mapping(1);
  auto display = GuiSketchDimensionController::annotation_for(value);
  display.label_bounds_dip = GuiSketchScreenRect{{-32, -10}, {32, 10}};
  const auto layout = checked(resolve_dimension_screen_layout(display, view));
  REQUIRE(layout.text_bounds);
  for (const auto& arrow : layout.arrows)
    for (const auto& p : arrow.polygon)
      CHECK_FALSE(layout.text_bounds->contains(p));
  GuiSketchInteractionScene scene;
  scene.annotations.push_back(display);
  auto controller = checked(GuiSketchInteractionController::create(view, scene));
  const auto hits = checked(controller.hits_at(layout.label_anchor));
  REQUIRE_FALSE(hits.empty());
  CHECK(hits.front().semantic_id == display.semantic_id);
  CHECK(hits.front().label_hit);
}
