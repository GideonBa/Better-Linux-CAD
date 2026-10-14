#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/gui_sketch_modify.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace block116_modify_tests {

void seed_modify_session(GuiDocumentSession& session, GuiSketchWorkbench& workbench) {
  REQUIRE(session.create_part(DocumentId("part.gui_modify"), "GUI Modify"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.modify"), "Modify", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.h"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.v"), {4.0, -5.0}, {4.0, 5.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));
}

bool near(Point2 first, Point2 second) {
  return std::hypot(first.x - second.x, first.y - second.y) <= 1.0e-6;
}

[[nodiscard]] std::size_t automatic_point_on_object_count(
    const SketchConstraintCatalog& catalog, const SketchTopology& topology,
    std::string_view trimmed_entity, Point2 endpoint, std::string_view cutter_entity) {
  const SketchTopologyEntity* entity = topology.find_entity(trimmed_entity);
  REQUIRE(entity != nullptr);
  std::vector<std::string> matching_points;
  for (const auto& point_id : entity->points()) {
    const SketchTopologyPoint* point = topology.find_point(point_id);
    REQUIRE(point != nullptr);
    if (near(point->position(), endpoint))
      matching_points.push_back(point_id.value());
  }
  REQUIRE(matching_points.size() == 1U);

  return static_cast<std::size_t>(std::count_if(
      catalog.constraints().begin(), catalog.constraints().end(), [&](const auto& intent) {
        return intent.kind() == SketchSolverConstraintKind::PointOnObject &&
               intent.source() == SketchConstraintIntentSource::Automatic &&
               intent.targets().size() == 2U &&
               intent.targets()[0].kind() == SketchConstraintIntentTargetKind::Point &&
               intent.targets()[0].id() == matching_points.front() &&
               intent.targets()[1].kind() == SketchConstraintIntentTargetKind::Entity &&
               intent.targets()[1].id() == cutter_entity;
      }));
}

[[nodiscard]] std::size_t automatic_coincident_count(
    const SketchConstraintCatalog& catalog, const SketchTopology& topology,
    std::string_view first_entity, Point2 first_endpoint, std::string_view second_entity,
    Point2 second_endpoint) {
  const auto point_at = [&](std::string_view entity_id, Point2 position) {
    const SketchTopologyEntity* entity = topology.find_entity(entity_id);
    REQUIRE(entity != nullptr);
    std::vector<std::string> matches;
    for (const auto& point_id : entity->points()) {
      const SketchTopologyPoint* point = topology.find_point(point_id);
      REQUIRE(point != nullptr);
      if (near(point->position(), position))
        matches.push_back(point_id.value());
    }
    REQUIRE(matches.size() == 1U);
    return matches.front();
  };
  const std::string first_point = point_at(first_entity, first_endpoint);
  const std::string second_point = point_at(second_entity, second_endpoint);
  return static_cast<std::size_t>(std::count_if(
      catalog.constraints().begin(), catalog.constraints().end(), [&](const auto& intent) {
        if (intent.kind() != SketchSolverConstraintKind::Coincident ||
            intent.source() != SketchConstraintIntentSource::Automatic ||
            intent.targets().size() != 2U)
          return false;
        const std::string& left = intent.targets()[0].id();
        const std::string& right = intent.targets()[1].id();
        return (left == first_point && right == second_point) ||
               (left == second_point && right == first_point);
      }));
}

void check_automatic_point_on_object(const GuiDocumentSession& session, const SketchId& sketch_id,
                                     std::string_view trimmed_entity, Point2 endpoint,
                                     std::string_view cutter_entity) {
  const Sketch* sketch = session.part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  auto catalog = session.sketch_constraint_catalog(sketch_id);
  REQUIRE(catalog);
  CHECK(automatic_point_on_object_count(catalog.value(), topology.value(), trimmed_entity,
                                        endpoint, cutter_entity) == 1U);
}

Parameter trim_length_parameter(const char* id, double millimeters) {
  auto quantity = Quantity::length_mm(millimeters, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return std::move(parameter.value());
}

} // namespace block116_modify_tests

TEST_CASE("Block 116 GUI trim previews and commits one atomic undoable transaction",
          "[gui][sketch-trim-extend][trim-coincident]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_modify_session(session, workbench);

  auto line_target = SketchConstraintIntentTarget::entity("entity/line.h");
  REQUIRE(line_target);
  auto horizontal = SketchConstraintIntent::create(
      SketchConstraintId("constraint.horizontal.line.h"),
      SketchSolverConstraintKind::Horizontal, {line_target.value()});
  REQUIRE(horizontal);
  auto horizontal_controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.modify"), std::move(horizontal.value()));
  REQUIRE(horizontal_controller);
  REQUIRE(horizontal_controller.value().commit(session));

  // This exact endpoint is replaced by the one-sided trim. The edit must
  // prune its stale Fixed intent before staging the replacement attachment.
  auto source_topology = SketchTopology::migrate_legacy(
      *session.part_document()->find_sketch(SketchId("sketch.modify")));
  REQUIRE(source_topology);
  const SketchTopologyEntity* source_line =
      source_topology.value().find_entity("entity/line.h");
  REQUIRE(source_line != nullptr);
  auto fixed_target = SketchConstraintIntentTarget::point(source_line->points().back());
  REQUIRE(fixed_target);
  auto fixed = SketchConstraintIntent::create(
      SketchConstraintId("constraint.fixed.line.h.end"), SketchSolverConstraintKind::Fixed,
      {fixed_target.value()});
  REQUIRE(fixed);
  auto fixed_controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.modify"), std::move(fixed.value()));
  REQUIRE(fixed_controller);
  REQUIRE(fixed_controller.value().commit(session));

  auto controller = GuiSketchModifyController::create(session, SketchId("sketch.modify"));
  REQUIRE(controller);
  auto preview = controller.value().trim(SketchEntityId("line.h"), {8.0, 0.0});
  REQUIRE(preview);
  CHECK(near(preview.value().candidate.find_line_segment(SketchEntityId("line.h"))->end(),
             {4.0, 0.0}));
  CHECK(preview.value().invalidated_ids.empty());
  // Preview never mutates the document.
  CHECK(near(session.part_document()->find_sketch(SketchId("sketch.modify"))
                 ->find_line_segment(SketchEntityId("line.h"))->end(),
             {10.0, 0.0}));

  REQUIRE(controller.value().commit(session));
  CHECK(session.undo_label() == "Trim sketch geometry");
  CHECK(near(session.part_document()->find_sketch(SketchId("sketch.modify"))
                 ->find_line_segment(SketchEntityId("line.h"))->end(),
             {4.0, 0.0}));
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.horizontal.line.h")) != nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.fixed.line.h.end")) == nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .constraints()
            .size() == 2U);
  check_automatic_point_on_object(session, SketchId("sketch.modify"), "entity/line.h",
                                  {4.0, 0.0}, "entity/line.v");

  REQUIRE(session.undo());
  CHECK(near(session.part_document()->find_sketch(SketchId("sketch.modify"))
                 ->find_line_segment(SketchEntityId("line.h"))->end(),
             {10.0, 0.0}));
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.horizontal.line.h")) != nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.fixed.line.h.end")) != nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .constraints()
            .size() == 2U);
  REQUIRE(session.redo());
  CHECK(near(session.part_document()->find_sketch(SketchId("sketch.modify"))
                 ->find_line_segment(SketchEntityId("line.h"))->end(),
             {4.0, 0.0}));
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .constraints()
            .size() == 2U);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.fixed.line.h.end")) == nullptr);
  check_automatic_point_on_object(session, SketchId("sketch.modify"), "entity/line.h",
                                  {4.0, 0.0}, "entity/line.v");
}

