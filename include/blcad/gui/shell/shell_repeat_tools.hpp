#pragma once

#include "blcad/gui/gui_types.hpp"

#include <QAction>
#include <QObject>
#include <QPointer>

class QWidget;
class QToolButton;

namespace blcad::gui {
class ShellRibbon;
class ShellWindow;

// Receives accepted command actions explicitly from their owners. Repeating
// invokes the ordinary command entry point with current inputs, never a saved
// task, parameter set or selection. No action discovery or trigger rewiring.
class ShellRepeatTools final : public QObject {
public:
  ShellRepeatTools(ShellWindow&, ShellRibbon&, QWidget* edit_group);
  void remember(QAction& accepted_command);
  void reset();
  void refresh();

private:
  [[nodiscard]] bool available() const;
  ShellWindow& window_;
  QAction* repeat_{nullptr};
  QToolButton* button_{nullptr};
  QPointer<QAction> command_;
  QMetaObject::Connection availability_connection_;
  GuiDocumentKind document_kind_{GuiDocumentKind::None};
};
} // namespace blcad::gui
