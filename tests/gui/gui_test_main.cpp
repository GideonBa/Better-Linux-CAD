#include <QApplication>
#include <QByteArray>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
  }

  QApplication application(argc, argv);
  QApplication::setApplicationName(QStringLiteral("BLCAD GUI Tests"));
  return Catch::Session().run(argc, argv);
}
