#pragma once

#include <QIcon>
#include <QStringView>

class QAbstractButton;
class QAction;
class QComboBox;
class QDialogButtonBox;
class QPushButton;
class QWidget;

namespace blcad::gui {

struct GuiBrowserNode;

// Curated local Pixelarticons only. Unsupported functions keep their labels.
[[nodiscard]] QIcon pixelart_icon(QStringView name);
[[nodiscard]] QIcon gui_icon_for_label(QStringView label);
[[nodiscard]] QIcon gui_icon_for_browser_node(const GuiBrowserNode& node);
void apply_gui_action_icon(QAction& action);
void apply_gui_button_icon(QAbstractButton& button);
void apply_gui_choice_icons(QComboBox& choices);
void apply_gui_dialog_icons(QDialogButtonBox& buttons);
[[nodiscard]] QPushButton* make_gui_button(const QString& label, QWidget* parent = nullptr);
[[nodiscard]] QPushButton* make_gui_button(QWidget* parent);

} // namespace blcad::gui
