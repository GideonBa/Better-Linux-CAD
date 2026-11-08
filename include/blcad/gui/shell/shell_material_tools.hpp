#pragma once

#include "blcad/gui/gui_materials.hpp"
#include <QObject>
#include <memory>

class QAction;
namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
class ShellMaterialTools final : public QObject {
public:
  ShellMaterialTools(ShellWindow&, ShellRibbon&, int manage_tab);
  ~ShellMaterialTools() override;
  [[nodiscard]] Result<std::size_t> begin(std::optional<DocumentId> part = {},
                                          std::optional<BodyId> body = {});
  [[nodiscard]] Result<std::size_t> stage_assignment();
  [[nodiscard]] Result<std::size_t> stage_snapshot(bool adopt = false);
  [[nodiscard]] Result<std::size_t> remove_snapshot();
  [[nodiscard]] Result<std::size_t> apply();
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] QAction& action() noexcept;
  void cancel();
  void refresh();

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
