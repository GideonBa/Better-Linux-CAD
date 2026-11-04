#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/gui_appearance.hpp"
#include "blcad/gui/gui_icons.hpp"

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>
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
  quick_access_ = new QWidget(this);
  quick_access_->setObjectName(QStringLiteral("blcad.ribbon.quick_access"));
  quick_actions_ = new QHBoxLayout(quick_access_);
  quick_actions_->setContentsMargins(12, 2, 12, 2);
  quick_actions_->setSpacing(4);
  auto* file = new QToolButton(quick_access_);
  file->setText(QStringLiteral("File"));
  file->setAccessibleName(QStringLiteral("File"));
  file->setObjectName(QStringLiteral("blcad.ribbon.file"));
  file->setPopupMode(QToolButton::InstantPopup);
  file_menu_ = new QMenu(file);
  file->setMenu(file_menu_);
  quick_actions_->addWidget(file);
  quick_actions_->addStretch(1);
  auto* search = new QToolButton(quick_access_);
  search->setObjectName(QStringLiteral("blcad.ribbon.search"));
  search->setText(QStringLiteral("Find Command  Ctrl+K"));
  search->setAccessibleName(QStringLiteral("Find Command"));
  quick_actions_->addWidget(search);
  connect(search, &QToolButton::clicked, this, &ShellRibbon::show_command_search);
  auto* search_action = new QAction(QStringLiteral("Find Command"), this);
  search_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
  addAction(search_action);
  connect(search_action, &QAction::triggered, this, &ShellRibbon::show_command_search);
  column->addWidget(quick_access_);
  // A standalone ribbon need not expose an empty document toolbar.
  quick_access_->hide();
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
  group->setProperty("ribbonTab", tab_index);
  group->setProperty("ribbonGroup", title);
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
  register_command(action, group->property("ribbonTab").toInt(),
                   group->property("ribbonGroup").toString());
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
    register_command(action, group->property("ribbonTab").toInt(),
                     group->property("ribbonGroup").toString());
    apply_gui_action_icon(*action);
    menu->addAction(action);
  }
  button->setMenu(menu);
  button->setMinimumWidth(std::max(56, button->sizeHint().width()));
  button_row(group)->addWidget(button);
  return button;
}

