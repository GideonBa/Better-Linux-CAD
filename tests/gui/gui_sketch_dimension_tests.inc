#include "blcad/gui/gui_sketch_dimension_binder.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/gui_sketch_interaction_binder.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/main_window.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace block115_dimension_tests {

Parameter length_parameter(const char* id, const char* name, double value) {
  return Parameter::create_length(ParameterId(id), name,
                                  Quantity::length_mm(value, id).value())
      .value();
}

Parameter angle_parameter(const char* id, const char* name, double value) {
  return Parameter::create_angle(ParameterId(id), name,
                                 Quantity::angle_deg(value, id).value())
      .value();
}

void seed_session(GuiDocumentSession& session, bool expression = false) {
  REQUIRE(session.create_part(DocumentId("part.gui_dimensions"), "GUI Dimensions"));
  REQUIRE(session.commit_part_transaction("Seed Block 115", [expression](PartDocument& part) {
    auto datum = DatumPlane::xy(DatumPlaneId("datum.xy"), "XY");
    if (datum.has_error()) return Result<std::size_t>::failure(datum.error());
    auto added = part.add_datum_plane(std::move(datum.value()));
    if (added.has_error()) return added;
    added = part.add_parameter(length_parameter("dimension.length", "dimension_length", 20.0));
    if (added.has_error()) return added;
    added = part.add_parameter(length_parameter("dimension.arc", "dimension_arc", 15.7079632679));
    if (added.has_error()) return added;
    added = part.add_parameter(angle_parameter("dimension.angle", "dimension_angle", 90.0));
    if (added.has_error()) return added;
    added = part.add_parameter(length_parameter("base.length", "base_length", 10.0));
    if (added.has_error()) return added;
    if (expression) {
      added = part.add_expression_parameter(ParameterId("dimension.expression"),
                                            "dimension_expression",
                                            ParameterType::Length,
                                            "base_length * 2");
      if (added.has_error()) return added;
    }
    auto sketch = Sketch::create(SketchId("sketch.dimensions"), "Dimensions",
                                 DatumPlaneId("datum.xy"));
    if (sketch.has_error()) return Result<std::size_t>::failure(sketch.error());
    auto line = LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {10.0, 0.0});
    if (line.has_error()) return Result<std::size_t>::failure(line.error());
    added = sketch.value().add_entity(std::move(line.value()));
    if (added.has_error()) return added;
    auto line_b = LineSegment::create(SketchEntityId("line.b"), {0.0, 0.0}, {0.0, 10.0});
    if (line_b.has_error()) return Result<std::size_t>::failure(line_b.error());
    added = sketch.value().add_entity(std::move(line_b.value()));
    if (added.has_error()) return added;
    auto arc = ArcSegment::create_three_point(SketchEntityId("arc.a"),
                                              {0.0, 0.0}, {5.0, 5.0}, {10.0, 0.0});
    if (arc.has_error()) return Result<std::size_t>::failure(arc.error());
    added = sketch.value().add_entity(std::move(arc.value()));
    if (added.has_error()) return added;
    return part.add_sketch(std::move(sketch.value()));
  }));
}

SketchConstraintIntentTarget entity(const char* id) {
  return SketchConstraintIntentTarget::entity(id).value();
}

SketchConstraintIntentTarget point(const SketchPointId& id) {
  return SketchConstraintIntentTarget::point(id).value();
}

SketchDimensionIntent line_length(const char* id, const char* parameter) {
  return SketchDimensionIntent::create(
             SketchDimensionId(id), SketchDimensionKind::Length,
             SketchDimensionMode::Driving, {entity("entity/line.a")},
             ParameterId(parameter))
      .value();
}

bool contains_dimension_kind(const std::vector<SketchDimensionKind>& values,
                             SketchDimensionKind kind) {
  return std::find(values.begin(), values.end(), kind) != values.end();
}

} // namespace block115_dimension_tests