TEST_CASE("Block 116 GUI trim constrains both survivors when the middle span is removed",
          "[gui][sketch-trim-extend][trim-coincident]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_trim_middle"), "GUI Trim Middle"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.trim.middle"), "Trim Middle",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.target"), {0.0, 0.0}, {12.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.cut.left"), {3.0, -3.0}, {3.0, 3.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.cut.right"), {9.0, -3.0}, {9.0, 3.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  auto controller =
      GuiSketchModifyController::create(session, SketchId("sketch.trim.middle"));
  REQUIRE(controller);
  REQUIRE(controller.value().trim(SketchEntityId("line.target"), {6.0, 0.0}));
  REQUIRE(controller.value().commit(session));

  const SketchId sketch_id("sketch.trim.middle");
  const Sketch* trimmed = session.part_document()->find_sketch(sketch_id);
  REQUIRE(trimmed != nullptr);
  REQUIRE(trimmed->find_line_segment(SketchEntityId("line.target")) != nullptr);
  REQUIRE(trimmed->find_line_segment(SketchEntityId("line.target.split.1")) != nullptr);
  const auto catalog = session.sketch_constraint_catalog(sketch_id);
  REQUIRE(catalog);
  CHECK(catalog.value().constraints().size() == 2U);
  check_automatic_point_on_object(session, sketch_id, "entity/line.target", {3.0, 0.0},
                                  "entity/line.cut.left");
  check_automatic_point_on_object(session, sketch_id, "entity/line.target.split.1", {9.0, 0.0},
                                  "entity/line.cut.right");

  // Geometry and both automatically authored intents are one history entry.
  CHECK(session.undo_label() == "Trim sketch geometry");
  REQUIRE(session.undo());
  const Sketch* restored = session.part_document()->find_sketch(sketch_id);
  REQUIRE(restored != nullptr);
  CHECK(restored->find_line_segment(SketchEntityId("line.target.split.1")) == nullptr);
  CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().empty());

  CHECK(session.redo_label() == "Trim sketch geometry");
  REQUIRE(session.redo());
  CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().size() == 2U);
  check_automatic_point_on_object(session, sketch_id, "entity/line.target", {3.0, 0.0},
                                  "entity/line.cut.left");
  check_automatic_point_on_object(session, sketch_id, "entity/line.target.split.1", {9.0, 0.0},
                                  "entity/line.cut.right");
}

