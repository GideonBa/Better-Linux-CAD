#include "blcad/gui/shell/shell_context_tools.hpp"
#include "blcad/gui/gui_icons.hpp"

#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QToolButton>

#include <algorithm>
#include <tuple>

namespace blcad::gui {
using FeatureCommand = ShellFeatureTools::Command;
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
    apply_gui_action_icon(*result);
    auto* button = new QToolButton(panel_);
    button->setObjectName(QStringLiteral("blcad.context.") + QString::fromLatin1(id));
    button->setDefaultAction(result);
    button->setToolButtonStyle(result->icon().isNull() ? Qt::ToolButtonTextOnly
                                                       : Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(24, 24));
    button->setToolTip(result->text());
    button->setAccessibleName(result->text());
    row->addWidget(button);
    return std::pair{result, button};
  };
  std::size_t index = 0;
  for (const auto& [kind, id, label] :
       {std::tuple{FeatureCommand::Extrude, "extrude", "Extrude"},
        std::tuple{FeatureCommand::Revolve, "revolve", "Revolve"},
        std::tuple{FeatureCommand::Sweep, "sweep", "Sweep"},
        std::tuple{FeatureCommand::Loft, "loft", "Loft"},
        std::tuple{FeatureCommand::Fillet, "fillet", "Fillet"},
        std::tuple{FeatureCommand::Chamfer, "chamfer", "Chamfer"},
        std::tuple{FeatureCommand::Shell, "shell", "Shell"},
        std::tuple{FeatureCommand::Draft, "draft", "Draft"},
        std::tuple{FeatureCommand::LinearPattern, "linear_pattern", "Linear Pattern"},
        std::tuple{FeatureCommand::CircularPattern, "circular_pattern", "Circular Pattern"},
        std::tuple{FeatureCommand::BodyMirror, "body_mirror", "Mirror"},
        std::tuple{FeatureCommand::BodyBoolean, "body_boolean", "Combine"},
        std::tuple{FeatureCommand::BodyTranslate, "body_translate", "Move"},
        std::tuple{FeatureCommand::BodyRotate, "body_rotate", "Rotate"},
        std::tuple{FeatureCommand::BodyScale, "body_scale", "Scale"},
        std::tuple{FeatureCommand::BoundarySurface, "surface_boundary", "Boundary Surface"},
        std::tuple{FeatureCommand::FillSurface, "surface_fill", "Fill Surface"},
        std::tuple{FeatureCommand::TrimSurface, "surface_trim", "Trim"},
        std::tuple{FeatureCommand::ExtendSurface, "surface_extend", "Extend"},
        std::tuple{FeatureCommand::SurfaceStitch, "surface_stitch", "Stitch"},
        std::tuple{FeatureCommand::ClosedShellToSolid, "surface_to_solid", "Solid"}}) {
    const auto [wrapper, button] = action(id, label);
    commands_.at(index++) = {kind, wrapper, button};
    connect(wrapper, &QAction::triggered, this, [this, kind] { begin(kind); });
  }
  using AssemblyCommand = ShellAssemblyTools::Command;
  for (const auto& [kind, id, label] :
       {std::tuple{AssemblyCommand::Placement, "assembly_placement", "Placement"},
        std::tuple{AssemblyCommand::Relationship, "assembly_relationship", "Relationship"},
        std::tuple{AssemblyCommand::Joint, "assembly_joint", "Joint"},
        std::tuple{AssemblyCommand::Motion, "assembly_motion", "Move"},
        std::tuple{AssemblyCommand::Solve, "assembly_solve", "Solve Group"}}) {
    const auto [wrapper, button] = action(id, label);
    commands_.at(index++) = {kind, wrapper, button};
    connect(wrapper, &QAction::triggered, this, [this, kind] { begin(kind); });
  }
  std::tie(cancel_, cancel_button_) = action("cancel", "Cancel");
  connect(cancel_, &QAction::triggered, this, [this] {
    if (cancel_->isEnabled() && panel_->isVisible() && consumed_ &&
        window_.session().task().active() &&
        window_.session().task().command_id() == consumed_->task_id &&
        (window_.feature_tools().active() || window_.assembly_tools().active())) {
      if (std::holds_alternative<ShellAssemblyTools::Context>(consumed_->context))
        window_.assembly_tools().cancel();
      else
        window_.feature_tools().cancel();
    }
  });
  window.viewport().installEventFilter(this);
  panel_->hide();
  refresh();
}

ShellContextTools::~ShellContextTools() {
  delete panel_;
}

