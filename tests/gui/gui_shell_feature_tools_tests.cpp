#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

using namespace blcad;
using namespace blcad::gui;

namespace {

QTreeWidgetItem* find_tree_item(QTreeWidgetItem* item, const QString& semantic_id) {
  if (item->data(0, Qt::UserRole).toString() == semantic_id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_tree_item(item->child(index), semantic_id))
      return found;
  return nullptr;
}

QTreeWidgetItem* find_tree_item(QTreeWidget& tree, const QString& semantic_id) {
  for (int index = 0; index < tree.topLevelItemCount(); ++index)
    if (auto* found = find_tree_item(tree.topLevelItem(index), semantic_id))
      return found;
  return nullptr;
}

QPushButton* find_push_button(QWidget& root, const QString& text) {
  const auto* panel = root.findChild<QWidget*>(QStringLiteral("blcad.shell.feature_task"));
  if (!panel)
    return nullptr;
  const auto buttons = panel->findChildren<QPushButton*>();
  const auto found = std::find_if(buttons.begin(), buttons.end(), [&text](const auto* button) {
    return button != nullptr && button->text() == text;
  });
  return found == buttons.end() ? nullptr : *found;
}

QToolButton* find_tool_button(QWidget& root, const QString& text) {
  const auto buttons = root.findChildren<QToolButton*>();
  const auto found = std::find_if(buttons.begin(), buttons.end(), [&text](const auto* button) {
    return button != nullptr &&
           (button->text() == text ||
            (button->defaultAction() != nullptr && button->defaultAction()->text() == text));
  });
  return found == buttons.end() ? nullptr : *found;
}

void create_finished_rectangle(ShellWindow& window, Point2 first = {5.0, 5.0},
                               Point2 second = {25.0, 15.0}) {
  REQUIRE(window.new_part(QStringLiteral("Featureteil")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(window.sketch_tools().add_pick(first));
  REQUIRE(window.sketch_tools().add_pick(second));
  REQUIRE_FALSE(window.sketch_tools().tool_active());
  const bool finished = window.finish_active_sketch();
  INFO(window.statusBar()->currentMessage().toStdString());
  REQUIRE(finished);
}

void create_raw_rectangle(ShellWindow& window, Point2 first = {5.0, 5.0},
                          Point2 second = {25.0, 15.0}) {
  REQUIRE(window.new_part(QStringLiteral("Raw-Profilteil")));
  auto sketch =
      Sketch::create(SketchId("sketch.raw"), "Raw-Skizze", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), first, {second.x, first.y}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), {second.x, first.y}, second).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.c"), second, {first.x, second.y}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.d"), {first.x, second.y}, first).value()));
  REQUIRE(window.session().commit_part_transaction(
      "Raw-Skizze laden", [sketch = std::move(sketch.value())](PartDocument& part) mutable {
        return part.add_sketch(std::move(sketch));
      }));
  window.refresh();
}

void draw_freehand_rectangle(ShellWindow& window, Point2 first, Point2 second,
                             bool leave_tool_active = false) {
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick(first));
  REQUIRE(tools.add_pick({second.x, first.y}));
  REQUIRE(tools.add_pick(second));
  REQUIRE(tools.add_pick({first.x, second.y}));
  REQUIRE(tools.add_pick(first));
  if (!leave_tool_active)
    tools.cancel_tool();
}

void click_widget(QWidget& widget, const QPoint& position) {
  const QPointF local(position);
  const QPointF global(widget.mapToGlobal(position));
  QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&widget, &press);
  QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
  QApplication::sendEvent(&widget, &release);
  qApp->processEvents();
}

void right_click_widget(QWidget& widget, const QPoint& position) {
  const QPointF local(position);
  const QPointF global(widget.mapToGlobal(position));
  QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::RightButton, Qt::RightButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&widget, &press);
  QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::RightButton, Qt::NoButton,
                      Qt::NoModifier);
  QApplication::sendEvent(&widget, &release);
  qApp->processEvents();
}

void double_click_widget(QWidget& widget, const QPoint& position) {
  click_widget(widget, position);
  const QPointF local(position);
  const QPointF global(widget.mapToGlobal(position));
  QMouseEvent press(QEvent::MouseButtonDblClick, local, global, Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&widget, &press);
  QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton,
                      Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&widget, &release);
  qApp->processEvents();
}

bool click_modal_ok_while_clicking(QWidget& trigger, const QPoint& position) {
  bool accepted = false;
  QTimer::singleShot(0, [&accepted] {
    auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
    if (dialog == nullptr)
      return;
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    auto* ok = buttons != nullptr ? buttons->button(QDialogButtonBox::Ok) : nullptr;
    if (ok == nullptr)
      return;
    accepted = true;
    click_widget(*ok, ok->rect().center());
  });
  click_widget(trigger, position);
  return accepted;
}

bool click_popup_action(QToolButton& trigger, const QString& action_text) {
  bool activated = false;
  QTimer::singleShot(0, [&activated, &action_text] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu == nullptr)
      return;
    const auto actions = menu->actions();
    const auto found = std::find_if(actions.begin(), actions.end(), [&action_text](QAction* action) {
      return action != nullptr && action->text() == action_text;
    });
    if (found == actions.end())
      return;
    const QRect action_rect = menu->actionGeometry(*found);
    if (action_rect.isEmpty())
      return;
    activated = true;
    click_widget(*menu, action_rect.center());
  });
  click_widget(trigger, trigger.rect().center());
  return activated;
}

} // namespace

TEST_CASE("Block 135 selecting a Sketch node exposes its unique profile to features",
          "[gui][shell-features][gui][modeling-workspace]") {
  ShellWindow window;
  create_finished_rectangle(window);

  auto& tree = window.model_browser();
  auto* sketch_item = find_tree_item(tree, QStringLiteral("sketch.skizze1"));
  REQUIRE(sketch_item != nullptr);
  tree.setCurrentItem(sketch_item);

  auto& tools = window.feature_tools();
  REQUIRE(tools.selected_profile().has_value());
  CHECK(tools.selected_profile()->first == SketchId("sketch.skizze1"));
  CHECK(tools.extrude_action().isEnabled());
  CHECK(tools.revolve_action().isEnabled());

  tools.extrude_action().trigger();
  REQUIRE(tools.active());
  tools.cancel();
  tools.revolve_action().trigger();
  REQUIRE(tools.active());
  tools.cancel();
}

TEST_CASE("Feature actions stay disabled while the selected Sketch is being edited",
          "[gui][shell-features][gui][extrude-regression]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_finished_rectangle(window);
  qApp->processEvents();

  auto& tree = window.model_browser();
  auto* sketch_item = find_tree_item(tree, QStringLiteral("sketch.skizze1"));
  REQUIRE(sketch_item != nullptr);
  tree.scrollToItem(sketch_item);
  qApp->processEvents();
  const QRect item_rect = tree.visualItemRect(sketch_item);
  REQUIRE_FALSE(item_rect.isEmpty());
  double_click_widget(*tree.viewport(), item_rect.center());

  REQUIRE(window.active_sketch().has_value());
  CHECK_FALSE(window.feature_tools().extrude_action().isEnabled());
  CHECK_FALSE(window.feature_tools().revolve_action().isEnabled());
  window.feature_tools().extrude_action().trigger();
  CHECK_FALSE(window.feature_tools().active());
}

TEST_CASE("Additive feature actions reject a CircularHolePattern-only Sketch",
          "[gui][shell-features][gui][extrude-regression]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Lochmuster")));
  REQUIRE(window.session().commit_part_transaction(
      "Lochmuster anlegen", [](PartDocument& part) -> Result<std::size_t> {
        auto radius = Parameter::create_length(
            ParameterId("pattern.radius"), "Radius",
            Quantity::length_mm(20.0, "pattern.radius").value());
        auto count = Parameter::create_count(
            ParameterId("pattern.count"), "Anzahl",
            Quantity::count(6.0, "pattern.count").value());
        auto diameter = Parameter::create_length(
            ParameterId("pattern.diameter"), "Durchmesser",
            Quantity::length_mm(5.0, "pattern.diameter").value());
        if (radius.has_error()) return Result<std::size_t>::failure(radius.error());
        if (count.has_error()) return Result<std::size_t>::failure(count.error());
        if (diameter.has_error()) return Result<std::size_t>::failure(diameter.error());
        if (auto added = part.add_parameter(std::move(radius.value())); added.has_error())
          return added;
        if (auto added = part.add_parameter(std::move(count.value())); added.has_error())
          return added;
        if (auto added = part.add_parameter(std::move(diameter.value())); added.has_error())
          return added;
        auto sketch = Sketch::create(SketchId("sketch.pattern"), "Lochmuster",
                                     DatumPlaneId("datum.xy"));
        if (sketch.has_error()) return Result<std::size_t>::failure(sketch.error());
        auto pattern = CircularHolePattern::create(
            ProfileId("pattern.holes"), ParameterId("pattern.radius"),
            ParameterId("pattern.count"), ParameterId("pattern.diameter"));
        if (pattern.has_error()) return Result<std::size_t>::failure(pattern.error());
        if (auto added = sketch.value().add_profile(std::move(pattern.value()));
            added.has_error())
          return added;
        return part.add_sketch(std::move(sketch.value()));
      }));
  window.refresh();

  auto* item = find_tree_item(window.model_browser(), QStringLiteral("sketch.pattern"));
  REQUIRE(item != nullptr);
  window.model_browser().setCurrentItem(item);
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  CHECK_FALSE(window.feature_tools().extrude_action().isEnabled());
  CHECK_FALSE(window.feature_tools().revolve_action().isEnabled());
}

