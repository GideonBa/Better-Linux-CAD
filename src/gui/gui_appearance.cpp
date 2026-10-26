#include "blcad/gui/gui_appearance.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QLocale>
#include <QPalette>
#include <QResource>
#include <QStyleFactory>

#include <array>
#include <utility>

// Keep the resource initializer at global scope for static-library linkage.
static void initialize_gui_appearance_assets() {
  Q_INIT_RESOURCE(blcad_gui_assets);
}

namespace blcad::gui {
namespace {

struct DesktopFonts {
  QString body;
  QString display;
  QString numeric;
};

QString register_font(const QString& filename, const QString& fallback) {
  const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/blcad/gui/fonts/") + filename);
  const auto families = QFontDatabase::applicationFontFamilies(id);
  return families.isEmpty() ? fallback : families.front();
}

const DesktopFonts& desktop_fonts() {
  // Registration is local and idempotent; no machine-installed font is required.
  static const DesktopFonts fonts = [] {
    const QString body =
        register_font(QStringLiteral("Barlow-Regular.ttf"), QStringLiteral("Sans Serif"));
    (void)register_font(QStringLiteral("Barlow-Medium.ttf"), body);
    return DesktopFonts{
        body, register_font(QStringLiteral("BarlowCondensed-Medium.ttf"), body),
        register_font(QStringLiteral("IBMPlexMono-Regular.ttf"), QStringLiteral("Monospace"))};
  }();
  return fonts;
}

QString color_name(QRgb color) {
  return QColor::fromRgb(color).name();
}

} // namespace

QFont appearance::heading_font(int pixel_size) {
  QFont font(desktop_fonts().display);
  font.setPixelSize(pixel_size);
  font.setWeight(QFont::Medium);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
  return font;
}

QFont appearance::numeric_font(int pixel_size) {
  QFont font(desktop_fonts().numeric);
  font.setPixelSize(pixel_size);
  font.setStyleHint(QFont::Monospace);
  font.setFixedPitch(true);
  return font;
}

void configure_gui_appearance(QApplication& application) {
  initialize_gui_appearance_assets();
  application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
  application.setAttribute(Qt::AA_DontUseNativeDialogs);
  QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

  QFont body(desktop_fonts().body);
  body.setPixelSize(14);
  application.setFont(body);

  QPalette palette;
  for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    const QColor text =
        QColor::fromRgb(group == QPalette::Disabled ? appearance::muted : appearance::ink);
    palette.setColor(group, QPalette::Window, QColor::fromRgb(appearance::paper));
    palette.setColor(group, QPalette::Button, QColor::fromRgb(appearance::paper));
    palette.setColor(group, QPalette::Base, Qt::white);
    palette.setColor(group, QPalette::AlternateBase, QColor::fromRgb(appearance::paper));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
      palette.setColor(group, role, text);
    palette.setColor(group, QPalette::BrightText, Qt::white);
    palette.setColor(group, QPalette::Highlight, QColor::fromRgb(appearance::blue));
    palette.setColor(group, QPalette::HighlightedText, Qt::white);
    palette.setColor(group, QPalette::PlaceholderText, QColor::fromRgb(appearance::muted));
    palette.setColor(group, QPalette::Link, QColor::fromRgb(appearance::blue));
    palette.setColor(group, QPalette::LinkVisited, QColor::fromRgb(appearance::blue));
    palette.setColor(group, QPalette::Light, Qt::white);
    for (const auto role : {QPalette::Midlight, QPalette::Mid, QPalette::Dark, QPalette::Shadow})
      palette.setColor(group, role, QColor::fromRgb(appearance::line));
    palette.setColor(group, QPalette::ToolTipBase, QColor::fromRgb(appearance::ink));
    palette.setColor(group, QPalette::ToolTipText, Qt::white);
  }
  application.setPalette(palette);