QAction* ShellContextTools::source_action(Command command) const {
  if (const auto* assembly = std::get_if<ShellAssemblyTools::Command>(&command))
    return window_.assembly_tools().context_action(*assembly);
  const auto kind = std::get<ShellFeatureTools::Command>(command);
  auto& owner = window_.feature_tools();
  if (auto* pattern = owner.pattern_action(kind))
    return pattern;
  if (auto* body = owner.body_action(kind))
    return body;
  if (auto* surface = owner.surface_action(kind))
    return surface;
  return kind == FeatureCommand::Extrude   ? &owner.extrude_action()
         : kind == FeatureCommand::Revolve ? &owner.revolve_action()
         : kind == FeatureCommand::Sweep   ? &owner.sweep_action()
         : kind == FeatureCommand::Loft    ? &owner.loft_action()
                                           : owner.finishing_action(kind);
}

std::optional<ShellContextTools::Context>
ShellContextTools::current_context(Command command) const {
  if (const auto* assembly = std::get_if<ShellAssemblyTools::Command>(&command)) {
    const auto context = window_.assembly_tools().context(*assembly);
    return context ? std::optional<Context>(*context) : std::nullopt;
  }
  const auto kind = std::get<ShellFeatureTools::Command>(command);
  const auto& owner = window_.feature_tools();
  if (kind == FeatureCommand::Extrude || kind == FeatureCommand::Revolve ||
      kind == FeatureCommand::Sweep || kind == FeatureCommand::Loft) {
    const auto profile = owner.profile_context();
    if (profile &&
        (kind == FeatureCommand::Extrude || (kind == FeatureCommand::Revolve && profile->profile) ||
         ((kind == FeatureCommand::Sweep || kind == FeatureCommand::Loft) &&
          profile->materialized)))
      return *profile;
  } else if (const auto finishing = owner.finishing_context(kind))
    return *finishing;
  if (owner.pattern_action(kind))
    if (const auto pattern = owner.pattern_context())
      return *pattern;
  if (const auto body = owner.body_context(kind))
    return *body;
  if (const auto surface = owner.surface_context(kind))
    return *surface;
  return std::nullopt;
}

void ShellContextTools::begin(Command command) {
  refresh();
  auto& owner = window_.feature_tools();
  const auto entry = std::find_if(commands_.begin(), commands_.end(), [command](const auto& entry) {
    return entry.command == command;
  });
  const auto context = current_context(command);
  auto* source = source_action(command);
  if (!panel_->isVisible() || entry == commands_.end() || !entry->action->isEnabled() || !source ||
      !source->isEnabled() || !context)
    return;
  ConsumedSelection previous{*context,
                             window_.session().selection().entries(),
                             window_.viewport().selected_semantic(),
                             {}};
  source->trigger();
  const auto* assembly = std::get_if<ShellAssemblyTools::Command>(&command);
  const auto kind =
      assembly ? ShellFeatureTools::Command::None : std::get<ShellFeatureTools::Command>(command);
  const bool active = assembly ? window_.assembly_tools().active() &&
                                     window_.assembly_tools().command() == *assembly
                               : owner.active() && owner.command() == kind;
  if (active && window_.session().task().active()) {
    previous.task_id = std::string(window_.session().task().command_id());
    consumed_ = std::move(previous);
    bool seeded = true;
    if (assembly)
      seeded =
          window_.assembly_tools().select_context(std::get<ShellAssemblyTools::Context>(*context));
    else if (const auto* profile = std::get_if<ShellFeatureTools::ProfileContext>(&*context)) {
      if (kind == FeatureCommand::Sweep || kind == FeatureCommand::Loft)
        seeded =
            profile->profile && owner.select_spatial_profile(profile->sketch, *profile->profile);
    } else if (const auto* finishing = std::get_if<ShellFeatureTools::FinishingContext>(&*context))
      seeded = owner.select_finishing_context(*finishing);
    else if (const auto* pattern = std::get_if<ShellFeatureTools::PatternContext>(&*context))
      seeded = owner.select_pattern_context(*pattern);
    else if (const auto* body = std::get_if<ShellFeatureTools::BodyContext>(&*context))
      seeded = owner.select_body_context(*body);
    else
      seeded = owner.select_surface_context(std::get<ShellFeatureTools::SurfaceContext>(*context));
    if (!seeded) {
      if (assembly)
        window_.assembly_tools().cancel();
      else
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
    const auto* finishing = std::get_if<ShellFeatureTools::FinishingContext>(&previous.context);
    const auto* pattern = std::get_if<ShellFeatureTools::PatternContext>(&previous.context);
    const auto* body = std::get_if<ShellFeatureTools::BodyContext>(&previous.context);
    const auto* assembly = std::get_if<ShellAssemblyTools::Context>(&previous.context);
    const bool restored =
        assembly    ? window_.assembly_tools().context_available(*assembly)
        : profile   ? restore_profile()
        : finishing ? window_.feature_tools().finishing_context_available(*finishing)
        : pattern   ? window_.feature_tools().pattern_context_available(*pattern)
        : body      ? window_.feature_tools().body_context_available(*body)
                    : window_.feature_tools().surface_context_available(
                     std::get<ShellFeatureTools::SurfaceContext>(previous.context));
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
                          (window_.feature_tools().active() || window_.assembly_tools().active());
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