TEST_CASE("Block 135 selecting visible Sketch geometry resolves its owning profile",
          "[gui][shell-features][gui][modeling-workspace]") {
  ShellWindow window;
  create_finished_rectangle(window);

  const auto* part = window.session().part_document();
  REQUIRE(part != nullptr);
  const auto* sketch = part->find_sketch(SketchId("sketch.skizze1"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->closed_profiles().size() == 1U);
  REQUIRE_FALSE(sketch->closed_profiles().front().line_segments().empty());
  const auto expected_profile = sketch->closed_profiles().front().id();
  const std::string line_semantic =
      "sketch/" + sketch->id().value() + "/entity/" +
      sketch->closed_profiles().front().line_segments().front().value();

  REQUIRE(window.viewport().select_semantic(line_semantic));
  auto& tools = window.feature_tools();
  REQUIRE(tools.selected_profile().has_value());
  CHECK(tools.selected_profile()->first == sketch->id());
  CHECK(tools.selected_profile()->second == expected_profile);
  CHECK(tools.extrude_action().isEnabled());
  CHECK(tools.revolve_action().isEnabled());

  tools.extrude_action().trigger();
  REQUIRE(tools.active());
  tools.cancel();
}

TEST_CASE("Block 135 a selected contour disambiguates a Sketch with multiple profiles",
          "[gui][shell-features][gui][modeling-workspace]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Mehrprofilteil")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  auto& sketch_tools = window.sketch_tools();
  REQUIRE(sketch_tools.begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(sketch_tools.add_pick({0.0, 0.0}));
  REQUIRE(sketch_tools.add_pick({10.0, 10.0}));
  REQUIRE(sketch_tools.begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(sketch_tools.add_pick({20.0, 0.0}));
  REQUIRE(sketch_tools.add_pick({30.0, 10.0}));
  REQUIRE(window.finish_active_sketch());

  auto& tools = window.feature_tools();
  CHECK_FALSE(tools.selected_profile().has_value());
  const auto* part = window.session().part_document();
  REQUIRE(part != nullptr);
  const auto* sketch = part->find_sketch(SketchId("sketch.skizze1"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->closed_profiles().size() == 2U);
  const auto& selected_contour = sketch->closed_profiles().front();
  const ProfileId selected_profile = selected_contour.id();
  REQUIRE_FALSE(selected_contour.line_segments().empty());
  const std::string line_semantic =
      "sketch/" + sketch->id().value() + "/entity/" +
      selected_contour.line_segments().front().value();

  REQUIRE(window.viewport().select_semantic(line_semantic));
  REQUIRE(tools.selected_profile().has_value());
  CHECK(tools.selected_profile()->second == selected_profile);
  CHECK(tools.extrude_action().isEnabled());
  tools.extrude_action().trigger();
  REQUIRE(tools.active());
  const FeatureId feature_id = tools.coordinator().extrude().feature_id();
  const BodyId body_id = tools.coordinator().extrude().result_body_id();
  REQUIRE(tools.apply());
  const auto* applied = window.session().part_document()->find_feature(feature_id);
  REQUIRE(applied != nullptr);
  REQUIRE(applied->profile().has_value());
  CHECK(*applied->profile() == selected_profile);
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body_id) != nullptr);
}

TEST_CASE("Block 135 Finish Sketch materializes one freehand closed contour for features",
          "[gui][shell-features][gui][sketch-finish]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Freihandprofil")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  auto& sketch_tools = window.sketch_tools();
  REQUIRE(sketch_tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(sketch_tools.add_pick({0.0, 0.0}));
  REQUIRE(sketch_tools.add_pick({20.0, 0.0}));
  REQUIRE(sketch_tools.add_pick({20.0, 15.0}));
  REQUIRE(sketch_tools.add_pick({0.0, 15.0}));
  REQUIRE(sketch_tools.add_pick({0.0, 0.0}));
  sketch_tools.cancel_tool();

  const auto* before = window.session().part_document()->find_sketch(SketchId("sketch.skizze1"));
  REQUIRE(before != nullptr);
  CHECK(before->profile_count() == 0U);
  const bool finished = window.finish_active_sketch();
  INFO(window.statusBar()->currentMessage().toStdString());
  REQUIRE(finished);

  const auto* after = window.session().part_document()->find_sketch(SketchId("sketch.skizze1"));
  REQUIRE(after != nullptr);
  CHECK(after->closed_profiles().size() == 1U);
  auto& tools = window.feature_tools();
  REQUIRE(tools.selected_profile().has_value());
  CHECK(tools.extrude_action().isEnabled());
  CHECK(tools.revolve_action().isEnabled());
  tools.extrude_action().trigger();
  REQUIRE(tools.active());
  tools.cancel();
}

TEST_CASE("Finish Sketch cancels a re-armed Line step and exposes the closed profile",
          "[gui][shell-features][gui][sketch-finish][extrude-regression]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Linienprofil")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  draw_freehand_rectangle(window, {0.0, 0.0}, {20.0, 15.0}, true);

  const auto* before =
      window.session().part_document()->find_sketch(SketchId("sketch.skizze1"));
  REQUIRE(before != nullptr);
  REQUIRE(before->profile_count() == 0U);
  REQUIRE(window.session().task().active());

  REQUIRE(window.finish_active_sketch());
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.active_sketch().has_value());
  const auto* finished =
      window.session().part_document()->find_sketch(SketchId("sketch.skizze1"));
  REQUIRE(finished != nullptr);
  CHECK(finished->closed_profiles().size() == 1U);
  REQUIRE_FALSE(finished->closed_profiles().front().line_segments().empty());

  auto& feature_tools = window.feature_tools();
  // Reproduce both user-facing selection routes after finishing without Esc.
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(feature_tools.extrude_action().isEnabled());
  auto* sketch_item =
      find_tree_item(window.model_browser(), QStringLiteral("sketch.skizze1"));
  REQUIRE(sketch_item != nullptr);
  window.model_browser().setCurrentItem(sketch_item);
  REQUIRE(feature_tools.extrude_action().isEnabled());
  feature_tools.extrude_action().trigger();
  REQUIRE(feature_tools.active());
  feature_tools.cancel();

  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(feature_tools.extrude_action().isEnabled());
  const std::string line_semantic =
      "sketch/sketch.skizze1/entity/" +
      finished->closed_profiles().front().line_segments().front().value();
  REQUIRE(window.viewport().select_semantic(line_semantic));
  REQUIRE(feature_tools.extrude_action().isEnabled());
  feature_tools.extrude_action().trigger();
  REQUIRE(feature_tools.active());
  const BodyId body = feature_tools.coordinator().extrude().result_body_id();
  REQUIRE(feature_tools.apply());
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);
}

TEST_CASE("Finish Sketch materializes every independent freehand contour",
          "[gui][shell-features][gui][sketch-finish][multi-profile]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Freihand-Mehrprofil")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  draw_freehand_rectangle(window, {0.0, 0.0}, {10.0, 10.0});
  draw_freehand_rectangle(window, {20.0, 0.0}, {30.0, 10.0});

  REQUIRE(window.finish_active_sketch());
  const auto* sketch =
      window.session().part_document()->find_sketch(SketchId("sketch.skizze1"));
  REQUIRE(sketch != nullptr);
  CHECK(sketch->closed_profiles().size() == 2U);
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  CHECK_FALSE(window.feature_tools().extrude_action().isEnabled());
}

