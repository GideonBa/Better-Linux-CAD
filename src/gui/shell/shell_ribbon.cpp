#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/gui_appearance.hpp"
#include "blcad/gui/gui_icons.hpp"

#include <QAction>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
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
  setObjectName(QStringLiteral("blcad.shell.ribbon"));
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);
  auto* header = new QWidget(this);
  header->setObjectName(QStringLiteral("blcad_ribbon_header"));
  auto* navigation = new QHBoxLayout(header);
  navigation->setContentsMargins(16, 0, 16, 0);
  navigation->setSpacing(16);
  auto* wordmark = new QLabel(QStringLiteral("BLCAD"), header);
  wordmark->setProperty("guiRole", "wordmark");
  wordmark->setAccessibleName(QStringLiteral("BLCAD parametric design"));
  navigation->addWidget(wordmark);
  tabs_ = new QTabBar(header);
  tabs_->setObjectName(QStringLiteral("blcad_ribbon_tabs"));
  tabs_->setDocumentMode(true);
  tabs_->setExpanding(false);
  tabs_->setUsesScrollButtons(false);
  tabs_->setDrawBase(false);
  tabs_->setFont(appearance::heading_font(17));
  tab_navigation_ = new QScrollArea(header);
  tab_navigation_->setObjectName(QStringLiteral("blcad_ribbon_navigation"));
  tab_navigation_->setFrameShape(QFrame::NoFrame);
  tab_navigation_->setWidgetResizable(false);
  tab_navigation_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tab_navigation_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // Only the visible slice of the tab strip participates in header sizing.
  // Its full content width must not push the navigation buttons offscreen.
  tab_navigation_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  tab_navigation_->setMinimumWidth(80);
  tab_navigation_->setWidget(tabs_);
  navigation->addWidget(tab_navigation_, 1);
  tab_navigation_buttons_ = new QWidget(header);
  tab_navigation_buttons_->setProperty("guiRole", "tabNavigation");
  tab_navigation_buttons_->setFixedSize(48, 32);
  // Overflow is reported while the header layout is being resized. Reserve
  // this small control slot even when hidden so showing it cannot invalidate
  // that same layout pass and leave the buttons outside the header.
  auto navigation_policy = tab_navigation_buttons_->sizePolicy();
  navigation_policy.setRetainSizeWhenHidden(true);
  tab_navigation_buttons_->setSizePolicy(navigation_policy);
  auto* arrows = new QHBoxLayout(tab_navigation_buttons_);
  arrows->setContentsMargins(0, 0, 0, 0);
  arrows->setSpacing(0);
  const auto arrow = [&](const char* id, const QString& text, const char* asset) {
    auto* button = new QToolButton(tab_navigation_buttons_);
    button->setObjectName(QString::fromLatin1(id));
    button->setProperty("guiRole", "tabNavigation");
    button->setIcon(pixelart_icon(QString::fromLatin1(asset)));
    button->setIconSize(QSize(24, 24));
    button->setFixedSize(24, 32);
    button->setAccessibleName(text);
    button->setToolTip(text);
    arrows->addWidget(button);
    return button;
  };
  previous_tab_ = arrow("blcad.ribbon.tab_previous", QStringLiteral("Scroll workspace tabs left"),
                        "chevron-left");
  next_tab_ = arrow("blcad.ribbon.tab_next", QStringLiteral("Scroll workspace tabs right"),
                    "chevron-right");
  navigation->addWidget(tab_navigation_buttons_);
  const auto scroll_tabs = [this](int direction) {
    auto* bar = tab_navigation_->horizontalScrollBar();
    bar->setValue(bar->value() +
                  direction * std::max(64, tab_navigation_->viewport()->width() / 2));
  };
  connect(previous_tab_, &QToolButton::clicked, this, [scroll_tabs] { scroll_tabs(-1); });
  connect(next_tab_, &QToolButton::clicked, this, [scroll_tabs] { scroll_tabs(1); });
  connect(tab_navigation_->horizontalScrollBar(), &QScrollBar::rangeChanged, this,
          [this] { update_tab_navigation(); });
  connect(tab_navigation_->horizontalScrollBar(), &QScrollBar::valueChanged, this,
          [this] { update_tab_navigation(); });
  pages_ = new QStackedWidget(this);
  // Keep the command surface reachable when a tab contains more groups than
  // fit beside the browser. Reserve room for the horizontal scrollbar as well
  // as the full button/caption height; group widths must not size the window.
  pages_->setFixedHeight(116);
  column->addWidget(header);
  column->addWidget(pages_);
  connect(tabs_, &QTabBar::currentChanged, pages_, &QStackedWidget::setCurrentIndex);
  connect(tabs_, &QTabBar::currentChanged, this, [this](int index) {
    const QRect tab = tabs_->tabRect(index);
    tab_navigation_->ensureVisible(tab.center().x(), tab.center().y(), tab.width() / 2 + 4, 0);
  });
}

