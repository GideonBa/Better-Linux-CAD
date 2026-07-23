#include "blcad/gui/gui_sketch_interaction.hpp"

#include "blcad/core/parameter.hpp"

#include <catch2/catch_test_macros.hpp>


#include <algorithm>
#include <cmath>

using namespace blcad;
using namespace blcad::gui;

namespace {

Parameter length_parameter(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id,
                                  Quantity::length_mm(value, id).value())
      .value();
}

GuiSketchPlaneView xy_plane() {
  return {DatumPlaneId("datum.xy"), {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
          {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
}

GuiSketchPlaneMapping mapping(double units_per_dip = 1.0) {
  return GuiSketchPlaneMapping::create_orthographic(xy_plane(), 200.0, 200.0,
                                                     {0.0, 0.0}, units_per_dip)
      .value();
}

GuiSketchInteractionScene simple_scene() {
  GuiSketchInteractionScene scene;
  scene.sketch = SketchId("sketch.simple");
  scene.curves.push_back({"sketch/sketch.simple/entity/line.a", "line.a:curve",
                          GuiSketchCurveKind::Line, {{-20.0, 0.0}, {20.0, 0.0}}, false});
  scene.curves.push_back({"sketch/sketch.simple/entity/line.b", "line.b:curve",
                          GuiSketchCurveKind::Line, {{7.0, -20.0}, {7.0, 20.0}}, false});
  scene.points.push_back({"sketch/sketch.simple/entity/line.a", "line.a:start",
                          {-20.0, 0.0}, GuiSketchSnapKind::Endpoint});
  scene.points.push_back({"sketch/sketch.simple/entity/line.a", "line.a:midpoint",
                          {0.0, 0.0}, GuiSketchSnapKind::Midpoint});
  scene.annotations.push_back({"sketch/sketch.simple/dimension/dim.a", "dim.a",
                               {0.0, 0.0}, GuiSketchHitKind::Dimension});
  scene.annotations.push_back({"sketch/sketch.simple/constraint/c.a", "c.a",
                               {0.0, 0.0}, GuiSketchHitKind::Glyph});
  scene.intersections.push_back({7.0, 0.0});
  return scene;
}

} // namespace

TEST_CASE("Block 107 plane mapping roundtrips pixels rays plane and model space",
          "[gui][sketch-hit-test]") {
  const auto plane_mapping = mapping(0.5);

  const GuiSketchScreenPoint screen{130.0, 80.0};
  const auto ray = plane_mapping.screen_to_ray(screen);
  REQUIRE(ray);
  CHECK(ray.value().direction == Vector3{0.0, 0.0, 1.0});

  const auto plane_point = plane_mapping.screen_to_plane(screen);
  REQUIRE(plane_point);
  CHECK(plane_point.value() == Point2{15.0, 10.0});

  const auto model_point = plane_mapping.screen_to_model(screen);
  REQUIRE(model_point);
  CHECK(model_point.value() == Point3{15.0, 10.0, 0.0});

  const auto screen_roundtrip = plane_mapping.model_to_screen(model_point.value());
  REQUIRE(screen_roundtrip);
  CHECK(screen_roundtrip.value() == screen);
}

TEST_CASE("Block 107 hit testing keeps point curve dimension glyph priority and cycles stacks",
          "[gui][sketch-hit-test]") {
  auto controller = GuiSketchInteractionController::create(mapping(), simple_scene()).value();

  const auto hits = controller.hits_at({100.0, 100.0});
  REQUIRE(hits);
  REQUIRE(hits.value().size() >= 4U);
  CHECK(hits.value()[0].kind == GuiSketchHitKind::Point);
  CHECK(hits.value()[1].kind == GuiSketchHitKind::Curve);
  CHECK(hits.value()[2].kind == GuiSketchHitKind::Dimension);
  CHECK(hits.value()[3].kind == GuiSketchHitKind::Glyph);

  const auto first = controller.cycle_hit({100.0, 100.0});
  const auto second = controller.cycle_hit({100.0, 100.0});
  const auto third = controller.cycle_hit({100.0, 100.0});
  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(third);
  REQUIRE(first.value().has_value());
  REQUIRE(second.value().has_value());
  REQUIRE(third.value().has_value());
  CHECK(first.value()->kind == GuiSketchHitKind::Point);
  CHECK(second.value()->kind == GuiSketchHitKind::Curve);
  CHECK(third.value()->kind == GuiSketchHitKind::Dimension);

  const auto reset = controller.cycle_hit({130.0, 130.0});
  REQUIRE(reset);
  CHECK_FALSE(reset.value().has_value());
}

TEST_CASE("Block 107 snapping covers origin axes geometry intersections nearest and inference",
          "[gui][sketch-snap]") {
  GuiSketchInteractionConfig config;
  config.grid.snap_enabled = false;
  auto controller =
      GuiSketchInteractionController::create(mapping(), simple_scene(), config).value();

  const auto midpoint = controller.snap({100.0, 100.0});
  REQUIRE(midpoint);
  CHECK(midpoint.value().snapped_point == Point2{0.0, 0.0});
  CHECK(midpoint.value().kind == GuiSketchSnapKind::Midpoint);

  const auto intersection = controller.snap({107.0, 100.0});
  REQUIRE(intersection);
  CHECK(intersection.value().snapped_point == Point2{7.0, 0.0});
  CHECK(intersection.value().kind == GuiSketchSnapKind::Intersection);

  const auto nearest = controller.snap({112.0, 88.0});
  REQUIRE(nearest);
  CHECK(nearest.value().snapped_point == Point2{7.0, 12.0});
  CHECK(nearest.value().kind == GuiSketchSnapKind::Nearest);

  GuiSketchInteractionScene empty_scene;
  empty_scene.sketch = SketchId("sketch.empty");
  auto inference_controller =
      GuiSketchInteractionController::create(mapping(), empty_scene, config).value();
  const auto horizontal = inference_controller.snap({104.0, 89.5}, Point2{0.0, 10.0});
  REQUIRE(horizontal);
  CHECK(horizontal.value().kind == GuiSketchSnapKind::HorizontalInference);
  CHECK(horizontal.value().snapped_point == Point2{4.0, 10.0});
  CHECK(horizontal.value().inference == "Horizontal");

  GuiSketchInteractionScene alignment_scene;
  alignment_scene.sketch = SketchId("sketch.alignment");
  alignment_scene.points.push_back({"sketch/sketch.alignment/entity/p", "p",
                                    {5.0, 5.0}, GuiSketchSnapKind::Endpoint});
  auto alignment_controller =
      GuiSketchInteractionController::create(mapping(), alignment_scene, config).value();
  const auto alignment = alignment_controller.snap({105.2, 91.0});
  REQUIRE(alignment);
  CHECK(alignment.value().kind == GuiSketchSnapKind::AlignmentX);
  CHECK(std::abs(alignment.value().snapped_point.x - 5.0) < 1.0e-12);
  CHECK(alignment.value().inference.find("Aligned X") != std::string::npos);
}

TEST_CASE("Block 107 grid snap and display remain zoom stable in device-independent pixels",
          "[gui][sketch-snap]") {
  GuiSketchInteractionScene scene;
  scene.sketch = SketchId("sketch.grid");
  GuiSketchInteractionConfig config;
  config.grid.spacing = 10.0;
  config.grid.major_every = 5U;
  auto controller =
      GuiSketchInteractionController::create(mapping(0.5), scene, config).value();

  const auto snap = controller.snap({125.0, 65.0});
  REQUIRE(snap);
  CHECK(snap.value().kind == GuiSketchSnapKind::Grid);
  CHECK(snap.value().snapped_point == Point2{10.0, 20.0});
  CHECK(snap.value().screen_distance_dip <= config.snap_tolerance_dip);

  const auto lines = controller.grid_lines(200.0, 200.0);
  REQUIRE(lines);
  CHECK_FALSE(lines.value().empty());
  CHECK(std::any_of(lines.value().begin(), lines.value().end(),
                    [](const GuiSketchScreenSegment& line) { return line.major; }));
  CHECK(lines.value().size() <= 514U);
}

TEST_CASE("Adaptive grid switches 1/10/100/1000 mm decades with zoom",
          "[gui][sketch-snap][grid-adaptive]") {
  GuiSketchInteractionScene scene;
  scene.sketch = SketchId("sketch.adaptive_grid");
  GuiSketchInteractionConfig config;
  config.grid.adaptive = true;

  const auto controller_at = [&](double units_per_dip) {
    return GuiSketchInteractionController::create(mapping(units_per_dip), scene, config).value();
  };
  // units_per_dip is plane units per pixel; the readable decade coarsens as
  // pixels-per-unit shrinks.
  CHECK(controller_at(0.05).effective_grid_spacing() == 1.0);
  CHECK(controller_at(0.5).effective_grid_spacing() == 10.0);
  CHECK(controller_at(5.0).effective_grid_spacing() == 100.0);
  CHECK(controller_at(50.0).effective_grid_spacing() == 1000.0);
  CHECK(controller_at(500.0).effective_grid_spacing() == 1000.0);

  // Non-adaptive configs keep the configured spacing verbatim.
  GuiSketchInteractionConfig fixed;
  fixed.grid.spacing = 10.0;
  CHECK(GuiSketchInteractionController::create(mapping(0.05), scene, fixed)
            .value()
            .effective_grid_spacing() == 10.0);

  // The rendered grid follows the effective decade: 1000 mm of plane across a
  // 200 dip viewport at 100 mm spacing yields ~22 lines, not ~200.
  const auto coarse = controller_at(5.0).grid_lines(200.0, 200.0);
  REQUIRE(coarse);
  CHECK(coarse.value().size() >= 18U);
  CHECK(coarse.value().size() <= 30U);
}

TEST_CASE("Block 107 box selection distinguishes window and crossing semantics",
          "[gui][sketch-box-selection]") {
  GuiSketchInteractionScene scene;
  scene.sketch = SketchId("sketch.box");
  scene.curves.push_back({"sketch/sketch.box/entity/inside", "inside",
                          GuiSketchCurveKind::Line, {{-10.0, 0.0}, {10.0, 0.0}}, false});
  scene.curves.push_back({"sketch/sketch.box/entity/crossing", "crossing",
                          GuiSketchCurveKind::Line, {{-50.0, 10.0}, {50.0, 10.0}}, false});
  auto controller = GuiSketchInteractionController::create(mapping(), scene).value();
  const GuiSketchScreenRect rectangle{{80.0, 80.0}, {120.0, 120.0}};

  const auto window = controller.box_select(rectangle, GuiSketchBoxSelectionMode::Window);
  REQUIRE(window);
  REQUIRE(window.value().size() == 1U);
  CHECK(window.value()[0].semantic_id == "sketch/sketch.box/entity/inside");

  const auto crossing = controller.box_select(rectangle, GuiSketchBoxSelectionMode::Crossing);
  REQUIRE(crossing);
  CHECK(crossing.value().size() == 2U);
  CHECK(crossing.value()[0].semantic_id == "sketch/sketch.box/entity/crossing");
  CHECK(crossing.value()[1].semantic_id == "sketch/sketch.box/entity/inside");
}

TEST_CASE("Block 107 scene builder publishes explicit curve snap and annotation products",
          "[gui][sketch-hit-test][gui][sketch-snap]") {
  auto part = PartDocument::create(DocumentId("part.interaction"), "Interaction").value();
  REQUIRE(part.add_parameter(length_parameter("circle.diameter", 20.0)));
  REQUIRE(part.add_parameter(length_parameter("dimension.length", 20.0)));
  auto datum = DatumPlane::xy(DatumPlaneId("datum.xy"), "XY");
  REQUIRE(datum);
  REQUIRE(part.add_datum_plane(std::move(datum.value())));

  auto sketch = Sketch::create(SketchId("sketch.interaction"), "Interaction",
                               DatumPlaneId("datum.xy"))
                    .value();
  REQUIRE(sketch.add_entity(
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {20.0, 0.0}).value()));
  REQUIRE(sketch.add_entity(ArcSegment::create_three_point(
      SketchEntityId("arc.a"), {20.0, 0.0}, {25.0, 5.0}, {20.0, 10.0}).value()));
  REQUIRE(sketch.add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.a"), {0.0, 10.0}, {5.0, 15.0}, {15.0, 15.0},
      {20.0, 10.0}).value()));
  REQUIRE(sketch.add_profile(CircleProfile::create(
      ProfileId("circle.a"), ParameterId("circle.diameter"), {40.0, 0.0}).value()));
  REQUIRE(sketch.add_constraint(SketchGeometricConstraint::create_horizontal(
      SketchConstraintId("constraint.horizontal"),
      SketchReferenceTarget::create_line_segment(SketchEntityId("line.a")).value()).value()));
  REQUIRE(sketch.add_dimension(SketchDrivingDimension::create_aligned_distance(
      SketchDimensionId("dimension.a"),
      SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.a")).value(),
      SketchReferenceTarget::create_line_segment_end(SketchEntityId("line.a")).value(),
      ParameterId("dimension.length")).value()));
  REQUIRE(part.add_sketch(std::move(sketch)));

  const auto* stored = part.find_sketch(SketchId("sketch.interaction"));
  REQUIRE(stored != nullptr);
  const auto scene = GuiSketchInteractionSceneBuilder{}.build(part, *stored);
  REQUIRE(scene);
  CHECK(scene.value().curves.size() == 4U);
  CHECK(scene.value().annotations.size() == 2U);
  CHECK(scene.value().unresolved_reference_count == 0U);
  CHECK(std::any_of(scene.value().points.begin(), scene.value().points.end(),
                    [](const GuiSketchPointPrimitive& point) {
                      return point.snap_kind == GuiSketchSnapKind::Center;
                    }));
  CHECK(std::any_of(scene.value().points.begin(), scene.value().points.end(),
                    [](const GuiSketchPointPrimitive& point) {
                      return point.snap_kind == GuiSketchSnapKind::Quadrant;
                    }));
}

