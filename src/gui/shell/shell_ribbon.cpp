#include "blcad/gui/shell/shell_ribbon.hpp"

#include <QAction>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace blcad::gui {
namespace {

// The group container owns a two-row layout: tool buttons above, caption
// below (Inventor panel look). The button row is the first layout item.
QHBoxLayout* button_row(QWidget* group) {
  auto* column = static_cast<QVBoxLayout*>(group->layout());
  return static_cast<QHBoxLayout*>(column->itemAt(0)->layout());
}

} // namespace

ShellRibbon::ShellRibbon(QWidget* parent) : QWidget(parent) {
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);
  tabs_ = new QTabBar(this);
  tabs_->setDocumentMode(true);
  tabs_->setExpanding(false);
  pages_ = new QStackedWidget(this);
  // Keep the command surface reachable when a tab contains more groups than
  // fit beside the browser. Reserve room for the horizontal scrollbar as well
  // as the full button/caption height; group widths must not size the window.
  pages_->setFixedHeight(116);
  column->addWidget(tabs_);
  column->addWidget(pages_);
  connect(tabs_, &QTabBar::currentChanged, pages_, &QStackedWidget::setCurrentIndex);
}

int ShellRibbon::add_tab(const QString& title) {
  auto* scroll = new QScrollArea(pages_);
  scroll->setObjectName(QStringLiteral("ribbon.page.%1").arg(tabs_->count()));
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* page = new QWidget(scroll);
  auto* row = new QHBoxLayout(page);
  row->setContentsMargins(8, 4, 8, 4);
  row->setSpacing(10);
  row->addStretch(1);
  scroll->setWidget(page);
  pages_->addWidget(scroll);
  return tabs_->addTab(title);
}

QWidget* ShellRibbon::add_group(int tab_index, const QString& title) {
  auto* scroll = qobject_cast<QScrollArea*>(pages_->widget(tab_index));
  if (scroll == nullptr)
    return nullptr;
  auto* page = scroll->widget();
  auto* group = new QFrame(page);
  static_cast<QFrame*>(group)->setFrameShape(QFrame::StyledPanel);
  auto* column = new QVBoxLayout(group);
  column->setContentsMargins(6, 4, 6, 2);
  column->setSpacing(2);
  auto* buttons = new QHBoxLayout();
  buttons->setSpacing(4);
  column->addLayout(buttons, 1);
  auto* caption = new QLabel(title, group);
  caption->setAlignment(Qt::AlignHCenter);
  QFont caption_font = caption->font();
  caption_font.setPointSizeF(caption_font.pointSizeF() * 0.85);
  caption->setFont(caption_font);
  column->addWidget(caption);
  auto* row = static_cast<QHBoxLayout*>(page->layout());
  row->insertWidget(row->count() - 1, group);
  return group;
}

QToolButton* ShellRibbon::add_button(QWidget* group, QAction* action) {
  auto* button = new QToolButton(group);
  button->setDefaultAction(action);
  button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  button->setAutoRaise(true);
  button->setMinimumSize(56, 64);
  button->setMinimumWidth(std::max(56, button->sizeHint().width()));
  button_row(group)->addWidget(button);
  return button;
}

QToolButton* ShellRibbon::add_menu_button(QWidget* group, const QString& title,
                                          const QList<QAction*>& actions) {
  auto* button = new QToolButton(group);
  button->setText(title);
  button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  button->setAutoRaise(true);
  button->setMinimumSize(56, 64);
  button->setPopupMode(QToolButton::InstantPopup);
  auto* menu = new QMenu(button);
  for (QAction* action : actions)
    menu->addAction(action);
  button->setMenu(menu);
  button->setMinimumWidth(std::max(56, button->sizeHint().width()));
  button_row(group)->addWidget(button);
  return button;
}

QToolButton* ShellRibbon::add_small_button(QWidget* group, QAction* action) {
  // Compact buttons live in one grid at the end of the button row, filled
  // column-major with two rows.
  auto* row = button_row(group);
  QGridLayout* grid = nullptr;
  if (row->count() > 0)
    grid = qobject_cast<QGridLayout*>(row->itemAt(row->count() - 1)->layout());
  if (grid == nullptr) {
    grid = new QGridLayout();
    grid->setSpacing(2);
    row->addLayout(grid);
  }
  auto* button = new QToolButton(group);
  button->setDefaultAction(action);
  button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  button->setAutoRaise(true);
  const int count = grid->count();
  grid->addWidget(button, count % 2, count / 2);
  return button;
}

void ShellRibbon::set_current_tab(int index) {
  tabs_->setCurrentIndex(index);
}

void ShellRibbon::add_widget(QWidget* group, QWidget* widget) {
  widget->setParent(group);
  button_row(group)->addWidget(widget);
}

int ShellRibbon::current_tab() const {
  return tabs_->currentIndex();
}

int ShellRibbon::tab_count() const {
  return tabs_->count();
}

QString ShellRibbon::tab_title(int index) const {
  return tabs_->tabText(index);
}

void ShellRibbon::set_tab_enabled(int index, bool enabled) {
  tabs_->setTabEnabled(index, enabled);
}

bool ShellRibbon::tab_enabled(int index) const {
  return tabs_->isTabEnabled(index);
}

} // namespace blcad::gui
