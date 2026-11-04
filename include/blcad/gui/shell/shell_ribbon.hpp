#pragma once

#include <QPointer>
#include <QWidget>

#include <vector>

class QAction;
class QStackedWidget;
class QScrollArea;
class QResizeEvent;
class QTabBar;
class QToolButton;
class QHBoxLayout;
class QMenu;

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
  // Compact labeled button with an optional icon; two per column in the group.
  QToolButton* add_small_button(QWidget* group, QAction* action);
  void add_widget(QWidget* group, QWidget* widget);
  // Explicitly registered shared actions; their owners retain all command logic.
  void add_file_action(QAction* action);
  QToolButton* add_quick_action(QAction* action);
  void show_command_search();

  void set_current_tab(int index);
  [[nodiscard]] int current_tab() const;
  [[nodiscard]] int tab_count() const;
  [[nodiscard]] QString tab_title(int index) const;
  void set_tab_enabled(int index, bool enabled);
  [[nodiscard]] bool tab_enabled(int index) const;

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  void update_tab_navigation();
  void register_command(QAction* action, int tab, const QString& group);
  struct Command {
    QPointer<QAction> action;
    int tab;
    QString group;
  };
  std::vector<Command> commands_;
  QWidget* quick_access_{nullptr};
  QHBoxLayout* quick_actions_{nullptr};
  QMenu* file_menu_{nullptr};
  QTabBar* tabs_{nullptr};
  QScrollArea* tab_navigation_{nullptr};
  QWidget* tab_navigation_buttons_{nullptr};
  QToolButton* previous_tab_{nullptr};
  QToolButton* next_tab_{nullptr};
  QStackedWidget* pages_{nullptr};
};

} // namespace blcad::gui
