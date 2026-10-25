#include "blcad/gui/shell/shell_feature_tools.hpp"

#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace blcad::gui {
namespace {
QString title(ShellFeatureTools::Command command) {
  using C = ShellFeatureTools::Command;
  switch (command) {
  case C::BoundarySurface:
    return QStringLiteral("Begrenzungsfläche");
  case C::FillSurface:
    return QStringLiteral("Füllfläche");
  case C::TrimSurface:
    return QStringLiteral("Fläche trimmen");
  case C::ExtendSurface:
    return QStringLiteral("Fläche erweitern");
  case C::SurfaceStitch:
    return QStringLiteral("Flächen vernähen");
  case C::ClosedShellToSolid:
    return QStringLiteral("Hülle zu Volumen");
  default:
    return {};
  }
}
Result<std::size_t> fail(std::string message) {
  return Result<std::size_t>::failure(Error::validation("gui.shell.surface", std::move(message)));
}
} // namespace

bool ShellFeatureTools::surface() const noexcept {
  return command_ == Command::BoundarySurface || command_ == Command::FillSurface ||
         command_ == Command::TrimSurface || command_ == Command::ExtendSurface ||
         command_ == Command::SurfaceStitch || command_ == Command::ClosedShellToSolid;
}
bool ShellFeatureTools::surface_scalar() const noexcept {
  return command_ == Command::ExtendSurface || command_ == Command::SurfaceStitch;
}

QAction* ShellFeatureTools::surface_action(Command command) const noexcept {
  for (const auto& [kind, action] : surface_actions_)
    if (kind == command)
      return action;
  return nullptr;
}

GuiSelectionKind ShellFeatureTools::surface_input_kind(const SurfaceInput& input) {
  if (input.surface)
    return input.surface->source_kind() == SurfaceReferenceSourceKind::Body
               ? GuiSelectionKind::Body
               : GuiSelectionKind::Face;
  if (input.boundary && input.boundary->source_kind() == BoundaryCurveSourceKind::SemanticEdge)
    return GuiSelectionKind::Edge;
  return GuiSelectionKind::SketchEntity;
}

std::optional<ShellFeatureTools::SurfaceContext>
ShellFeatureTools::surface_context_for(Command command, const GuiSelection& selection) const {
  if (!surface_action(command) || session_.document_kind() != GuiDocumentKind::Part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_document() ||
      !session_.part_shape_cache() || !session_.derived_results_fresh())
    return std::nullopt;
  const bool curves = command == Command::BoundarySurface || command == Command::FillSurface;
  if (curves ? selection.kind != GuiSelectionKind::SketchEntity &&
                   selection.kind != GuiSelectionKind::Edge
             : selection.kind != GuiSelectionKind::Body && selection.kind != GuiSelectionKind::Face)
    return std::nullopt;
  if (selection.kind == GuiSelectionKind::Body) {
    // Reject ordinary solid-Body preselection before resolving all subshape inputs.
    // A Surface Body is already a complete typed source for these target roles.
    for (const auto& body : session_.part_document()->bodies())
      if (body.kind() == BodyKind::Surface && body.visibility() == BodyVisibility::Visible &&
          selection.semantic_id == "body/" + body.id().value()) {
        const auto* shape = session_.part_shape_cache()->find_body_shape(body.id());
        if (shape && !shape->empty())
          return SurfaceContext{command, selection,
                                SurfaceReference::create_body(body.id()).value()};
      }
    return std::nullopt;
  }
  for (const auto& input : surface_inputs()) {
    if (input.semantic_id != selection.semantic_id || surface_input_kind(input) != selection.kind)
      continue;
    if (curves && input.boundary)
      return SurfaceContext{command, selection, *input.boundary};
    if (!curves && input.surface &&
        (command != Command::ClosedShellToSolid ||
         input.surface->source_kind() == SurfaceReferenceSourceKind::Body))
      return SurfaceContext{command, selection, *input.surface};
  }
  return std::nullopt;
}