TEST_CASE("Block 116 GUI trim stages automatic contact with surviving driving dimensions",
          "[gui][sketch-trim-extend][trim-coincident]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_trim_driving"), "GUI Trim Driving"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(session.commit_part_transaction("Seed target length", [](PartDocument& part) {
    return part.add_parameter(trim_length_parameter("trim.target.length", 10.0));
  }));
  auto sketch = Sketch::create(SketchId("sketch.trim.driving"), "Trim Driving",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.target"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.cut"), {4.0, -5.0}, {4.0, 5.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  const SketchId sketch_id("sketch.trim.driving");
  auto target_entity = SketchConstraintIntentTarget::entity("entity/line.target");
  auto cutter_entity = SketchConstraintIntentTarget::entity("entity/line.cut");
  REQUIRE(target_entity);
  REQUIRE(cutter_entity);
  auto horizontal = SketchConstraintIntent::create(
      SketchConstraintId("constraint.horizontal.target"),
      SketchSolverConstraintKind::Horizontal, {target_entity.value()});
  auto fixed_cutter = SketchConstraintIntent::create(
      SketchConstraintId("constraint.fixed.cutter"), SketchSolverConstraintKind::Fixed,
      {cutter_entity.value()});
  REQUIRE(horizontal);
  REQUIRE(fixed_cutter);
  auto horizontal_controller = GuiSketchConstraintController::create(
      session, sketch_id, std::move(horizontal.value()));
  REQUIRE(horizontal_controller);
  REQUIRE(horizontal_controller.value().commit(session));
  auto fixed_controller = GuiSketchConstraintController::create(
      session, sketch_id, std::move(fixed_cutter.value()));
  REQUIRE(fixed_controller);
  REQUIRE(fixed_controller.value().commit(session));

  auto length = SketchDimensionIntent::create(
      SketchDimensionId("dimension.target.length"), SketchDimensionKind::Length,
      SketchDimensionMode::Driving, {target_entity.value()},
      ParameterId("trim.target.length"));
  REQUIRE(length);
  auto dimension_controller =
      GuiSketchDimensionController::create(session, sketch_id, std::move(length.value()));
  REQUIRE(dimension_controller);
  REQUIRE(dimension_controller.value().commit(session));

  auto modify = GuiSketchModifyController::create(session, sketch_id);
  REQUIRE(modify);
  REQUIRE(modify.value().trim(SketchEntityId("line.target"), {8.0, 0.0}));
  REQUIRE(modify.value().commit(session));

  const Sketch* trimmed = session.part_document()->find_sketch(sketch_id);
  REQUIRE(trimmed != nullptr);
  const LineSegment* target = trimmed->find_line_segment(SketchEntityId("line.target"));
  REQUIRE(target != nullptr);
  CHECK(near(target->end(), {4.0, 0.0}));
  CHECK(std::hypot(target->end().x - target->start().x,
                   target->end().y - target->start().y) == Catch::Approx(10.0).margin(1.0e-6));
  CHECK(session.sketch_dimension_catalog(sketch_id)
            .value()
            .find(SketchDimensionId("dimension.target.length")) != nullptr);
  check_automatic_point_on_object(session, sketch_id, "entity/line.target", {4.0, 0.0},
                                  "entity/line.cut");

  REQUIRE(session.undo());
  const LineSegment* restored =
      session.part_document()->find_sketch(sketch_id)->find_line_segment(
          SketchEntityId("line.target"));
  REQUIRE(restored != nullptr);
  CHECK(near(restored->start(), {0.0, 0.0}));
  CHECK(near(restored->end(), {10.0, 0.0}));
  CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().size() == 2U);
  CHECK(session.sketch_dimension_catalog(sketch_id)
            .value()
            .find(SketchDimensionId("dimension.target.length")) != nullptr);
}

