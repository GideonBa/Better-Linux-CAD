#include "blcad/gui/shell/shell_profile_context_tools.hpp"

#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QToolButton>

#include <algorithm>

namespace blcad::gui {
ShellProfileContextTools::ShellProfileContextTools(ShellWindow& window)
    : QObject(&window), window_(window) {
  panel_ = new QFrame(&window.viewport());
  panel_->setObjectName(QStringLiteral("blcad.context.toolbar"));
  panel_->setFrameShape(QFrame::StyledPanel);
  panel_->setAutoFillBackground(true);
  auto* row = new QHBoxLayout(panel_);
  row->setContentsMargins(6, 4, 6, 4);
  row->setSpacing(4);
  const auto action = [&](const char* id, const char* label) {
    auto* result = new QAction(QString::fromUtf8(label), this);
    result->setObjectName(QStringLiteral("blcad.action.context_") + QString::fromLatin1(id));
    auto* button = new QToolButton(panel_);
    button->setObjectName(QStringLiteral("blcad.context.") + QString::fromLatin1(id));
    button->setDefaultAction(result);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    buttons_.at(static_cast<std::size_t>(row->count())) = button;
    row->addWidget(button);
    return result;
  };
  extrude_ = action("extrude", "Extrusion");
  revolve_ = action("revolve", "Drehung");
  cancel_ = action("cancel", "Abbrechen");
  connect(extrude_, &QAction::triggered, this, [this] { begin(false); });
  connect(revolve_, &QAction::triggered, this, [this] { begin(true); });
  connect(cancel_, &QAction::triggered, this, [this] {
    if (cancel_->isEnabled() && panel_->isVisible() && consumed_ &&
        window_.session().task().active() &&
        window_.session().task().command_id() == consumed_->task_id &&
        window_.feature_tools().active())
      window_.feature_tools().cancel();
  });
  window.viewport().installEventFilter(this);
  panel_->hide();
  refresh();
}

ShellProfileContextTools::~ShellProfileContextTools() {
  delete panel_;
}

void ShellProfileContextTools::begin(bool revolve) {
  refresh();
  auto* command = revolve ? revolve_ : extrude_;
  const auto context = window_.feature_tools().profile_context();
  if (!panel_->isVisible() || !command->isEnabled() || !context)
    return;
  ConsumedSelection previous{*context,
                             window_.session().selection().entries(),
                             window_.viewport().selected_semantic(),
                             {}};
  auto& source =
      revolve ? window_.feature_tools().revolve_action() : window_.feature_tools().extrude_action();
  source.trigger();
  if (window_.feature_tools().active() && window_.session().task().active()) {
    previous.task_id = std::string(window_.session().task().command_id());
    consumed_ = std::move(previous);
    window_.session().selection().clear();
    window_.viewport().clear_selection();
  }
  refresh();
}

void ShellProfileContextTools::finish(bool applied) {
  if (!consumed_)
    return;
  auto previous = std::move(*consumed_);
  consumed_.reset();
  if (!applied && !window_.session().task().active()) {
    const bool restored = previous.context.profile
                              ? window_.feature_tools().select_profile(previous.context.sketch,
                                                                       *previous.context.profile)
                              : window_.feature_tools().select_open_line(previous.context.sketch);
    if (restored) {
      if (previous.viewport)
        (void)window_.viewport().select_semantic(previous.viewport->semantic_id);
      else
        window_.viewport().clear_selection();
      // The selection callback can clear a browser-only handoff. Publish the
      // verified typed fallback after restoring the authoritative viewport.
      if (previous.context.profile)
        (void)window_.feature_tools().select_profile(previous.context.sketch,
                                                     *previous.context.profile);
      else
        (void)window_.feature_tools().select_open_line(previous.context.sketch);
      window_.session().selection().clear();
      for (auto& selection : previous.entries)
        (void)window_.session().selection().add(std::move(selection));
    }
  }
  refresh();
}

void ShellProfileContextTools::reset() {
  consumed_.reset();
  anchor_ = {16, 16};
  panel_->hide();
}

void ShellProfileContextTools::refresh() {
  const bool guarded = !window_.session().has_document() || window_.active_sketch() ||
                       window_.viewport().sketch_focus_active() ||
                       window_.session().reference_repair_mode() ||
                       QWidget::mouseGrabber() == &window_.viewport();
  const auto context = !guarded ? window_.feature_tools().profile_context() : std::nullopt;
  const bool idle = context && !window_.session().task().active();
  const bool cancelling = !guarded && consumed_ && window_.session().task().active() &&
                          window_.session().task().command_id() == consumed_->task_id &&
                          window_.feature_tools().active();
  const bool extrude = idle && window_.feature_tools().extrude_action().isEnabled();
  const bool revolve =
      idle && context->profile && window_.feature_tools().revolve_action().isEnabled();
  // Hidden QActions are effectively disabled. Derive visibility from owner
  // eligibility rather than the wrapper's previous effective enabled state.
  extrude_->setVisible(extrude);
  revolve_->setVisible(revolve);
  cancel_->setVisible(cancelling);
  extrude_->setEnabled(extrude);
  revolve_->setEnabled(revolve);
  cancel_->setEnabled(cancelling);
  buttons_[0]->setVisible(extrude_->isVisible());
  buttons_[1]->setVisible(revolve_->isVisible());
  buttons_[2]->setVisible(cancel_->isVisible());
  if (!extrude_->isVisible() && !revolve_->isVisible() && !cancel_->isVisible()) {
    panel_->hide();
    return;
  }
  place();
}

void ShellProfileContextTools::place() {
  const auto size = panel_->sizeHint();
  const auto viewport = window_.viewport().rect();
  if (size.width() + 16 > viewport.width() || size.height() + 16 > viewport.height()) {
    panel_->hide();
    return;
  }
  panel_->setGeometry(std::clamp(anchor_.x() + 12, 8, viewport.width() - size.width() - 8),
                      std::clamp(anchor_.y() + 12, 8, viewport.height() - size.height() - 8),
                      size.width(), size.height());
  panel_->show();
  panel_->raise();
}

bool ShellProfileContextTools::eventFilter(QObject* object, QEvent* event) {
  if (object == &window_.viewport()) {
    if (event->type() == QEvent::MouseButtonPress) {
      const auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton)
        anchor_ = mouse->position().toPoint();
    } else if (event->type() == QEvent::Resize)
      refresh();
  }
  return QObject::eventFilter(object, event);
}
} // namespace blcad::gui