std::optional<ShellFeatureTools::SurfaceContext>
ShellFeatureTools::surface_context(Command command) const {
  if (active() || session_.task().active() || window_.active_sketch() ||
      session_.reference_repair_mode() || session_.selection().size() != 1)
    return std::nullopt;
  const auto& selection = session_.selection().entries().front();
  if (!session_.selection().allows(selection.kind) ||
      !(viewport_.selection_filter_mask() & selection_kind_bit(selection.kind)))
    return std::nullopt;
  const auto& scope = viewport_.selection_semantic_scope();
  if (scope && std::find(scope->begin(), scope->end(), selection.semantic_id) == scope->end())
    return std::nullopt;
  return surface_context_for(command, selection);
}

bool ShellFeatureTools::surface_context_available(const SurfaceContext& context) const {
  const auto current = surface_context_for(context.command, context.selection);
  const auto key = [](const auto& source) {
    return std::visit([](const auto& reference) { return reference.stable_key(); }, source);
  };
  return current && current->source.index() == context.source.index() &&
         key(current->source) == key(context.source);
}

bool ShellFeatureTools::select_surface_context(const SurfaceContext& context) {
  if (!surface() || command_ != context.command || !session_.task().active() ||
      session_.task().command_id() != command_id() || !surface_context_available(context))
    return false;
  const bool curves = command_ == Command::BoundarySurface || command_ == Command::FillSurface;
  auto* combo = curves ? surface_source_ : surface_target_;
  for (int row = 1; row < combo->count(); ++row) {
    const int index = combo->itemData(row).toInt();
    if (surface_inputs_.at(index).semantic_id != context.selection.semantic_id)
      continue;
    {
      const QSignalBlocker source(surface_source_), target(surface_target_), pick(surface_pick_),
          order(surface_order_);
      surface_source_->setCurrentIndex(0);
      surface_target_->setCurrentIndex(0);
      surface_order_->clear();
      combo->setCurrentIndex(row);
      surface_pick_->setCurrentIndex(
          command_ == Command::TrimSurface || command_ == Command::ExtendSurface ? 1 : 0);
      if (curves || command_ == Command::SurfaceStitch) {
        auto* item = new QListWidgetItem(QString::fromStdString(surface_inputs_[index].label),
                                         surface_order_);
        item->setData(Qt::UserRole, index);
        surface_order_->setCurrentItem(item);
      }
    }
    update_surface_pick_scope();
    refresh_surface_preview();
    return true;
  }
  return false;
}

std::vector<ShellFeatureTools::SurfaceInput>
ShellFeatureTools::surface_inputs(std::vector<geometry::FinishingSelectionTarget>* targets) const {
  std::vector<SurfaceInput> choices;
  const auto* part = session_.part_document();
  const auto* cache = session_.part_shape_cache();
  if (!part || !cache)
    return choices;
  for (const auto& path : part->path_curves()) {
    auto boundary = BoundaryCurveReference::create_path_curve(path.id());
    if (boundary)
      choices.push_back({path.name(),
                         "path/" + path.id().value(),
                         boundary.value(),
                         {},
                         TrimmingReference::create_boundary_curve(boundary.value()).value()});
  }
  for (const auto& input : spatial_inputs_for(Command::Sweep))
    if (input.profile) {
      auto profile = ProfileRegionReference::create(input.profile->first, input.profile->second,
                                                    PartFeatureInputRole::ProfileRegion);
      if (profile)
        choices.push_back({input.label,
                           input.semantic_id,
                           {},
                           {},
                           TrimmingReference::create_profile_region(profile.value()).value()});
    }
  for (const auto& body : part->bodies()) {
    if (body.visibility() != BodyVisibility::Visible || !cache->find_body_shape(body.id()))
      continue;
    if (body.kind() == BodyKind::Surface) {
      choices.push_back({body.name(),
                         "body/" + body.id().value(),
                         {},
                         SurfaceReference::create_body(body.id()).value(),
                         {}});
      continue;
    }
    if (body.kind() != BodyKind::Solid)
      continue;
    for (auto role : {PartFeatureInputRole::FilletEdge, PartFeatureInputRole::ShellRemovalFace}) {
      auto resolved = geometry::FinishingSelectionBuilder::build(*part, *cache, body.id(), role);
      if (!resolved)
        continue;
      for (const auto& target : resolved.value()) {
        if (const auto* edge = std::get_if<EdgeReference>(&target.reference)) {
          const auto* semantic = std::get_if<SemanticEdgeReference>(&edge->source());
          if (!semantic)
            continue;
          auto boundary = BoundaryCurveReference::create_semantic_edge(*semantic);
          if (!boundary)
            continue;
          choices.push_back({target.label,
                             target.semantic_id,
                             boundary.value(),
                             {},
                             TrimmingReference::create_boundary_curve(boundary.value()).value()});
        } else {
          const auto* semantic = std::get_if<SemanticFaceReference>(
              &std::get<FaceReference>(target.reference).source());
          if (!semantic)
            continue;
          auto surface = SurfaceReference::create_semantic_face(*semantic);
          if (!surface)
            continue;
          choices.push_back({target.label, target.semantic_id, {}, surface.value(), {}});
        }
        if (targets)
          targets->push_back(target);
      }
    }
  }
  return choices;
}