TEST_CASE("Block 116 GUI trim uses Coincident when the cutter has an endpoint at the contact",
          "[gui][sketch-trim-extend][trim-coincident]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_trim_endpoint"), "GUI Trim Endpoint"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.trim.endpoint"), "Trim Endpoint",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.target"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.cut"), {4.0, 0.0}, {4.0, 5.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  const SketchId sketch_id("sketch.trim.endpoint");
  auto controller = GuiSketchModifyController::create(session, sketch_id);
  REQUIRE(controller);
  REQUIRE(controller.value().trim(SketchEntityId("line.target"), {8.0, 0.0}));
  REQUIRE(controller.value().commit(session));

  const Sketch* trimmed = session.part_document()->find_sketch(sketch_id);
  REQUIRE(trimmed != nullptr);
  REQUIRE(trimmed->find_line_segment(SketchEntityId("line.target")) != nullptr);
  CHECK(near(trimmed->find_line_segment(SketchEntityId("line.target"))->end(), {4.0, 0.0}));
  auto topology = SketchTopology::migrate_legacy(*trimmed);
  REQUIRE(topology);
  const auto catalog = session.sketch_constraint_catalog(sketch_id);
  REQUIRE(catalog);
  CHECK(catalog.value().constraints().size() == 1U);
  CHECK(automatic_coincident_count(catalog.value(), topology.value(), "entity/line.target",
                                   {4.0, 0.0}, "entity/line.cut", {4.0, 0.0}) == 1U);
}

