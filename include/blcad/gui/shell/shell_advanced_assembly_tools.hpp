#pragma once
#include "blcad/core/assembly_representation.hpp"
#include <QObject>
#include <memory>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QTableWidget;
namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
class ShellAdvancedAssemblyTools final : public QObject {
public:
  enum class Mode { Flexible, Adaptive, AssemblyHole, Representations, Context };
  ShellAdvancedAssemblyTools(ShellWindow&, ShellRibbon&, int assembly_tab);
  ~ShellAdvancedAssemblyTools() override;
  [[nodiscard]] Result<std::size_t> begin(Mode);
  [[nodiscard]] Result<std::size_t> preview();
  [[nodiscard]] Result<std::size_t> apply();
  [[nodiscard]] Result<std::size_t> store_position(AssemblyPositionId, std::string);
  [[nodiscard]] Result<std::size_t> recall_position(AssemblyPositionId);
  [[nodiscard]] Result<std::size_t> recall_design_view(AssemblyDesignViewId);
  [[nodiscard]] QAction& action(Mode) noexcept;
  [[nodiscard]] QComboBox& occurrence_editor() noexcept;
  [[nodiscard]] QComboBox& flexibility_editor() noexcept;
  [[nodiscard]] QComboBox& joint_editor() noexcept;
  [[nodiscard]] QComboBox& coordinate_editor() noexcept;
  [[nodiscard]] QDoubleSpinBox& coordinate_value_editor() noexcept;
  [[nodiscard]] QComboBox& source_editor() noexcept;
  [[nodiscard]] QComboBox& source_geometry_editor() noexcept;
  [[nodiscard]] QComboBox& target_editor() noexcept;
  [[nodiscard]] QComboBox& target_construction_editor() noexcept;
  [[nodiscard]] QComboBox& layout_editor() noexcept;
  [[nodiscard]] QTableWidget& participant_editor() noexcept;
  [[nodiscard]] QComboBox& position_editor() noexcept;
  [[nodiscard]] QComboBox& design_view_editor() noexcept;
  [[nodiscard]] QTableWidget& display_editor() noexcept;
  [[nodiscard]] QLineEdit& name_editor() noexcept;
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