void ShellFeatureTools::build_surface_controls(QWidget* panel) {
  surface_controls_ = new QWidget(panel);
  surface_controls_->setMinimumWidth(280);
  auto* column = new QVBoxLayout(surface_controls_);
  column->setContentsMargins(0, 0, 0, 0);
  auto* form = new QFormLayout;
  surface_form_ = form;
  column->addLayout(form);
  surface_pick_ = new QComboBox(surface_controls_);
  surface_pick_->setObjectName("blcad.surface.pick_role");
  form->addRow(QStringLiteral("Modellklick wählt"), surface_pick_);
  surface_target_ = new QComboBox(surface_controls_);
  surface_target_->setObjectName("blcad.surface.target");
  form->addRow(QStringLiteral("Fläche / Hülle"), surface_target_);
  surface_source_ = new QComboBox(surface_controls_);
  surface_source_->setObjectName("blcad.surface.source");
  form->addRow(QStringLiteral("Kurve / Kontur"), surface_source_);
  surface_add_ = new QPushButton(QStringLiteral("Zur Liste hinzufügen"), surface_controls_);
  surface_add_->setObjectName("blcad.surface.add");
  form->addRow(surface_add_);
  surface_order_ = new QListWidget(surface_controls_);
  surface_order_->setObjectName("blcad.surface.order");
  surface_order_->setDragDropMode(QAbstractItemView::InternalMove);
  surface_order_->setMinimumHeight(100);
  surface_order_->setMaximumHeight(180);
  form->addRow(surface_order_);
  surface_order_buttons_ = new QWidget(surface_controls_);
  auto* buttons = new QHBoxLayout(surface_order_buttons_);
  buttons->setContentsMargins(0, 0, 0, 0);
  for (int direction : {-1, 1, 0}) {
    auto* button = new QPushButton(direction < 0   ? QStringLiteral("Hoch")
                                   : direction > 0 ? QStringLiteral("Runter")
                                                   : QStringLiteral("Entfernen"),
                                   surface_order_buttons_);
    button->setObjectName(direction < 0   ? "blcad.surface.move_up"
                          : direction > 0 ? "blcad.surface.move_down"
                                          : "blcad.surface.remove");
    buttons->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, direction] {
      const int row = surface_order_->currentRow(), to = row + direction;
      if (!surface() || row < 0)
        return;
      {
        const QSignalBlocker block(surface_order_);
        if (direction == 0)
          delete surface_order_->takeItem(row);
        else if (to >= 0 && to < surface_order_->count()) {
          auto* item = surface_order_->takeItem(row);
          surface_order_->insertItem(to, item);
          surface_order_->setCurrentItem(item);
        }
      }
      refresh_surface_preview();
    });
  }
  form->addRow(surface_order_buttons_);
  surface_hint_ = new QLabel(surface_controls_);
  surface_hint_->setWordWrap(true);
  column->addWidget(surface_hint_);
  panel->layout()->addWidget(surface_controls_);
  surface_controls_->hide();
  for (auto* combo : {surface_source_, surface_target_})
    connect(combo, &QComboBox::currentIndexChanged, this, [this] {
      if (surface())
        refresh_surface_preview();
    });
  connect(surface_pick_, &QComboBox::currentIndexChanged, this, [this] {
    if (surface())
      update_surface_pick_scope();
  });
  connect(surface_add_, &QPushButton::clicked, this, [this] {
    if (!surface())
      return;
    const int choice = (command_ == Command::SurfaceStitch ? surface_target_ : surface_source_)
                           ->currentData()
                           .toInt();
    auto added = append_surface_input(choice);
    if (!added)
      report(added.error());
  });
  connect(surface_order_->model(), &QAbstractItemModel::rowsMoved, this, [this] {
    if (surface())
      refresh_surface_preview();
  });
}

