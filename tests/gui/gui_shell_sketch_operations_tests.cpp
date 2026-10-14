#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
namespace {
using Kind = GuiSketchOperationKind;
void enter(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Operations")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Seed geometry", [id](PartDocument& part) -> Result<std::size_t> {
        Sketch sketch = *part.find_sketch(id);
        auto added =
            sketch.add_entity(LineSegment::create(SketchEntityId("line"), {0, 0}, {10, 0}).value());
        if (!added)
          return added;
        added = sketch.add_entity(SketchPoint::create(SketchEntityId("point"), {30, 40}).value());
        if (!added)
          return added;
        return part.update_sketch(std::move(sketch));
      }));
  window.refresh();
  window.sketch_tools().publish_scene();
}
std::string json(ShellWindow& window) {
  return serialize_part_document_to_json(*window.session().part_document()).value();
}
void select_line(ShellWindow& window, const std::string& id = "line") {
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/" + id}));
  window.sketch_tools().refresh_enablement();
}
} // namespace

TEST_CASE("Sketch operation previews cancel cleanly and every transform commits one history entry",
          "[gui][sketch-transform-pattern][sketch-offset-project]") {
  ShellWindow window;
  enter(window);
  const auto id = *window.active_sketch();
  GuiSketchOperationRequest r;
  r.entities = {SketchEntityId("line")};
  Point2 expected_start{}, expected_end{};
  std::size_t expected_lines = 1;
  SECTION("move") {
    r.kind = Kind::Move;
    r.first = {3, 4};
    expected_start = {3, 4};
    expected_end = {13, 4};
  }
  SECTION("rotate") {
    r.kind = Kind::Rotate;
    r.value = std::numbers::pi / 2;
    expected_end = {0, 10};
  }
  SECTION("scale") {
    r.kind = Kind::Scale;
    r.value = 2;
    expected_end = {20, 0};
  }
  SECTION("copy") {
    r.kind = Kind::Copy;
    r.first = {0, 10};
    expected_lines = 2;
    expected_start = {0, 10};
    expected_end = {10, 10};
  }
  SECTION("mirror") {
    r.kind = Kind::Mirror;
    r.first = {0, 5};
    r.second = {1, 5};
    expected_lines = 2;
    expected_start = {0, 10};
    expected_end = {10, 10};
  }
  SECTION("rectangular pattern") {
    r.kind = Kind::RectangularPattern;
    r.first = {20, 0};
    r.second = {0, 10};
    r.first_count = 2;
    r.second_count = 2;
    expected_lines = 4;
    expected_start = {20, 10};
    expected_end = {30, 10};
  }
  SECTION("circular pattern") {
    r.kind = Kind::CircularPattern;
    r.first_count = 4;
    r.value = 2 * std::numbers::pi;
    expected_lines = 4;
    expected_end = {0, -10};
  }
  SECTION("offset") {
    r.kind = Kind::Offset;
    r.value = 3;
    expected_lines = 2;
    expected_start = {0, 3};
    expected_end = {10, 3};
  }
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.session().save_as(dir.filePath("operation.blcad.json").toStdString()));
  const auto before = json(window);
  const auto old_label = std::string(window.session().undo_label());
  auto preview = window.sketch_tools().preview_operation(r);
  INFO((preview.has_error() ? preview.error().message() : ""));
  REQUIRE(preview);
  CHECK(window.viewport().sketch_operation_preview_count() ==
        (expected_lines == 1 ? 1 : expected_lines - 1));
  CHECK(json(window) == before);
  CHECK_FALSE(window.session().dirty());
  CHECK_FALSE(window.viewport().sketch_selection_enabled());
  window.sketch_tools().cancel_operation_preview();
  CHECK(window.viewport().sketch_operation_preview_count() == 0U);
  CHECK(window.viewport().sketch_selection_enabled());
  CHECK(json(window) == before);
  CHECK(window.session().undo_label() == old_label);
  auto applied = window.sketch_tools().apply_operation(r);
  INFO((applied.has_error() ? applied.error().message() : ""));
  REQUIRE(applied);
  CHECK(window.viewport().sketch_operation_preview_count() == 0U);
  CHECK(window.session().dirty());
  const auto* sketch = window.session().part_document()->find_sketch(id);
  REQUIRE(sketch);
  REQUIRE(sketch->line_segments().size() == expected_lines);
  REQUIRE(sketch->find_point(SketchEntityId("point")));
  const auto& last = sketch->line_segments().back();
  CHECK(last.start().x == Catch::Approx(expected_start.x).margin(1e-8));
  CHECK(last.start().y == Catch::Approx(expected_start.y).margin(1e-8));
  CHECK(last.end().x == Catch::Approx(expected_end.x).margin(1e-8));
  CHECK(last.end().y == Catch::Approx(expected_end.y).margin(1e-8));
  const auto after = json(window);
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  REQUIRE(window.session().redo());
  CHECK(json(window) == after);
  REQUIRE(window.session().save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("operation.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
}

TEST_CASE("Sketch transforms respect fixed geometry constraints and driving dimensions atomically",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  GuiSketchOperationRequest r;
  r.entities = {SketchEntityId("line")};
  SECTION("fixed line rejects a translation") {
    REQUIRE(window.sketch_tools().apply_constraint(SketchSolverConstraintKind::Fixed));
    r.kind = Kind::Move;
    r.first = {2, 0};
  }
  SECTION("horizontal line rejects a quarter rotation") {
    REQUIRE(window.sketch_tools().apply_constraint(SketchSolverConstraintKind::Horizontal));
    r.kind = Kind::Rotate;
    r.value = std::numbers::pi / 2;
  }
  SECTION("driving length rejects scaling") {
    REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
    r.kind = Kind::Scale;
    r.value = 2;
  }
  const auto before = json(window);
  const auto label = std::string(window.session().undo_label());
  CHECK_FALSE(window.sketch_tools().preview_operation(r));
  CHECK_FALSE(window.sketch_tools().apply_operation(r));
  CHECK(json(window) == before);
  CHECK(window.session().undo_label() == label);
  CHECK(window.viewport().sketch_selection_enabled());
}

TEST_CASE("Sketch copies report catalog relationships and preserve exact curved profiles",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  GuiSketchOperationRequest copy;
  copy.kind = Kind::Copy;
  copy.entities = {SketchEntityId("line")};
  copy.first = {0, 20};
  auto preview = window.sketch_tools().preview_operation(copy);
  REQUIRE(preview);
  CHECK(preview.value().uncopied_references.size() == 1U);
  REQUIRE(window.sketch_tools().apply_operation(copy));
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Ellipse));
  REQUIRE(tools.add_pick({30, 0}));
  REQUIRE(tools.add_pick({50, 0}));
  REQUIRE(tools.add_pick({30, 10}));
  tools.cancel_tool();
  const auto* source = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(source->arc_closed_profiles().size() == 1U);
  const auto profile = source->arc_closed_profiles().front().id();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "profile/" + profile.value()}));
  GuiSketchOperationRequest mirror;
  mirror.kind = Kind::Mirror;
  mirror.second = {0, 1};
  REQUIRE(tools.apply_operation(mirror));
  const auto* after = window.session().part_document()->find_sketch(*window.active_sketch());
  CHECK(after->arc_closed_profiles().size() == 2U);
  REQUIRE(after->spline_segments().size() == 8U);
  for (const auto& spline : after->spline_segments())
    CHECK(spline.rational());
  const auto mirrored_profile = after->arc_closed_profiles().back().id();
  const auto sketch_id = *window.active_sketch();
  REQUIRE(window.finish_active_sketch());
  REQUIRE(window.feature_tools().select_profile(sketch_id, mirrored_profile));
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(5));
  REQUIRE(window.feature_tools().apply());
  REQUIRE(window.session().part_shape_cache()->final_shape());
}