TEST_CASE("An unrelated raw entity never falls back to the Sketch's unique profile",
          "[gui][shell-features][gui][modeling-workspace][selection]") {
  ShellWindow window;
  create_finished_rectangle(window);
  REQUIRE(window.feature_tools().selected_profile().has_value());

  GuiSketchWorkbench workbench;
  REQUIRE(workbench.add_line(
      window.session(), SketchId("sketch.skizze1"),
      LineSegment::create(SketchEntityId("line.unrelated"), {40.0, 0.0}, {50.0, 5.0})
          .value()));
  window.refresh();

  REQUIRE(window.viewport().select_semantic(
      "sketch/sketch.skizze1/entity/line.unrelated"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  CHECK_FALSE(window.feature_tools().extrude_action().isEnabled());
  CHECK_FALSE(window.feature_tools().revolve_action().isEnabled());
}

TEST_CASE("Block 135 Finish Sketch hands one profile to Extrude and commits atomically",
          "[gui][shell-features][gui][extrude-revolve]") {
  ShellWindow window;
  create_finished_rectangle(window);

  auto& tools = window.feature_tools();
  REQUIRE(tools.selected_profile().has_value());
  CHECK(tools.extrude_action().isEnabled());
  CHECK(tools.revolve_action().isEnabled());
  const auto* before = window.session().part_document();
  REQUIRE(before != nullptr);
  const std::size_t parameter_count = before->parameter_count();
  const std::size_t body_count = before->body_count();
  const std::size_t persistent_presentations = window.viewport().presentation_count();

  REQUIRE(tools.begin_extrude());
  CHECK(tools.active());
  CHECK(tools.command() == ShellFeatureTools::Command::Extrude);
  CHECK(window.session().task().stage() == GuiTaskStage::Preview);
  REQUIRE(tools.manipulators().handles().size() == 1U);
  CHECK(tools.manipulators().handles().front().id == kExtrudeExtentHandleId);
  CHECK(window.viewport().feature_preview_active());
  CHECK(window.viewport().presentation_count() == persistent_presentations);

  // Auto-provisioned Body and parameter remain candidate-only before Apply.
  CHECK(window.session().part_document()->body_count() == body_count);
  CHECK(window.session().part_document()->parameter_count() == parameter_count);
  const FeatureId feature = tools.coordinator().extrude().feature_id();
  const BodyId body = tools.coordinator().extrude().result_body_id();
  const ParameterId distance = tools.coordinator().extrude().distance_parameter_id();

  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = std::string(kExtrudeExtentHandleId);
  candidate.value_kind = GuiViewportManipulatorValueKind::Distance;
  candidate.source = GuiViewportManipulatorInputSource::Numeric;
  candidate.scalar_value = 32.0;
  REQUIRE(tools.accept_manipulator_candidate(candidate));
  CHECK(tools.coordinator().extrude().distance() == 32.0);
  CHECK(tools.numeric_editor().text() == QStringLiteral("32"));
  CHECK(window.viewport().feature_preview_active());

  REQUIRE(tools.apply());
  CHECK_FALSE(tools.active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  const auto* part = window.session().part_document();
  REQUIRE(part != nullptr);
  CHECK(part->find_feature(feature) != nullptr);
  REQUIRE(part->find_parameter(distance) != nullptr);
  CHECK(part->find_parameter(distance)->value().millimeters() == 32.0);
  CHECK(part->find_body(body) != nullptr);
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);
  CHECK(window.viewport().select_semantic("body/" + body.value()));

  // Body + parameter + feature are one history entry.
  REQUIRE(window.undo());
  part = window.session().part_document();
  REQUIRE(part != nullptr);
  CHECK(part->find_feature(feature) == nullptr);
  CHECK(part->find_parameter(distance) == nullptr);
  CHECK(part->find_body(body) == nullptr);
}

TEST_CASE("Block 135 cancelling an auto-provisioned Extrude leaves the Part unchanged",
          "[gui][shell-features][gui][extrude-revolve]") {
  ShellWindow window;
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  const auto* before = window.session().part_document();
  REQUIRE(before != nullptr);
  const std::size_t parameters = before->parameter_count();
  const std::size_t bodies = before->body_count();
  const std::size_t features = before->feature_count();

  SECTION("Extrude") {
    REQUIRE(tools.begin_extrude());
    const FeatureId feature = tools.coordinator().extrude().feature_id();
    REQUIRE(tools.set_numeric_value(18.0));
    tools.cancel();

    const auto* after = window.session().part_document();
    REQUIRE(after != nullptr);
    CHECK(after->parameter_count() == parameters);
    CHECK(after->body_count() == bodies);
    CHECK(after->feature_count() == features);
    CHECK(after->find_feature(feature) == nullptr);
    CHECK_FALSE(window.session().task().active());
    CHECK(tools.manipulators().handles().empty());
    CHECK_FALSE(window.viewport().feature_preview_active());
  }

  SECTION("Revolve") {
    REQUIRE(tools.begin_revolve());
    const FeatureId feature = tools.coordinator().revolve().feature_id();
    REQUIRE(tools.set_numeric_value(120.0));
    tools.cancel();

    const auto* after = window.session().part_document();
    REQUIRE(after != nullptr);
    CHECK(after->parameter_count() == parameters);
    CHECK(after->body_count() == bodies);
    CHECK(after->feature_count() == features);
    CHECK(after->find_revolve_feature(feature) == nullptr);
    CHECK_FALSE(window.session().task().active());
    CHECK(tools.manipulators().handles().empty());
  }
}

TEST_CASE("Block 135 numeric input is unit-strict and rejects incompatible candidates",
          "[gui][shell-features][gui][manipulator-numeric-coupling]") {
  ShellWindow window;
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_extrude());

  CHECK(tools.numeric_editor().objectName() == QStringLiteral("blcad.manipulator_numeric_hud"));
  auto* overlay = window.viewport().findChild<QWidget*>(
      QStringLiteral("blcad.viewport_manipulator_overlay"), Qt::FindDirectChildrenOnly);
  REQUIRE(overlay != nullptr);
  CHECK(overlay->isVisibleTo(&window.viewport()));

  REQUIRE(tools.set_numeric_text(QStringLiteral("12.5 mm")));
  CHECK(tools.coordinator().extrude().distance() == 12.5);
  CHECK(tools.numeric_editor().text() == QStringLiteral("12.5"));

  CHECK(tools.set_numeric_text(QStringLiteral("25 deg")).has_error());
  CHECK(tools.set_numeric_text(QStringLiteral("13,5 mm")).has_error());
  CHECK(tools.set_numeric_text(QStringLiteral("0 mm")).has_error());
  CHECK(tools.coordinator().extrude().distance() == 12.5);
  CHECK(tools.numeric_editor().text() == QStringLiteral("12.5"));

  GuiViewportManipulatorCandidate incompatible;
  incompatible.handle_id = std::string(kRevolveAngleHandleId);
  incompatible.value_kind = GuiViewportManipulatorValueKind::Angle;
  incompatible.source = GuiViewportManipulatorInputSource::Numeric;
  incompatible.scalar_value = 90.0;
  CHECK(tools.accept_manipulator_candidate(incompatible).has_error());
  CHECK(tools.coordinator().extrude().distance() == 12.5);
  tools.cancel();
}

TEST_CASE("Block 135 feature manipulators reject out-of-domain values without mutation",
          "[gui][shell-features][gui][viewport-manipulators]") {
  ShellWindow window;
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();

  REQUIRE(tools.begin_extrude());
  auto extent = tools.coordinator().extrude().extent_handle();
  REQUIRE(extent.has_value());
  CHECK(extent->minimum_value > 0.0);
  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = std::string(kExtrudeExtentHandleId);
  candidate.value_kind = GuiViewportManipulatorValueKind::Distance;
  for (const double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    candidate.scalar_value = invalid;
    CHECK_FALSE(tools.coordinator().extrude().apply_manipulator(candidate));
    CHECK(tools.coordinator().extrude().distance() == 10.0);
  }
  tools.cancel();

  REQUIRE(tools.begin_revolve());
  auto angle = tools.coordinator().revolve().angle_handle();
  REQUIRE(angle.has_value());
  CHECK(angle->minimum_value > 0.0);
  CHECK(angle->maximum_value < 360.0);
  candidate.handle_id = std::string(kRevolveAngleHandleId);
  candidate.value_kind = GuiViewportManipulatorValueKind::Angle;
  for (const double invalid : {0.0, -1.0, 360.0,
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    candidate.scalar_value = invalid;
    CHECK_FALSE(tools.coordinator().revolve().apply_manipulator(candidate));
    CHECK(tools.coordinator().revolve().angle() == 180.0);
  }
  tools.cancel();
}

TEST_CASE("Block 135 failed candidate preview restores the last visible valid value",
          "[gui][shell-features][gui][viewport-manipulators][preview]") {
  ShellWindow window;
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_extrude());
  REQUIRE(window.viewport().feature_preview_active());

  // Positive Length values below the geometry tolerance pass the controller's
  // domain check but cannot form an OCCT extrusion. This exercises the rollback
  // after the real shape preview, rather than the earlier candidate validation.
  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = std::string(kExtrudeExtentHandleId);
  candidate.value_kind = GuiViewportManipulatorValueKind::Distance;
  candidate.source = GuiViewportManipulatorInputSource::Drag;
  candidate.scalar_value = 1.0e-10;
  const auto rejected = tools.accept_manipulator_candidate(candidate);
  REQUIRE(rejected.has_error());
  CHECK(tools.coordinator().extrude().distance() == 10.0);
  CHECK(tools.numeric_editor().text() == QStringLiteral("10"));
  CHECK(window.viewport().feature_preview_active());

  const ParameterId distance = tools.coordinator().extrude().distance_parameter_id();
  REQUIRE(tools.apply());
  const auto* parameter = window.session().part_document()->find_parameter(distance);
  REQUIRE(parameter != nullptr);
  CHECK(parameter->value().millimeters() == 10.0);
}

TEST_CASE("Block 135 failed geometry preview keeps Apply disabled across shell refreshes",
          "[gui][shell-features][gui][viewport-manipulators][preview]") {
  ShellWindow window;
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_extrude());
  auto* apply_button = find_push_button(window, QStringLiteral("Anwenden"));
  REQUIRE(apply_button != nullptr);
  REQUIRE(apply_button->isEnabled());

  const FeatureId feature = tools.coordinator().extrude().feature_id();
  const std::size_t body_count = window.session().part_document()->body_count();
  tools.coordinator().extrude().set_distance(1.0e-10);
  const auto rejected = tools.apply();
  REQUIRE(rejected.has_error());
  CHECK(tools.active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(apply_button->isEnabled());
  CHECK(window.session().part_document()->find_feature(feature) == nullptr);
  CHECK(window.session().part_document()->body_count() == body_count);

  // A generic selection/document refresh must not mistake an active command
  // for a valid geometry preview.
  tools.refresh_enablement();
  CHECK_FALSE(apply_button->isEnabled());

  // Publishing a real preview again is the only path that re-enables Apply.
  REQUIRE(tools.set_numeric_value(12.0));
  CHECK(window.viewport().feature_preview_active());
  CHECK(apply_button->isEnabled());
  tools.cancel();
}

TEST_CASE("Block 135 Apply rejects a mismatched task before committing the document",
          "[gui][shell-features][gui][extrude-revolve][task-state]") {
  ShellWindow window;
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_extrude());
  const FeatureId feature = tools.coordinator().extrude().feature_id();
  const std::size_t body_count = window.session().part_document()->body_count();
  const std::size_t parameter_count = window.session().part_document()->parameter_count();
  auto* apply_button = find_push_button(window, QStringLiteral("Anwenden"));
  REQUIRE(apply_button != nullptr);

  SECTION("task is no longer in Preview") {
    REQUIRE(window.session().task().return_to_editing());
  }

  SECTION("Preview belongs to another command") {
    REQUIRE(window.session().task().cancel());
    REQUIRE(window.session().task().begin("part.revolve"));
    REQUIRE(window.session().task().begin_parameter_editing());
    REQUIRE(window.session().task().show_preview());
  }

  tools.refresh_enablement();
  CHECK_FALSE(apply_button->isEnabled());
  const auto rejected = tools.apply();
  REQUIRE(rejected.has_error());
  CHECK(tools.active());
  const auto* part = window.session().part_document();
  REQUIRE(part != nullptr);
  CHECK(part->find_feature(feature) == nullptr);
  CHECK(part->body_count() == body_count);
  CHECK(part->parameter_count() == parameter_count);
  tools.cancel();
}

