#include "blcad/gui/shell/shell_window.hpp"
#include "blcad/gui/startup_splash.hpp"

#include <QApplication>

#include <algorithm>

int main(int argc, char* argv[]) {
#if defined(Q_OS_LINUX)
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("DISPLAY"))
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif
  QApplication application(argc, argv);
  QApplication::setApplicationName(QStringLiteral("BLCAD"));
  QApplication::setOrganizationName(QStringLiteral("BLCAD"));

  blcad::gui::StartupSplash splash;
  splash.show();
  application.processEvents();
  splash.set_progress(20);

  blcad::gui::ShellWindow window;
  splash.set_progress(55);
  application.processEvents();
  bool duration_valid = false;
  const int configured_duration =
      qEnvironmentVariableIntValue("BLCAD_SPLASH_DURATION_MS", &duration_valid);
  const int splash_duration = duration_valid ? std::clamp(configured_duration, 250, 60000) : 700;
  splash.start(splash_duration, [&window, &splash] {
    window.show();
    splash.close();
  });
  return application.exec();
}
