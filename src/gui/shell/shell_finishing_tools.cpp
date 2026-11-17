#include "advanced_fillet_controls.hpp"
#include "advanced_surface_controls.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"

#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace blcad::gui {
namespace {
Result<std::vector<geometry::FinishingSelectionTarget>>
finishing_targets(const GuiDocumentSession& session, const BodyId& body, PartFeatureInputRole role,
                  const AdvancedFilletControls* advanced = nullptr) {
  const auto* document =
      advanced && advanced->input_document ? &*advanced->input_document : session.part_document();
  const auto* cache =
      advanced && advanced->input_cache ? &*advanced->input_cache : session.part_shape_cache();
  const auto prefix =
      advanced && advanced->input_cache ? advanced->input_prefix : session.history_prefix_count();
  return query_with_step_import_context(session, [&](auto* context) {
    return geometry::FinishingSelectionBuilder::build(
        *document, *cache, body, role, context, prefix,
        advanced && advanced->editing ? geometry::FinishingSelectionVisibility::IncludeHiddenInputs
                                      : geometry::FinishingSelectionVisibility::VisibleOnly);
  });
}
PartFeatureInputRole selection_role(ShellFeatureTools::Command command) {
  using C = ShellFeatureTools::Command;
  if (command == C::Fillet)
    return PartFeatureInputRole::FilletEdge;
  if (command == C::Chamfer)
    return PartFeatureInputRole::ChamferEdge;
  if (command == C::Shell)
    return PartFeatureInputRole::ShellRemovalFace;
  return PartFeatureInputRole::DraftFace;
}
QString target_label(const std::string& label) {
  const auto marker = label.rfind(" / ");
  if (marker == std::string::npos)
    return QString::fromStdString(label);
  const auto role = label.substr(marker + 3);
  for (const auto& [key, text] : {std::pair{"top_front", "Top Front"},
                                  {"top_back", "Top Back"},
                                  {"top_right", "Top Right"},
                                  {"top_left", "Top Left"},
                                  {"bottom_front", "Bottom Front"},
                                  {"bottom_back", "Bottom Back"},
                                  {"bottom_right", "Bottom Right"},
                                  {"bottom_left", "Bottom Left"},
                                  {"front_right", "Front Right"},
                                  {"front_left", "Front Left"},
                                  {"back_right", "Back Right"},
                                  {"back_left", "Back Left"},
                                  {"top", "Top"},
                                  {"bottom", "Bottom"},
                                  {"right", "Right"},
                                  {"left", "Left"},
                                  {"front", "Front"},
                                  {"back", "Back"}})
    if (role == key)
      return QString::fromStdString(label.substr(0, marker)) + QStringLiteral(" / ") +
             QString::fromUtf8(text);
  return QString::fromStdString(label);
}
void preserve_finishing_row_heights(QWidget& controls, QLayout& form) {
  // Preserve full row heights inside the resizable task scroll child, including
  // mode-dependent rows after switching between different finishing commands.
  form.invalidate();
  // Wrapped hint text needs more height than the unconstrained size hint.
  const int height =
      std::max(form.sizeHint().height(), form.totalHeightForWidth(controls.minimumWidth()));
  controls.setMinimumHeight(height);
  controls.updateGeometry();
}
QString command_title(ShellFeatureTools::Command command) {
  using C = ShellFeatureTools::Command;
  if (command == C::Fillet)
    return QStringLiteral("Fillet");
  if (command == C::Chamfer)
    return QStringLiteral("Chamfer");
  if (command == C::Shell)
    return QStringLiteral("Shell");
  return QStringLiteral("Draft");
}
} // namespace

bool ShellFeatureTools::finishing() const noexcept {
  return command_ == Command::Fillet || command_ == Command::Chamfer ||
         command_ == Command::Shell || command_ == Command::Draft;
}

QAction* ShellFeatureTools::finishing_action(Command command) const noexcept {
  for (const auto& [kind, action] : finishing_actions_)
    if (kind == command)
      return action;
  return nullptr;
}

std::uint32_t ShellFeatureTools::idle_finishing_selection_mask() const {
  if (active() || session_.task().active() || window_.active_sketch() ||
      session_.reference_repair_mode() || session_.document_kind() != GuiDocumentKind::Part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_shape_cache() ||
      !session_.part_modeling_ready())
    return 0;
  const auto mask = viewport_.selection_filter_mask();
  return mask == selection_kind_bit(GuiSelectionKind::Edge) ||
                 mask == selection_kind_bit(GuiSelectionKind::Face)
             ? mask
             : 0U;
}