QToolButton* ShellRibbon::add_small_button(QWidget* group, QAction* action) {
  register_command(action, group->property("ribbonTab").toInt(),
                   group->property("ribbonGroup").toString());
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

void ShellRibbon::register_command(QAction* action, int tab, const QString& group) {
  if (std::none_of(commands_.begin(), commands_.end(),
                   [action](const auto& command) { return command.action == action; }))
    commands_.push_back({action, tab, group});
}

void ShellRibbon::add_file_action(QAction* action) {
  apply_gui_action_icon(*action);
  file_menu_->addAction(action);
  register_command(action, -1, QStringLiteral("File"));
  quick_access_->show();
}

QToolButton* ShellRibbon::add_quick_action(QAction* action) {
  apply_gui_action_icon(*action);
  auto* button = new QToolButton(quick_access_);
  button->setObjectName(QStringLiteral("blcad.quick.") + action->objectName());
  button->setDefaultAction(action);
  button->setIconSize(QSize(24, 24));
  button->setToolButtonStyle(action->icon().isNull() ? Qt::ToolButtonTextOnly
                                                     : Qt::ToolButtonIconOnly);
  button->setAccessibleName(action->text());
  quick_actions_->insertWidget(quick_actions_->count() - 2, button);
  register_command(action, -1, QStringLiteral("Quick Access"));
  quick_access_->show();
  return button;
}

void ShellRibbon::show_command_search() {
  QDialog dialog(window());
  dialog.setObjectName(QStringLiteral("blcad.command_search"));
  dialog.setWindowTitle(QStringLiteral("Find Command"));
  dialog.resize(640, 440);
  auto* column = new QVBoxLayout(&dialog);
  auto* query = new QLineEdit(&dialog);
  query->setObjectName(QStringLiteral("blcad.command_search.query"));
  query->setPlaceholderText(QStringLiteral("Search commands or groups…"));
  query->setAccessibleName(QStringLiteral("Search commands"));
  column->addWidget(query);
  auto* results = new QTreeWidget(&dialog);
  results->setObjectName(QStringLiteral("blcad.command_search.results"));
  results->setRootIsDecorated(false);
  results->setHeaderLabels(
      {QStringLiteral("Command"), QStringLiteral("Location"), QStringLiteral("Shortcut")});
  results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  results->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  column->addWidget(results, 1);
  auto* hint = new QLabel(&dialog);
  hint->setWordWrap(true);
  hint->setMinimumHeight(48);
  column->addWidget(hint);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  // Replace Qt's colored standard-button icons with the existing desktop catalog.
  for (auto* button : buttons->buttons())
    button->setIcon(QIcon{});
  apply_gui_dialog_icons(*buttons);
  auto* run = buttons->button(QDialogButtonBox::Ok);
  run->setText(QStringLiteral("Run Command"));
  column->addWidget(buttons);
  const auto available = [this](const Command& command) {
    return command.action && command.action->isVisible() && command.action->isEnabled() &&
           (command.tab < 0 || tab_enabled(command.tab));
  };
  const auto update_hint = [&] {
    auto* item = results->currentItem();
    if (!item) {
      run->setEnabled(false);
      hint->setText(QStringLiteral("No matching commands. Try a different name or group."));
      return;
    }
    const auto& command = commands_.at(item->data(0, Qt::UserRole).toULongLong());
    run->setEnabled(available(command));
    hint->setText((available(command)
                       ? QString()
                       : QStringLiteral("Unavailable for the current selection or task. ")) +
                  (command.action ? command.action->toolTip() : QString()));
  };
  const auto rebuild = [&] {
    const auto words = query->text().simplified().split(' ', Qt::SkipEmptyParts);
    results->clear();
    QTreeWidgetItem* first_enabled = nullptr;
    for (std::size_t i = 0; i < commands_.size(); ++i) {
      const auto& command = commands_[i];
      if (!command.action || !command.action->isVisible())
        continue;
      const QString location = command.tab < 0
                                   ? command.group
                                   : tab_title(command.tab) + QStringLiteral(" / ") + command.group;
      const QString title = command.action->text().remove('&');
      const QString searchable = title + ' ' + location;
      if (!std::all_of(words.begin(), words.end(), [&](const auto& word) {
            return searchable.contains(word, Qt::CaseInsensitive);
          }))
        continue;
      auto* item = new QTreeWidgetItem(
          results,
          {title, location, command.action->shortcut().toString(QKeySequence::NativeText)});
      item->setIcon(0, command.action->icon());
      item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(i));
      if (!available(command))
        for (int col = 0; col < 3; ++col)
          item->setForeground(col, palette().color(QPalette::Disabled, QPalette::Text));
      else if (!first_enabled)
        first_enabled = item;
    }
    results->setCurrentItem(first_enabled ? first_enabled : results->topLevelItem(0));
    update_hint();
  };
  connect(query, &QLineEdit::textChanged, &dialog, rebuild);
  for (const auto& [key, direction] : {std::pair{Qt::Key_Down, 1}, std::pair{Qt::Key_Up, -1}}) {
    auto* navigation = new QShortcut(QKeySequence(key), &dialog);
    navigation->setContext(Qt::WidgetWithChildrenShortcut);
    connect(navigation, &QShortcut::activated, &dialog, [results, direction] {
      if (results->topLevelItemCount() == 0)
        return;
      const int row = std::clamp(results->indexOfTopLevelItem(results->currentItem()) + direction,
                                 0, results->topLevelItemCount() - 1);
      results->setCurrentItem(results->topLevelItem(row));
      results->scrollToItem(results->currentItem());
    });
  }
  connect(results, &QTreeWidget::currentItemChanged, &dialog, update_hint);
  for (const auto& command : commands_)
    if (command.action)
      connect(command.action, &QAction::changed, &dialog, rebuild);
  const auto accept = [&] {
    if (run->isEnabled())
      dialog.accept();
  };
  connect(results, &QTreeWidget::itemActivated, &dialog, accept);
  connect(query, &QLineEdit::returnPressed, &dialog, accept);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  rebuild();
  query->setFocus();
  if (dialog.exec() != QDialog::Accepted || !results->currentItem())
    return;
  const auto command = commands_.at(results->currentItem()->data(0, Qt::UserRole).toULongLong());
  if (!available(command))
    return;
  // Close the modal chooser before the owning command opens its task/dialog.
  if (command.tab >= 0)
    set_current_tab(command.tab);
  command.action->trigger();
}

} // namespace blcad::gui
