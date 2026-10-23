#pragma once

#include "blcad/gui/gui_feature_edit.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include <QObject>
#include <memory>

class QWidget;
namespace blcad::gui {
class ShellWindow;
class ShellFeatureEditTools final : public QObject {
public:
  explicit ShellFeatureEditTools(ShellWindow&);
  ~ShellFeatureEditTools() override;
  [[nodiscard]] Result<std::size_t> begin(FeatureId);
  [[nodiscard]] Result<std::size_t> apply();
  void cancel();
  void refresh();
  void refresh_presentation();
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] QWidget* field(const QString& path) const;
  [[nodiscard]] GuiViewportManipulatorLayer& manipulators();
  [[nodiscard]] Result<std::size_t> set_drag_text(const QString&);

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
