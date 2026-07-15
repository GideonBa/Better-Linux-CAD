#include "blcad/gui/main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;

namespace {

QTreeWidgetItem* find_item(QTreeWidgetItem* item, QStringView id) {
  if (item->data(0, Qt::UserRole).toString() == id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_item(item->child(index), id))
      return found;
  return nullptr;
}

} // namespace

TEST_CASE("Qt application shell exposes the frozen CAD regions", "[gui][application-shell]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  window.show();
  qApp->processEvents();

  REQUIRE(window.findChild<QToolBar*>(QStringLiteral("blcad.command_bar")) != nullptr);
  const auto* tabs = window.findChild<QTabBar*>(QStringLiteral("blcad.workspace_tabs"));
  REQUIRE(tabs != nullptr);
  CHECK(tabs->count() == 4);
  CHECK(tabs->tabText(0) == QStringLiteral("Part"));
  CHECK(tabs->tabText(1) == QStringLiteral("Assembly"));
  CHECK(tabs->tabText(2) == QStringLiteral("Inspect"));
  CHECK(tabs->tabText(3) == QStringLiteral("Exchange"));

  CHECK(window.findChild<QWidget*>(QStringLiteral("blcad.model_browser")) != nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("blcad.task_panel")) != nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("blcad.occt_viewport")) != nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("blcad.diagnostics")) != nullptr);
  CHECK(window.findChild<QStatusBar*>(QStringLiteral("blcad.status_bar")) != nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("blcad.sketch.command_groups")) != nullptr);
  CHECK(window.findChild<QLineEdit*>(QStringLiteral("blcad.sketch.numeric_hud")) != nullptr);
  CHECK(window.findChild<QLabel*>(QStringLiteral("blcad.sketch.cursor_status")) != nullptr);
  CHECK(window.findChild<QLabel*>(QStringLiteral("blcad.sketch.snap_status")) != nullptr);
  CHECK(window.findChild<QLabel*>(QStringLiteral("blcad.sketch.dof_status")) != nullptr);
  CHECK(window.findChild<QLabel*>(QStringLiteral("blcad.sketch.solve_status")) != nullptr);
  CHECK(window.findChildren<QDockWidget*>().size() == 3);

  const auto* new_part = window.findChild<QAction*>(QStringLiteral("blcad.action.new_part"));
  const auto* open = window.findChild<QAction*>(QStringLiteral("blcad.action.open"));
  const auto* save = window.findChild<QAction*>(QStringLiteral("blcad.action.save"));
  const auto* save_as = window.findChild<QAction*>(QStringLiteral("blcad.action.save_as"));
  const auto* recompute = window.findChild<QAction*>(QStringLiteral("blcad.action.recompute"));
  const auto* wireframe =
      window.findChild<QAction*>(QStringLiteral("blcad.action.display_wireframe"));
  const auto* fit_all = window.findChild<QAction*>(QStringLiteral("blcad.action.fit_all"));
  const auto* create_sketch =
      window.findChild<QAction*>(QStringLiteral("blcad.action.create_sketch"));
  const auto* finish_sketch =
      window.findChild<QAction*>(QStringLiteral("blcad.action.finish_sketch"));
  const auto* sketch_line =
      window.findChild<QAction*>(QStringLiteral("blcad.action.sketch_line"));
  const auto* sketch_repeat =
      window.findChild<QAction*>(QStringLiteral("blcad.action.sketch_repeat"));
  const auto* repair_sketch =
      window.findChild<QAction*>(QStringLiteral("blcad.action.repair_sketch"));
  REQUIRE(new_part != nullptr);
  REQUIRE(open != nullptr);
  REQUIRE(save != nullptr);
  REQUIRE(save_as != nullptr);
  REQUIRE(recompute != nullptr);
  REQUIRE(wireframe != nullptr);
  REQUIRE(fit_all != nullptr);
  REQUIRE(create_sketch != nullptr);
  REQUIRE(finish_sketch != nullptr);
  REQUIRE(sketch_line != nullptr);
  REQUIRE(sketch_repeat != nullptr);
  REQUIRE(repair_sketch != nullptr);
  CHECK(new_part->isEnabled());
  CHECK(open->isEnabled());
  CHECK_FALSE(save->isEnabled());
  CHECK_FALSE(save_as->isEnabled());
  CHECK_FALSE(recompute->isEnabled());
  CHECK_FALSE(create_sketch->isEnabled());
  CHECK_FALSE(finish_sketch->isEnabled());
  CHECK_FALSE(sketch_line->isEnabled());
  CHECK_FALSE(sketch_repeat->isEnabled());

  REQUIRE(window.session().create_part(DocumentId("part.shell"), "Shell Part"));
  window.refresh_command_state();
  CHECK(save->isEnabled());
  CHECK(save_as->isEnabled());
  CHECK(recompute->isEnabled());
  CHECK(create_sketch->isEnabled());
  CHECK(window.windowTitle().contains(QStringLiteral("Shell Part *")));
}

