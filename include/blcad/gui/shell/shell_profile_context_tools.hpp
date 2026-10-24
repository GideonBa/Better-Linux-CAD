#pragma once

#include "blcad/gui/shell/shell_feature_tools.hpp"

#include <QObject>
#include <QPoint>

#include <array>

class QFrame;
class QToolButton;

namespace blcad::gui {
class ShellWindow;

// Bounded profile-context presentation. Typed preselection comes from the
// feature owner; existing command entry points own task/preview/commit state.
class ShellProfileContextTools final : public QObject {
public:
  explicit ShellProfileContextTools(ShellWindow&);
  ~ShellProfileContextTools() override;
  void refresh();
  void reset();
  void finish(bool applied);

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct ConsumedSelection {
    ShellFeatureTools::ProfileContext context;
    std::vector<GuiSelection> entries;
    std::optional<GuiSelection> viewport;
    std::string task_id;
  };
  void begin(ShellFeatureTools::Command command);
  void place();
  ShellWindow& window_;
  QFrame* panel_{nullptr};
  QAction* extrude_{nullptr};
  QAction* revolve_{nullptr};
  QAction* sweep_{nullptr};
  QAction* loft_{nullptr};
  QAction* cancel_{nullptr};
  std::array<QToolButton*, 5> buttons_{};
  QPoint anchor_{16, 16};
  std::optional<ConsumedSelection> consumed_;
};
} // namespace blcad::gui