std::optional<ShellFeatureTools::FinishingContext>
ShellFeatureTools::finishing_context(Command command) const {
  const bool edge = command == Command::Fillet || command == Command::Chamfer;
  if ((!edge && command != Command::Shell && command != Command::Draft) ||
      idle_finishing_selection_mask() !=
          selection_kind_bit(edge ? GuiSelectionKind::Edge : GuiSelectionKind::Face) ||
      session_.selection().size() != 1)
    return std::nullopt;
  const auto& selected = session_.selection().entries().front();
  if (selected.kind != (edge ? GuiSelectionKind::Edge : GuiSelectionKind::Face) ||
      !session_.selection().allows(selected.kind))
    return std::nullopt;
  const auto& scope = viewport_.selection_semantic_scope();
  if (scope && std::find(scope->begin(), scope->end(), selected.semantic_id) == scope->end())
    return std::nullopt;
  std::optional<FinishingContext> context;
  for (const auto& body : session_.part_document()->bodies()) {
    auto targets = finishing_targets(session_, body.id(), selection_role(command));
    if (targets)
      for (const auto& target : targets.value())
        if (target.semantic_id == selected.semantic_id) {
          if (context)
            return std::nullopt;
          context = FinishingContext{command, body.id(), target.semantic_id};
        }
  }
  return context;
}

bool ShellFeatureTools::finishing_context_available(const FinishingContext& context) const {
  if (!finishing_action(context.command) || !session_.part_document() ||
      !session_.part_shape_cache())
    return false;
  auto targets = finishing_targets(session_, context.body, selection_role(context.command));
  return targets &&
         std::count_if(targets.value().begin(), targets.value().end(), [&](const auto& target) {
           return target.semantic_id == context.semantic_id;
         }) == 1;
}

bool ShellFeatureTools::select_finishing_context(const FinishingContext& context) {
  if (!finishing() || command_ != context.command || !session_.task().active() ||
      session_.task().command_id() != command_id() || !finishing_context_available(context))
    return false;
  const int body = finishing_body_->findData(QString::fromStdString(context.body.value()));
  if (body < 0)
    return false;
  finishing_body_->setCurrentIndex(body);
  for (std::size_t i = 0; i < finishing_targets_.size(); ++i)
    if (finishing_targets_[i].semantic_id == context.semantic_id) {
      finishing_targets_list_->item(static_cast<int>(i))->setSelected(true);
      return true;
    }
  return false;
}
bool ShellFeatureTools::angular_value() const noexcept {
  return command_ == Command::Revolve || command_ == Command::Draft ||
         command_ == Command::CircularPattern || command_ == Command::BodyRotate ||
         command_ == Command::Sweep;
}
std::string_view ShellFeatureTools::command_id() const noexcept {
  switch (command_) {
  case Command::Extrude:
    return "part.extrude";
  case Command::Revolve:
    return "part.revolve";
  case Command::Fillet:
    return "part.fillet";
  case Command::Chamfer:
    return "part.chamfer";
  case Command::Shell:
    return "part.shell";
  case Command::Draft:
    return "part.draft";
  case Command::LinearPattern:
    return "part.linear_pattern";
  case Command::CircularPattern:
    return "part.circular_pattern";
  case Command::BodyMirror:
    return "part.mirror";
  case Command::BodyBoolean:
    return "part.body_boolean";
  case Command::BodyTranslate:
  case Command::BodyRotate:
  case Command::BodyScale:
    return "part.body_transform";
  case Command::PathCurve:
    return "part.path_curve";
  case Command::Sweep:
    return "part.sweep";
  case Command::Loft:
    return "part.loft";
  case Command::BoundarySurface:
  case Command::FillSurface:
    return "surface.boundary_fill";
  case Command::TrimSurface:
  case Command::ExtendSurface:
    return "surface.trim_extend";
  case Command::SurfaceStitch:
    return "surface.stitch";
  case Command::ClosedShellToSolid:
    return "surface.closed_shell_to_solid";
  case Command::OffsetSurface:
  case Command::Thicken:
    return "surface.offset_thicken";
  case Command::None:
    return {};
  }
  return {};
}

