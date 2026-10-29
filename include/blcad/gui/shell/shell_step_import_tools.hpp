#pragma once

#include "blcad/core/result.hpp"
#include <QObject>
#include <memory>

namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
struct GuiSelection;

// Owns source registration and disposable STEP geometry inspection.
class ShellStepImportTools final : public QObject {
public:
  ShellStepImportTools(ShellWindow&, ShellRibbon&, int model_tab, int assembly_tab);
  ~ShellStepImportTools() override;
  [[nodiscard]] Result<std::size_t> begin();
  [[nodiscard]] Result<std::size_t> inspect();
  [[nodiscard]] Result<std::size_t> inspect_geometry();
  [[nodiscard]] Result<std::size_t> prepare_topology();
  [[nodiscard]] Result<std::size_t> prepare_reference();
  [[nodiscard]] bool accept_selection(const GuiSelection&);
  void clear_selected_reference();
  [[nodiscard]] Result<std::size_t> preview();
  [[nodiscard]] Result<std::size_t> apply();
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] bool can_undo_after_cancel() const noexcept;
  [[nodiscard]] bool can_redo_after_cancel() const noexcept;
  void cancel();
  void refresh();

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
