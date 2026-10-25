#pragma once

#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"

#include <QObject>
#include <QPoint>

#include <array>
#include <variant>

class QFrame;
class QToolButton;

namespace blcad::gui {
class ShellWindow;

// Bounded selection-first presentation. Verified preselection comes from the
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
                   ShellFeatureTools::PatternContext, ShellFeatureTools::BodyContext,
                   ShellFeatureTools::SurfaceContext, ShellAssemblyTools::Context>;
  using Command = std::variant<ShellFeatureTools::Command, ShellAssemblyTools::Command>;
  struct Entry {
    Command command{ShellFeatureTools::Command::None};
    QAction* action{nullptr};
    QToolButton* button{nullptr};
  };
  struct ConsumedSelection {
    Context context;
    std::vector<GuiSelection> entries;
    std::optional<GuiSelection> viewport;
    std::string task_id;
  };
  void begin(Command command);
  [[nodiscard]] QAction* source_action(Command command) const;
  [[nodiscard]] std::optional<Context> current_context(Command command) const;
  void place();
  ShellWindow& window_;
  QFrame* panel_{nullptr};
  std::array<Entry, 26> commands_{};
  QAction* cancel_{nullptr};
  QToolButton* cancel_button_{nullptr};
  QPoint anchor_{16, 16};
  std::optional<ConsumedSelection> consumed_;
};
} // namespace blcad::gui