void ShellFeatureTools::build_finishing_controls(QWidget* panel, QFormLayout&) {
  finishing_controls_ = new QWidget(panel);
  auto* column = new QVBoxLayout(finishing_controls_);
  const auto add_row = [&](const QString& label, QWidget* input) {
    // Keep the label and editor in one explicit row. Hiding the row removes its
    // complete layout contribution; re-showing it retains the editor's height.
    auto* row = new QWidget(finishing_controls_);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(label, row));
    layout->addWidget(input, 1);
    column->addWidget(row);
  };
  finishing_controls_->setMinimumWidth(280);
  finishing_body_ = new QComboBox(finishing_controls_);
  finishing_body_->setObjectName(QStringLiteral("blcad.finishing.body"));
  add_row(QStringLiteral("Body"), finishing_body_);
  finishing_targets_list_ = new QListWidget(finishing_controls_);
  finishing_targets_list_->setObjectName(QStringLiteral("blcad.finishing.targets"));
  finishing_targets_list_->setSelectionMode(QAbstractItemView::MultiSelection);
  finishing_targets_list_->setMinimumHeight(170);
  column->addWidget(new QLabel(QStringLiteral("Select Elements"), finishing_controls_));
  column->addWidget(finishing_targets_list_);
  auto* hint = new QLabel(QStringLiteral("Click edges or faces in the model or list. "
                                         "Click again to remove them from the selection."),
                          finishing_controls_);
  hint->setWordWrap(true);
  column->addWidget(hint);
  finishing_mode_ = new QComboBox(finishing_controls_);
  finishing_mode_->setObjectName(QStringLiteral("blcad.finishing.mode"));
  add_row(QStringLiteral("Mode"), finishing_mode_);
  secondary_numeric_editor_ = new QLineEdit(finishing_controls_);
  secondary_numeric_editor_->setObjectName(QStringLiteral("blcad.finishing.secondary"));
  add_row(QStringLiteral("Second Size"), secondary_numeric_editor_);
  draft_axis_ = new QComboBox(finishing_controls_);
  draft_axis_->setObjectName(QStringLiteral("blcad.finishing.pull_axis"));
  add_row(QStringLiteral("Pull Direction"), draft_axis_);
  draft_plane_ = new QComboBox(finishing_controls_);
  draft_plane_->setObjectName(QStringLiteral("blcad.finishing.neutral_plane"));
  add_row(QStringLiteral("Neutral Plane"), draft_plane_);
  // Give each input its style-derived lower bound before computing scroll bounds.
  for (QWidget* input : std::array<QWidget*, 5>{
           finishing_body_, finishing_mode_, secondary_numeric_editor_, draft_axis_, draft_plane_})
    input->setMinimumHeight(input->minimumSizeHint().height());
  advanced_fillet_ = std::make_unique<AdvancedFilletControls>(
      finishing_controls_, finishing_mode_,
      [this] {
        if (command_ == Command::Fillet)
          refresh_finishing_preview();
      },
      [this] {
        if (command_ == Command::Fillet)
          reload_finishing_targets();
      });
  column->addWidget(advanced_fillet_->panel);
  advanced_fillet_->panel->hide();
  panel->layout()->addWidget(finishing_controls_);
  finishing_controls_->hide();
  connect(finishing_body_, &QComboBox::currentIndexChanged, this, [this] {
    if (finishing()) {
      if (command_ == Command::Fillet) {
        advanced_fillet_->body = BodyId(finishing_body_->currentData().toString().toStdString());
        advanced_fillet_->clear_inputs();
      }
      reload_finishing_targets();
    }
  });
  connect(finishing_targets_list_, &QListWidget::itemSelectionChanged, this, [this] {
    if (finishing()) {
      if (command_ == Command::Fillet) {
        std::vector<std::string> selected;
        for (int i = 0; i < finishing_targets_list_->count(); ++i)
          if (finishing_targets_list_->item(i)->isSelected())
            selected.push_back(finishing_targets_[i].semantic_id);
        auto captured = advanced_fillet_->capture(finishing_targets_, selected);
        if (!captured) {
          task_summary_->setText(QString::fromStdString(captured.error().message()));
          reload_finishing_targets(false);
          return;
        }
      }
      refresh_finishing_preview();
    }
  });
  connect(finishing_mode_, &QComboBox::currentIndexChanged, this, [this] {
    if (!finishing())
      return;
    if (command_ == Command::Fillet) {
      advanced_fillet_->remember_values();
      if (!advanced_fillet_->editing && finishing_mode_->currentIndex() == 0) {
        advanced_fillet_->input_document.reset();
        advanced_fillet_->input_cache.reset();
        advanced_fillet_->input_prefix.reset();
      }
      advanced_fillet_->configure();
      advanced_fillet_->panel->setVisible(typed_finishing());
      value_label_->setVisible(!typed_finishing());
      numeric_editor_->setVisible(!typed_finishing());
      preserve_finishing_row_heights(*finishing_controls_, *finishing_controls_->layout());
      reload_finishing_targets();
      return;
    }
    const bool second = command_ == Command::Chamfer && finishing_mode_->currentIndex() > 0;
    secondary_numeric_editor_->parentWidget()->setVisible(second);
    if (second) {
      finishing_second_value_ = finishing_mode_->currentIndex() == 2 ? 45 : 1;
      const QSignalBlocker block(secondary_numeric_editor_);
      secondary_numeric_editor_->setText(QString::number(finishing_second_value_));
      secondary_numeric_editor_->setPlaceholderText(finishing_mode_->currentIndex() == 2 ? "deg"
                                                                                         : "mm");
    }
    preserve_finishing_row_heights(*finishing_controls_, *finishing_controls_->layout());
    refresh_finishing_preview();
  });
  connect(secondary_numeric_editor_, &QLineEdit::editingFinished, this, [this] {
    if (finishing()) {
      auto preview = set_secondary_numeric_text(secondary_numeric_editor_->text());
      if (!preview)
        report(preview.error());
    }
  });
  for (auto* input : {draft_axis_, draft_plane_})
    connect(input, &QComboBox::currentIndexChanged, this, [this] {
      if (command_ == Command::Draft)
        refresh_finishing_preview();
    });
}