TEST_CASE("Block 135 reports a failed manipulator candidate when the drag is released",
          "[gui][shell-features][gui][viewport-manipulators][preview]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_extrude());
  tools.refresh_presentation();

  const auto presentations = tools.manipulators().presentations();
  REQUIRE(presentations.size() == 1U);
  const QPointF endpoint(presentations.front().endpoint.x,
                         presentations.front().endpoint.y);
  auto& viewport = window.viewport();
  QMouseEvent press(QEvent::MouseButtonPress, endpoint,
                    viewport.mapToGlobal(endpoint.toPoint()), Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &press);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  REQUIRE(tools.manipulators().set_numeric_input("0.0000000001 mm"));
  const std::size_t diagnostic_count = window.session().diagnostics().size();

  QMouseEvent release(QEvent::MouseButtonRelease, endpoint,
                      viewport.mapToGlobal(endpoint.toPoint()), Qt::LeftButton,
                      Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &release);
  qApp->processEvents();

  CHECK(tools.manipulators().active_handle_id().empty());
  CHECK(tools.coordinator().extrude().distance() == 10.0);
  CHECK(tools.numeric_editor().text() == QStringLiteral("10"));
  CHECK(window.viewport().feature_preview_active());
  REQUIRE(window.session().diagnostics().size() == diagnostic_count + 1U);
  const auto& diagnostic = window.session().diagnostics().back();
  CHECK(window.statusBar()->currentMessage().contains(
      QString::fromStdString(diagnostic.message)));
  tools.cancel();
}

TEST_CASE("Block 135 profile handoff follows current semantic selection exactly",
          "[gui][shell-features][gui][modeling-workspace]") {
  ShellWindow window;
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.selected_profile().has_value());
  const auto profile = *tools.selected_profile();

  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(tools.selected_profile().has_value());
  CHECK_FALSE(tools.extrude_action().isEnabled());
  CHECK_FALSE(tools.revolve_action().isEnabled());

  const std::string semantic =
      "sketch/" + profile.first.value() + "/profile/" + profile.second.value();
  REQUIRE(window.viewport().select_semantic(semantic));
  REQUIRE(tools.selected_profile().has_value());
  CHECK(tools.extrude_action().isEnabled());
  CHECK(tools.revolve_action().isEnabled());
  REQUIRE(window.session().selection().size() == 1U);
  CHECK(window.session().selection().entries().front().semantic_id == semantic);

  REQUIRE(tools.begin_extrude());
  tools.cancel();
  REQUIRE(window.session().selection().size() == 1U);
  CHECK(window.session().selection().entries().front().semantic_id == semantic);
}

TEST_CASE("Block 135 auto-provisioning skips conflicting generated parameter names",
          "[gui][shell-features][gui][extrude-revolve]") {
  ShellWindow window;
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.selected_profile().has_value());
  const auto profile = *tools.selected_profile();
  REQUIRE(window.session().commit_part_transaction(
      "Conflicting user parameter", [](PartDocument& part) {
        auto quantity = Quantity::length_mm(4.0, "user.depth");
        if (quantity.has_error())
          return Result<std::size_t>::failure(quantity.error());
        auto parameter = Parameter::create_length(ParameterId("user.depth"), "Extrusion 1 Tiefe",
                                                  quantity.value());
        if (parameter.has_error())
          return Result<std::size_t>::failure(parameter.error());
        return part.add_parameter(std::move(parameter.value()));
      }));
  window.refresh();
  REQUIRE(tools.select_profile(profile.first, profile.second));

  const auto begun = tools.begin_extrude();
  INFO((begun.has_error() ? begun.error().message() : "extrude started"));
  REQUIRE(begun);
  CHECK(tools.coordinator().extrude().feature_id() == FeatureId("feature.extrusion.2"));
  CHECK(tools.coordinator().extrude().distance_parameter_id() ==
        ParameterId("parameter.extrusion.2.depth"));
  CHECK(tools.coordinator().extrude().result_body_id() == BodyId("body.2"));
  tools.cancel();
}

TEST_CASE("Block 135 shell-owned manipulator overlay routes real viewport drag events",
          "[gui][shell-features][gui][viewport-manipulators]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_extrude());
  tools.refresh_presentation();

  const auto presentations = tools.manipulators().presentations();
  REQUIRE(presentations.size() == 1U);
  const auto& handle = presentations.front();
  const double dx = handle.endpoint.x - handle.anchor.x;
  const double dy = handle.endpoint.y - handle.anchor.y;
  const double magnitude = std::hypot(dx, dy);
  REQUIRE(magnitude > 1.0);
  const QPointF from(handle.endpoint.x, handle.endpoint.y);
  const QPointF to(handle.endpoint.x + 30.0 * dx / magnitude,
                   handle.endpoint.y + 30.0 * dy / magnitude);
  auto& viewport = window.viewport();
  QMouseEvent press(QEvent::MouseButtonPress, from, viewport.mapToGlobal(from.toPoint()),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &press);
  QMouseEvent move(QEvent::MouseMove, to, viewport.mapToGlobal(to.toPoint()), Qt::NoButton,
                   Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &move);
  REQUIRE(tools.set_numeric_text(QStringLiteral("22 mm")));
  CHECK(tools.manipulators().numeric_override_active());
  const QPointF farther(to.x() + 25.0 * dx / magnitude, to.y() + 25.0 * dy / magnitude);
  QMouseEvent overridden_move(QEvent::MouseMove, farther, viewport.mapToGlobal(farther.toPoint()),
                              Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &overridden_move);
  CHECK(tools.coordinator().extrude().distance() == 22.0);
  QMouseEvent release(QEvent::MouseButtonRelease, farther, viewport.mapToGlobal(farther.toPoint()),
                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &release);
  qApp->processEvents();

  CHECK(tools.coordinator().extrude().distance() == 22.0);
  CHECK(tools.manipulators().active_handle_id().empty());
  tools.cancel();
}

TEST_CASE("Block 135 Escape rolls back an active drag before cancelling the command",
          "[gui][shell-features][gui][viewport-manipulators]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_finished_rectangle(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_extrude());
  tools.refresh_presentation();

  const auto presentations = tools.manipulators().presentations();
  REQUIRE(presentations.size() == 1U);
  const auto& handle = presentations.front();
  const double dx = handle.endpoint.x - handle.anchor.x;
  const double dy = handle.endpoint.y - handle.anchor.y;
  const double magnitude = std::hypot(dx, dy);
  REQUIRE(magnitude > 1.0);
  const QPointF from(handle.endpoint.x, handle.endpoint.y);
  const QPointF to(handle.endpoint.x + 30.0 * dx / magnitude,
                   handle.endpoint.y + 30.0 * dy / magnitude);
  auto& viewport = window.viewport();
  QMouseEvent press(QEvent::MouseButtonPress, from, viewport.mapToGlobal(from.toPoint()),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &press);
  QMouseEvent move(QEvent::MouseMove, to, viewport.mapToGlobal(to.toPoint()), Qt::NoButton,
                   Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &move);
  REQUIRE(tools.coordinator().extrude().distance() != 10.0);

  QKeyEvent first_escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&tools.numeric_editor(), &first_escape);
  CHECK(tools.active());
  CHECK(tools.manipulators().active_handle_id().empty());
  CHECK(tools.coordinator().extrude().distance() == 10.0);
  CHECK(tools.numeric_editor().text() == QStringLiteral("10"));

  QKeyEvent second_escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&tools.numeric_editor(), &second_escape);
  CHECK_FALSE(tools.active());
  CHECK_FALSE(window.session().task().active());
}

TEST_CASE("Block 135 Revolve uses a profile-plane origin axis and angle manipulator",
          "[gui][shell-features][gui][extrude-revolve]") {
  ShellWindow window;
  // The rectangle stays on one side of the X origin axis selected for XY.
  create_finished_rectangle(window, {5.0, 5.0}, {20.0, 20.0});
  auto& tools = window.feature_tools();

  CHECK(tools.begin_revolve(DatumAxisId("datum.axis.z")).has_error());
  CHECK_FALSE(tools.active());
  REQUIRE(tools.begin_revolve());
  CHECK(tools.command() == ShellFeatureTools::Command::Revolve);
  CHECK(tools.coordinator().revolve().extent_mode() == RevolveExtentMode::Angle);
  CHECK(tools.coordinator().revolve().angle() == 180.0);
  REQUIRE(tools.manipulators().handles().size() == 1U);
  CHECK(tools.manipulators().handles().front().id == kRevolveAngleHandleId);

  const FeatureId feature = tools.coordinator().revolve().feature_id();
  const BodyId body = tools.coordinator().revolve().result_body_id();
  REQUIRE(tools.set_numeric_value(270.0));
  REQUIRE(tools.apply());

  const auto* part = window.session().part_document();
  REQUIRE(part != nullptr);
  const auto* revolve = part->find_revolve_feature(feature);
  REQUIRE(revolve != nullptr);
  CHECK(revolve->axis().source_node_id() == "datum.axis.x");
  CHECK(revolve->extent().mode() == RevolveExtentMode::Angle);
  REQUIRE(revolve->extent().angle_deg().has_value());
  CHECK(*revolve->extent().angle_deg() == 270.0);
  CHECK(part->find_body(body) != nullptr);
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);

  REQUIRE(window.undo());
  part = window.session().part_document();
  REQUIRE(part != nullptr);
  CHECK(part->find_revolve_feature(feature) == nullptr);
  CHECK(part->find_body(body) == nullptr);
}