TEST_CASE("Block 116 GUI trim constrains new arc spline and circle endpoints to their cutters",
          "[gui][sketch-trim-extend][trim-coincident]") {
  using namespace block116_modify_tests;

  SECTION("arc") {
    GuiDocumentSession session;
    GuiSketchWorkbench workbench;
    REQUIRE(session.create_part(DocumentId("part.gui_trim_arc"), "GUI Trim Arc"));
    REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
    auto sketch =
        Sketch::create(SketchId("sketch.trim.arc"), "Trim Arc", DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    REQUIRE(sketch.value().add_entity(
        ArcSegment::create_three_point(SketchEntityId("arc.target"), {5.0, 2.0}, {7.0, 0.0},
                                       {5.0, -2.0})
            .value()));
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId("line.cut"), {6.0, 0.0}, {8.0, 0.0}).value()));
    REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

    auto controller = GuiSketchModifyController::create(session, SketchId("sketch.trim.arc"));
    REQUIRE(controller);
    REQUIRE(controller.value().trim(SketchEntityId("arc.target"), {6.5, 1.5}));
    REQUIRE(controller.value().commit(session));

    const SketchId sketch_id("sketch.trim.arc");
    REQUIRE(session.part_document()->find_sketch(sketch_id)->find_arc_segment(
                SketchEntityId("arc.target")) != nullptr);
    CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().size() == 1U);
    check_automatic_point_on_object(session, sketch_id, "entity/arc.target", {7.0, 0.0},
                                    "entity/line.cut");
  }

  SECTION("cubic spline") {
    GuiDocumentSession session;
    GuiSketchWorkbench workbench;
    REQUIRE(session.create_part(DocumentId("part.gui_trim_spline"), "GUI Trim Spline"));
    REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
    auto sketch = Sketch::create(SketchId("sketch.trim.spline"), "Trim Spline",
                                 DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    REQUIRE(sketch.value().add_entity(
        SplineSegment::create_cubic_bezier(SketchEntityId("spline.target"), {0.0, 0.0},
                                           {3.0, 6.0}, {7.0, 6.0}, {10.0, 0.0})
            .value()));
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId("line.cut.left"), {2.40625, -4.0},
                            {2.40625, 8.0})
            .value()));
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId("line.cut.right"), {7.59375, -4.0},
                            {7.59375, 8.0})
            .value()));
    REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

    auto controller =
        GuiSketchModifyController::create(session, SketchId("sketch.trim.spline"));
    REQUIRE(controller);
    REQUIRE(controller.value().trim(SketchEntityId("spline.target"), {5.0, 4.5}));
    REQUIRE(controller.value().commit(session));

    const SketchId sketch_id("sketch.trim.spline");
    CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().size() == 2U);
    check_automatic_point_on_object(session, sketch_id, "entity/spline.target",
                                    {2.40625, 3.375}, "entity/line.cut.left");
    check_automatic_point_on_object(session, sketch_id, "entity/spline.target.split.1",
                                    {7.59375, 3.375}, "entity/line.cut.right");
  }

  SECTION("circle profile") {
    GuiDocumentSession session;
    GuiSketchWorkbench workbench;
    REQUIRE(session.create_part(DocumentId("part.gui_trim_circle"), "GUI Trim Circle"));
    REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
    REQUIRE(session.commit_part_transaction("Seed circle diameter", [](PartDocument& part) {
      return part.add_parameter(trim_length_parameter("circle.trim.diameter", 20.0));
    }));
    auto sketch = Sketch::create(SketchId("sketch.trim.circle"), "Trim Circle",
                                 DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId("line.cut"), {0.0, -20.0}, {0.0, 20.0}).value()));
    REQUIRE(sketch.value().add_profile(
        CircleProfile::create(ProfileId("profile.circle"),
                              ParameterId("circle.trim.diameter"))
            .value()));
    REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

    auto controller =
        GuiSketchModifyController::create(session, SketchId("sketch.trim.circle"));
    REQUIRE(controller);
    REQUIRE(controller.value().trim(ProfileId("profile.circle"), {10.0, 0.0}));
    REQUIRE(controller.value().commit(session));

    const SketchId sketch_id("sketch.trim.circle");
    const Sketch* trimmed = session.part_document()->find_sketch(sketch_id);
    REQUIRE(trimmed != nullptr);
    REQUIRE(trimmed->arc_segments().size() == 1U);
    const std::string arc_entity = "entity/" + trimmed->arc_segments().front().id().value();
    CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().size() == 2U);
    check_automatic_point_on_object(session, sketch_id, arc_entity, {0.0, 10.0},
                                    "entity/line.cut");
    check_automatic_point_on_object(session, sketch_id, arc_entity, {0.0, -10.0},
                                    "entity/line.cut");
  }

  SECTION("circle profile as cutter") {
    GuiDocumentSession session;
    GuiSketchWorkbench workbench;
    REQUIRE(session.create_part(DocumentId("part.gui_trim_circle_cutter"),
                                "GUI Trim Circle Cutter"));
    REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
    REQUIRE(session.commit_part_transaction("Seed circle diameter", [](PartDocument& part) {
      return part.add_parameter(trim_length_parameter("circle.cutter.diameter", 20.0));
    }));
    auto sketch = Sketch::create(SketchId("sketch.trim.circle_cutter"), "Trim Circle Cutter",
                                 DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId("line.target"), {-15.0, 0.0}, {15.0, 0.0}).value()));
    REQUIRE(sketch.value().add_profile(
        CircleProfile::create(ProfileId("profile.circle"),
                              ParameterId("circle.cutter.diameter"))
            .value()));
    REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

    const SketchId sketch_id("sketch.trim.circle_cutter");
    auto controller = GuiSketchModifyController::create(session, sketch_id);
    REQUIRE(controller);
    REQUIRE(controller.value().trim(SketchEntityId("line.target"), {0.0, 0.0}));
    REQUIRE(controller.value().commit(session));

    CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().size() == 2U);
    check_automatic_point_on_object(session, sketch_id, "entity/line.target", {-10.0, 0.0},
                                    "profile/profile.circle");
    check_automatic_point_on_object(session, sketch_id, "entity/line.target.split.1",
                                    {10.0, 0.0}, "profile/profile.circle");
  }
}