Result<std::size_t> ShellFeatureTools::begin_surface(Command command) {
  const auto* part = session_.part_document();
  if (active() || session_.task().active() || window_.active_sketch() || !part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_shape_cache() ||
      title(command).isEmpty())
    return fail("Flächenbefehl benötigt ein Bauteil ohne aktiven Befehl");
  surface_targets_.clear();
  surface_inputs_ = surface_inputs(&surface_targets_);
  const QSignalBlocker source(surface_source_), target(surface_target_), pick(surface_pick_),
      order(surface_order_);
  surface_source_->clear();
  surface_target_->clear();
  surface_order_->clear();
  surface_pick_->clear();
  surface_source_->addItem(QStringLiteral("Bitte wählen"), -1);
  surface_target_->addItem(QStringLiteral("Bitte wählen"), -1);
  for (std::size_t i = 0; i < surface_inputs_.size(); ++i) {
    const auto& input = surface_inputs_[i];
    if (input.surface && (command != Command::ClosedShellToSolid ||
                          input.surface->source_kind() == SurfaceReferenceSourceKind::Body))
      surface_target_->addItem(QString::fromStdString(input.label), static_cast<int>(i));
    bool eligible =
        command == Command::TrimSurface ? input.trimming.has_value() : input.boundary.has_value();
    if (command == Command::TrimSurface && input.boundary &&
        input.boundary->source_kind() == BoundaryCurveSourceKind::SemanticEdge)
      eligible = false;
    if (eligible && input.boundary &&
        input.boundary->source_kind() == BoundaryCurveSourceKind::PathCurve) {
      const auto id = std::get<PathCurveId>(input.boundary->source());
      const auto* path = part->find_path_curve(id);
      if (command == Command::TrimSurface)
        eligible = path && path->closure() == PathClosure::Closed;
      if (command == Command::ExtendSurface) {
        auto segments = geometry::GeometryRecomputeExecutor{}.resolve_path_segments(
            *part, id, *session_.part_shape_cache());
        eligible = segments && segments.value().size() == 1 &&
                   segments.value().front().kind == geometry::SweepPathSegmentKind::Line;
      }
    }
    if (eligible)
      surface_source_->addItem(QString::fromStdString(input.label), static_cast<int>(i));
  }
  const bool curves = command == Command::BoundarySurface || command == Command::FillSurface;
  const bool stitch = command == Command::SurfaceStitch;
  if ((curves && surface_source_->count() < 2) || (!curves && surface_target_->count() < 2) ||
      ((command == Command::TrimSurface || command == Command::ExtendSurface) &&
       surface_source_->count() < 2) ||
      (stitch && surface_target_->count() < 3)) {
    surface_targets_.clear();
    return fail("Zuerst passende Pfade, Flächen oder eine geschlossene Hülle erstellen");
  }
  surface_pick_->addItem(curves   ? QStringLiteral("Kurve hinzufügen")
                         : stitch ? QStringLiteral("Fläche hinzufügen")
                                  : QStringLiteral("Zielfläche / Hülle"));
  if (command == Command::TrimSurface || command == Command::ExtendSurface)
    surface_pick_->addItem(command == Command::TrimSurface ? QStringLiteral("Trimmkontur")
                                                           : QStringLiteral("Außenkante"));
  for (std::size_t n = 1;; ++n) {
    const auto suffix = std::to_string(n);
    surface_feature_ = FeatureId("feature.surface." + suffix);
    surface_result_ = BodyId("body.surface." + suffix);
    surface_parameter_ = ParameterId("parameter.surface." + suffix);
    surface_name_ = title(command).toStdString() + " " + suffix;
    if (part->dependency_graph().has_node(surface_feature_.value()) ||
        part->find_body(surface_result_) || part->find_parameter(surface_parameter_) ||
        std::any_of(part->bodies().begin(), part->bodies().end(),
                    [&](const auto& b) { return b.name() == surface_name_; }) ||
        std::any_of(part->parameters().begin(), part->parameters().end(),
                    [&](const auto& p) { return p.name() == surface_name_ + " Maß"; }))
      continue;
    break;
  }
  command_ = command;
  if (!session_.task().begin(std::string(command_id())) ||
      !session_.task().begin_parameter_editing() || !session_.task().show_preview()) {
    command_ = Command::None;
    surface_targets_.clear();
    if (session_.task().active())
      (void)session_.task().cancel();
    return fail("Flächenaufgabe konnte nicht gestartet werden");
  }
  surface_value_ = stitch ? 0.001 : 5;
  previous_selection_mask_ = viewport_.selection_filter_mask();
  previous_semantic_scope_ = viewport_.selection_semantic_scope();
  auto* form = surface_form_;
  form->setRowVisible(surface_target_, !curves);
  form->setRowVisible(surface_source_, curves || command == Command::TrimSurface ||
                                           command == Command::ExtendSurface);
  for (QWidget* widget :
       std::array<QWidget*, 3>{surface_add_, surface_order_, surface_order_buttons_})
    form->setRowVisible(widget, curves || stitch);
  surface_hint_->setText(
      command == Command::BoundarySurface
          ? QStringLiteral("Zwei Kurven verbinden oder drei bis vier zusammenhängende Randkurven "
                           "schließen. Liste ziehen oder mit Hoch/Runter ordnen.")
      : command == Command::FillSurface
          ? QStringLiteral("Einen geschlossenen Rand aus Pfaden oder Kanten in zusammenhängender "
                           "Reihenfolge auswählen. Liste ziehen oder mit Hoch/Runter ordnen.")
      : command == Command::TrimSurface
          ? QStringLiteral("Zielfläche und geschlossene Kontur wählen. Erhalten bleibt die Fläche "
                           "innerhalb der Kontur.")
      : command == Command::ExtendSurface
          ? QStringLiteral("Zielfläche und eine lineare Außenkante wählen. Den Abstand am Pfeil "
                           "ziehen oder als Maß eingeben.")
      : command == Command::SurfaceStitch
          ? QStringLiteral("Angrenzende Flächen hinzufügen und die Spalttoleranz eingeben. Eine "
                           "geschlossene Hülle lässt sich anschließend in Volumen umwandeln.")
          : QStringLiteral("Eine geschlossene Hülle auswählen. Offene oder mehrdeutige Hüllen "
                           "zeigen eine Fehlermeldung vor dem Anwenden."));
  show_task_panel(title(command), QStringLiteral("mm"), surface_value_);
  window_.refresh_feature_selection_scene();
  update_surface_pick_scope();
  refresh_surface_preview();
  window_.refresh();
  return Result<std::size_t>::success(1);
}

