#pragma once

#include "blcad/core/result.hpp"
#include <QObject>
#include <memory>

namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
class ShellPhysicalPropertiesTools final : public QObject {
public:
  ShellPhysicalPropertiesTools(ShellWindow&, ShellRibbon&, int inspect_tab);
  ~ShellPhysicalPropertiesTools() override;
  [[nodiscard]] Result<std::size_t> begin();
  [[nodiscard]] Result<std::size_t> update_query();
  [[nodiscard]] Result<std::size_t> copy();
  [[nodiscard]] bool active() const noexcept;
  void cancel();
  void refresh();
  void refresh_presentation();

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
