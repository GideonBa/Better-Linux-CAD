#include "blcad/gui/main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

using namespace blcad::gui;

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

  REQUIRE(window.session().create_part(blcad::DocumentId("part.shell"), "Shell Part"));
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