void ShellRibbon::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  update_tab_navigation();
}

void ShellRibbon::update_tab_navigation() {
  if (tabs_->count() == 0)
    return;
  tabs_->setFixedSize(tabs_->sizeHint());
  tab_navigation_->setFixedHeight(tabs_->sizeHint().height());
  auto* bar = tab_navigation_->horizontalScrollBar();
  previous_tab_->setEnabled(bar->value() > bar->minimum());
  next_tab_->setEnabled(bar->value() < bar->maximum());
  tab_navigation_buttons_->setVisible(bar->maximum() > bar->minimum());
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
  row->setContentsMargins(12, 8, 12, 6);
  row->setSpacing(12);
  row->addStretch(1);
  scroll->setWidget(page);
  pages_->addWidget(scroll);
  const int index = tabs_->addTab(gui_icon_for_label(title), title);
  tabs_->setTabToolTip(index, title);
  tabs_->setIconSize(QSize(24, 24));
  update_tab_navigation();
  return index;
}

QWidget* ShellRibbon::add_group(int tab_index, const QString& title) {
  auto* scroll = qobject_cast<QScrollArea*>(pages_->widget(tab_index));
  if (scroll == nullptr)
    return nullptr;
  auto* page = scroll->widget();
  auto* group = new QFrame(page);
  group->setProperty("guiRole", "ribbonGroup");
  group->setFrameShape(QFrame::NoFrame);
  auto* column = new QVBoxLayout(group);
  column->setContentsMargins(0, 0, 12, 0);
  column->setSpacing(4);
  auto* buttons = new QHBoxLayout();
  buttons->setSpacing(4);
  column->addLayout(buttons, 1);
  auto* caption = new QLabel(title, group);
  caption->setProperty("guiRole", "ribbonCaption");
  caption->setAlignment(Qt::AlignLeft);
  QFont caption_font = appearance::heading_font(13);
  caption_font.setCapitalization(QFont::AllUppercase);
  caption->setFont(caption_font);
  column->addWidget(caption);
  auto* row = static_cast<QHBoxLayout*>(page->layout());
  row->insertWidget(row->count() - 1, group);
  return group;
}

QToolButton* ShellRibbon::add_button(QWidget* group, QAction* action) {
  apply_gui_action_icon(*action);
  auto* button = new QToolButton(group);
  button->setProperty("guiRole", "ribbonCommand");
  button->setDefaultAction(action);
  button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  button->setIconSize(QSize(24, 24));
  button->setAutoRaise(true);
  button->setMinimumSize(56, 64);
  button->setMinimumWidth(std::max(56, button->sizeHint().width()));
  button_row(group)->addWidget(button);
  return button;
}

QToolButton* ShellRibbon::add_menu_button(QWidget* group, const QString& title,
                                          const QList<QAction*>& actions) {
  auto* button = new QToolButton(group);
  button->setProperty("guiRole", "ribbonCommand");
  button->setText(title);
  button->setIcon(gui_icon_for_label(title));
  button->setIconSize(QSize(24, 24));
  button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  button->setAutoRaise(true);
  button->setMinimumSize(56, 64);
  button->setPopupMode(QToolButton::InstantPopup);
  auto* menu = new QMenu(button);
  for (QAction* action : actions) {
    apply_gui_action_icon(*action);
    menu->addAction(action);
  }
  button->setMenu(menu);
  button->setMinimumWidth(std::max(56, button->sizeHint().width()));
  button_row(group)->addWidget(button);
  return button;
}

QToolButton* ShellRibbon::add_small_button(QWidget* group, QAction* action) {
  apply_gui_action_icon(*action);
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
  button->setProperty("guiRole", "ribbonCompact");
  button->setDefaultAction(action);
  button->setToolButtonStyle(action->icon().isNull() ? Qt::ToolButtonTextOnly
                                                     : Qt::ToolButtonTextBesideIcon);
  button->setIconSize(QSize(24, 24));
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