TEST_CASE("Extrusion starts through real tree and ribbon mouse clicks",
          "[gui][shell-features][gui][mouse-regression]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_finished_rectangle(window);
  qApp->processEvents();

  auto& tree = window.model_browser();
  auto* sketch_item = find_tree_item(tree, QStringLiteral("sketch.skizze1"));
  REQUIRE(sketch_item != nullptr);
  auto* datum_item = find_tree_item(tree, QStringLiteral("datum.xy"));
  REQUIRE(datum_item != nullptr);
  tree.scrollToItem(datum_item);
  qApp->processEvents();
  const QRect datum_rect = tree.visualItemRect(datum_item);
  REQUIRE_FALSE(datum_rect.isEmpty());
  click_widget(*tree.viewport(), datum_rect.center());
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  CHECK_FALSE(window.feature_tools().extrude_action().isEnabled());

  tree.scrollToItem(sketch_item);
  qApp->processEvents();
  const QRect item_rect = tree.visualItemRect(sketch_item);
  REQUIRE_FALSE(item_rect.isEmpty());
  click_widget(*tree.viewport(), item_rect.center());
  CHECK(tree.currentItem() == sketch_item);

  auto& action = window.feature_tools().extrude_action();
  REQUIRE(action.isEnabled());
  QToolButton* extrude_button = nullptr;
  for (auto* button : window.findChildren<QToolButton*>())
    if (button->defaultAction() == &action) {
      extrude_button = button;
      break;
    }
  REQUIRE(extrude_button != nullptr);
  INFO("button geometry=" << extrude_button->geometry().x() << ','
                          << extrude_button->geometry().y() << ' '
                          << extrude_button->geometry().width() << 'x'
                          << extrude_button->geometry().height());
  INFO("button visible=" << extrude_button->isVisible()
                         << " visible-region-empty=" << extrude_button->visibleRegion().isEmpty());
  REQUIRE(extrude_button->isVisible());
  REQUIRE_FALSE(extrude_button->visibleRegion().isEmpty());
  click_widget(*extrude_button, extrude_button->rect().center());
  REQUIRE(window.feature_tools().active());

  const BodyId body = window.feature_tools().coordinator().extrude().result_body_id();
  auto* apply_button = find_push_button(window, QStringLiteral("Anwenden"));
  REQUIRE(apply_button != nullptr);
  REQUIRE(apply_button->isVisible());
  REQUIRE(apply_button->isEnabled());
  click_widget(*apply_button, apply_button->rect().center());
  CHECK_FALSE(window.feature_tools().active());
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);
}

TEST_CASE("A line-drawn face extrudes through the complete native mouse workflow",
          "[gui][shell-features][gui][mouse-regression][native-mouse-regression]"
          "[line-profile-regression]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();

  // Create the document through the same ribbon button and modal confirmation
  // a user operates. No test-side Sketch or line entity is constructed.
  auto* new_button = find_tool_button(window, QStringLiteral("Neu"));
  REQUIRE(new_button != nullptr);
  REQUIRE(new_button->isVisible());
  REQUIRE(click_modal_ok_while_clicking(*new_button, new_button->rect().center()));
  REQUIRE(window.session().part_document() != nullptr);

  // Select XY in the browser with an actual mouse click, then enter the Sketch
  // with the ribbon button.
  auto& tree = window.model_browser();
  auto* xy_item = find_tree_item(tree, QStringLiteral("datum.xy"));
  REQUIRE(xy_item != nullptr);
  tree.scrollToItem(xy_item);
  qApp->processEvents();
  const QRect xy_rect = tree.visualItemRect(xy_item);
  REQUIRE_FALSE(xy_rect.isEmpty());
  click_widget(*tree.viewport(), xy_rect.center());

  auto* create_sketch = find_tool_button(window, QStringLiteral("2D-Skizze erstellen"));
  REQUIRE(create_sketch != nullptr);
  REQUIRE(create_sketch->isVisible());
  REQUIRE(create_sketch->isEnabled());
  click_widget(*create_sketch, create_sketch->rect().center());
  REQUIRE(window.active_sketch().has_value());
  const SketchId sketch_id = *window.active_sketch();

  auto* line_button = find_tool_button(window, QStringLiteral("Linie"));
  REQUIRE(line_button != nullptr);
  REQUIRE(line_button->isVisible());
  REQUIRE(click_popup_action(*line_button, QStringLiteral("Linie")));
  REQUIRE(window.sketch_tools().tool_active());

  auto& viewport = window.viewport();
  const QPoint center = viewport.rect().center();
  const QPoint first = center + QPoint{-120, 80};
  const QPoint second = center + QPoint{120, 80};
  const QPoint third = center + QPoint{120, -80};
  const QPoint fourth = center + QPoint{-120, -80};
  for (const QPoint point : {first, second, third, fourth}) {
    REQUIRE(viewport.rect().contains(point));
    click_widget(viewport, point);
  }
  // Deliberately miss the first corner by several pixels. It is inside the
  // endpoint snap tolerance but lies exactly on (and is therefore closer to)
  // the first line's interior.
  click_widget(viewport, first + QPoint{4, 0});

  const auto* drawn = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(drawn != nullptr);
  REQUIRE(drawn->line_segments().size() == 4U);
  REQUIRE_FALSE(drawn->line_segments().empty());
  for (const auto& line : drawn->line_segments())
    INFO(line.id().value() << ": (" << line.start().x << ',' << line.start().y << ") -> ("
                           << line.end().x << ',' << line.end().y << ')');
  CHECK(drawn->line_segments().back().end() == drawn->line_segments().front().start());
  CHECK(drawn->profile_count() == 0U);
  Point2 profile_center{};
  for (const auto& line : drawn->line_segments()) {
    profile_center.x += line.start().x;
    profile_center.y += line.start().y;
  }
  profile_center.x /= static_cast<double>(drawn->line_segments().size());
  profile_center.y /= static_cast<double>(drawn->line_segments().size());

  auto* finish = find_tool_button(window, QStringLiteral("Skizze fertig stellen"));
  REQUIRE(finish != nullptr);
  REQUIRE(finish->isVisible());
  REQUIRE(finish->isEnabled());
  click_widget(*finish, finish->rect().center());
  REQUIRE_FALSE(window.active_sketch().has_value());

  const auto* finished = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(finished != nullptr);
  REQUIRE(finished->closed_profiles().size() == 1U);
  const ProfileId profile_id = finished->closed_profiles().front().id();
  REQUIRE(window.feature_tools().selected_profile().has_value());

  // A finished line region must be a selectable 3D profile face, not merely
  // four visible edges. Clear the Finish handoff, then click its interior.
  if (!viewport.native_viewer_available()) {
    SUCCEED("native OCCT viewer unavailable after mouse-authored profile was materialized");
    return;
  }
  REQUIRE(viewport.select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  viewport.set_projection(GuiViewportProjection::Orthographic);
  REQUIRE(viewport.set_plane_camera({profile_center.x, profile_center.y, 0.0}, {0.0, 0.0, 1.0},
                                    {0.0, 1.0, 0.0}));
  qApp->processEvents();
  const auto interior = viewport.model_to_screen({profile_center.x, profile_center.y, 0.0});
  REQUIRE(interior);
  const QPoint interior_click(static_cast<int>(std::lround(interior.value().x)),
                              static_cast<int>(std::lround(interior.value().y)));
  REQUIRE(viewport.rect().contains(interior_click));
  click_widget(viewport, interior_click);
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->semantic_id ==
        "sketch/" + sketch_id.value() + "/profile/" + profile_id.value());
  REQUIRE(window.feature_tools().selected_profile().has_value());

  auto& extrude_action = window.feature_tools().extrude_action();
  REQUIRE(extrude_action.isEnabled());
  QToolButton* extrude_button = nullptr;
  for (auto* button : window.findChildren<QToolButton*>())
    if (button->defaultAction() == &extrude_action) {
      extrude_button = button;
      break;
    }
  REQUIRE(extrude_button != nullptr);
  REQUIRE(extrude_button->isVisible());
  click_widget(*extrude_button, extrude_button->rect().center());
  REQUIRE(window.feature_tools().active());
  REQUIRE(viewport.feature_preview_active());
  const BodyId body = window.feature_tools().coordinator().extrude().result_body_id();

  auto* apply_button = find_push_button(window, QStringLiteral("Anwenden"));
  REQUIRE(apply_button != nullptr);
  REQUIRE(apply_button->isVisible());
  REQUIRE(apply_button->isEnabled());
  click_widget(*apply_button, apply_button->rect().center());
  CHECK_FALSE(window.feature_tools().active());
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);
}