TEST_CASE("Block 116 GUI trim authors no constraint for removal or a rejected request",
          "[gui][sketch-trim-extend][trim-coincident]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_trim_no_contact"), "GUI Trim No Contact"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.trim.no_contact"), "Trim No Contact",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.solo"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  const SketchId sketch_id("sketch.trim.no_contact");
  auto controller = GuiSketchModifyController::create(session, sketch_id);
  REQUIRE(controller);
  REQUIRE(controller.value().trim(SketchEntityId("line.solo"), {5.0, 0.0}));
  REQUIRE(controller.value().commit(session));
  CHECK(session.part_document()->find_sketch(sketch_id)->line_segments().empty());
  CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().empty());

  REQUIRE(session.undo());
  const std::string history_before_rejection(session.undo_label());
  auto rejected_controller = GuiSketchModifyController::create(session, sketch_id);
  REQUIRE(rejected_controller);
  CHECK_FALSE(rejected_controller.value().trim(SketchEntityId("missing"), {5.0, 0.0}));
  CHECK_FALSE(rejected_controller.value().has_preview());
  CHECK_FALSE(rejected_controller.value().commit(session));
  CHECK(session.undo_label() == history_before_rejection);
  CHECK(session.part_document()->find_sketch(sketch_id)->line_segments().size() == 1U);
  CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().empty());
}

TEST_CASE("Block 116 GUI trim constrains line and arc endpoints to spline interiors",
          "[gui][sketch-trim-extend][trim-coincident]") {
  using namespace block116_modify_tests;

  SECTION("line against spline") {
    GuiDocumentSession session;
    GuiSketchWorkbench workbench;
    REQUIRE(session.create_part(DocumentId("part.gui_trim_line_spline"),
                                "GUI Trim Line Against Spline"));
    REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
    auto sketch = Sketch::create(SketchId("sketch.trim.line_spline"), "Trim Line Against Spline",
                                 DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId("line.target"), {0.0, 0.0}, {10.0, 0.0}).value()));
    REQUIRE(sketch.value().add_entity(
        SplineSegment::create_cubic_bezier(SketchEntityId("spline.cut"), {4.0, -5.0},
                                           {4.0, -2.0}, {4.0, 2.0}, {4.0, 5.0})
            .value()));
    REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

    const SketchId sketch_id("sketch.trim.line_spline");
    auto controller = GuiSketchModifyController::create(session, sketch_id);
    REQUIRE(controller);
    REQUIRE(controller.value().trim(SketchEntityId("line.target"), {8.0, 0.0}));
    REQUIRE(controller.value().commit(session));

    CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().size() == 1U);
    check_automatic_point_on_object(session, sketch_id, "entity/line.target", {4.0, 0.0},
                                    "entity/spline.cut");
  }

  SECTION("arc against spline") {
    GuiDocumentSession session;
    GuiSketchWorkbench workbench;
    REQUIRE(session.create_part(DocumentId("part.gui_trim_arc_spline"),
                                "GUI Trim Arc Against Spline"));
    REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
    auto sketch = Sketch::create(SketchId("sketch.trim.arc_spline"), "Trim Arc Against Spline",
                                 DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    REQUIRE(sketch.value().add_entity(
        ArcSegment::create_three_point(SketchEntityId("arc.target"), {5.0, 2.0}, {7.0, 0.0},
                                       {5.0, -2.0})
            .value()));
    REQUIRE(sketch.value().add_entity(
        SplineSegment::create_cubic_bezier(SketchEntityId("spline.cut"), {6.0, 0.0},
                                           {6.5, 0.0}, {7.5, 0.0}, {8.0, 0.0})
            .value()));
    REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

    const SketchId sketch_id("sketch.trim.arc_spline");
    auto controller = GuiSketchModifyController::create(session, sketch_id);
    REQUIRE(controller);
    REQUIRE(controller.value().trim(SketchEntityId("arc.target"), {6.5, 1.5}));
    REQUIRE(controller.value().commit(session));

    CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().size() == 1U);
    check_automatic_point_on_object(session, sketch_id, "entity/arc.target", {7.0, 0.0},
                                    "entity/spline.cut");
  }
}

