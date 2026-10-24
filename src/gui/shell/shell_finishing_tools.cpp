#include "blcad/gui/shell/shell_feature_tools.hpp"

#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalBlocker>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace blcad::gui {
namespace {
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
  for (const auto& [key, text] : {std::pair{"top_front", "Oben vorne"},
                                  {"top_back", "Oben hinten"},
                                  {"top_right", "Oben rechts"},
                                  {"top_left", "Oben links"},
                                  {"bottom_front", "Unten vorne"},
                                  {"bottom_back", "Unten hinten"},
                                  {"bottom_right", "Unten rechts"},
                                  {"bottom_left", "Unten links"},
                                  {"front_right", "Vorne rechts"},
                                  {"front_left", "Vorne links"},
                                  {"back_right", "Hinten rechts"},
                                  {"back_left", "Hinten links"},
                                  {"top", "Oben"},
                                  {"bottom", "Unten"},
                                  {"right", "Rechts"},
                                  {"left", "Links"},
                                  {"front", "Vorne"},
                                  {"back", "Hinten"}})
    if (role == key)
      return QString::fromStdString(label.substr(0, marker)) + QStringLiteral(" / ") +
             QString::fromUtf8(text);
  return QString::fromStdString(label);
}
QString command_title(ShellFeatureTools::Command command) {
  using C = ShellFeatureTools::Command;
  if (command == C::Fillet)
    return QStringLiteral("Verrunden");
  if (command == C::Chamfer)
    return QStringLiteral("Fase");
  if (command == C::Shell)
    return QStringLiteral("Aushöhlen");
  return QStringLiteral("Formschräge");
}
} // namespace

bool ShellFeatureTools::finishing() const noexcept {
  return command_ == Command::Fillet || command_ == Command::Chamfer ||
         command_ == Command::Shell || command_ == Command::Draft;
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
  case Command::None:
    return {};
  }
  return {};
}

