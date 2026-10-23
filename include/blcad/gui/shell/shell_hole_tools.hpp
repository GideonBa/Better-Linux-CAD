#pragma once

#include "blcad/core/result.hpp"
#include <QObject>
#include <memory>

namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
struct GuiSelection;

class ShellHoleTools final : public QObject {
public:
  ShellHoleTools(ShellWindow&, ShellRibbon&, int model_tab);
  ~ShellHoleTools() override;
  [[nodiscard]] Result<std::size_t> begin();
  [[nodiscard]] Result<std::size_t> preview();
  [[nodiscard]] Result<std::size_t> apply();
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] bool accept_selection(const GuiSelection&);
  void cancel();
  void refresh();

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
