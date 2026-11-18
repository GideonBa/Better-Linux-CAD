#pragma once
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include <QObject>
#include <memory>
class QWidget;
namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
class GuiViewportManipulatorLayer;
struct GuiSelection;
class ShellDirectEditTools final : public QObject {
public:
  enum class Mode { Move, Rotate, Offset, Size, DeleteHeal };
  ShellDirectEditTools(ShellWindow&, ShellRibbon&, int);
  ~ShellDirectEditTools() override;
  Result<std::size_t> begin(Mode = Mode::Move);
  Result<std::size_t> begin_edit(FeatureId);
  Result<std::size_t> preview();
  Result<std::size_t> apply();
  void cancel();
  void refresh();
  bool active() const noexcept;
  bool accept_selection(const GuiSelection&);
  QWidget* field(const QString&) const;
  GuiViewportManipulatorLayer& manipulators();
  Result<std::size_t> set_drag_text(const QString&);

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