TEST_CASE("Associative projection and break link are previewed persisted and undoable",
          "[gui][sketch-project][sketch-offset-project]") {
  ShellWindow window;
  enter(window);
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Construction sources", [](PartDocument& part) -> Result<std::size_t> {
        auto added =
            part.add_construction_line(ConstructionLine::create_explicit(
                                           ConstructionLineId("axis"), "Axis", {0, 5, 0}, {1, 0, 0})
                                           .value());
        if (!added)
          return added;
        return part.add_construction_point(
            ConstructionPoint::create_explicit(ConstructionPointId("origin"), "Point", {2, 3, 0})
                .value());
      }));
  GuiSketchOperationRequest project;
  project.kind = Kind::ProjectLine;
  project.projected_line = ProjectedSketchLine::create_from_construction_line(
                               SketchEntityId("reference"), ConstructionLineId("axis"))
                               .value();
  const auto before = json(window);
  REQUIRE(window.sketch_tools().preview_operation(project));
  CHECK(json(window) == before);
  REQUIRE(window.sketch_tools().apply_operation(project));
  const auto projected = json(window);
  auto point = project;
  point.kind = Kind::ProjectPoint;
  point.projected_line.reset();
  point.projected_point = ProjectedSketchPoint::create_from_construction_point(
                              SketchEntityId("reference.point"), ConstructionPointId("origin"))
                              .value();
  REQUIRE(window.sketch_tools().apply_operation(point));
  REQUIRE(window.session().undo());
  CHECK(json(window) == projected);
  GuiSketchOperationRequest detach;
  detach.kind = Kind::BreakLinkLine;
  detach.entities = {SketchEntityId("reference")};
  detach.value = 40;
  REQUIRE(window.sketch_tools().apply_operation(detach));
  const auto* result = window.session().part_document()->find_sketch(id);
  CHECK(result->projected_lines().empty());
  const auto* line = result->find_line_segment(SketchEntityId("reference"));
  REQUIRE(line);
  CHECK(line->start() == Point2{-20, 5});
  CHECK(line->end() == Point2{20, 5});
  REQUIRE(result->find_point(SketchEntityId("point")));
  REQUIRE(window.session().undo());
  CHECK(json(window) == projected);
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  project.projected_line = ProjectedSketchLine::create_from_construction_line(
                               SketchEntityId("missing"), ConstructionLineId("missing"))
                               .value();
  CHECK_FALSE(window.sketch_tools().preview_operation(project));
  CHECK_FALSE(window.sketch_tools().apply_operation(project));
  CHECK(json(window) == before);
}

