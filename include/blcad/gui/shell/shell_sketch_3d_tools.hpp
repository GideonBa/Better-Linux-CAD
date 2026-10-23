#pragma once
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch_3d.hpp"
#include <QObject>
#include <memory>

namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
class GuiViewportManipulatorLayer;
class ShellSketch3DTools final : public QObject {
public:
  ShellSketch3DTools(ShellWindow&, ShellRibbon&, int model_tab);
  ~ShellSketch3DTools() override;
  [[nodiscard]] Result<std::size_t> begin();
  [[nodiscard]] Result<std::size_t> begin_edit(Sketch3DId);
  [[nodiscard]] Result<std::size_t> preview();
  [[nodiscard]] Result<std::size_t> apply();
  [[nodiscard]] bool active() const noexcept;
  void cancel();
  void refresh();
  void refresh_presentation();
  [[nodiscard]] const GuiViewportManipulatorLayer& manipulators() const noexcept;

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  [[nodiscard]] Result<std::size_t> open(Sketch3D, bool editing);
};
} // namespace blcad::gui