Result<std::size_t> ShellFeatureTools::begin_finishing(Command command,
                                                       std::optional<FeatureId> editing) {
  const auto fail = [](std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.finishing", std::move(message)));
  };
  if (active() || session_.task().active() || window_.active_sketch() ||
      session_.document_kind() != GuiDocumentKind::Part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_shape_cache())
    return fail("Finishing requires a part without an active command");
  if (command != Command::Fillet && command != Command::Chamfer && command != Command::Shell &&
      command != Command::Draft)
    return fail("Unknown finishing function");
  const auto& part = *session_.part_document();
  std::optional<FilletFeature> saved;
  if (editing) {
    const auto* record = part.find_fillet_feature(*editing);
    if (command != Command::Fillet || !record)
      return fail("Select an existing Fillet to edit");
    saved = *record;
  }
  if (command == Command::Fillet) {
    const QSignalBlocker mode(finishing_mode_);
    finishing_mode_->clear();
    finishing_mode_->addItems({"Constant Radius", "Variable Radius", "Face Blend", "Full Round"});
    if (saved) {
      advanced_fillet_->load(part, *saved);
      auto history = part.executable_history();
      if (!history)
        return Result<std::size_t>::failure(history.error());
      const auto found =
          std::find_if(history.value().begin(), history.value().end(),
                       [&](const auto& row) { return row.node_id == saved->id().value(); });
      if (found == history.value().end())
        return fail("Fillet input history is unavailable");
      const auto prefix = std::size_t(found - history.value().begin());
      if (session_.history_prefix_count() && prefix >= *session_.history_prefix_count())
        return fail("Move End of Part after this Fillet before editing it");
      auto cache = geometry::ShapeCache::create(ShapeCacheId("fillet.edit.inputs"));
      if (!cache)
        return Result<std::size_t>::failure(cache.error());
      auto computed = query_with_step_import_context(session_, [&](auto* context) {
        return geometry::GeometryRecomputeExecutor(context).execute_history_prefix(part, prefix,
                                                                                   cache.value());
      });
      if (!computed)
        return Result<std::size_t>::failure(computed.error());
      advanced_fillet_->input_cache = std::move(cache.value());
      advanced_fillet_->input_prefix = prefix;
    } else
      advanced_fillet_->reset(part, FeatureId("fillet.draft"), "Fillet", BodyId(""));
  }
  {
    const QSignalBlocker body_block(finishing_body_), mode_block(finishing_mode_),
        axis_block(draft_axis_), plane_block(draft_plane_);
    finishing_body_->clear();
    for (const auto& body : part.bodies()) {
      auto targets = finishing_targets(
          session_, body.id(),
          command == Command::Fillet ? advanced_fillet_->role() : selection_role(command),
          command == Command::Fillet ? advanced_fillet_.get() : nullptr);
      if (targets && !targets.value().empty())
        finishing_body_->addItem(QString::fromStdString(body.name()),
                                 QString::fromStdString(body.id().value()));
    }
    if (finishing_body_->count() == 0)
      return fail("No unambiguously referenceable edges or faces available");
    for (const auto& selection : session_.selection().entries()) {
      if (selection.kind != GuiSelectionKind::Body || !selection.semantic_id.starts_with("body/"))
        continue;
      const int index =
          finishing_body_->findData(QString::fromStdString(selection.semantic_id.substr(5)));
      if (index >= 0)
        finishing_body_->setCurrentIndex(index);
    }
    if (saved) {
      const int index =
          finishing_body_->findData(QString::fromStdString(saved->target_body().value()));
      if (index < 0)
        return fail("Fillet target has no evaluated input geometry");
      finishing_body_->setCurrentIndex(index);
    }
    if (command != Command::Fillet)
      finishing_mode_->clear();
    if (command == Command::Chamfer)
      finishing_mode_->addItems({QStringLiteral("Equal Distances"), QStringLiteral("Two Distances"),
                                 QStringLiteral("Distance and Angle")});
    if (command == Command::Shell)
      finishing_mode_->addItems({QStringLiteral("Inward"), QStringLiteral("Outward")});
    draft_axis_->clear();
    draft_plane_->clear();
    for (const auto& axis : part.datum_axes())
      if (axis.kind() == DatumAxisKind::Explicit)
        draft_axis_->addItem(QString::fromStdString(axis.name()),
                             QString::fromStdString(axis.id().value()));
    for (const auto& plane : part.datum_planes())
      draft_plane_->addItem(QString::fromStdString(plane.name()),
                            QString::fromStdString(plane.id().value()));
    const int z = draft_axis_->findData(QStringLiteral("datum.axis.z"));
    if (z >= 0)
      draft_axis_->setCurrentIndex(z);
  }
  const auto title = command_title(command);
  if (saved) {
    finishing_feature_ = saved->id();
    finishing_name_ = saved->name();
    const auto parameters = saved->parameter_dependencies();
    finishing_parameter_ = parameters.empty() ? ParameterId("") : parameters.front();
    finishing_second_parameter_ = ParameterId("");
  } else
    for (std::size_t i = 1;; ++i) {
      const std::string base = "finishing." + std::to_string(i);
      const FeatureId id("feature." + base);
      const bool feature_taken =
          part.find_feature(id) || part.find_revolve_feature(id) || part.find_fillet_feature(id) ||
          part.find_chamfer_feature(id) || part.find_shell_feature(id) ||
          part.find_draft_feature(id) || part.find_sweep_feature(id) ||
          part.find_loft_feature(id) || part.find_surface_feature(id) ||
          part.find_linear_pattern_feature(id) || part.find_circular_pattern_feature(id) ||
          part.find_mirror_feature(id) || part.find_body_boolean_feature(id);
      const std::string name = title.toStdString() + " " + std::to_string(i);
      if (feature_taken || part.find_parameter(ParameterId("parameter." + base + ".first")) ||
          part.find_parameter(ParameterId("parameter." + base + ".second")) ||
          std::any_of(part.parameters().begin(), part.parameters().end(), [&](const auto& p) {
            return p.name() == name + " Size" || p.name() == name + " Second Size";
          }))
        continue;
      finishing_feature_ = id;
      finishing_name_ = name;
      finishing_parameter_ = ParameterId("parameter." + base + ".first");
      finishing_second_parameter_ = ParameterId("parameter." + base + ".second");
      break;
    }
  if (command == Command::Fillet) {
    if (!saved)
      advanced_fillet_->reset(part, finishing_feature_, finishing_name_,
                              BodyId(finishing_body_->currentData().toString().toStdString()));
    advanced_fillet_->entry_selection = session_.selection();
    advanced_fillet_->entry_viewport_selection = viewport_.selected_semantic();
  }
  finishing_body_->setEnabled(!saved.has_value());
  previous_selection_mask_ = viewport_.selection_filter_mask();
  previous_semantic_scope_ = viewport_.selection_semantic_scope();
  command_ = command;
  if (!session_.task().begin(std::string(command_id())) ||
      !session_.task().begin_parameter_editing() || !session_.task().show_preview()) {
    command_ = Command::None;
    if (session_.task().active())
      (void)session_.task().cancel();
    return fail("Could not start editing task");
  }
  finishing_first_value_ = command == Command::Draft ? 5 : 1;
  finishing_second_value_ = 1;
  secondary_numeric_editor_->setText(QStringLiteral("1"));
  show_task_panel(saved ? QString("Edit %1").arg(QString::fromStdString(saved->name())) : title,
                  angular_value() ? QStringLiteral("deg") : QStringLiteral("mm"),
                  finishing_first_value_);
  finishing_mode_->parentWidget()->setVisible(
      command == Command::Fillet || command == Command::Chamfer || command == Command::Shell);
  advanced_fillet_->panel->setVisible(typed_finishing());
  secondary_numeric_editor_->parentWidget()->hide();
  draft_axis_->parentWidget()->setVisible(command == Command::Draft);
  draft_plane_->parentWidget()->setVisible(command == Command::Draft);
  preserve_finishing_row_heights(*finishing_controls_, *finishing_controls_->layout());
  reload_finishing_targets();
  window_.refresh();
  return Result<std::size_t>::success(1);
}