TEST_CASE("Qt shell refuses workspace replacement while a task is active",
          "[gui][application-shell][command-state]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;

  REQUIRE(window.request_workspace(GuiWorkspace::Assembly));
  CHECK(window.session().workspace() == GuiWorkspace::Assembly);

  REQUIRE(window.session().task().begin("assembly.mate"));
  window.refresh_command_state();
  CHECK_FALSE(window.request_workspace(GuiWorkspace::Inspect));
  CHECK(window.session().workspace() == GuiWorkspace::Assembly);
  CHECK(window.statusBar()->currentMessage() == QStringLiteral("Task active — Apply or Cancel"));

  REQUIRE(window.session().task().cancel());
  REQUIRE(window.request_workspace(GuiWorkspace::Inspect));
  CHECK(window.session().workspace() == GuiWorkspace::Inspect);
}

TEST_CASE("Block 106 shell enters edits repeats and finishes a contextual Sketch",
          "[gui][application-shell][gui][sketch-workspace][gui][sketch-command-lifecycle]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  window.show();
  qApp->processEvents();

  REQUIRE(window.session().create_part(DocumentId("part.block106"), "Block 106"));
  REQUIRE(window.sketch_workbench().create_xy_datum(window.session(), DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(window.sketch_workbench().create_sketch(
      window.session(),
      Sketch::create(SketchId("sketch.block106"), "Block 106 Sketch", DatumPlaneId("datum.xy")).value()));
  window.refresh_command_state();
  qApp->processEvents();

  auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("blcad.model_browser"));
  auto* enter = window.findChild<QAction*>(QStringLiteral("blcad.action.edit_sketch"));
  auto* finish = window.findChild<QAction*>(QStringLiteral("blcad.action.finish_sketch"));
  auto* line = window.findChild<QAction*>(QStringLiteral("blcad.action.sketch_line"));
  auto* repeat = window.findChild<QAction*>(QStringLiteral("blcad.action.sketch_repeat"));
  auto* hud = window.findChild<QLineEdit*>(QStringLiteral("blcad.sketch.numeric_hud"));
  auto* groups = window.findChild<QWidget*>(QStringLiteral("blcad.sketch.command_groups"));
  auto* viewport = window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"));
  REQUIRE(tree != nullptr);
  REQUIRE(enter != nullptr);
  REQUIRE(finish != nullptr);
  REQUIRE(line != nullptr);
  REQUIRE(repeat != nullptr);
  REQUIRE(hud != nullptr);
  REQUIRE(groups != nullptr);
  REQUIRE(viewport != nullptr);

  QTreeWidgetItem* sketch_item = nullptr;
  for (int index = 0; index < tree->topLevelItemCount() && !sketch_item; ++index)
    sketch_item = find_item(tree->topLevelItem(index), u"sketch.block106");
  REQUIRE(sketch_item != nullptr);
  tree->setCurrentItem(sketch_item);
  enter->trigger();
  qApp->processEvents();

  CHECK(window.session().workspace() == GuiWorkspace::Sketch);
  CHECK(window.sketch_workspace().active());
  REQUIRE(window.active_sketch().has_value());
  CHECK(window.active_sketch()->value() == "sketch.block106");
  CHECK(groups->isVisible());
  CHECK(viewport->projection() == GuiViewportProjection::Orthographic);
  CHECK(viewport->selection_filter_mask() == GuiSketchWorkspace::selection_filter_mask());
  CHECK(viewport->sketch_focus_active());
  CHECK(viewport->sketch_focus_id() == "sketch.block106");
  CHECK(viewport->sketch_surroundings_mode() == GuiSketchSurroundingsMode::Dim);
  CHECK(viewport->cursor().shape() == Qt::CrossCursor);
  CHECK(finish->isEnabled());
  CHECK(line->isEnabled());

  line->trigger();
  qApp->processEvents();
  CHECK(window.sketch_workspace().stage() == GuiSketchInteractionStage::NumericInput);
  CHECK(window.sketch_workspace().status().focus == GuiSketchFocusTarget::NumericHud);
  CHECK(hud->isVisible());
  CHECK(hud->hasFocus());
  hud->setText(QStringLiteral("line.block106, 0, 0, 25, 0"));
  REQUIRE(QMetaObject::invokeMethod(hud, "returnPressed", Qt::DirectConnection));
  qApp->processEvents();

  CHECK(window.sketch_workspace().stage() == GuiSketchInteractionStage::Idle);
  CHECK_FALSE(window.session().task().active());
  const auto* sketch = window.session().part_document()->find_sketch(SketchId("sketch.block106"));
  REQUIRE(sketch != nullptr);
  CHECK(sketch->line_segments().size() == 1);
  CHECK(repeat->isEnabled());

  repeat->trigger();
  qApp->processEvents();
  CHECK(window.sketch_workspace().stage() == GuiSketchInteractionStage::NumericInput);
  REQUIRE(window.sketch_workspace().escape(window.session()));
  REQUIRE(window.sketch_workspace().escape(window.session()));
  window.refresh_command_state();
  CHECK(window.sketch_workspace().stage() == GuiSketchInteractionStage::Idle);
  CHECK_FALSE(window.session().task().active());

  finish->trigger();
  qApp->processEvents();
  CHECK(window.session().workspace() == GuiWorkspace::Part);
  CHECK_FALSE(window.sketch_workspace().active());
  CHECK_FALSE(window.active_sketch().has_value());
  CHECK_FALSE(groups->isVisible());
  CHECK_FALSE(viewport->sketch_focus_active());
  CHECK(viewport->selection_filter_mask() == 0xFFFFFFFFU);
  CHECK(viewport->projection() == GuiViewportProjection::Perspective);
}
