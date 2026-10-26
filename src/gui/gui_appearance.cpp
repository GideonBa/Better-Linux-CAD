#include "blcad/gui/gui_appearance.hpp"

#include <QApplication>
#include <QLocale>
#include <QPalette>
#include <QResource>
#include <QStyleFactory>

// Keep the resource initializer at global scope for static-library linkage.
static void initialize_gui_appearance_assets() {
  Q_INIT_RESOURCE(blcad_gui_assets);
}

namespace blcad::gui {

void configure_gui_appearance(QApplication& application) {
  initialize_gui_appearance_assets();
  // Explicit controls keep desktop theme engines from adding gradients,
  // rounded frames, or a different foreground/background palette.
  application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
  application.setAttribute(Qt::AA_DontUseNativeDialogs);
  QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

  QPalette palette;
  for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    for (int role = 0; role < QPalette::NColorRoles; ++role)
      palette.setColor(group, static_cast<QPalette::ColorRole>(role), Qt::black);
    for (const auto role :
         {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::BrightText,
          QPalette::Light, QPalette::Midlight, QPalette::Mid, QPalette::Dark, QPalette::Shadow,
          QPalette::Link, QPalette::LinkVisited, QPalette::HighlightedText, QPalette::ToolTipText,
          QPalette::PlaceholderText})
      palette.setColor(group, role, Qt::white);
  }
  application.setPalette(palette);
  // Qt 6.4 derives a half-opacity PlaceholderText brush from a QSS color rule.
  // Keep foreground colors in the application palette so placeholders stay white.
  application.setStyleSheet(QStringLiteral(R"qss(
* {
  background-color: #000000;
  border-radius: 0px;
  selection-color: #ffffff;
  selection-background-color: #000000;
}
QToolTip, QMenu, QDockWidget, QFrame[frameShape="6"], QGroupBox {
  border: 1px solid #ffffff;
}
QPushButton, QToolButton, QComboBox, QLineEdit, QAbstractSpinBox,
QTextEdit, QPlainTextEdit, QAbstractItemView, QTabBar::tab, QMenuBar::item,
QHeaderView::section {
  border: 1px solid #ffffff;
  background-color: #000000;
}
QPushButton, QToolButton, QComboBox, QLineEdit, QAbstractSpinBox {
  padding: 3px 5px;
}
QAbstractButton:hover, QComboBox:hover, QTabBar::tab:hover, QMenuBar::item:selected {
  border: 1px dashed #ffffff;
}
QAbstractButton:pressed, QAbstractButton:checked, QAbstractButton:focus,
QComboBox:focus, QLineEdit:focus, QAbstractSpinBox:focus {
  border: 2px solid #ffffff;
}
QAbstractButton:disabled, QComboBox:disabled, QLineEdit:disabled,
QAbstractSpinBox:disabled {
  border: 1px dotted #ffffff;
}
QAbstractButton:disabled { font-style: italic; }
QComboBox::drop-down, QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
  background-color: #000000;
  border: 1px solid #ffffff;
  border-radius: 0px;
}
QComboBox::down-arrow, QAbstractSpinBox::down-arrow, QScrollBar::down-arrow,
QHeaderView::down-arrow, QToolButton::menu-indicator, QToolButton::menu-arrow {
  image: url(:/blcad/gui/arrow-down.xpm);
  width: 9px; height: 5px;
}
QAbstractSpinBox::up-arrow, QScrollBar::up-arrow, QHeaderView::up-arrow {
  image: url(:/blcad/gui/arrow-up.xpm);
  width: 9px; height: 5px;
}
QScrollBar::left-arrow {
  image: url(:/blcad/gui/arrow-left.xpm);
  width: 5px; height: 9px;
}
QScrollBar::right-arrow, QMenu::right-arrow {
  image: url(:/blcad/gui/arrow-right.xpm);
  width: 5px; height: 9px;
}
QMenu::item { padding: 4px 20px; border: 1px solid #000000; }
QMenu::item:selected { border: 1px solid #ffffff; }
QMenu::item:disabled { font-style: italic; }
QMenu::separator { height: 1px; background-color: #ffffff; margin: 3px 0px; }
QHeaderView::section { padding: 3px; }
QAbstractItemView::item { border: 1px solid #000000; }
QAbstractItemView::item:selected, QAbstractItemView::item:hover {
  background-color: #000000;
  border: 1px solid #ffffff;
}
QTabWidget::pane { border: 1px solid #ffffff; }
QTabBar::tab { padding: 4px 10px; }
QTabBar::tab:selected { border-bottom: 3px solid #ffffff; }
QDockWidget::title { border: 1px solid #ffffff; padding: 3px; }
QSplitter::handle { background-color: #000000; border: 1px solid #ffffff; }
QScrollBar:horizontal { height: 15px; border: 1px solid #ffffff; }
QScrollBar:vertical { width: 15px; border: 1px solid #ffffff; }
QScrollBar::handle { background-color: #000000; border: 1px solid #ffffff; min-width: 16px; min-height: 16px; }
QScrollBar::handle:hover { border: 2px solid #ffffff; }
QScrollBar::add-line, QScrollBar::sub-line { background-color: #000000; border: 1px solid #ffffff; }
QScrollBar::add-page, QScrollBar::sub-page { background-color: #000000; }
QCheckBox::indicator, QRadioButton::indicator {
  width: 12px; height: 12px; border: 1px solid #ffffff; border-radius: 0px;
  background-color: #000000;
}
QCheckBox::indicator:checked, QRadioButton::indicator:checked,
QCheckBox::indicator:indeterminate { border: 4px solid #ffffff; }
QProgressBar { border: 1px solid #ffffff; text-align: center; }
QProgressBar::chunk { background-color: #ffffff; }
QSlider::groove:horizontal { height: 3px; border: 1px solid #ffffff; }
QSlider::groove:vertical { width: 3px; border: 1px solid #ffffff; }
QSlider::handle { border: 1px solid #ffffff; background-color: #000000; width: 12px; height: 12px; }
QStatusBar::item { border: none; }
)qss"));
}

} // namespace blcad::gui