TEST_CASE("Block 115 dimension family compatibility covers all authored families",
          "[gui][sketch-dimensions]") {
  using namespace block115_dimension_tests;
  GuiDocumentSession session;
  seed_session(session);
  const Sketch* sketch = session.part_document()->find_sketch(SketchId("sketch.dimensions"));
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  const auto* line_a = topology.value().find_entity("entity/line.a");
  const auto* line_b = topology.value().find_entity("entity/line.b");
  REQUIRE(line_a != nullptr);
  REQUIRE(line_b != nullptr);

  const auto point_pair = GuiSketchDimensionController::compatible_kinds(
      topology.value(), {point(line_a->points()[0]), point(line_a->points()[1])});
  CHECK(contains_dimension_kind(point_pair, SketchDimensionKind::HorizontalDistance));
  CHECK(contains_dimension_kind(point_pair, SketchDimensionKind::VerticalDistance));
  CHECK(contains_dimension_kind(point_pair, SketchDimensionKind::AlignedDistance));
  CHECK(contains_dimension_kind(point_pair, SketchDimensionKind::PointToPointDistance));

  const auto line_single = GuiSketchDimensionController::compatible_kinds(
      topology.value(), {entity("entity/line.a")});
  CHECK(contains_dimension_kind(line_single, SketchDimensionKind::Length));
  const auto arc_single = GuiSketchDimensionController::compatible_kinds(
      topology.value(), {entity("entity/arc.a")});
  CHECK(contains_dimension_kind(arc_single, SketchDimensionKind::Radius));
  CHECK(contains_dimension_kind(arc_single, SketchDimensionKind::Diameter));
  CHECK(contains_dimension_kind(arc_single, SketchDimensionKind::ArcLength));
  const auto line_pair = GuiSketchDimensionController::compatible_kinds(
      topology.value(), {entity("entity/line.a"), entity("entity/line.b")});
  CHECK(contains_dimension_kind(line_pair, SketchDimensionKind::Angle));

  const auto compatible_parameters = GuiSketchDimensionController::compatible_parameters(
      *session.part_document(), SketchDimensionKind::Length);
  CHECK(std::find(compatible_parameters.begin(), compatible_parameters.end(),
                  ParameterId("dimension.length")) != compatible_parameters.end());
  const auto angle_parameters = GuiSketchDimensionController::compatible_parameters(
      *session.part_document(), SketchDimensionKind::Angle);
  REQUIRE(angle_parameters.size() == 1U);
  CHECK(angle_parameters.front() == ParameterId("dimension.angle"));
}

TEST_CASE("Block 115 GUI driving dimension commits solved geometry and exact undo redo",
          "[gui][sketch-dimensions]") {
  using namespace block115_dimension_tests;
  GuiDocumentSession session;
  seed_session(session);
  const auto original_end = session.part_document()
                                ->find_sketch(SketchId("sketch.dimensions"))
                                ->find_line_segment(SketchEntityId("line.a"))
                                ->end();
  auto controller = GuiSketchDimensionController::create(
      session, SketchId("sketch.dimensions"),
      line_length("dimension.length", "dimension.length"));
  REQUIRE(controller);
  CHECK(controller.value().preview().authoring.accepted);
  CHECK(controller.value().preview().annotation.hit_kind == GuiSketchHitKind::Dimension);
  CHECK(controller.value().preview().glyph.value_text == "20.000 mm");
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.dimensions"))
            ->find_line_segment(SketchEntityId("line.a"))
            ->end() == original_end);

  REQUIRE(controller.value().commit(session));
  CHECK(session.undo_label() == "Add sketch dimension");
  const auto* committed = session.part_document()
                              ->find_sketch(SketchId("sketch.dimensions"))
                              ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(committed != nullptr);
  const double committed_length = std::hypot(committed->end().x - committed->start().x,
                                             committed->end().y - committed->start().y);
  CHECK(committed_length == Catch::Approx(20.0).margin(1.0e-6));
  auto catalog = session.sketch_dimension_catalog(SketchId("sketch.dimensions"));
  REQUIRE(catalog);
  REQUIRE(catalog.value().dimensions().size() == 1U);

  REQUIRE(controller.value().undo(session));
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.dimensions"))
            ->find_line_segment(SketchEntityId("line.a"))
            ->end() == original_end);
  CHECK(session.sketch_dimension_catalog(SketchId("sketch.dimensions"))
            .value().dimensions().empty());
  REQUIRE(controller.value().redo(session));
  CHECK(session.sketch_dimension_catalog(SketchId("sketch.dimensions"))
            .value().dimensions().size() == 1U);
}

