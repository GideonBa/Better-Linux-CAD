#pragma once

#include "blcad/gui/shell/shell_feature_tools.hpp"

#include <QObject>
#include <QPoint>

#include <array>
#include <variant>

class QFrame;
class QToolButton;

namespace blcad::gui {
class ShellWindow;

// Bounded profile/finishing/pattern presentation. Verified preselection comes from the
// feature owner; existing command entry points own task/preview/commit state.
class ShellContextTools final : public QObject {
public:
  explicit ShellContextTools(ShellWindow&);
  ~ShellContextTools() override;
  void refresh();
  void reset();
  void finish(bool applied);

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  using Context =
      std::variant<ShellFeatureTools::ProfileContext, ShellFeatureTools::FinishingContext,
                   ShellFeatureTools::PatternContext>;
  struct Entry {
    ShellFeatureTools::Command command{ShellFeatureTools::Command::None};
    QAction* action{nullptr};
    QToolButton* button{nullptr};
  };
  struct ConsumedSelection {
    Context context;
    std::vector<GuiSelection> entries;
    std::optional<GuiSelection> viewport;
    std::string task_id;
  };
  void begin(ShellFeatureTools::Command command);
  [[nodiscard]] QAction* source_action(ShellFeatureTools::Command command) const;
  [[nodiscard]] std::optional<Context> current_context(ShellFeatureTools::Command command) const;
  void place();
  ShellWindow& window_;
  QFrame* panel_{nullptr};
  std::array<Entry, 11> commands_{};
  QAction* cancel_{nullptr};
  QToolButton* cancel_button_{nullptr};
  QPoint anchor_{16, 16};
  std::optional<ConsumedSelection> consumed_;
};
} // namespace blcad::gui