Result<std::size_t> ShellFeatureTools::append_surface_input(int choice) {
  const bool curves = command_ == Command::BoundarySurface || command_ == Command::FillSurface;
  if (!surface() || (!curves && command_ != Command::SurfaceStitch) || choice < 0 ||
      static_cast<std::size_t>(choice) >= surface_inputs_.size() ||
      (curves ? !surface_inputs_[choice].boundary : !surface_inputs_[choice].surface))
    return fail("Ungültiges Flächenelement");
  for (int row = 0; row < surface_order_->count(); ++row)
    if (surface_order_->item(row)->data(Qt::UserRole).toInt() == choice)
      return fail("Element ist bereits in der Liste");
  auto* item =
      new QListWidgetItem(QString::fromStdString(surface_inputs_[choice].label), surface_order_);
  item->setData(Qt::UserRole, choice);
  surface_order_->setCurrentItem(item);
  refresh_surface_preview();
  return Result<std::size_t>::success(surface_order_->count());
}

Result<std::size_t> ShellFeatureTools::rebuild_surface_controller() {
  if (coordinator_.active())
    coordinator_.cancel();
  layer_.cancel();
  viewport_.clear_feature_preview();
  auto& controller = coordinator_.surface();
  auto body =
      Body::create(surface_result_, surface_name_,
                   command_ == Command::ClosedShellToSolid ? BodyKind::Solid : BodyKind::Surface);
  if (!body)
    return Result<std::size_t>::failure(body.error());
  const auto input = [&](QComboBox* combo) -> const SurfaceInput* {
    const int index = combo->currentData().toInt();
    return index >= 0 && static_cast<std::size_t>(index) < surface_inputs_.size()
               ? &surface_inputs_[index]
               : nullptr;
  };
  const auto *target = input(surface_target_), *source = input(surface_source_);
  Result<std::size_t> begun = fail("Passende Zielfläche und Kontur auswählen");
  std::vector<Parameter> parameters;
  if (surface_scalar()) {
    auto quantity = Quantity::length_mm(surface_value_, surface_parameter_.value());
    if (!quantity)
      return Result<std::size_t>::failure(quantity.error());
    auto parameter =
        Parameter::create_length(surface_parameter_, surface_name_ + " Maß", quantity.value());
    if (!parameter)
      return Result<std::size_t>::failure(parameter.error());
    parameters.push_back(std::move(parameter.value()));
  }
  if (command_ == Command::BoundarySurface || command_ == Command::FillSurface) {
    begun = command_ == Command::BoundarySurface
                ? controller.begin_boundary(session_, surface_feature_, surface_name_,
                                            surface_result_, body.value())
                : controller.begin_fill(session_, surface_feature_, surface_name_, surface_result_,
                                        body.value());
  } else if (command_ == Command::SurfaceStitch)
    begun = controller.begin_stitch(session_, surface_feature_, surface_name_, surface_parameter_,
                                    surface_result_, body.value(), parameters);
  else if (target && target->surface) {
    if (command_ == Command::ClosedShellToSolid)
      begun =
          controller.begin_closed_shell_to_solid(session_, surface_feature_, surface_name_,
                                                 *target->surface, surface_result_, body.value());
    else if (source && command_ == Command::TrimSurface && source->trimming)
      begun = controller.begin_trim(session_, surface_feature_, surface_name_, *target->surface,
                                    *source->trimming, surface_result_, body.value());
    else if (source && command_ == Command::ExtendSurface && source->boundary) {
      begun = controller.begin_extend(session_, surface_feature_, surface_name_, *target->surface,
                                      *source->boundary, surface_parameter_, surface_result_,
                                      body.value(), parameters);
      if (begun) {
        auto frame = geometry::GeometryRecomputeExecutor{}.surface_extension_frame(
            *session_.part_document(), *target->surface, *source->boundary,
            *session_.part_shape_cache());
        if (!frame)
          return Result<std::size_t>::failure(frame.error());
        GuiFeatureManipulatorFrame manipulator;
        manipulator.origin = frame.value().origin;
        manipulator.axis = frame.value().direction;
        controller.set_manipulator_frame(manipulator);
      }
    }
  }
  if (!begun)
    return begun;
  for (int row = 0; row < surface_order_->count(); ++row) {
    const auto& selected =
        surface_inputs_.at(surface_order_->item(row)->data(Qt::UserRole).toInt());
    auto added = command_ == Command::SurfaceStitch
                     ? controller.add_surface(*selected.surface)
                     : controller.add_boundary_curve(*selected.boundary);
    if (!added)
      return added;
  }
  coordinator_.refresh_handles();
  return Result<std::size_t>::success(1);
}

