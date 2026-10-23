#pragma once

#include "blcad/core/result.hpp"
#include <QObject>
#include <memory>

namespace blcad::gui {
class ShellWindow;
class ShellRibbon;

class ShellParameterTools final : public QObject {
public:
  ShellParameterTools(ShellWindow&, ShellRibbon&, int model_tab);
  ~ShellParameterTools() override;
  [[nodiscard]] Result<std::size_t> begin();
  [[nodiscard]] Result<std::size_t> stage();
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