TEST_CASE("Block 115 reference dimension measures without changing Sketch geometry",
          "[gui][sketch-dimensions]") {
  using namespace block115_dimension_tests;
  GuiDocumentSession session;
  seed_session(session);
  const Sketch* sketch = session.part_document()->find_sketch(SketchId("sketch.dimensions"));
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  const auto* line = topology.value().find_entity("entity/line.a");
  REQUIRE(line != nullptr);
  auto reference = SketchDimensionIntent::create(
      SketchDimensionId("dimension.reference"),
      SketchDimensionKind::PointToPointDistance,
      SketchDimensionMode::Reference,
      {point(line->points()[0]), point(line->points()[1])});
  REQUIRE(reference);
  const Point2 before = sketch->find_line_segment(SketchEntityId("line.a"))->end();
  auto controller = GuiSketchDimensionController::create(
      session, sketch->id(), reference.value());
  REQUIRE(controller);
  CHECK(controller.value().preview().authoring.accepted);
  CHECK(controller.value().preview().glyph.value_text == "(10.000 mm)");
  REQUIRE(controller.value().commit(session));
  CHECK(session.part_document()
            ->find_sketch(sketch->id())
            ->find_line_segment(SketchEntityId("line.a"))
            ->end() == before);
  auto annotations = GuiSketchDimensionController::accepted_annotations(session, sketch->id());
  REQUIRE(annotations);
  REQUIRE(annotations.value().size() == 1U);
  CHECK(annotations.value().front().semantic_id ==
        "sketch/sketch.dimensions/dimension/dimension.reference");
}

TEST_CASE("Block 115 expression editing resolves parameters and re-solves the Sketch atomically",
          "[gui][sketch-dimensions][integration][sketch-expression-edit]") {
  using namespace block115_dimension_tests;
  GuiDocumentSession session;
  seed_session(session, true);
  auto controller = GuiSketchDimensionController::create(
      session, SketchId("sketch.dimensions"),
      line_length("dimension.expression", "dimension.expression"));
  REQUIRE(controller);
  REQUIRE(controller.value().commit(session));
  REQUIRE(GuiSketchDimensionController::edit_formula(
      session, SketchId("sketch.dimensions"), SketchDimensionId("dimension.expression"),
      "base_length * 3"));
  const Parameter* parameter =
      session.part_document()->find_parameter(ParameterId("dimension.expression"));
  REQUIRE(parameter != nullptr);
  CHECK(parameter->value().millimeters() == Catch::Approx(30.0));
  const auto* line = session.part_document()
                         ->find_sketch(SketchId("sketch.dimensions"))
                         ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(line != nullptr);
  CHECK(std::hypot(line->end().x - line->start().x,
                   line->end().y - line->start().y) ==
        Catch::Approx(30.0).margin(1.0e-6));
  CHECK(session.undo_label() == "Edit sketch dimension expression");
}

