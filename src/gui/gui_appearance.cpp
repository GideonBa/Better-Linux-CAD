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
        QColor::fromRgb(group == QPalette::Disabled ? appearance::muted : appearance::text);
    palette.setColor(group, QPalette::Window, QColor::fromRgb(appearance::panel));
    palette.setColor(group, QPalette::Button, QColor::fromRgb(appearance::panel));
    palette.setColor(group, QPalette::Base, QColor::fromRgb(appearance::base));
    palette.setColor(group, QPalette::AlternateBase, QColor::fromRgb(appearance::panel));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
      palette.setColor(group, role, text);
    palette.setColor(group, QPalette::BrightText, QColor::fromRgb(appearance::text));
    palette.setColor(group, QPalette::Highlight, QColor::fromRgb(appearance::accent));
    palette.setColor(group, QPalette::HighlightedText, QColor::fromRgb(appearance::base));
    palette.setColor(group, QPalette::PlaceholderText, QColor::fromRgb(appearance::muted));
    palette.setColor(group, QPalette::Link, QColor::fromRgb(appearance::accent));
    palette.setColor(group, QPalette::LinkVisited, QColor::fromRgb(appearance::accent));
    palette.setColor(group, QPalette::Light, QColor::fromRgb(appearance::edge));
    for (const auto role : {QPalette::Midlight, QPalette::Mid, QPalette::Dark, QPalette::Shadow})
      palette.setColor(group, role, QColor::fromRgb(appearance::line));
    palette.setColor(group, QPalette::ToolTipBase, QColor::fromRgb(appearance::tooltip));
    palette.setColor(group, QPalette::ToolTipText, QColor::fromRgb(appearance::text));
  }
  application.setPalette(palette);

  // Foreground defaults stay in the palette: Qt 6.4 otherwise derives a
  // half-opacity placeholder from a QSS color rule. Display roles opt in below.
  QString stylesheet = QStringLiteral(R"qss(
* {
  font-family: "@body@";
  font-size: 14px;
  border-radius: 0px;
  selection-color: @base@;
  selection-background-color: @accent@;
}
QWidget { background-color: @panel@; }
QLabel { background-color: transparent; }
QMainWindow::separator { width: 4px; height: 4px; background: @panel@; }
QMainWindow::separator:hover { background: @accent@; }
QDockWidget { font-family: "@display@"; font-size: 20px; }
QDockWidget::title {
  color: @accent@; background: @panel@;
  border: none; border-top: 1px solid @line@;
  padding: 8px 12px;
}
QDockWidget::close-button, QDockWidget::float-button {
  border: none; background: transparent; padding: 3px;
}
QDockWidget::close-button:hover, QDockWidget::float-button:hover { background: @hover@; }
QFrame, QScrollArea { border: none; }
QGroupBox {
  border: none; border-top: 1px solid @line@;
  margin-top: 18px; padding-top: 12px;
}
QGroupBox::title { color: @accent@; subcontrol-origin: margin; left: 0px; padding: 0px 4px 0px 0px; }
QPushButton {
  background: @panel@; border: 1px solid @edge@;
  min-height: 22px; padding: 5px 12px;
}
QPushButton:hover { background: @hover@; border-color: @accent@; }
QPushButton:pressed, QPushButton:checked, QPushButton[guiRole="primary"] {
  background: @selection@; color: @text@; border-color: @accent@;
}
QPushButton[guiRole="primary"]:hover { background: @hover@; border-color: @accent@; }
QPushButton:focus { border: 1px dashed @accent@; }
QPushButton[guiRole="primary"]:focus, QPushButton:checked:focus { border: 1px dashed @text@; }
QPushButton:disabled, QPushButton[guiRole="primary"]:disabled {
  background: @panel@; color: @muted@; border: 1px solid @line@;
}
QToolButton {
  border: 1px solid transparent; background: transparent; padding: 5px 8px;
}
QToolButton[guiRole="ribbonCompact"] { padding: 2px 8px; }
QToolButton:hover { background: @hover@; border-color: @line@; }
QToolButton:pressed, QToolButton:checked { background: @selection@; color: @text@; border-color: @accent@; }
QToolButton:focus { border: 1px dashed @accent@; }
QToolButton:checked:focus { border-color: @accent@; }
QToolButton:disabled { color: @muted@; background: transparent; border-color: transparent; }
QToolButton:checked:disabled { background: @hover@; color: @muted@; border-color: @edge@; }
QLineEdit, QAbstractSpinBox, QTextEdit, QPlainTextEdit {
  background: @base@; border: 1px solid @edge@;
  selection-background-color: @accent@; selection-color: @base@;
}
QLineEdit, QAbstractSpinBox { font-family: "@numeric@"; font-size: 13px; padding: 5px 8px; min-height: 20px; }
QTextEdit, QPlainTextEdit { padding: 8px; }
QLineEdit:focus, QAbstractSpinBox:focus, QTextEdit:focus, QPlainTextEdit:focus { border-color: @accent@; }
QLineEdit:disabled, QAbstractSpinBox:disabled { background: @panel@; }
QComboBox { background: @base@; border: 1px solid @edge@; padding: 5px 24px 5px 8px; min-height: 20px; }
QComboBox:focus, QComboBox:hover { border-color: @accent@; }
QComboBox:disabled { background: @panel@; }
QComboBox::drop-down { width: 20px; background: @chrome@; border: none; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
  width: 18px; background: @chrome@; border: none;
}
QComboBox::drop-down:disabled, QAbstractSpinBox::up-button:disabled,
QAbstractSpinBox::down-button:disabled { background: @disabled_surface@; }
QComboBox::down-arrow, QAbstractSpinBox::down-arrow, QScrollBar::down-arrow,
QHeaderView::down-arrow, QToolButton::menu-indicator, QToolButton::menu-arrow {
  image: url(:/blcad/gui/arrow-down.xpm); width: 9px; height: 5px;
}
QAbstractSpinBox::up-arrow, QScrollBar::up-arrow, QHeaderView::up-arrow {
  image: url(:/blcad/gui/arrow-up.xpm); width: 9px; height: 5px;
}
QScrollBar::left-arrow { image: url(:/blcad/gui/arrow-left.xpm); width: 5px; height: 9px; }
QScrollBar::right-arrow, QMenu::right-arrow { image: url(:/blcad/gui/arrow-right.xpm); width: 5px; height: 9px; }
QMenu::right-arrow { background: @chrome@; padding: 2px; }
QToolButton::menu-indicator { background: @chrome@; subcontrol-position: bottom right; padding: 2px; }
QMenu { background: @chrome@; color: @text@; border: 1px solid @edge@; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 10px; border: 1px solid transparent; }
QMenu::item:selected { background: @selection@; color: @text@; border-color: @accent@; }
QMenu::item:disabled { color: @muted@; }
QMenu::separator { height: 1px; background: @edge@; margin: 5px 8px; }
QMenuBar { background: @chrome@; color: @text@; }
QMenuBar::item { padding: 7px 12px; background: transparent; }
QMenuBar::item:selected { background: @hover@; }
QAbstractItemView { background: @base@; alternate-background-color: @panel@; border: none; }
QAbstractItemView::item { padding: 3px 4px; border: 1px solid transparent; }
QAbstractItemView::item:hover { background: @hover@; }
QAbstractItemView::item:selected { background: @selection@; color: @text@; border-color: @accent@; }
QAbstractItemView::item:focus { border: 1px dashed @accent@; }
QAbstractItemView::item:selected:focus { border-color: @accent@; }
QComboBox QAbstractItemView { background: @base@; border: 1px solid @line@; }
QHeaderView::section {
  background: @chrome@; color: @text@; border: none;
  padding: 6px 8px; font-family: "@display@"; font-size: 16px;
}
QTableView { gridline-color: @line@; }
QTabWidget::pane { border: 1px solid @line@; }
QTabBar::tab {
  background: @panel@; color: @muted@; padding: 8px 14px;
  border: none; border-bottom: 2px solid transparent;
  font-family: "@display@"; font-size: 17px;
}
QTabBar::tab:hover { color: @accent@; background: @hover@; }
QTabBar::tab:selected { color: @accent@; border-bottom-color: @accent@; }
QTabBar::tab:disabled { color: @muted@; }
QCheckBox, QRadioButton { spacing: 7px; }
QCheckBox::indicator, QRadioButton::indicator {
  width: 12px; height: 12px; border: 1px solid @muted@; background: @base@;
}
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: @accent@; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: @accent@; border: 1px solid @accent@; }
QCheckBox::indicator:indeterminate { background: @hover@; border: 1px solid @accent@; }
QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { border-color: @line@; background: @panel@; }
QCheckBox::indicator:checked:disabled, QRadioButton::indicator:checked:disabled { border-color: @muted@; background: @muted@; }
QCheckBox::indicator:indeterminate:disabled { border-color: @muted@; background: @hover@; }
QSplitter::handle { background: @panel@; }
QSplitter::handle:hover { background: @line@; }
QScrollBar:horizontal { background: @panel@; height: 14px; margin: 0px 16px; }
QScrollBar:vertical { background: @panel@; width: 14px; margin: 16px 0px; }
QScrollBar::handle { background: @edge@; border: 3px solid @panel@; }
QScrollBar::handle:horizontal { min-width: 18px; }
QScrollBar::handle:vertical { min-height: 18px; }
QScrollBar::handle:hover { background: @muted@; }
QScrollBar::add-line, QScrollBar::sub-line { background: @chrome@; width: 16px; height: 16px; border: none; }
QScrollBar::add-line:horizontal { subcontrol-position: right; subcontrol-origin: margin; }
QScrollBar::sub-line:horizontal { subcontrol-position: left; subcontrol-origin: margin; }
QScrollBar::add-line:vertical { subcontrol-position: bottom; subcontrol-origin: margin; }
QScrollBar::sub-line:vertical { subcontrol-position: top; subcontrol-origin: margin; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QProgressBar { border: 1px solid @line@; text-align: center; background: @base@; }
QProgressBar::chunk { background: @selection@; }
QSlider::groove:horizontal { height: 2px; background: @line@; }
QSlider::groove:vertical { width: 2px; background: @line@; }
QSlider::handle { background: @accent@; width: 12px; height: 12px; margin: -5px; }
QStatusBar { background: @chrome@; color: @text@; padding: 4px 8px; }
QStatusBar QLabel { color: @text@; }
QStatusBar::item { border: none; }
QToolTip { color: @text@; background: @tooltip@; border: none; padding: 6px 8px; }
QPlainTextEdit[objectName="blcad.shell.diagnostics"] {
  font-family: "@numeric@"; font-size: 12px; background: @panel@;
  border: none; border-top: 1px solid @line@;
}
QWidget#blcad_ribbon_header { background: @chrome@; }
QWidget#blcad_ribbon_header QLabel { color: @text@; }
QLabel[guiRole="wordmark"] { font-family: "@display@"; font-size: 34px; padding: 2px 0px; }
QTabBar#blcad_ribbon_tabs { background: @chrome@; }
QTabBar#blcad_ribbon_tabs::tab { background: @chrome@; color: @text@; border-bottom: 2px solid transparent; }
QTabBar#blcad_ribbon_tabs::tab:hover { background: @hover@; }
QTabBar#blcad_ribbon_tabs::tab:selected { border-bottom-color: @text@; }
QTabBar#blcad_ribbon_tabs::tab:disabled { color: @muted@; }
QScrollArea#blcad_ribbon_navigation, QScrollArea#blcad_ribbon_navigation QWidget,
QWidget[guiRole="tabNavigation"] { background: @chrome@; }
QToolButton[guiRole="tabNavigation"] {
  background: @chrome@; color: @text@; border: none; padding: 0px;
}
QToolButton[guiRole="tabNavigation"]:hover { background: @hover@; }
QToolButton[guiRole="tabNavigation"]:focus { border: 1px dashed @text@; }
QFrame[guiRole="ribbonGroup"] { border: none; border-right: 1px solid @line@; }
QLabel[guiRole="ribbonCaption"] { color: @muted@; font-family: "@display@"; font-size: 13px; }
QToolButton[guiRole="ribbonCommand"] { font-family: "@display@"; font-size: 17px; }
)qss");
  for (const auto& [token, value] : std::array<std::pair<QString, QString>, 15>{
           {{QStringLiteral("@panel@"), color_name(appearance::panel)},
            {QStringLiteral("@base@"), color_name(appearance::base)},
            {QStringLiteral("@chrome@"), color_name(appearance::chrome)},
            {QStringLiteral("@selection@"), color_name(appearance::selection)},
            {QStringLiteral("@hover@"), color_name(appearance::hover)},
            {QStringLiteral("@disabled_surface@"), color_name(appearance::disabled_surface)},
            {QStringLiteral("@text@"), color_name(appearance::text)},
            {QStringLiteral("@muted@"), color_name(appearance::muted)},
            {QStringLiteral("@accent@"), color_name(appearance::accent)},
            {QStringLiteral("@line@"), color_name(appearance::line)},
            {QStringLiteral("@edge@"), color_name(appearance::edge)},
            {QStringLiteral("@tooltip@"), color_name(appearance::tooltip)},
            {QStringLiteral("@body@"), desktop_fonts().body},
            {QStringLiteral("@display@"), desktop_fonts().display},
            {QStringLiteral("@numeric@"), desktop_fonts().numeric}}})
    stylesheet.replace(token, value);
  application.setStyleSheet(stylesheet);
}

} // namespace blcad::gui
