#include "blcad/gui/gui_sketch_constraint_binder.hpp"
#include "blcad/gui/gui_sketch_dimension_binder.hpp"
#include "blcad/gui/gui_sketch_interaction_binder.hpp"
#include "blcad/gui/main_window.hpp"
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

  blcad::gui::MainWindow window;
  blcad::gui::install_sketch_interaction_binder(window);
  blcad::gui::install_sketch_constraint_binder(window);
  blcad::gui::install_sketch_dimension_binder(window);
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
