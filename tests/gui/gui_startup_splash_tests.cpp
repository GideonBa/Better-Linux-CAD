#include "blcad/gui/startup_splash.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QImage>
#include <QPainter>

using namespace blcad::gui;

TEST_CASE("GUI startup splash embeds artwork and bounds progress", "[gui][startup-splash]") {
  StartupSplash splash;
  CHECK(splash.objectName() == QStringLiteral("blcad.startup_splash"));
  CHECK(splash.sizeHint() == QSize(780, 560));
  CHECK(splash.has_artwork());

  splash.set_progress(48);
  CHECK(splash.progress() == 48);
  splash.set_progress(150);
  CHECK(splash.progress() == 100);
  splash.set_progress(-10);
  CHECK(splash.progress() == 0);

  splash.set_progress(48);
  QImage rendered(splash.size(), QImage::Format_ARGB32_Premultiplied);
  rendered.fill(Qt::transparent);
  QPainter painter(&rendered);
  splash.render(&painter);
  CHECK(rendered.pixelColor(10, 410) == QColor(Qt::black));

  const QString screenshot_path = qEnvironmentVariable("BLCAD_SPLASH_SCREENSHOT");
  if (!screenshot_path.isEmpty())
    REQUIRE(rendered.save(screenshot_path));
}
