#include "blcad/gui/shell/shell_context_tools.hpp"

#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QToolButton>

#include <algorithm>
#include <tuple>

namespace blcad::gui {
using Command = ShellFeatureTools::Command;
ShellContextTools::ShellContextTools(ShellWindow& window) : QObject(&window), window_(window) {
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
    row->addWidget(button);
    return std::pair{result, button};
  };
  std::size_t index = 0;
  for (const auto& [kind, id, label] :
       {std::tuple{Command::Extrude, "extrude", "Extrusion"},
        std::tuple{Command::Revolve, "revolve", "Drehung"},
        std::tuple{Command::Sweep, "sweep", "Sweep"}, std::tuple{Command::Loft, "loft", "Loft"},
        std::tuple{Command::Fillet, "fillet", "Verrunden"},
        std::tuple{Command::Chamfer, "chamfer", "Fase"},
        std::tuple{Command::Shell, "shell", "Aushöhlen"},
        std::tuple{Command::Draft, "draft", "Formschräge"}}) {
    const auto [wrapper, button] = action(id, label);
    commands_.at(index++) = {kind, wrapper, button};
    connect(wrapper, &QAction::triggered, this, [this, kind] { begin(kind); });
  }
  std::tie(cancel_, cancel_button_) = action("cancel", "Abbrechen");
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

ShellContextTools::~ShellContextTools() {
  delete panel_;
}

QAction* ShellContextTools::source_action(Command kind) const {
  auto& owner = window_.feature_tools();
  return kind == Command::Extrude   ? &owner.extrude_action()
         : kind == Command::Revolve ? &owner.revolve_action()
         : kind == Command::Sweep   ? &owner.sweep_action()
         : kind == Command::Loft    ? &owner.loft_action()
                                    : owner.finishing_action(kind);
}

std::optional<ShellContextTools::Context> ShellContextTools::current_context(Command kind) const {
  const auto& owner = window_.feature_tools();
  if (kind == Command::Extrude || kind == Command::Revolve || kind == Command::Sweep ||
      kind == Command::Loft) {
    const auto profile = owner.profile_context();
    if (profile && (kind == Command::Extrude || (kind == Command::Revolve && profile->profile) ||
                    ((kind == Command::Sweep || kind == Command::Loft) && profile->materialized)))
      return *profile;
  } else if (const auto finishing = owner.finishing_context(kind))
    return *finishing;
  return std::nullopt;
}

void ShellContextTools::begin(Command kind) {
  refresh();
  auto& owner = window_.feature_tools();
  const auto entry = std::find_if(commands_.begin(), commands_.end(),
                                  [kind](const auto& command) { return command.command == kind; });
  const auto context = current_context(kind);
  auto* source = source_action(kind);
  if (!panel_->isVisible() || entry == commands_.end() || !entry->action->isEnabled() || !source ||
      !source->isEnabled() || !context)
    return;
  ConsumedSelection previous{*context,
                             window_.session().selection().entries(),
                             window_.viewport().selected_semantic(),
                             {}};
  source->trigger();
  if (owner.active() && owner.command() == kind && window_.session().task().active()) {
    previous.task_id = std::string(window_.session().task().command_id());
    consumed_ = std::move(previous);
    bool seeded = true;
    if (const auto* profile = std::get_if<ShellFeatureTools::ProfileContext>(&*context)) {
      if (kind == Command::Sweep || kind == Command::Loft)
        seeded =
            profile->profile && owner.select_spatial_profile(profile->sketch, *profile->profile);
    } else
      seeded =
          owner.select_finishing_context(std::get<ShellFeatureTools::FinishingContext>(*context));
    if (!seeded) {
      owner.cancel();
      return;
    }
    window_.session().selection().clear();
    window_.viewport().clear_selection();
  }
  refresh();
}

void ShellContextTools::finish(bool applied) {
  if (!consumed_)
    return;
  auto previous = std::move(*consumed_);
  consumed_.reset();
  if (!applied && !window_.session().task().active()) {
    const auto* profile = std::get_if<ShellFeatureTools::ProfileContext>(&previous.context);
    const auto restore_profile = [&] {
      return profile->profile
                 ? window_.feature_tools().select_profile(profile->sketch, *profile->profile)
                 : window_.feature_tools().select_open_line(profile->sketch);
    };
    const bool restored =
        profile ? restore_profile()
                : window_.feature_tools().finishing_context_available(
                      std::get<ShellFeatureTools::FinishingContext>(previous.context));
    if (restored) {
      if (previous.viewport)
        (void)window_.viewport().select_semantic(previous.viewport->semantic_id);
      else
        window_.viewport().clear_selection();
      // The selection callback can clear a browser-only handoff. Publish the
      // verified typed fallback after restoring the authoritative viewport.
      if (profile)
        (void)restore_profile();
      window_.session().selection().clear();
      for (auto& selection : previous.entries)
        (void)window_.session().selection().add(std::move(selection));
    }
  }
  refresh();
}

void ShellContextTools::reset() {
  consumed_.reset();
  anchor_ = {16, 16};
  panel_->hide();
}

void ShellContextTools::refresh() {
  const bool guarded = !window_.session().has_document() || window_.active_sketch() ||
                       window_.viewport().sketch_focus_active() ||
                       window_.session().reference_repair_mode() ||
                       QWidget::mouseGrabber() == &window_.viewport();
  const bool cancelling = !guarded && consumed_ && window_.session().task().active() &&
                          window_.session().task().command_id() == consumed_->task_id &&
                          window_.feature_tools().active();
  // Hidden QActions are effectively disabled. Derive visibility from owner
  // eligibility rather than the wrapper's previous effective enabled state.
  bool visible = cancelling;
  for (const auto& entry : commands_) {
    auto* source = source_action(entry.command);
    const bool enabled = !guarded && !window_.session().task().active() && source &&
                         source->isEnabled() && current_context(entry.command).has_value();
    entry.action->setVisible(enabled);
    entry.action->setEnabled(enabled);
    entry.button->setVisible(enabled);
    visible = visible || enabled;
  }
  cancel_->setVisible(cancelling);
  cancel_->setEnabled(cancelling);
  cancel_button_->setVisible(cancelling);
  if (!visible) {
    panel_->hide();
    return;
  }
  place();
}

void ShellContextTools::place() {
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

bool ShellContextTools::eventFilter(QObject* object, QEvent* event) {
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