void ShellFeatureTools::reload_finishing_targets(bool preview) {
  const auto* part = session_.part_document();
  const auto* cache = session_.part_shape_cache();
  finishing_targets_.clear();
  {
    const QSignalBlocker block(finishing_targets_list_);
    finishing_targets_list_->clear();
    if (part && cache) {
      auto targets = finishing_targets(
          session_, BodyId(finishing_body_->currentData().toString().toStdString()),
          command_ == Command::Fillet ? advanced_fillet_->role() : selection_role(command_),
          command_ == Command::Fillet ? advanced_fillet_.get() : nullptr);
      if (targets)
        finishing_targets_ = std::move(targets.value());
    }
    const auto keys = command_ == Command::Fillet ? advanced_fillet_->selected_keys()
                                                  : std::vector<std::string>{};
    if (command_ == Command::Fillet)
      advanced_fillet_->labels.clear();
    for (const auto& target : finishing_targets_) {
      finishing_targets_list_->addItem(target_label(target.label));
      if (command_ == Command::Fillet) {
        const auto key = std::visit(
            [](const auto& r) {
              using T = std::decay_t<decltype(r)>;
              if constexpr (std::is_same_v<T, EdgeReference>)
                return interactive_edge_identity(r);
              else
                return interactive_face_identity(r);
            },
            target.reference);
        advanced_fillet_->labels[key] = target_label(target.label);
        if (std::find(keys.begin(), keys.end(), key) != keys.end())
          finishing_targets_list_->item(finishing_targets_list_->count() - 1)->setSelected(true);
      }
    }
    if (command_ == Command::Fillet)
      advanced_fillet_->render_members();
  }
  viewport_.set_selection_filter_mask(
      selection_kind_bit((command_ == Command::Fillet && finishing_mode_->currentIndex() < 2) ||
                                 command_ == Command::Chamfer
                             ? GuiSelectionKind::Edge
                             : GuiSelectionKind::Face));
  std::vector<std::string> scope;
  for (const auto& target : finishing_targets_)
    scope.push_back(target.semantic_id);
  viewport_.set_selection_semantic_scope(std::move(scope));
  window_.refresh_feature_selection_scene();
  if (preview)
    refresh_finishing_preview();
}

