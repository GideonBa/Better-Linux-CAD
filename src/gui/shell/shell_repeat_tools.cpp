#include "blcad/gui/shell/shell_repeat_tools.hpp"

#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QKeySequence>
#include <QToolButton>

#include <algorithm>

namespace blcad::gui {
ShellRepeatTools::ShellRepeatTools(ShellWindow& window, ShellRibbon& ribbon, QWidget* edit_group)
    : QObject(&window), window_(window) {
  repeat_ = new QAction(this);
  repeat_->setObjectName(QStringLiteral("blcad.action.repeat_last_command"));
  repeat_->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
  repeat_->setShortcutContext(Qt::WindowShortcut);
  window.addAction(repeat_);
  button_ = ribbon.add_button(edit_group, repeat_);
  button_->setObjectName(QStringLiteral("blcad.repeat.button"));
  connect(repeat_, &QAction::triggered, this, [this] {
    if (!available())
      return;
    command_->trigger();
    window_.refresh();
  });
  refresh();
}

bool ShellRepeatTools::available() const {
  return command_ && command_->isEnabled() && window_.session().has_document() &&
         window_.session().document_kind() == document_kind_ &&
         !window_.session().task().active() && !window_.session().reference_repair_mode() &&
         !window_.active_sketch() && !window_.viewport().sketch_focus_active() &&
         QWidget::mouseGrabber() != &window_.viewport();
}

void ShellRepeatTools::remember(QAction& accepted_command) {
  disconnect(availability_connection_);
  command_ = &accepted_command;
  document_kind_ = window_.session().document_kind();
  // Observe the explicitly supplied owner's enablement, including a Finish
  // Sketch handoff published after the shell's ordinary refresh. Its existing
  // triggered handler remains the sole command-start route.
  availability_connection_ =
      connect(&accepted_command, &QAction::changed, this, [this] { refresh(); });
  refresh();
}

void ShellRepeatTools::refresh() {
  repeat_->setText(command_ ? QStringLiteral("Erneut: %1").arg(command_->text())
                            : QStringLiteral("Befehl wiederholen"));
  repeat_->setToolTip(
      command_
          ? QStringLiteral("%1 mit aktuellen Eingaben neu starten (Strg+R)").arg(command_->text())
          : QStringLiteral("Zuerst einen Modellierungsbefehl anwenden"));
  repeat_->setEnabled(available());
  // Ribbon rows can shrink to button minimum widths. Track the changing
  // action label so the command name remains readable on both backends.
  button_->setMinimumWidth(std::max(56, button_->sizeHint().width()));
}

void ShellRepeatTools::reset() {
  disconnect(availability_connection_);
  command_.clear();
  document_kind_ = GuiDocumentKind::None;
  refresh();
}
} // namespace blcad::gui