TEST_CASE("Block 115 session save open roundtrips dimension sidecar and solved geometry",
          "[gui][sketch-dimensions]") {
  using namespace block115_dimension_tests;
  GuiDocumentSession session;
  seed_session(session);
  auto controller = GuiSketchDimensionController::create(
      session, SketchId("sketch.dimensions"),
      line_length("dimension.saved", "dimension.length"));
  REQUIRE(controller);
  REQUIRE(controller.value().commit(session));

  const auto path = std::filesystem::temp_directory_path() /
                    "blcad_block115_dimension_session.blcad.json";
  const auto dimension_sidecar =
      std::filesystem::path(path.string() + ".sketch-dimensions.json");
  const auto constraint_sidecar =
      std::filesystem::path(path.string() + ".sketch-constraints.json");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  std::filesystem::remove(dimension_sidecar, ignored);
  std::filesystem::remove(constraint_sidecar, ignored);
  REQUIRE(session.save_as(path));
  CHECK(std::filesystem::exists(path));
  CHECK(std::filesystem::exists(dimension_sidecar));
  CHECK_FALSE(session.dirty());

  GuiDocumentSession restored;
  REQUIRE(restored.open_file(path));
  auto catalog = restored.sketch_dimension_catalog(SketchId("sketch.dimensions"));
  REQUIRE(catalog);
  REQUIRE(catalog.value().dimensions().size() == 1U);
  CHECK(catalog.value().dimensions().front().id() == SketchDimensionId("dimension.saved"));
  const auto* line = restored.part_document()
                         ->find_sketch(SketchId("sketch.dimensions"))
                         ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(line != nullptr);
  CHECK(std::hypot(line->end().x - line->start().x,
                   line->end().y - line->start().y) ==
        Catch::Approx(20.0).margin(1.0e-6));
  CHECK_FALSE(restored.dirty());

  std::filesystem::remove(path, ignored);
  std::filesystem::remove(dimension_sidecar, ignored);
  std::filesystem::remove(constraint_sidecar, ignored);
}

TEST_CASE("Block 115 accepted dimensions remain active in later solver-backed drags",
          "[gui][sketch-dimensions][integration][sketch-live-solve]") {
  using namespace block115_dimension_tests;
  GuiDocumentSession session;
  seed_session(session);
  auto authoring = GuiSketchDimensionController::create(
      session, SketchId("sketch.dimensions"),
      line_length("dimension.drag", "dimension.length"));
  REQUIRE(authoring);
  REQUIRE(authoring.value().commit(session));

  auto drag = GuiSketchDragController::create(session, SketchId("sketch.dimensions"));
  REQUIRE(drag);
  CHECK(std::any_of(drag.value().source_system().constraints().begin(),
                    drag.value().source_system().constraints().end(), [](const auto& constraint) {
                      return constraint.id() == "dimension/dimension.drag";
                    }));
  const auto* line = session.part_document()
                         ->find_sketch(SketchId("sketch.dimensions"))
                         ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(line != nullptr);
  const auto handle = std::find_if(
      drag.value().handles().begin(), drag.value().handles().end(), [line](const auto& current) {
        return current.kind == GuiSketchDragHandleKind::Endpoint &&
               current.position == line->end();
      });
  REQUIRE(handle != drag.value().handles().end());
  REQUIRE(drag.value().begin(handle->id));
  auto preview = drag.value().flush({35.0, 12.0});
  REQUIRE(preview);
  const auto* preview_line =
      preview.value().preview_sketch().find_line_segment(SketchEntityId("line.a"));
  REQUIRE(preview_line != nullptr);
  CHECK(std::hypot(preview_line->end().x - preview_line->start().x,
                   preview_line->end().y - preview_line->start().y) ==
        Catch::Approx(20.0).margin(1.0e-6));
  REQUIRE(drag.value().commit_session_constraints(session));
  CHECK(session.sketch_dimension_catalog(SketchId("sketch.dimensions"))
            .value().dimensions().size() == 1U);
}

TEST_CASE("Block 115 Qt binder registers all dimension commands on the Sketch shell",
          "[gui][sketch-dimensions]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  install_sketch_interaction_binder(window);
  for (const auto* object_name : {
           "blcad.action.sketch_dimension.horizontal",
           "blcad.action.sketch_dimension.vertical",
           "blcad.action.sketch_dimension.aligned",
           "blcad.action.sketch_dimension.point_to_point",
           "blcad.action.sketch_dimension.length",
           "blcad.action.sketch_dimension.radius",
           "blcad.action.sketch_dimension.diameter",
           "blcad.action.sketch_dimension.angle",
           "blcad.action.sketch_dimension.arc_length",
           "blcad.action.sketch_dimension.edit",
           "blcad.action.sketch_dimension.rebind",
           "blcad.action.sketch_dimension.toggle_mode",
       })
    REQUIRE(window.findChild<QAction*>(QString::fromUtf8(object_name)) != nullptr);
}