TEST_CASE("Block 116 GUI trim prunes an invalidated entity id that still exists",
          "[gui][sketch-trim-extend]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_modify_session(session, workbench);
  REQUIRE(session.commit_part_transaction(
      "Add second cutter", [](PartDocument& part) -> Result<std::size_t> {
        const SketchId sketch_id("sketch.modify");
        const Sketch* source = part.find_sketch(sketch_id);
        if (source == nullptr)
          return Result<std::size_t>::failure(Error::validation(sketch_id.value(), "no sketch"));
        Sketch candidate = *source;
        auto added = candidate.add_entity(
            LineSegment::create(SketchEntityId("line.v2"), {8.0, -5.0}, {8.0, 5.0}).value());
        if (added.has_error()) return added;
        return part.update_sketch(std::move(candidate));
      }));

  auto target = SketchConstraintIntentTarget::entity("entity/line.h");
  REQUIRE(target);
  auto horizontal = SketchConstraintIntent::create(
      SketchConstraintId("constraint.horizontal.split"), SketchSolverConstraintKind::Horizontal,
      {target.value()});
  REQUIRE(horizontal);
  auto constraint_controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.modify"), std::move(horizontal.value()));
  REQUIRE(constraint_controller);
  REQUIRE(constraint_controller.value().commit(session));

  auto topology = SketchTopology::migrate_legacy(
      *session.part_document()->find_sketch(SketchId("sketch.modify")));
  REQUIRE(topology);
  const auto* line_entity = topology.value().find_entity("entity/line.h");
  REQUIRE(line_entity != nullptr);
  REQUIRE(line_entity->points().size() == 2U);
  auto endpoint = SketchConstraintIntentTarget::point(line_entity->points()[1]);
  REQUIRE(endpoint);
  auto fixed_endpoint = SketchConstraintIntent::create(
      SketchConstraintId("constraint.fixed.split.end"), SketchSolverConstraintKind::Fixed,
      {endpoint.value()});
  REQUIRE(fixed_endpoint);
  auto endpoint_controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.modify"), std::move(fixed_endpoint.value()));
  REQUIRE(endpoint_controller);
  REQUIRE(endpoint_controller.value().commit(session));

  auto length = SketchDimensionIntent::create(
      SketchDimensionId("dimension.length.split"), SketchDimensionKind::Length,
      SketchDimensionMode::Reference, {target.value()});
  REQUIRE(length);
  auto dimension_controller = GuiSketchDimensionController::create(
      session, SketchId("sketch.modify"), std::move(length.value()));
  REQUIRE(dimension_controller);
  REQUIRE(dimension_controller.value().commit(session));

  auto controller = GuiSketchModifyController::create(session, SketchId("sketch.modify"));
  REQUIRE(controller);
  auto preview = controller.value().trim(SketchEntityId("line.h"), {6.0, 0.0});
  REQUIRE(preview);
  CHECK(std::find(preview.value().invalidated_ids.begin(), preview.value().invalidated_ids.end(),
                  "line.h") != preview.value().invalidated_ids.end());
  REQUIRE(controller.value().commit(session));

  const Sketch* trimmed = session.part_document()->find_sketch(SketchId("sketch.modify"));
  REQUIRE(trimmed != nullptr);
  CHECK(trimmed->find_line_segment(SketchEntityId("line.h")) != nullptr);
  CHECK(trimmed->find_line_segment(SketchEntityId("line.h.split.1")) != nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.horizontal.split")) == nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.fixed.split.end")) == nullptr);
  CHECK(session.sketch_dimension_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchDimensionId("dimension.length.split")) == nullptr);

  REQUIRE(session.undo());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.modify"))
            ->find_line_segment(SketchEntityId("line.h.split.1")) == nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.horizontal.split")) != nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchConstraintId("constraint.fixed.split.end")) != nullptr);
  CHECK(session.sketch_dimension_catalog(SketchId("sketch.modify"))
            .value()
            .find(SketchDimensionId("dimension.length.split")) != nullptr);
}