void ShellFeatureTools::build_finishing_controls(QWidget* panel, QFormLayout&) {
  finishing_controls_ = new QWidget(panel);
  auto* form = new QFormLayout(finishing_controls_);
  finishing_controls_->setMinimumWidth(280);
  finishing_body_ = new QComboBox(finishing_controls_);
  finishing_body_->setObjectName(QStringLiteral("blcad.finishing.body"));
  form->addRow(QStringLiteral("Körper"), finishing_body_);
  finishing_targets_list_ = new QListWidget(finishing_controls_);
  finishing_targets_list_->setObjectName(QStringLiteral("blcad.finishing.targets"));
  finishing_targets_list_->setSelectionMode(QAbstractItemView::MultiSelection);
  finishing_targets_list_->setMinimumHeight(170);
  form->addRow(new QLabel(QStringLiteral("Elemente auswählen"), finishing_controls_));
  form->addRow(finishing_targets_list_);
  auto* hint =
      new QLabel(QStringLiteral("Kanten oder Flächen im Modell oder in der Liste anklicken. "
                                "Ein weiterer Klick entfernt sie aus der Auswahl."),
                 finishing_controls_);
  hint->setWordWrap(true);
  form->addRow(hint);
  finishing_mode_ = new QComboBox(finishing_controls_);
  finishing_mode_->setObjectName(QStringLiteral("blcad.finishing.mode"));
  form->addRow(QStringLiteral("Modus"), finishing_mode_);
  secondary_numeric_editor_ = new QLineEdit(finishing_controls_);
  secondary_numeric_editor_->setObjectName(QStringLiteral("blcad.finishing.secondary"));
  form->addRow(QStringLiteral("Zweites Maß"), secondary_numeric_editor_);
  draft_axis_ = new QComboBox(finishing_controls_);
  draft_axis_->setObjectName(QStringLiteral("blcad.finishing.pull_axis"));
  form->addRow(QStringLiteral("Zugrichtung"), draft_axis_);
  draft_plane_ = new QComboBox(finishing_controls_);
  draft_plane_->setObjectName(QStringLiteral("blcad.finishing.neutral_plane"));
  form->addRow(QStringLiteral("Neutrale Ebene"), draft_plane_);
  panel->layout()->addWidget(finishing_controls_);
  finishing_controls_->hide();
  connect(finishing_body_, &QComboBox::currentIndexChanged, this, [this] {
    if (finishing())
      reload_finishing_targets();
  });
  connect(finishing_targets_list_, &QListWidget::itemSelectionChanged, this, [this] {
    if (finishing())
      refresh_finishing_preview();
  });
  connect(finishing_mode_, &QComboBox::currentIndexChanged, this, [this, form] {
    if (!finishing())
      return;
    const bool second = command_ == Command::Chamfer && finishing_mode_->currentIndex() > 0;
    form->setRowVisible(secondary_numeric_editor_, second);
    if (second) {
      finishing_second_value_ = finishing_mode_->currentIndex() == 2 ? 45 : 1;
      const QSignalBlocker block(secondary_numeric_editor_);
      secondary_numeric_editor_->setText(QString::number(finishing_second_value_));
      secondary_numeric_editor_->setPlaceholderText(finishing_mode_->currentIndex() == 2 ? "deg"
                                                                                         : "mm");
    }
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

Result<std::size_t> ShellFeatureTools::begin_finishing(Command command) {
  const auto fail = [](std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.finishing", std::move(message)));
  };
  if (active() || session_.task().active() || window_.active_sketch() ||
      session_.document_kind() != GuiDocumentKind::Part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_shape_cache())
    return fail("Bearbeitung benötigt ein Bauteil ohne aktiven Befehl");
  if (command != Command::Fillet && command != Command::Chamfer && command != Command::Shell &&
      command != Command::Draft)
    return fail("Unbekannte Bearbeitungsfunktion");
  const auto& part = *session_.part_document();
  {
    const QSignalBlocker body_block(finishing_body_), mode_block(finishing_mode_),
        axis_block(draft_axis_), plane_block(draft_plane_);
    finishing_body_->clear();
    for (const auto& body : part.bodies()) {
      auto targets = geometry::FinishingSelectionBuilder::build(part, *session_.part_shape_cache(),
                                                                body.id(), selection_role(command));
      if (targets && !targets.value().empty())
        finishing_body_->addItem(QString::fromStdString(body.name()),
                                 QString::fromStdString(body.id().value()));
    }
    if (finishing_body_->count() == 0)
      return fail("Keine eindeutig referenzierbaren Kanten oder Flächen vorhanden");
    for (const auto& selection : session_.selection().entries()) {
      if (selection.kind != GuiSelectionKind::Body || !selection.semantic_id.starts_with("body/"))
        continue;
      const int index =
          finishing_body_->findData(QString::fromStdString(selection.semantic_id.substr(5)));
      if (index >= 0)
        finishing_body_->setCurrentIndex(index);
    }
    finishing_mode_->clear();
    if (command == Command::Chamfer)
      finishing_mode_->addItems({QStringLiteral("Gleiche Abstände"),
                                 QStringLiteral("Zwei Abstände"),
                                 QStringLiteral("Abstand und Winkel")});
    if (command == Command::Shell)
      finishing_mode_->addItems({QStringLiteral("Nach innen"), QStringLiteral("Nach außen")});
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
  for (std::size_t i = 1;; ++i) {
    const std::string base = "finishing." + std::to_string(i);
    const FeatureId id("feature." + base);
    const bool feature_taken =
        part.find_feature(id) || part.find_revolve_feature(id) || part.find_fillet_feature(id) ||
        part.find_chamfer_feature(id) || part.find_shell_feature(id) ||
        part.find_draft_feature(id) || part.find_sweep_feature(id) || part.find_loft_feature(id) ||
        part.find_surface_feature(id) || part.find_linear_pattern_feature(id) ||
        part.find_circular_pattern_feature(id) || part.find_mirror_feature(id) ||
        part.find_body_boolean_feature(id);
    const std::string name = title.toStdString() + " " + std::to_string(i);
    if (feature_taken || part.find_parameter(ParameterId("parameter." + base + ".first")) ||
        part.find_parameter(ParameterId("parameter." + base + ".second")) ||
        std::any_of(part.parameters().begin(), part.parameters().end(), [&](const auto& p) {
          return p.name() == name + " Maß" || p.name() == name + " Zweites Maß";
        }))
      continue;
    finishing_feature_ = id;
    finishing_name_ = name;
    finishing_parameter_ = ParameterId("parameter." + base + ".first");
    finishing_second_parameter_ = ParameterId("parameter." + base + ".second");
    break;
  }
  previous_selection_mask_ = viewport_.selection_filter_mask();
  command_ = command;
  if (!session_.task().begin(std::string(command_id())) ||
      !session_.task().begin_parameter_editing() || !session_.task().show_preview()) {
    command_ = Command::None;
    if (session_.task().active())
      (void)session_.task().cancel();
    return fail("Bearbeitungsaufgabe konnte nicht gestartet werden");
  }
  finishing_first_value_ = command == Command::Draft ? 5 : 1;
  finishing_second_value_ = 1;
  secondary_numeric_editor_->setText(QStringLiteral("1"));
  auto* form = static_cast<QFormLayout*>(finishing_controls_->layout());
  form->setRowVisible(finishing_mode_, command == Command::Chamfer || command == Command::Shell);
  form->setRowVisible(secondary_numeric_editor_, false);
  form->setRowVisible(draft_axis_, command == Command::Draft);
  form->setRowVisible(draft_plane_, command == Command::Draft);
  show_task_panel(title, angular_value() ? QStringLiteral("deg") : QStringLiteral("mm"),
                  finishing_first_value_);
  reload_finishing_targets();
  window_.refresh();
  return Result<std::size_t>::success(1);
}

void ShellFeatureTools::reload_finishing_targets() {
  const auto* part = session_.part_document();
  const auto* cache = session_.part_shape_cache();
  finishing_targets_.clear();
  {
    const QSignalBlocker block(finishing_targets_list_);
    finishing_targets_list_->clear();
    if (part && cache) {
      auto targets = geometry::FinishingSelectionBuilder::build(
          *part, *cache, BodyId(finishing_body_->currentData().toString().toStdString()),
          selection_role(command_));
      if (targets)
        finishing_targets_ = std::move(targets.value());
    }
    for (const auto& target : finishing_targets_)
      finishing_targets_list_->addItem(target_label(target.label));
  }
  viewport_.set_selection_filter_mask(selection_kind_bit(
      command_ == Command::Fillet || command_ == Command::Chamfer ? GuiSelectionKind::Edge
                                                                  : GuiSelectionKind::Face));
  window_.refresh_feature_selection_scene();
  refresh_finishing_preview();
}

Result<std::size_t> ShellFeatureTools::rebuild_finishing_controller() {
  if (viewport_.mouseGrabber() == &viewport_)
    viewport_.release_interaction_mouse();
  layer_.cancel();
  drag_start_value_.reset();
  coordinator_.cancel();
  const BodyId body(finishing_body_->currentData().toString().toStdString());
  const bool angular = angular_value();
  auto quantity = angular ? Quantity::angle_deg(finishing_first_value_)
                          : Quantity::length_mm(finishing_first_value_);
  if (!quantity)
    return Result<std::size_t>::failure(quantity.error());
  auto parameter = angular ? Parameter::create_angle(finishing_parameter_, finishing_name_ + " Maß",
                                                     quantity.value())
                           : Parameter::create_length(finishing_parameter_,
                                                      finishing_name_ + " Maß", quantity.value());
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
                                                   finishing_name_ + " Zweites Maß", q.value())
                         : Parameter::create_length(finishing_second_parameter_,
                                                    finishing_name_ + " Zweites Maß", q.value());
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
  const auto count = finishing_targets_list_->selectedItems().size();
  const bool edges = command_ == Command::Fillet || command_ == Command::Chamfer;
  return QStringLiteral("%1 %2 ausgewählt. Vorschau gültig.")
      .arg(count)
      .arg(edges ? (count == 1 ? QStringLiteral("Kante") : QStringLiteral("Kanten"))
                 : (count == 1 ? QStringLiteral("Fläche") : QStringLiteral("Flächen")));
}

void ShellFeatureTools::refresh_finishing_preview() {
  if (coordinator_.active())
    finishing_first_value_ = current_value();
  auto begun = rebuild_finishing_controller();
  if (!begun) {
    preview_valid_ = false;
    viewport_.clear_feature_preview();
    task_summary_->setText(QString::fromStdString(begun.error().message()));
    refresh_enablement();
    synchronize_overlay();
    return;
  }
  synchronize_numeric_editor(current_value());
  (void)preview_and_publish();
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
  if (finishing())
    for (const auto& target : finishing_targets_)
      items.push_back({std::holds_alternative<EdgeReference>(target.reference)
                           ? geometry::ViewportSceneKind::Edge
                           : geometry::ViewportSceneKind::Face,
                       target.semantic_id, target.shape});
  if (surface())
    for (const auto& target : surface_targets_)
      items.push_back({std::holds_alternative<EdgeReference>(target.reference)
                           ? geometry::ViewportSceneKind::Edge
                           : geometry::ViewportSceneKind::Face,
                       target.semantic_id, target.shape});
  return items;
}
} // namespace blcad::gui
