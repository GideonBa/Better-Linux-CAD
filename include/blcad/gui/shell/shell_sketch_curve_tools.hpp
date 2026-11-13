#pragma once
#include "blcad/core/result.hpp"
#include <QObject>
#include <memory>
class QAction;
namespace blcad::gui {
class ShellWindow;
class ShellSketchTools;
class GuiSketchWorkspace;
class ShellSketchCurveTools final : public QObject {
public:
  ShellSketchCurveTools(ShellWindow&, ShellSketchTools&, GuiSketchWorkspace&);
  ~ShellSketchCurveTools() override;
  [[nodiscard]] QAction& equation_action() const;
  [[nodiscard]] QAction& edit_action() const;
  [[nodiscard]] QAction& smooth_action() const;
  [[nodiscard]] Result<std::size_t> begin_equation(bool edit = false);
  [[nodiscard]] Result<std::size_t> begin_smooth();
  [[nodiscard]] Result<std::size_t> preview();
  [[nodiscard]] Result<std::size_t> apply();
  [[nodiscard]] bool active() const noexcept;
  void cancel();
  void refresh();

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