TEST_CASE("A raw closed Sketch extrudes from a real tree click atomically",
          "[gui][shell-features][gui][mouse-regression][raw-profile-regression]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_raw_rectangle(window);
  qApp->processEvents();

  const auto* before = window.session().part_document()->find_sketch(SketchId("sketch.raw"));
  REQUIRE(before != nullptr);
  REQUIRE(before->profile_count() == 0U);
  const std::size_t parameters = window.session().part_document()->parameter_count();
  const std::size_t bodies = window.session().part_document()->body_count();
  const std::size_t features = window.session().part_document()->feature_count();

  auto& tree = window.model_browser();
  auto* sketch_item = find_tree_item(tree, QStringLiteral("sketch.raw"));
  REQUIRE(sketch_item != nullptr);
  tree.scrollToItem(sketch_item);
  qApp->processEvents();
  const QRect item_rect = tree.visualItemRect(sketch_item);
  REQUIRE_FALSE(item_rect.isEmpty());
  click_widget(*tree.viewport(), item_rect.center());
  REQUIRE(window.feature_tools().selected_profile().has_value());
  REQUIRE(window.feature_tools().extrude_action().isEnabled());

  REQUIRE(window.feature_tools().begin_extrude());
  CHECK(window.viewport().feature_preview_active());
  // Selection and preview remain candidate-only.
  CHECK(window.session().part_document()->find_sketch(SketchId("sketch.raw"))->profile_count() ==
        0U);
  CHECK(window.session().part_document()->parameter_count() == parameters);
  CHECK(window.session().part_document()->body_count() == bodies);
  CHECK(window.session().part_document()->feature_count() == features);

  window.feature_tools().cancel();
  CHECK(window.session().part_document()->find_sketch(SketchId("sketch.raw"))->profile_count() ==
        0U);
  REQUIRE(window.feature_tools().begin_extrude());
  const FeatureId feature = window.feature_tools().coordinator().extrude().feature_id();
  const BodyId body = window.feature_tools().coordinator().extrude().result_body_id();
  const ParameterId parameter =
      window.feature_tools().coordinator().extrude().distance_parameter_id();
  REQUIRE(window.feature_tools().apply());

  const auto* part = window.session().part_document();
  REQUIRE(part != nullptr);
  REQUIRE(part->find_sketch(SketchId("sketch.raw"))->profile_count() == 1U);
  CHECK(part->find_feature(feature) != nullptr);
  CHECK(part->find_parameter(parameter) != nullptr);
  CHECK(part->find_body(body) != nullptr);
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);

  REQUIRE(window.undo());
  part = window.session().part_document();
  REQUIRE(part != nullptr);
  CHECK(part->find_sketch(SketchId("sketch.raw"))->profile_count() == 0U);
  CHECK(part->find_feature(feature) == nullptr);
  CHECK(part->find_parameter(parameter) == nullptr);
  CHECK(part->find_body(body) == nullptr);
}

TEST_CASE("A raw curved Sketch extrudes from a browser click with exact preview and one transaction",
          "[gui][shell-features][raw-profile-regression][sketch-curved-regions]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Gekrümmtes Rohprofil")));
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  if (QApplication::platformName() == QStringLiteral("xcb"))
    REQUIRE(window.viewport().native_viewer_available());
  const auto capture = [&](const QString& name) {
    const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (directory.isEmpty()) return;
    QDir().mkpath(directory);
    QEventLoop frame;
    QTimer::singleShot(150, &frame, &QEventLoop::quit);
    frame.exec();
    CHECK(window.grab().save(directory + '/' + name + ".png"));
  };
  auto sketch = Sketch::create(SketchId("raw.curved"), "Halbkreis", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_entity(ArcSegment::create_three_point(SketchEntityId("top"), {-10, 0}, {0, 10}, {10, 0}).value()));
  REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId("base"), {-10, 0}, {10, 0}).value()));
  REQUIRE(window.session().commit_part_transaction("Load curved sketch", [sketch](PartDocument& part) {
    return part.add_sketch(sketch);
  }));
  window.refresh();
  qApp->processEvents();
  auto& tree = window.model_browser();
  auto* item = find_tree_item(tree, QStringLiteral("raw.curved"));
  REQUIRE(item);
  tree.scrollToItem(item);
  qApp->processEvents();
  click_widget(*tree.viewport(), tree.visualItemRect(item).center());
  REQUIRE(window.feature_tools().selected_profile());
  capture(QStringLiteral("raw-curved-profile"));
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(5));
  CHECK(window.viewport().feature_preview_active());
  capture(QStringLiteral("curved-extrude-preview"));
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  window.feature_tools().cancel();
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(5));
  REQUIRE(window.feature_tools().apply());
  const auto* part = window.session().part_document();
  REQUIRE(part->find_sketch(sketch.id())->arc_closed_profiles().size() == 1);
  REQUIRE(window.session().part_shape_cache()->final_shape());
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*window.session().part_shape_cache()->final_shape()).volume_mm3 ==
        Catch::Approx(std::acos(-1.0) * 50 * 5).epsilon(1e-8));
  capture(QStringLiteral("curved-extrude-applied"));
  const auto after = serialize_part_document_to_json(*part).value();
  REQUIRE(window.undo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  REQUIRE(window.redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after);
}

TEST_CASE("Extrusion starts from a native 3D sketch click",
          "[gui][shell-features][gui][native-mouse-regression]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_finished_rectangle(window);
  qApp->processEvents();

  auto& viewport = window.viewport();
  if (!viewport.native_viewer_available()) {
    SUCCEED("native OCCT viewer unavailable on this Qt platform");
    return;
  }

  REQUIRE(viewport.select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  CHECK_FALSE(window.feature_tools().extrude_action().isEnabled());
  CHECK(viewport.selection_filter_mask() == 0xFFFFFFFFU);
  viewport.set_projection(GuiViewportProjection::Orthographic);
  REQUIRE(viewport.set_plane_camera({15.0, 10.0, 0.0}, {0.0, 0.0, 1.0},
                                    {0.0, 1.0, 0.0}));
  qApp->processEvents();

  const auto edge_middle = viewport.model_to_screen({15.0, 5.0, 0.0});
  REQUIRE(edge_middle);
  const QPoint click_point(static_cast<int>(std::lround(edge_middle.value().x)),
                           static_cast<int>(std::lround(edge_middle.value().y)));
  INFO("native sketch click=" << click_point.x() << ',' << click_point.y()
                               << " viewport=" << viewport.width() << 'x'
                               << viewport.height());
  REQUIRE(viewport.rect().contains(click_point));
  click_widget(viewport, click_point);

  INFO("selected semantic: "
       << (viewport.selected_semantic() ? viewport.selected_semantic()->semantic_id : "none"));
  REQUIRE(viewport.selected_semantic().has_value());
  REQUIRE(window.feature_tools().selected_profile().has_value());
  REQUIRE(window.feature_tools().extrude_action().isEnabled());

  window.feature_tools().extrude_action().trigger();
  REQUIRE(window.feature_tools().active());
  CHECK(viewport.feature_preview_active());
  const BodyId body = window.feature_tools().coordinator().extrude().result_body_id();
  REQUIRE(window.feature_tools().apply());
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);
}

TEST_CASE("Extrusion starts from a native 3D profile interior click",
          "[gui][shell-features][gui][native-mouse-regression][profile-interior]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_finished_rectangle(window);
  qApp->processEvents();

  auto& viewport = window.viewport();
  if (!viewport.native_viewer_available()) {
    SUCCEED("native OCCT viewer unavailable on this Qt platform");
    return;
  }

  const auto* sketch =
      window.session().part_document()->find_sketch(SketchId("sketch.skizze1"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->closed_profiles().size() == 1U);
  const std::string expected_semantic =
      "sketch/sketch.skizze1/profile/" + sketch->closed_profiles().front().id().value();

  REQUIRE(viewport.select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  viewport.set_projection(GuiViewportProjection::Orthographic);
  REQUIRE(viewport.set_plane_camera({15.0, 10.0, 0.0}, {0.0, 0.0, 1.0},
                                    {0.0, 1.0, 0.0}));
  qApp->processEvents();

  // Deliberately click the center, well away from every edge. This can only
  // hit the profile face, not one of the legacy edge presentations.
  const auto interior = viewport.model_to_screen({15.0, 10.0, 0.0});
  REQUIRE(interior);
  const QPoint click_point(static_cast<int>(std::lround(interior.value().x)),
                           static_cast<int>(std::lround(interior.value().y)));
  REQUIRE(viewport.rect().contains(click_point));
  click_widget(viewport, click_point);

  INFO("selected semantic: "
       << (viewport.selected_semantic() ? viewport.selected_semantic()->semantic_id : "none"));
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->semantic_id == expected_semantic);
  REQUIRE(window.feature_tools().selected_profile().has_value());
  REQUIRE(window.feature_tools().extrude_action().isEnabled());

  window.feature_tools().extrude_action().trigger();
  REQUIRE(window.feature_tools().active());
  CHECK(viewport.feature_preview_active());
  const BodyId body = window.feature_tools().coordinator().extrude().result_body_id();
  REQUIRE(window.feature_tools().apply());
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);
}

TEST_CASE("A raw closed Sketch extrudes from a native 3D interior click",
          "[gui][shell-features][gui][native-mouse-regression][raw-profile-regression]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  create_raw_rectangle(window);
  qApp->processEvents();

  auto& viewport = window.viewport();
  if (!viewport.native_viewer_available()) {
    SUCCEED("native OCCT viewer unavailable on this Qt platform");
    return;
  }

  REQUIRE(viewport.select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  CHECK(viewport.selection_filter_mask() == 0xFFFFFFFFU);
  viewport.set_projection(GuiViewportProjection::Orthographic);
  REQUIRE(viewport.set_plane_camera({15.0, 10.0, 0.0}, {0.0, 0.0, 1.0},
                                    {0.0, 1.0, 0.0}));
  qApp->processEvents();

  const auto interior = viewport.model_to_screen({15.0, 10.0, 0.0});
  REQUIRE(interior);
  const QPoint click_point(static_cast<int>(std::lround(interior.value().x)),
                           static_cast<int>(std::lround(interior.value().y)));
  REQUIRE(viewport.rect().contains(click_point));
  click_widget(viewport, click_point);
  INFO("selected semantic: "
       << (viewport.selected_semantic() ? viewport.selected_semantic()->semantic_id : "none"));
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->semantic_id ==
        "sketch/sketch.raw/profile/generated.region.sketch.raw.0");
  REQUIRE(window.feature_tools().selected_profile().has_value());
  REQUIRE(window.feature_tools().extrude_action().isEnabled());

  REQUIRE(window.feature_tools().begin_extrude());
  CHECK(viewport.feature_preview_active());
  CHECK(window.session().part_document()->find_sketch(SketchId("sketch.raw"))->profile_count() ==
        0U);
  const BodyId body = window.feature_tools().coordinator().extrude().result_body_id();
  REQUIRE(window.feature_tools().apply());
  CHECK(window.session().part_document()->find_sketch(SketchId("sketch.raw"))->profile_count() ==
        1U);
  REQUIRE(window.session().part_shape_cache() != nullptr);
  CHECK(window.session().part_shape_cache()->find_body_result(body) != nullptr);
}