TEST_CASE("Sketch operation ribbon drives live numeric preview Apply and Escape",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  auto* action = window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.move"));
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  bool visited = false;
  bool accept = false;
  SECTION("apply") {
    accept = true;
  }
  SECTION("cancel") {}
  QTimer::singleShot(0, [&] {
    auto* dialog = window.findChild<QDialog*>(QStringLiteral("sketch.operation.dialog"));
    CHECK(dialog != nullptr);
    if (!dialog)
      return;
    auto* x = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("sketch.operation.x"));
    CHECK(x != nullptr);
    if (!x) {
      dialog->reject();
      return;
    }
    x->setValue(25);
    CHECK(json(window) == before);
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    CHECK(controls != nullptr);
    visited = true;
    if (accept && controls)
      controls->button(QDialogButtonBox::Ok)->click();
    else
      dialog->reject();
  });
  action->trigger();
  REQUIRE(visited);
  CHECK_FALSE(window.session().task().active());
  CHECK(window.viewport().sketch_selection_enabled());
  if (accept) {
    const auto* line = window.session()
                           .part_document()
                           ->find_sketch(*window.active_sketch())
                           ->find_line_segment(SketchEntityId("line"));
    REQUIRE(line);
    CHECK(line->start() == Point2{25, 0});
    REQUIRE(window.session().undo());
  }
  CHECK(json(window) == before);
}

