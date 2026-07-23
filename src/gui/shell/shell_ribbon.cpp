#include "blcad/gui/shell/shell_ribbon.hpp"

#include <QAction>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

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
  pages_->setFixedHeight(96);
  column->addWidget(tabs_);
  column->addWidget(pages_);
  connect(tabs_, &QTabBar::currentChanged, pages_, &QStackedWidget::setCurrentIndex);
}

int ShellRibbon::add_tab(const QString& title) {
  auto* page = new QWidget(pages_);
  auto* row = new QHBoxLayout(page);
  row->setContentsMargins(8, 4, 8, 4);
  row->setSpacing(10);
  row->addStretch(1);
  pages_->addWidget(page);
  return tabs_->addTab(title);
}

QWidget* ShellRibbon::add_group(int tab_index, const QString& title) {
  auto* page = pages_->widget(tab_index);
  if (page == nullptr)
    return nullptr;
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
