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
class ShellSketchDrivenTools final : public QObject {
public:
  enum class Mode { Rib, Web, Emboss, Engrave };
  ShellSketchDrivenTools(ShellWindow&, ShellRibbon&, int);
  ~ShellSketchDrivenTools() override;
  Result<std::size_t> begin(Mode);
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
  Result<std::size_t> edit_source_sketch();
  void resume_source_sketch(SketchId);
  bool editing_source_sketch() const noexcept;

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