Result<std::size_t> ShellFeatureTools::rebuild_finishing_controller() {
  if (viewport_.mouseGrabber() == &viewport_)
    viewport_.release_interaction_mouse();
  layer_.cancel();
  drag_start_value_.reset();
  coordinator_.cancel();
  if (typed_finishing())
    return rebuild_advanced_fillet_controller();
  const BodyId body(finishing_body_->currentData().toString().toStdString());
  const bool angular = angular_value();
  auto quantity = angular ? Quantity::angle_deg(finishing_first_value_)
                          : Quantity::length_mm(finishing_first_value_);
  if (!quantity)
    return Result<std::size_t>::failure(quantity.error());
  auto parameter = angular ? Parameter::create_angle(finishing_parameter_,
                                                     finishing_name_ + " Size", quantity.value())
                           : Parameter::create_length(finishing_parameter_,
                                                      finishing_name_ + " Size", quantity.value());
  if (!parameter)
    return Result<std::size_t>::failure(parameter.error());
  std::vector<Parameter> pending{parameter.value()};
  Result<std::size_t> begun = Result<std::size_t>::success(1);
  if (command_ == Command::Fillet)
    begun = coordinator_.finishing().begin_fillet(session_, body, finishing_feature_,
                                                  finishing_name_, finishing_parameter_, pending);
  else if (command_ == Command::Chamfer) {
    const int mode = finishing_mode_->currentIndex();
    if (mode > 0) {
      auto q = mode == 2 ? Quantity::angle_deg(finishing_second_value_)
                         : Quantity::length_mm(finishing_second_value_);
      if (!q)
        return Result<std::size_t>::failure(q.error());
      auto p = mode == 2 ? Parameter::create_angle(finishing_second_parameter_,
                                                   finishing_name_ + " Second Size", q.value())
                         : Parameter::create_length(finishing_second_parameter_,
                                                    finishing_name_ + " Second Size", q.value());
      if (!p)
        return Result<std::size_t>::failure(p.error());
      pending.push_back(p.value());
    }
    if (mode == 0)
      begun = coordinator_.finishing().begin_chamfer_equal(
          session_, body, finishing_feature_, finishing_name_, finishing_parameter_, pending);
    else if (mode == 1)
      begun = coordinator_.finishing().begin_chamfer_two_distance(
          session_, body, finishing_feature_, finishing_name_, finishing_parameter_,
          finishing_second_parameter_, pending);
    else
      begun = coordinator_.finishing().begin_chamfer_distance_angle(
          session_, body, finishing_feature_, finishing_name_, finishing_parameter_,
          finishing_second_parameter_, pending);
  } else if (command_ == Command::Shell)
    begun = coordinator_.shell_draft().begin_shell(
        session_, body, finishing_feature_, finishing_name_, finishing_parameter_,
        finishing_mode_->currentIndex() == 0 ? ShellDirection::Inward : ShellDirection::Outward,
        pending);
  else {
    auto axis = AxisReference::create_datum_axis(
        PartFeatureInputRole::DraftPullDirection,
        DatumAxisId(draft_axis_->currentData().toString().toStdString()));
    auto plane = PlaneReference::create_datum_plane(
        PartFeatureInputRole::DraftNeutralPlane,
        DatumPlaneId(draft_plane_->currentData().toString().toStdString()));
    if (!axis)
      return Result<std::size_t>::failure(axis.error());
    if (!plane)
      return Result<std::size_t>::failure(plane.error());
    begun = coordinator_.shell_draft().begin_draft(session_, body, finishing_feature_,
                                                   finishing_name_, axis.value(), plane.value(),
                                                   finishing_parameter_, pending);
  }
  if (!begun)
    return begun;
  GuiFeatureManipulatorFrame frame;
  frame.axis = {1, 0, 0};
  bool anchored = false;
  for (int i = 0; i < finishing_targets_list_->count(); ++i) {
    if (!finishing_targets_list_->item(i)->isSelected())
      continue;
    const auto& target = finishing_targets_[static_cast<std::size_t>(i)];
    if (!anchored) {
      frame.origin = target.anchor;
      anchored = true;
    }
    auto added = std::visit(
        [&](const auto& reference) -> Result<std::size_t> {
          using T = std::decay_t<decltype(reference)>;
          if constexpr (std::is_same_v<T, EdgeReference>)
            return coordinator_.finishing().add_edge(reference);
          else
            return coordinator_.shell_draft().add_face(reference);
        },
        target.reference);
    if (!added)
      return added;
  }
  if (command_ == Command::Fillet || command_ == Command::Chamfer)
    coordinator_.finishing().set_manipulator_frame(frame);
  else
    coordinator_.shell_draft().set_manipulator_frame(frame);
  coordinator_.refresh_handles();
  return begun;
}

