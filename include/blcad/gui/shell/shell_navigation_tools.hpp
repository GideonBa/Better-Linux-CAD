#pragma once

#include "blcad/gui/gui_modeling_workspace.hpp"

#include <QObject>

class QAction;
class QComboBox;

namespace blcad::gui {
class ShellRibbon;
class ShellWindow;

// Shell-owned transient camera controls. Core intent and model history stay
// with the document; camera snapshots use the retained workspace authority.
class ShellNavigationTools final : public QObject {
public:
  ShellNavigationTools(ShellWindow& window, ShellRibbon& ribbon, int view_tab);
  void refresh();
  void reset();

private:
  [[nodiscard]] bool available() const;
  void reload_bookmarks(const QString& selected);
  void restored(bool success);

  ShellWindow& window_;
  GuiModelingWorkspace workspace_;
  std::vector<QAction*> views_;
  QAction* fit_{nullptr};
  QAction* home_{nullptr};
  QAction* capture_home_{nullptr};
  QAction* save_{nullptr};
  QAction* restore_{nullptr};
  QAction* remove_{nullptr};
  QComboBox* bookmarks_{nullptr};
  bool has_home_{false};
};
} // namespace blcad::gui
