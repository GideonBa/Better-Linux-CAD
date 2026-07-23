#pragma once

#include <QWidget>

class QAction;
class QStackedWidget;
class QTabBar;
class QToolButton;

namespace blcad::gui {

// GUI Shell Reset MVP-9R Block 133: the Inventor-style ribbon — one tab bar
// over pages of grouped, labeled tool buttons. This is the shell's single
// command surface. ShellWindow constructs tabs, groups, and buttons directly
// and keeps the returned references; the ribbon never discovers anything via
// findChild and never installs itself anywhere.
class ShellRibbon final : public QWidget {
public:
  explicit ShellRibbon(QWidget* parent = nullptr);

  // Returns the tab index for use with add_group/set_current_tab.
  [[nodiscard]] int add_tab(const QString& title);
  // Returns the group container inside the tab page; pass it to add_button.
  [[nodiscard]] QWidget* add_group(int tab_index, const QString& title);
  QToolButton* add_button(QWidget* group, QAction* action);
  // Large button opening a popup menu of variant actions (Inventor-style
  // tool families, e.g. the rectangle variants).
  QToolButton* add_menu_button(QWidget* group, const QString& title,
                               const QList<QAction*>& actions);
  // Compact text button; stacked two per column inside the group.
  QToolButton* add_small_button(QWidget* group, QAction* action);

  void set_current_tab(int index);
  [[nodiscard]] int current_tab() const;
  [[nodiscard]] int tab_count() const;
  [[nodiscard]] QString tab_title(int index) const;
  void set_tab_enabled(int index, bool enabled);
  [[nodiscard]] bool tab_enabled(int index) const;

private:
  QTabBar* tabs_{nullptr};
  QStackedWidget* pages_{nullptr};
};

} // namespace blcad::gui