QString ShellFeatureTools::finishing_summary() const {
  if (typed_finishing())
    return QString("%1: typed inputs and contacts verified. Preview valid.")
        .arg(finishing_mode_->currentText());
  const auto count = finishing_targets_list_->selectedItems().size();
  const bool edges = command_ == Command::Fillet || command_ == Command::Chamfer;
  return QStringLiteral("%1 %2 selected. Preview valid.")
      .arg(count)
      .arg(edges ? (count == 1 ? QStringLiteral("Edge") : QStringLiteral("Edges"))
                 : (count == 1 ? QStringLiteral("Face") : QStringLiteral("Faces")));
}

void ShellFeatureTools::refresh_finishing_preview() {
  if (typed_finishing())
    advanced_fillet_->failure = false;
  const auto* draft = coordinator_.finishing().typed_draft();
  if (coordinator_.active() && (!draft || !draft->record.parameter_dependencies().empty()))
    finishing_first_value_ = current_value();
  auto begun = rebuild_finishing_controller();
  if (!begun) {
    preview_valid_ = false;
    viewport_.clear_feature_preview();
    task_summary_->setText(QString::fromStdString(begun.error().message()));
    if (typed_finishing())
      show_fillet_failure(begun.error());
    refresh_enablement();
    synchronize_overlay();
    return;
  }
  synchronize_numeric_editor(current_value());
  auto preview = preview_and_publish();
  if (typed_finishing()) {
    if (preview)
      advanced_fillet_->capability->setText(finishing_summary());
    else
      show_fillet_failure(preview.error());
  }
  synchronize_overlay();
}

