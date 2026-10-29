#include "blcad/gui/gui_icons.hpp"

#include "blcad/gui/gui_appearance.hpp"
#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QCryptographicHash>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenu>
#include <QToolButton>

using namespace blcad::gui;

namespace {

QByteArray resource(const QString& path) {
  QFile file(QStringLiteral(":/blcad/gui/pixelarticons/") + path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}

void check_pixels(const QImage& source, QColor foreground) {
  const auto image = source.convertToFormat(QImage::Format_ARGB32);
  bool has_glyph = false;
  bool has_transparency = false;
  bool exact_colors = true;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const auto pixel = image.pixelColor(x, y);
      has_transparency = has_transparency || pixel.alpha() == 0;
      has_glyph = has_glyph || pixel.alpha() == 255;
      exact_colors = exact_colors && (pixel.alpha() == 0 || pixel == foreground);
    }
  }
  CHECK(has_glyph);
  CHECK(has_transparency);
  CHECK(exact_colors); // No smoothing, fractional alpha or Fusion gray recoloring.
}

} // namespace

TEST_CASE("Bundled Pixelarticons retain their license and verified nonempty pixel masks",
          "[gui][icons]") {
  const auto catalog = QJsonDocument::fromJson(resource("catalog.json")).object();
  const auto manifest = QJsonDocument::fromJson(resource("source-manifest.json")).object();
  REQUIRE(catalog.value("icons").toArray().size() > 0);
  CHECK(catalog.value("revision") == manifest.value("revision"));
  CHECK(catalog.value("license").toString() == "MIT");
  CHECK(QCryptographicHash::hash(resource("LICENSE"), QCryptographicHash::Sha256).toHex() ==
        manifest.value("license_sha256").toString().toLatin1());
  const auto files = manifest.value("files").toObject();
  for (const auto& entry : catalog.value("icons").toArray()) {
    const QString name = entry.toString();
    INFO(name.toStdString());
    const auto icon = pixelart_icon(name);
    REQUIRE_FALSE(icon.isNull());
    CHECK(icon.name() == name);
    CHECK(QCryptographicHash::hash(resource("masks/" + name + ".xpm"), QCryptographicHash::Sha256)
              .toHex() == files.value(name).toObject().value("mask_sha256").toString().toLatin1());
    const auto image = icon.pixmap(QSize(24, 24), 1.0).toImage();
    REQUIRE(image.size() == QSize(24, 24));
    check_pixels(image, QColor::fromRgb(appearance::text));
  }
  CHECK(pixelart_icon(u"missing-icon").isNull());
}

TEST_CASE("Pixelarticons keep visible state colors and sharp masks at fractional display scales",
          "[gui][icons]") {
  const auto icon = pixelart_icon(u"save");
  REQUIRE_FALSE(icon.isNull());
  for (const qreal ratio : {1.0, 1.5, 2.0}) {
    for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected, QIcon::Disabled}) {
      for (const auto state : {QIcon::Off, QIcon::On}) {
        const auto pixmap = icon.pixmap(QSize(24, 24), ratio, mode, state);
        CHECK(pixmap.size() == QSize(24, 24) * ratio);
        CHECK(pixmap.devicePixelRatio() == ratio);
        const auto color = mode == QIcon::Disabled                       ? appearance::muted
                           : mode == QIcon::Active || state == QIcon::On ? appearance::accent
                                                                         : appearance::text;
        check_pixels(pixmap.toImage(), QColor::fromRgb(color));
      }
    }
  }
}

TEST_CASE("Curated ribbon icons retain command labels callbacks and explicit text fallbacks",
          "[gui][icons][ribbon]") {
  ShellRibbon ribbon;
  const int tab = ribbon.add_tab("3D Model");
  auto* group = ribbon.add_group(tab, "Commands");
  QAction save("Save As", &ribbon);
  save.setObjectName("blcad.action.save_as");
  save.setToolTip("Save to another path");
  save.setShortcut(QKeySequence::SaveAs);
  auto* button = ribbon.add_button(group, &save);
  CHECK(button->icon().name() == "save");
  CHECK(button->text() == "Save As");
  CHECK(button->toolTip() == "Save to another path");
  CHECK(save.shortcut() == QKeySequence::SaveAs);
  bool triggered = false;
  QObject::connect(&save, &QAction::triggered, [&] { triggered = true; });
  button->click();
  CHECK(triggered);

  QAction repeat(&ribbon); // This command receives its changing label after ribbon creation.
  repeat.setObjectName("blcad.action.repeat_last_command");
  auto* repeat_button = ribbon.add_button(group, &repeat);
  CHECK(repeat_button->icon().name() == "repeat");
  repeat.setText("Repeat: Loft");
  CHECK(repeat_button->icon().name() == "repeat");

  QAction move("Move", &ribbon);
  auto* compact = ribbon.add_small_button(group, &move);
  CHECK(compact->icon().name() == "move");
  CHECK(compact->toolButtonStyle() == Qt::ToolButtonTextBesideIcon);
  CHECK(compact->text() == "Move");
  QAction loft("Loft", &ribbon);
  auto* unsupported = ribbon.add_small_button(group, &loft);
  CHECK(unsupported->icon().isNull());
  CHECK(unsupported->toolButtonStyle() == Qt::ToolButtonTextOnly);
  CHECK(unsupported->text() == "Loft");
  for (const auto* label : {"Extrude", "Revolve", "Fillet", "Chamfer", "Sweep", "Shell"})
    CHECK(gui_icon_for_label(QString::fromLatin1(label)).isNull());

  QAction circle("Circle (3 Points)", &ribbon);
  auto* menu = ribbon.add_menu_button(group, "Circle", {&circle});
  CHECK(menu->icon().name() == "circle");
  CHECK(circle.icon().name() == "circle");
  CHECK(menu->menu()->actions().front() == &circle);
}

TEST_CASE("Browser icons use typed metadata rather than user-authored names",
          "[gui][icons][model-browser]") {
  GuiBrowserNode parameter{GuiBrowserNodeKind::Parameter, "p.custom", "part.custom", "Copy"};
  CHECK(gui_icon_for_browser_node(parameter).name() == "calculator");
  GuiBrowserNode feature{GuiBrowserNodeKind::Feature, "f.custom", "part.custom", "Save"};
  CHECK(gui_icon_for_browser_node(feature).isNull());
  GuiBrowserNode record{GuiBrowserNodeKind::Sketch, "opaque/id", "part.custom", "arbitrary"};
  record.properties.push_back(
      {"family", "Record family", "circle-profile", GuiPropertyKind::Enumeration, false, {}, {}});
  CHECK(gui_icon_for_browser_node(record).name() == "circle");
  record.properties.front().value = "line";
  CHECK(gui_icon_for_browser_node(record).isNull());
  record.properties.front().value = "arc-profile";
  CHECK(gui_icon_for_browser_node(record).isNull());
  GuiBrowserNode diagnostic{GuiBrowserNodeKind::Analysis, "issue", "part.custom", "issue"};
  diagnostic.properties.push_back(
      {"severity", "Severity", "error", GuiPropertyKind::Status, false, {}, {}});
  CHECK(gui_icon_for_browser_node(diagnostic).name() == "square-alert");
  GuiBrowserNode datum{GuiBrowserNodeKind::Datum, "opaque/datum", "part.custom", "Custom Name"};
  datum.viewport_semantic_id = "datum-plane/opaque/datum";
  CHECK(gui_icon_for_browser_node(datum).name() == "section");
  datum.viewport_semantic_id = "datum-axis/opaque/datum";
  CHECK(gui_icon_for_browser_node(datum).isNull());
}
