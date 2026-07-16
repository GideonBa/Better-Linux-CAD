#include <QApplication>
#include <QByteArray>

#include <catch2/catch_session.hpp>

#include "gui_sketch_constraint_tests.inc"
#include "gui_sketch_constraint_redundancy_tests.inc"
#include "gui_sketch_constraint_family_tests.inc"
#include "gui_sketch_dimension_tests.inc"

int main(int argc, char* argv[]) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
  }

  QApplication application(argc, argv);
  QApplication::setApplicationName(QStringLiteral("BLCAD GUI Tests"));
  return Catch::Session().run(argc, argv);
}
