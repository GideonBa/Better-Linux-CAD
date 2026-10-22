#pragma once
class QWidget;
namespace blcad::gui {
class GuiViewportManipulatorLayer;
QWidget* make_shell_manipulator_overlay(QWidget* parent, const GuiViewportManipulatorLayer& layer);
} // namespace blcad::gui