TEST_CASE("A rectangle containing a circle exposes and extrudes the face with a hole",
          "[gui][shell-features][gui][mouse-regression][native-mouse-regression]"
          "[nested-profile-regression]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();

  // Author the complete example through the same visible controls a user
  // operates. No profile or entity is inserted directly into the document.
  auto* new_button = find_tool_button(window, QStringLiteral("Neu"));
  REQUIRE(new_button != nullptr);
  REQUIRE(new_button->isVisible());
  REQUIRE(click_modal_ok_while_clicking(*new_button, new_button->rect().center()));
  REQUIRE(window.session().part_document() != nullptr);

  auto& tree = window.model_browser();
  auto* xy_item = find_tree_item(tree, QStringLiteral("datum.xy"));
  REQUIRE(xy_item != nullptr);
  tree.scrollToItem(xy_item);
  qApp->processEvents();
  const QRect xy_rect = tree.visualItemRect(xy_item);
  REQUIRE_FALSE(xy_rect.isEmpty());
  click_widget(*tree.viewport(), xy_rect.center());

  auto* create_sketch = find_tool_button(window, QStringLiteral("2D-Skizze erstellen"));
  REQUIRE(create_sketch != nullptr);
  REQUIRE(create_sketch->isVisible());
  REQUIRE(create_sketch->isEnabled());
  click_widget(*create_sketch, create_sketch->rect().center());
  REQUIRE(window.active_sketch().has_value());
  const SketchId sketch_id = *window.active_sketch();

  auto& viewport = window.viewport();
  const QPoint screen_center = viewport.rect().center();
  const QPoint outer_first = screen_center + QPoint{-140, 100};
  const QPoint outer_second = screen_center + QPoint{140, -100};
  const QPoint circle_edge = screen_center + QPoint{55, 0};
  for (const QPoint point : {outer_first, outer_second, screen_center, circle_edge})
    REQUIRE(viewport.rect().contains(point));

  auto* rectangle_button = find_tool_button(window, QStringLiteral("Rechteck"));
  REQUIRE(rectangle_button != nullptr);
  REQUIRE(rectangle_button->isVisible());
  REQUIRE(click_popup_action(*rectangle_button, QStringLiteral("Rechteck (2 Punkte)")));
  REQUIRE(window.sketch_tools().tool_active());
  click_widget(viewport, outer_first);
  click_widget(viewport, outer_second);
  CHECK_FALSE(window.sketch_tools().tool_active());

  auto* circle_button = find_tool_button(window, QStringLiteral("Kreis"));
  REQUIRE(circle_button != nullptr);
  REQUIRE(circle_button->isVisible());
  REQUIRE(click_popup_action(*circle_button, QStringLiteral("Kreis (Mitte/Radius)")));
  REQUIRE(window.sketch_tools().tool_active());
  click_widget(viewport, screen_center);
  click_widget(viewport, circle_edge);
  CHECK_FALSE(window.sketch_tools().tool_active());

  const auto* authored = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(authored != nullptr);
  REQUIRE(authored->closed_profiles().size() == 1U);
  REQUIRE(authored->circle_profiles().size() == 1U);
  REQUIRE(authored->line_segments().size() == 4U);
  const ProfileId outer_profile_id = authored->closed_profiles().front().id();

  double x_min = std::numeric_limits<double>::infinity();
  double y_min = std::numeric_limits<double>::infinity();
  double x_max = -std::numeric_limits<double>::infinity();
  double y_max = -std::numeric_limits<double>::infinity();
  for (const auto& line : authored->line_segments()) {
    for (const Point2 point : {line.start(), line.end()}) {
      x_min = std::min(x_min, point.x);
      y_min = std::min(y_min, point.y);
      x_max = std::max(x_max, point.x);
      y_max = std::max(y_max, point.y);
    }
  }
  const auto& circle = authored->circle_profiles().front();
  const ProfileId circle_profile_id = circle.id();
  const auto* diameter_parameter =
      window.session().part_document()->find_parameter(circle.diameter_parameter());
  REQUIRE(diameter_parameter != nullptr);
  const double circle_radius = diameter_parameter->value().millimeters() * 0.5;
  const Point2 circle_center = circle.center();
  REQUIRE(circle_center.x - circle_radius > x_min);
  REQUIRE(circle_center.x + circle_radius < x_max);
  REQUIRE(circle_center.y - circle_radius > y_min);
  REQUIRE(circle_center.y + circle_radius < y_max);

  auto* finish = find_tool_button(window, QStringLiteral("Skizze fertig stellen"));
  REQUIRE(finish != nullptr);
  REQUIRE(finish->isVisible());
  REQUIRE(finish->isEnabled());
  click_widget(*finish, finish->rect().center());
  REQUIRE_FALSE(window.active_sketch().has_value());

  // Native picking is required here: a programmatic semantic selection would
  // bypass the crucial question of which bounded face the user can click.
  if (!viewport.native_viewer_available()) {
    SUCCEED("native OCCT viewer unavailable after nested profiles were authored");
    return;
  }

  REQUIRE(viewport.select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  viewport.set_projection(GuiViewportProjection::Orthographic);
  REQUIRE(viewport.set_plane_camera({circle_center.x, circle_center.y, 0.0}, {0.0, 0.0, 1.0},
                                    {0.0, 1.0, 0.0}));
  qApp->processEvents();

  // The inner bounded face remains independently selectable. Classifying the
  // circle as a hole of its enclosing profile must not destroy that choice.
  const auto center_screen =
      viewport.model_to_screen({circle_center.x, circle_center.y, 0.0});
  REQUIRE(center_screen);
  const QPoint center_click(static_cast<int>(std::lround(center_screen.value().x)),
                            static_cast<int>(std::lround(center_screen.value().y)));
  REQUIRE(viewport.rect().contains(center_click));
  click_widget(viewport, center_click);
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->semantic_id ==
        "sketch/" + sketch_id.value() + "/profile/" + circle_profile_id.value());
  REQUIRE(window.feature_tools().selected_profile().has_value());
  CHECK(window.feature_tools().selected_profile()->second == circle_profile_id);

  REQUIRE(viewport.select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());

  // Click inside the rectangle but outside the circle. This bounded face is
  // the rectangle-with-hole region requested by the user.
  const double ring_x =
      circle_center.x + circle_radius + (x_max - circle_center.x - circle_radius) * 0.5;
  const Point2 ring_point{ring_x, circle_center.y};
  const auto ring_screen = viewport.model_to_screen({ring_point.x, ring_point.y, 0.0});
  REQUIRE(ring_screen);
  const QPoint ring_click(static_cast<int>(std::lround(ring_screen.value().x)),
                          static_cast<int>(std::lround(ring_screen.value().y)));
  REQUIRE(viewport.rect().contains(ring_click));
  click_widget(viewport, ring_click);

  INFO("selected semantic: "
       << (viewport.selected_semantic() ? viewport.selected_semantic()->semantic_id : "none"));
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->semantic_id ==
        "sketch/" + sketch_id.value() + "/profile/" + outer_profile_id.value());
  REQUIRE(window.feature_tools().selected_profile().has_value());
  CHECK(window.feature_tools().selected_profile()->second == outer_profile_id);
  REQUIRE(window.feature_tools().extrude_action().isEnabled());

  auto& extrude_action = window.feature_tools().extrude_action();
  QToolButton* extrude_button = nullptr;
  for (auto* button : window.findChildren<QToolButton*>())
    if (button->defaultAction() == &extrude_action) {
      extrude_button = button;
      break;
    }
  REQUIRE(extrude_button != nullptr);
  REQUIRE(extrude_button->isVisible());
  click_widget(*extrude_button, extrude_button->rect().center());
  REQUIRE(window.feature_tools().active());
  REQUIRE(viewport.feature_preview_active());
  const double depth = window.feature_tools().coordinator().extrude().distance();
  const BodyId body = window.feature_tools().coordinator().extrude().result_body_id();

  auto* apply_button = find_push_button(window, QStringLiteral("Anwenden"));
  REQUIRE(apply_button != nullptr);
  REQUIRE(apply_button->isVisible());
  REQUIRE(apply_button->isEnabled());
  click_widget(*apply_button, apply_button->rect().center());
  CHECK_FALSE(window.feature_tools().active());

  REQUIRE(window.session().part_shape_cache() != nullptr);
  const auto* body_result = window.session().part_shape_cache()->find_body_result(body);
  REQUIRE(body_result != nullptr);
  const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(body_result->shape);
  REQUIRE_FALSE(summary.is_null);
  CHECK(summary.solid_count == 1U);
  const double outer_area = (x_max - x_min) * (y_max - y_min);
  const double hole_area = std::acos(-1.0) * circle_radius * circle_radius;
  const double expected_volume = (outer_area - hole_area) * depth;
  INFO("actual volume=" << summary.volume_mm3 << " expected=" << expected_volume
                         << " full rectangle=" << outer_area * depth);
  CHECK(std::abs(summary.volume_mm3 - expected_volume) <=
        std::max(1.0e-6, expected_volume * 1.0e-8));
}