TEST_CASE("Sketch numeric operation editor removes invalid previews and permits correction",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  auto* action =
      window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.scale"));
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  bool visited = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = window.findChild<QDialog*>(QStringLiteral("sketch.operation.dialog"));
    CHECK(dialog);
    if (!dialog)
      return;
    auto* factor = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("sketch.operation.value"));
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    CHECK(factor);
    CHECK(controls);
    if (!factor || !controls) {
      dialog->reject();
      return;
    }
    CHECK(window.viewport().sketch_operation_preview_count() == 1U);
    factor->setValue(0);
    CHECK_FALSE(controls->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(window.viewport().sketch_operation_preview_count() == 0U);
    CHECK(json(window) == before);
    factor->setValue(3);
    CHECK(controls->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(window.viewport().sketch_operation_preview_count() == 1U);
    visited = true;
    controls->button(QDialogButtonBox::Ok)->click();
  });
  action->trigger();
  REQUIRE(visited);
  CHECK(window.session()
            .part_document()
            ->find_sketch(*window.active_sketch())
            ->find_line_segment(SketchEntityId("line"))
            ->end() == Point2{30, 0});
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

TEST_CASE("Sketch projection chooser applies an associative source and persists its identity",
          "[gui][sketch-project]") {
  ShellWindow window;
  enter(window);
  REQUIRE(window.session().commit_part_transaction("Construction line", [](PartDocument& part) {
    return part.add_construction_line(ConstructionLine::create_explicit(ConstructionLineId("axis"),
                                                                        "Projection axis",
                                                                        {0, 7, 0}, {1, 0, 0})
                                          .value());
  }));
  window.refresh();
  auto* action =
      window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.project"));
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  bool visited = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = window.findChild<QDialog*>(QStringLiteral("sketch.operation.dialog"));
    CHECK(dialog);
    if (!dialog)
      return;
    auto* source = dialog->findChild<QComboBox*>(QStringLiteral("sketch.operation.source"));
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    CHECK(source);
    CHECK(controls);
    if (!source || !controls) {
      dialog->reject();
      return;
    }
    CHECK(source->count() == 1);
    CHECK(json(window) == before);
    CHECK(window.viewport().sketch_operation_preview_count() == 1U);
    visited = true;
    controls->button(QDialogButtonBox::Ok)->click();
  });
  action->trigger();
  REQUIRE(visited);
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch->projected_lines().size() == 1U);
  CHECK(sketch->projected_lines().front().construction_line() == ConstructionLineId("axis"));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.session().save_as(dir.filePath("projected.blcad.json").toStdString()));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("projected.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == json(window));
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

TEST_CASE("Sketch rigid transforms preserve compatible driving intent and reject stale selections",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  const auto sketch_id = *window.active_sketch();
  const auto catalog = window.session().sketch_dimension_catalog(sketch_id).value();
  const auto before = json(window);
  GuiSketchOperationRequest move;
  move.kind = Kind::Move;
  move.entities = {SketchEntityId("line")};
  move.first = {5, 7};
  auto result = window.sketch_tools().apply_operation(move);
  INFO((result.has_error() ? result.error().message() : ""));
  REQUIRE(result);
  CHECK(window.session()
            .part_document()
            ->find_sketch(sketch_id)
            ->find_line_segment(SketchEntityId("line"))
            ->start() == Point2{5, 7});
  CHECK(window.session().sketch_dimension_catalog(sketch_id).value().dimensions() ==
        catalog.dimensions());
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  move.entities = {SketchEntityId("missing")};
  CHECK_FALSE(window.sketch_tools().preview_operation(move));
  CHECK_FALSE(window.sketch_tools().apply_operation(move));
  CHECK(json(window) == before);
}