  // Foreground defaults stay in the palette: Qt 6.4 otherwise derives a
  // half-opacity placeholder from a QSS color rule. Display roles opt in below.
  QString stylesheet = QStringLiteral(R"qss(
* {
  font-family: "@body@";
  font-size: 14px;
  border-radius: 0px;
  selection-color: #ffffff;
  selection-background-color: @blue@;
}
QWidget { background-color: @paper@; }
QLabel { background-color: transparent; }
QMainWindow::separator { width: 4px; height: 4px; background: @paper@; }
QMainWindow::separator:hover { background: @blue@; }
QDockWidget { font-family: "@display@"; font-size: 20px; }
QDockWidget::title {
  color: @blue@; background: @paper@;
  border: none; border-top: 1px solid @line@;
  padding: 8px 12px;
}
QDockWidget::close-button, QDockWidget::float-button {
  border: none; background: transparent; padding: 3px;
}
QDockWidget::close-button:hover, QDockWidget::float-button:hover { background: @wash@; }
QFrame, QScrollArea { border: none; }
QGroupBox {
  border: none; border-top: 1px solid @line@;
  margin-top: 18px; padding-top: 12px;
}
QGroupBox::title { color: @blue@; subcontrol-origin: margin; left: 0px; padding: 0px 4px 0px 0px; }
QPushButton {
  background: @paper@; border: 1px solid @edge@;
  min-height: 22px; padding: 5px 12px;
}
QPushButton:hover { background: @wash@; border-color: @blue@; }
QPushButton:pressed, QPushButton:checked, QPushButton[guiRole="primary"] {
  background: @blue@; color: @paper@; border-color: @blue@;
}
QPushButton[guiRole="primary"]:hover { background: @ink@; border-color: @ink@; }
QPushButton:focus { border: 1px dashed @blue@; }
QPushButton[guiRole="primary"]:focus, QPushButton:checked:focus { border: 1px dashed #ffffff; }
QPushButton:disabled, QPushButton[guiRole="primary"]:disabled {
  background: @paper@; color: @muted@; border: 1px solid @line@;
}
QToolButton {
  border: 1px solid transparent; background: transparent; padding: 5px 8px;
}
QToolButton:hover { background: @wash@; border-color: @line@; }
QToolButton:pressed, QToolButton:checked { background: @blue@; color: @paper@; border-color: @blue@; }
QToolButton:focus { border: 1px dashed @blue@; }
QToolButton:checked:focus { border-color: @paper@; }
QToolButton:disabled { color: @muted@; background: transparent; border-color: transparent; }
QToolButton:checked:disabled { background: @wash@; color: @muted@; border-color: @edge@; }
QLineEdit, QAbstractSpinBox, QTextEdit, QPlainTextEdit {
  background: #ffffff; border: 1px solid @edge@;
  selection-background-color: @blue@; selection-color: #ffffff;
}
QLineEdit, QAbstractSpinBox { font-family: "@numeric@"; font-size: 13px; padding: 5px 8px; min-height: 20px; }
QTextEdit, QPlainTextEdit { padding: 8px; }
QLineEdit:focus, QAbstractSpinBox:focus, QTextEdit:focus, QPlainTextEdit:focus { border-color: @blue@; }
QLineEdit:disabled, QAbstractSpinBox:disabled { background: @paper@; }
QComboBox { background: #ffffff; border: 1px solid @edge@; padding: 5px 24px 5px 8px; min-height: 20px; }
QComboBox:focus, QComboBox:hover { border-color: @blue@; }
QComboBox:disabled { background: @paper@; }
QComboBox::drop-down { width: 20px; background: @blue@; border: none; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
  width: 18px; background: @blue@; border: none;
}
QComboBox::drop-down:disabled, QAbstractSpinBox::up-button:disabled,
QAbstractSpinBox::down-button:disabled { background: @muted@; }
QComboBox::down-arrow, QAbstractSpinBox::down-arrow, QScrollBar::down-arrow,
QHeaderView::down-arrow, QToolButton::menu-indicator, QToolButton::menu-arrow {
  image: url(:/blcad/gui/arrow-down.xpm); width: 9px; height: 5px;
}
QAbstractSpinBox::up-arrow, QScrollBar::up-arrow, QHeaderView::up-arrow {
  image: url(:/blcad/gui/arrow-up.xpm); width: 9px; height: 5px;
}
QScrollBar::left-arrow { image: url(:/blcad/gui/arrow-left.xpm); width: 5px; height: 9px; }
QScrollBar::right-arrow, QMenu::right-arrow { image: url(:/blcad/gui/arrow-right.xpm); width: 5px; height: 9px; }
QMenu::right-arrow { background: @blue@; padding: 2px; }
QToolButton::menu-indicator { background: @blue@; subcontrol-position: bottom right; padding: 2px; }
QMenu { background: @blue@; color: @paper@; border: 1px solid @blue@; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 10px; border: 1px solid transparent; }
QMenu::item:selected { background: @paper@; color: @blue@; }
QMenu::item:disabled { color: #b9b5ed; }
QMenu::separator { height: 1px; background: #6864f7; margin: 5px 8px; }
QMenuBar { background: @blue@; color: @paper@; }
QMenuBar::item { padding: 7px 12px; background: transparent; }
QMenuBar::item:selected { background: @ink@; }
QAbstractItemView { background: #ffffff; alternate-background-color: @paper@; border: none; }
QAbstractItemView::item { padding: 3px 4px; border: 1px solid transparent; }
QAbstractItemView::item:hover { background: @wash@; }
QAbstractItemView::item:selected { background: @blue@; color: @paper@; }
QAbstractItemView::item:focus { border: 1px dashed @blue@; }
QAbstractItemView::item:selected:focus { border-color: @paper@; }
QComboBox QAbstractItemView { background: #ffffff; border: 1px solid @line@; }
QHeaderView::section {
  background: @blue@; color: @paper@; border: none;
  padding: 6px 8px; font-family: "@display@"; font-size: 16px;
}
QTableView { gridline-color: @line@; }
QTabWidget::pane { border: 1px solid @line@; }
QTabBar::tab {
  background: @paper@; color: @muted@; padding: 8px 14px;
  border: none; border-bottom: 2px solid transparent;
  font-family: "@display@"; font-size: 17px;
}
QTabBar::tab:hover { color: @blue@; background: @wash@; }
QTabBar::tab:selected { color: @blue@; border-bottom-color: @blue@; }
QTabBar::tab:disabled { color: @muted@; }
QCheckBox, QRadioButton { spacing: 7px; }
QCheckBox::indicator, QRadioButton::indicator {
  width: 12px; height: 12px; border: 1px solid @muted@; background: #ffffff;
}
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: @blue@; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: @blue@; border: 1px solid @blue@; }
QCheckBox::indicator:indeterminate { background: @wash@; border: 1px solid @blue@; }
QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { border-color: @line@; background: @paper@; }
QCheckBox::indicator:checked:disabled, QRadioButton::indicator:checked:disabled { border-color: @muted@; background: @muted@; }
QCheckBox::indicator:indeterminate:disabled { border-color: @muted@; background: @wash@; }
QSplitter::handle { background: @paper@; }
QSplitter::handle:hover { background: @line@; }
QScrollBar:horizontal { background: @paper@; height: 14px; margin: 0px 16px; }
QScrollBar:vertical { background: @paper@; width: 14px; margin: 16px 0px; }
QScrollBar::handle { background: @edge@; border: 3px solid @paper@; }
QScrollBar::handle:horizontal { min-width: 18px; }
QScrollBar::handle:vertical { min-height: 18px; }
QScrollBar::handle:hover { background: @muted@; }
QScrollBar::add-line, QScrollBar::sub-line { background: @blue@; width: 16px; height: 16px; border: none; }
QScrollBar::add-line:horizontal { subcontrol-position: right; subcontrol-origin: margin; }
QScrollBar::sub-line:horizontal { subcontrol-position: left; subcontrol-origin: margin; }
QScrollBar::add-line:vertical { subcontrol-position: bottom; subcontrol-origin: margin; }
QScrollBar::sub-line:vertical { subcontrol-position: top; subcontrol-origin: margin; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QProgressBar { border: 1px solid @line@; text-align: center; background: #ffffff; }
QProgressBar::chunk { background: @blue@; }
QSlider::groove:horizontal { height: 2px; background: @line@; }
QSlider::groove:vertical { width: 2px; background: @line@; }
QSlider::handle { background: @blue@; width: 12px; height: 12px; margin: -5px; }
QStatusBar { background: @blue@; color: @paper@; padding: 4px 8px; }
QStatusBar QLabel { color: @paper@; }
QStatusBar::item { border: none; }
QToolTip { color: @paper@; background: @ink@; border: none; padding: 6px 8px; }
QPlainTextEdit[objectName="blcad.shell.diagnostics"] {
  font-family: "@numeric@"; font-size: 12px; background: @paper@;
  border: none; border-top: 1px solid @line@;
}
QWidget#blcad_ribbon_header { background: @blue@; }
QWidget#blcad_ribbon_header QLabel { color: @paper@; }
QLabel[guiRole="wordmark"] { font-family: "@display@"; font-size: 34px; padding: 2px 0px; }
QTabBar#blcad_ribbon_tabs { background: @blue@; }
QTabBar#blcad_ribbon_tabs::tab { background: @blue@; color: @paper@; border-bottom: 2px solid transparent; }
QTabBar#blcad_ribbon_tabs::tab:hover { background: @ink@; }
QTabBar#blcad_ribbon_tabs::tab:selected { border-bottom-color: @paper@; }
QTabBar#blcad_ribbon_tabs::tab:disabled { color: #b9b5ed; }
QScrollArea#blcad_ribbon_navigation, QScrollArea#blcad_ribbon_navigation QWidget,
QWidget[guiRole="tabNavigation"] { background: @blue@; }
QToolButton[guiRole="tabNavigation"] {
  background: @blue@; color: @paper@; border: none; padding: 0px;
}
QToolButton[guiRole="tabNavigation"]:hover { background: @ink@; }
QToolButton[guiRole="tabNavigation"]:focus { border: 1px dashed @paper@; }
QFrame[guiRole="ribbonGroup"] { border: none; border-right: 1px solid @line@; }
QLabel[guiRole="ribbonCaption"] { color: @muted@; font-family: "@display@"; font-size: 13px; }
QToolButton[guiRole="ribbonCommand"] { font-family: "@display@"; font-size: 17px; }
)qss");
  for (const auto& [token, value] : std::array<std::pair<QString, QString>, 11>{
           {{QStringLiteral("@blue@"), color_name(appearance::blue)},
            {QStringLiteral("@paper@"), color_name(appearance::paper)},
            {QStringLiteral("@ink@"), color_name(appearance::ink)},
            {QStringLiteral("@muted@"), color_name(appearance::muted)},
            {QStringLiteral("@line@"), color_name(appearance::line)},
            {QStringLiteral("@edge@"), color_name(appearance::edge)},
            {QStringLiteral("@wash@"), color_name(appearance::wash)},
            {QStringLiteral("@canvas@"), color_name(appearance::canvas)},
            {QStringLiteral("@body@"), desktop_fonts().body},
            {QStringLiteral("@display@"), desktop_fonts().display},
            {QStringLiteral("@numeric@"), desktop_fonts().numeric}}})
    stylesheet.replace(token, value);
  application.setStyleSheet(stylesheet);
}

} // namespace blcad::gui