TEST_CASE("Block 116 GUI fillet inserts a tangent arc through one transaction",
          "[gui][sketch-trim-extend]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_fillet"), "GUI Fillet"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.fillet"), "Fillet", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.h"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.v"), {0.0, 0.0}, {0.0, 10.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  auto controller = GuiSketchModifyController::create(session, SketchId("sketch.fillet"));
  REQUIRE(controller);
  auto preview = controller.value().fillet(SketchEntityId("line.h"), SketchEntityId("line.v"), 2.0);
  REQUIRE(preview);
  CHECK(preview.value().candidate.arc_segments().size() == 1U);
  REQUIRE(controller.value().commit(session));
  CHECK(session.undo_label() == "Fillet sketch corner");
  const Sketch* committed = session.part_document()->find_sketch(SketchId("sketch.fillet"));
  REQUIRE(committed != nullptr);
  CHECK(committed->arc_segments().size() == 1U);
  CHECK(near(committed->find_line_segment(SketchEntityId("line.h"))->start(), {2.0, 0.0}));
}

TEST_CASE("Block 116 GUI modify prunes stale Block-114 references atomically",
          "[gui][sketch-trim-extend]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_modify_catalog"), "GUI Modify Catalog"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.solo"), "Solo", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.solo"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  // Accept a Block-114 Horizontal constraint that references entity/line.solo.
  auto target = SketchConstraintIntentTarget::entity("entity/line.solo");
  REQUIRE(target);
  auto candidate = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId("constraint.horizontal.solo"), GuiSketchSnapKind::HorizontalInference,
      target.value());
  REQUIRE(candidate);
  REQUIRE(candidate.value().has_value());
  auto constraint_controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.solo"), *candidate.value());
  REQUIRE(constraint_controller);
  REQUIRE(constraint_controller.value().commit(session));

  // Also retain a point-targeted intent so the pruning path covers both
  // semantic entity ids and topology point ids.
  auto topology = SketchTopology::migrate_legacy(
      *session.part_document()->find_sketch(SketchId("sketch.solo")));
  REQUIRE(topology);
  const auto* line_entity = topology.value().find_entity("entity/line.solo");
  REQUIRE(line_entity != nullptr);
  REQUIRE_FALSE(line_entity->points().empty());
  auto point_target = SketchConstraintIntentTarget::point(line_entity->points().front());
  REQUIRE(point_target);
  auto fixed = SketchConstraintIntent::create(
      SketchConstraintId("constraint.fixed.solo.start"), SketchSolverConstraintKind::Fixed,
      {point_target.value()});
  REQUIRE(fixed);
  auto fixed_controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.solo"), std::move(fixed.value()));
  REQUIRE(fixed_controller);
  REQUIRE(fixed_controller.value().commit(session));

  auto controller = GuiSketchModifyController::create(session, SketchId("sketch.solo"));
  REQUIRE(controller);
  // The line has no intersections, so trim removes it entirely. Its stale
  // catalog reference is pruned in the same undoable transaction.
  auto preview = controller.value().trim(SketchEntityId("line.solo"), {5.0, 0.0});
  REQUIRE(preview);
  CHECK(preview.value().candidate.line_segments().empty());
  auto committed = controller.value().commit(session);
  REQUIRE(committed);
  CHECK(session.part_document()->find_sketch(SketchId("sketch.solo"))
            ->find_line_segment(SketchEntityId("line.solo")) == nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.solo"))
            .value().constraints().empty());

  REQUIRE(session.undo());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.solo"))
            ->find_line_segment(SketchEntityId("line.solo")) != nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.solo"))
            .value().constraints().size() == 2U);
}