void ShellFeatureTools::refresh_surface_preview() {
  if (!surface())
    return;
  auto rebuilt = rebuild_surface_controller();
  if (!rebuilt) {
    preview_valid_ = false;
    viewport_.clear_feature_preview();
    task_summary_->setText(QString::fromStdString(rebuilt.error().message()));
    refresh_enablement();
    synchronize_overlay();
    return;
  }
  (void)preview_and_publish();
}

void ShellFeatureTools::update_surface_pick_scope() {
  std::vector<std::string> scope;
  const bool curves = command_ == Command::BoundarySurface || command_ == Command::FillSurface;
  const auto* combo =
      curves || surface_pick_->currentIndex() == 1 ? surface_source_ : surface_target_;
  for (int row = 1; row < combo->count(); ++row) {
    const auto& input = surface_inputs_.at(combo->itemData(row).toInt());
    scope.push_back(input.semantic_id);
    if (input.trimming &&
        input.trimming->source_kind() == TrimmingReferenceSourceKind::ProfileRegion) {
      const auto& profile = std::get<ProfileRegionReference>(input.trimming->source());
      for (const char* family : {"rectangle-profile/", "circle-profile/", "closed-profile/",
                                 "arc-profile/", "composite-profile/"})
        scope.push_back("sketch/" + profile.sketch().value() + "/" + family +
                        profile.profile().value());
    }
  }
  viewport_.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Body) |
                                      selection_kind_bit(GuiSelectionKind::Face) |
                                      selection_kind_bit(GuiSelectionKind::Edge) |
                                      selection_kind_bit(GuiSelectionKind::SketchEntity));
  viewport_.set_selection_semantic_scope(std::move(scope));
}