TEST_CASE("Nested line polygons expose and extrude the outer face with a polygon hole",
          "[gui][shell-features][gui][mouse-regression][native-mouse-regression]"
          "[nested-line-profile-regression]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();

  // Create the part, enter an XY sketch, and draw both contours exclusively
  // through the same visible controls and native viewport clicks as a user.
  auto* new_button = find_tool_button(window, QStringLiteral("Neu"));
  REQUIRE(new_button != nullptr);
  REQUIRE(new_button->isVisible());
  REQUIRE(click_modal_ok_while_clicking(*new_button, new_button->rect().center()));
  REQUIRE(window.session().part_document() != nullptr);

  auto& tree = window.model_browser();
  auto* xy_item = find_tree_item(tree, QStringLiteral("datum.xy"));
  REQUIRE(xy_item != nullptr);
  tree.scrollToItem(xy_item);
  qApp->processEvents();
  const QRect xy_rect = tree.visualItemRect(xy_item);
  REQUIRE_FALSE(xy_rect.isEmpty());
  click_widget(*tree.viewport(), xy_rect.center());

  auto* create_sketch = find_tool_button(window, QStringLiteral("2D-Skizze erstellen"));
  REQUIRE(create_sketch != nullptr);
  REQUIRE(create_sketch->isVisible());
  REQUIRE(create_sketch->isEnabled());
  click_widget(*create_sketch, create_sketch->rect().center());
  REQUIRE(window.active_sketch().has_value());
  const SketchId sketch_id = *window.active_sketch();

  auto& viewport = window.viewport();
  const QPoint screen_center = viewport.rect().center();
  const std::vector<QPoint> outer_clicks{
      screen_center + QPoint{-150, 70}, screen_center + QPoint{-70, -110},
      screen_center + QPoint{130, -90}, screen_center + QPoint{160, 70},
      screen_center + QPoint{0, 130}};
  const std::vector<QPoint> inner_clicks{screen_center + QPoint{-45, 35},
                                         screen_center + QPoint{0, -50},
                                         screen_center + QPoint{55, 35}};
  for (const QPoint& click : outer_clicks)
    REQUIRE(viewport.rect().contains(click));
  for (const QPoint& click : inner_clicks)
    REQUIRE(viewport.rect().contains(click));

  auto* line_button = find_tool_button(window, QStringLiteral("Linie"));
  REQUIRE(line_button != nullptr);
  REQUIRE(line_button->isVisible());
  REQUIRE(click_popup_action(*line_button, QStringLiteral("Linie")));
  REQUIRE(window.sketch_tools().tool_active());
  for (const QPoint& click : outer_clicks)
    click_widget(viewport, click);
  click_widget(viewport, outer_clicks.front());

  // A right click ends the empty, automatically re-armed chain just like it
  // does in the interactive application. Start a fresh disconnected chain.
  right_click_widget(viewport, outer_clicks.front());
  REQUIRE_FALSE(window.sketch_tools().tool_active());
  REQUIRE(click_popup_action(*line_button, QStringLiteral("Linie")));
  REQUIRE(window.sketch_tools().tool_active());
  for (const QPoint& click : inner_clicks)
    click_widget(viewport, click);
  click_widget(viewport, inner_clicks.front());

  const auto* authored = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(authored != nullptr);
  for (const auto& line : authored->line_segments())
    INFO(line.id().value() << ": (" << line.start().x << ',' << line.start().y << ") -> ("
                           << line.end().x << ',' << line.end().y << ')');
  REQUIRE(authored->line_segments().size() == 8U);
  CHECK(authored->profile_count() == 0U);

  auto* finish = find_tool_button(window, QStringLiteral("Skizze fertig stellen"));
  REQUIRE(finish != nullptr);
  REQUIRE(finish->isVisible());
  REQUIRE(finish->isEnabled());
  click_widget(*finish, finish->rect().center());
  REQUIRE_FALSE(window.active_sketch().has_value());

  const auto* finished = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(finished != nullptr);
  REQUIRE(finished->closed_profiles().size() == 2U);

  const auto polygon_area = [](const std::vector<Point2>& vertices) {
    double twice_area = 0.0;
    for (std::size_t index = 0U; index < vertices.size(); ++index) {
      const Point2 current = vertices[index];
      const Point2 next = vertices[(index + 1U) % vertices.size()];
      twice_area += current.x * next.y - next.x * current.y;
    }
    return std::abs(twice_area) * 0.5;
  };
  struct ProfileRegion {
    ProfileId id;
    std::vector<Point2> vertices;
    double area = 0.0;
  };
  std::vector<ProfileRegion> regions;
  for (const ClosedProfile& profile : finished->closed_profiles()) {
    auto vertices = finished->closed_profile_vertices(profile);
    REQUIRE(vertices);
    regions.push_back({profile.id(), std::move(vertices.value()), 0.0});
    regions.back().area = polygon_area(regions.back().vertices);
  }
  std::sort(regions.begin(), regions.end(),
            [](const auto& left, const auto& right) { return left.area > right.area; });
  REQUIRE(regions[0].area > regions[1].area);
  const ProfileRegion& outer = regions[0];
  const ProfileRegion& inner = regions[1];

  Point2 inner_center{};
  for (const Point2 vertex : inner.vertices) {
    inner_center.x += vertex.x;
    inner_center.y += vertex.y;
  }
  inner_center.x /= static_cast<double>(inner.vertices.size());
  inner_center.y /= static_cast<double>(inner.vertices.size());

  // Native picking is the behavior under test: semantic selection alone
  // would not prove that the coplanar outer presentation really has a hole.
  if (!viewport.native_viewer_available()) {
    SUCCEED("native OCCT viewer unavailable after nested line profiles were authored");
    return;
  }

  REQUIRE(viewport.select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());
  viewport.set_projection(GuiViewportProjection::Orthographic);
  REQUIRE(viewport.set_plane_camera({inner_center.x, inner_center.y, 0.0}, {0.0, 0.0, 1.0},
                                    {0.0, 1.0, 0.0}));
  qApp->processEvents();

  const auto inner_screen = viewport.model_to_screen({inner_center.x, inner_center.y, 0.0});
  REQUIRE(inner_screen);
  const QPoint inner_click(static_cast<int>(std::lround(inner_screen.value().x)),
                           static_cast<int>(std::lround(inner_screen.value().y)));
  REQUIRE(viewport.rect().contains(inner_click));
  click_widget(viewport, inner_click);
  INFO("inner selected semantic: "
       << (viewport.selected_semantic() ? viewport.selected_semantic()->semantic_id : "none"));
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->semantic_id ==
        "sketch/" + sketch_id.value() + "/profile/" + inner.id.value());
  REQUIRE(window.feature_tools().selected_profile().has_value());
  CHECK(window.feature_tools().selected_profile()->second == inner.id);

  REQUIRE(viewport.select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.feature_tools().selected_profile().has_value());

  const auto farthest = std::max_element(
      outer.vertices.begin(), outer.vertices.end(), [&inner_center](Point2 left, Point2 right) {
        const auto squared_distance = [&inner_center](Point2 point) {
          const double dx = point.x - inner_center.x;
          const double dy = point.y - inner_center.y;
          return dx * dx + dy * dy;
        };
        return squared_distance(left) < squared_distance(right);
      });
  REQUIRE(farthest != outer.vertices.end());
  const Point2 ring_point{inner_center.x + (farthest->x - inner_center.x) * 0.7,
                          inner_center.y + (farthest->y - inner_center.y) * 0.7};
  const auto ring_screen = viewport.model_to_screen({ring_point.x, ring_point.y, 0.0});
  REQUIRE(ring_screen);
  const QPoint ring_click(static_cast<int>(std::lround(ring_screen.value().x)),
                          static_cast<int>(std::lround(ring_screen.value().y)));
  REQUIRE(viewport.rect().contains(ring_click));
  click_widget(viewport, ring_click);
  INFO("outer selected semantic: "
       << (viewport.selected_semantic() ? viewport.selected_semantic()->semantic_id : "none"));
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->semantic_id ==
        "sketch/" + sketch_id.value() + "/profile/" + outer.id.value());
  REQUIRE(window.feature_tools().selected_profile().has_value());
  CHECK(window.feature_tools().selected_profile()->second == outer.id);

  auto& extrude_action = window.feature_tools().extrude_action();
  REQUIRE(extrude_action.isEnabled());
  QToolButton* extrude_button = nullptr;
  for (auto* button : window.findChildren<QToolButton*>())
    if (button->defaultAction() == &extrude_action) {
      extrude_button = button;
      break;
    }
  REQUIRE(extrude_button != nullptr);
  REQUIRE(extrude_button->isVisible());
  click_widget(*extrude_button, extrude_button->rect().center());
  REQUIRE(window.feature_tools().active());
  REQUIRE(viewport.feature_preview_active());
  const double depth = window.feature_tools().coordinator().extrude().distance();
  const BodyId body = window.feature_tools().coordinator().extrude().result_body_id();

  auto* apply_button = find_push_button(window, QStringLiteral("Anwenden"));
  REQUIRE(apply_button != nullptr);
  REQUIRE(apply_button->isVisible());
  REQUIRE(apply_button->isEnabled());
  click_widget(*apply_button, apply_button->rect().center());
  CHECK_FALSE(window.feature_tools().active());

  REQUIRE(window.session().part_shape_cache() != nullptr);
  const auto* body_result = window.session().part_shape_cache()->find_body_result(body);
  REQUIRE(body_result != nullptr);
  const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(body_result->shape);
  REQUIRE_FALSE(summary.is_null);
  CHECK(summary.solid_count == 1U);
  const double expected_volume = (outer.area - inner.area) * depth;
  INFO("actual volume=" << summary.volume_mm3 << " expected=" << expected_volume
                        << " full outer=" << outer.area * depth);
  CHECK(std::abs(summary.volume_mm3 - expected_volume) <=
        std::max(1.0e-6, expected_volume * 1.0e-8));
}
