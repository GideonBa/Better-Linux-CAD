#include <QApplication>
#include <QByteArray>

#include <catch2/catch_session.hpp>

// Block-107 focused tests are kept as a separate source module and included once in the existing
// GUI test translation unit so the optional GUI target matrix remains unchanged.
#include "gui_sketch_interaction_tests.cpp"

int main(int argc, char* argv[]) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
  }

  QApplication application(argc, argv);
  QApplication::setApplicationName(QStringLiteral("BLCAD GUI Tests"));
  return Catch::Session().run(argc, argv);
}