bool ShellFeatureTools::accept_finishing_selection(const GuiSelection& selection) {
  if (!finishing())
    return false;
  for (std::size_t i = 0; i < finishing_targets_.size(); ++i)
    if (finishing_targets_[i].semantic_id == selection.semantic_id) {
      auto* item = finishing_targets_list_->item(static_cast<int>(i));
      item->setSelected(!item->isSelected());
      return true;
    }
  return false;
}

std::vector<geometry::ViewportSceneItem> ShellFeatureTools::finishing_selection_items() const {
  std::vector<geometry::ViewportSceneItem> items;
  if (const auto mask = idle_finishing_selection_mask()) {
    const bool edges = mask == selection_kind_bit(GuiSelectionKind::Edge);
    for (const auto& body : session_.part_document()->bodies()) {
      auto targets = finishing_targets(session_, body.id(),
                                       edges ? PartFeatureInputRole::FilletEdge
                                             : PartFeatureInputRole::ShellRemovalFace);
      if (targets)
        for (const auto& target : targets.value())
          items.push_back(
              {edges ? geometry::ViewportSceneKind::Edge : geometry::ViewportSceneKind::Face,
               target.semantic_id, target.shape, SketchLineRole::Geometry, body.id().value()});
      if (!edges) {
        auto supports = query_with_step_import_context(session_, [&](auto* context) {
          return geometry::ResultFaceSupportGeometry::selectable_faces(
              *session_.part_document(), *session_.part_shape_cache(), body.id(), context,
              session_.history_prefix_count());
        });
        if (supports)
          for (const auto& target : supports.value()) {
            if (std::any_of(items.begin(), items.end(), [&](const auto& item) {
                  return item.semantic_id == target.semantic_id;
                }))
              continue;
            items.push_back({geometry::ViewportSceneKind::Face, target.semantic_id, target.shape,
                             SketchLineRole::Geometry, body.id().value()});
          }
      }
    }
  }
  if (finishing())
    for (const auto& target : finishing_targets_)
      items.push_back({std::holds_alternative<EdgeReference>(target.reference)
                           ? geometry::ViewportSceneKind::Edge
                           : geometry::ViewportSceneKind::Face,
                       target.semantic_id, target.shape, SketchLineRole::Geometry,
                       finishing_body_->currentData().toString().toStdString()});
  if (command_ == Command::Loft) {
    auto supports = loft_selection_items();
    items.insert(items.end(), supports.begin(), supports.end());
  }
  if (command_ == Command::PathCurve)
    if (const auto edges = native_spatial_edges(); edges)
      for (const auto& edge : edges.value())
        items.push_back({geometry::ViewportSceneKind::Edge, edge.target.semantic_id,
                         edge.target.shape, SketchLineRole::Geometry, edge.owner.value()});
  if (surface())
    for (const auto& input : surface_inputs_)
      if (input.surface && input.surface->source_kind() == SurfaceReferenceSourceKind::Body &&
          input.semantic_id.starts_with("surface-input/")) {
        const auto& owner = std::get<BodyId>(input.surface->source());
        const auto* cache = advanced_surface_->input_cache ? &*advanced_surface_->input_cache
                                                           : session_.part_shape_cache();
        const auto* shape = cache->find_body_shape(owner);
        if (shape)
          items.push_back({geometry::ViewportSceneKind::SurfaceBody, input.semantic_id, *shape,
                           SketchLineRole::Geometry, owner.value()});
      }
  if (surface())
    for (const auto& target : surface_targets_) {
      // Surface inputs retain their typed producer reference; resolve its
      // Body without interpreting the serialized transient selection identity.
      const auto source = std::visit(
          [](const auto& reference) {
            return std::visit(
                [](const auto& producer) -> std::optional<FeatureId> {
                  using T = std::decay_t<decltype(producer)>;
                  if constexpr (std::is_same_v<T, ImportedTopologyReference>)
                    return std::nullopt;
                  else if constexpr (std::is_same_v<T, ResultFaceReference>)
                    return producer.producer();
                  else
                    return producer.source_feature();
                },
                reference.source());
          },
          target.reference);
      const auto* feature = source ? session_.part_document()->find_feature(*source) : nullptr;
      const std::string owner_body_id =
          feature && feature->body_result_context()
              ? feature->body_result_context()->effective_produced_body().value()
              : std::string{};
      items.push_back({std::holds_alternative<EdgeReference>(target.reference)
                           ? geometry::ViewportSceneKind::Edge
                           : geometry::ViewportSceneKind::Face,
                       target.semantic_id, target.shape, SketchLineRole::Geometry, owner_body_id});
    }
  return items;
}
} // namespace blcad::gui
