#include "blcad/gui/gui_appearance.hpp"
#include <QApplication>
#include <QByteArray>
#include <QTemporaryDir>

#include <catch2/catch_session.hpp>

#include "gui_sketch_constraint_family_tests.inc"
#include "gui_sketch_constraint_redundancy_tests.inc"
#include "gui_sketch_constraint_tests.inc"
#include "gui_sketch_dimension_selection_tests.inc"
#include "gui_sketch_modify_tests.inc"

int main(int argc, char* argv[]) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
  }

  QApplication application(argc, argv);
  QTemporaryDir presets;
  if (!presets.isValid())
    return 1;
  qputenv("BLCAD_PRESETS_PATH", presets.filePath("feature-presets.json").toUtf8());
  blcad::gui::configure_gui_appearance(application);
  QApplication::setApplicationName(QStringLiteral("BLCAD GUI Tests"));
  return Catch::Session().run(argc, argv);
}