bool ShellFeatureTools::accept_surface_selection(const GuiSelection& selection) {
  if (!surface() || !layer_.active_handle_id().empty())
    return false;
  const bool curves = command_ == Command::BoundarySurface || command_ == Command::FillSurface;
  auto* combo = curves || surface_pick_->currentIndex() == 1 ? surface_source_ : surface_target_;
  for (int row = 1; row < combo->count(); ++row) {
    const int index = combo->itemData(row).toInt();
    const auto& input = surface_inputs_.at(index);
    if (surface_input_kind(input) != selection.kind)
      continue;
    bool match = input.semantic_id == selection.semantic_id;
    if (!match && input.trimming &&
        input.trimming->source_kind() == TrimmingReferenceSourceKind::ProfileRegion) {
      const auto& profile = std::get<ProfileRegionReference>(input.trimming->source());
      for (const char* family : {"rectangle-profile/", "circle-profile/", "closed-profile/",
                                 "arc-profile/", "composite-profile/"})
        match |= selection.semantic_id ==
                 "sketch/" + profile.sketch().value() + "/" + family + profile.profile().value();
    }
    if (!match)
      continue;
    combo->setCurrentIndex(row);
    if (curves || command_ == Command::SurfaceStitch)
      (void)append_surface_input(index);
    return true;
  }
  return false;
}

void ShellFeatureTools::refresh_surface_enablement(bool idle_part) {
  // Use source contracts for enablement; geometric suitability is checked by preview.
  bool curves = false, surfaces = false, shell = false;
  std::size_t surface_count = 0;
  if (idle_part) {
    const auto* part = session_.part_document();
    const auto* cache = session_.part_shape_cache();
    curves = !part->path_curves().empty();
    if (cache)
      for (const auto& body : part->bodies())
        if (body.visibility() == BodyVisibility::Visible && cache->find_body_shape(body.id())) {
          shell |= body.kind() == BodyKind::Surface;
          if (body.kind() == BodyKind::Surface)
            ++surface_count;
          else if (body.kind() == BodyKind::Solid) {
            auto faces = geometry::FinishingSelectionBuilder::build(
                *part, *cache, body.id(), PartFeatureInputRole::ShellRemovalFace);
            if (faces)
              surface_count += faces.value().size();
            auto edges = geometry::FinishingSelectionBuilder::build(
                *part, *cache, body.id(), PartFeatureInputRole::FilletEdge);
            curves |= edges && !edges.value().empty();
          }
        }
    surfaces = surface_count > 0;
  }
  for (const auto& [command, action] : surface_actions_)
    action->setEnabled(
        idle_part &&
        (command == Command::BoundarySurface || command == Command::FillSurface ? curves
         : command == Command::ClosedShellToSolid                               ? shell
         : command == Command::SurfaceStitch
             ? surface_count >= 2
             : surfaces && (curves || (command == Command::TrimSurface &&
                                       !spatial_inputs_for(Command::Sweep).empty()))));
}
} // namespace blcad::gui
