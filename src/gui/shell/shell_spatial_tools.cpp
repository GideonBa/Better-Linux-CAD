#include "blcad/gui/shell/shell_feature_tools.hpp"

#include "blcad/gui/gui_modeling_workspace.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace blcad::gui {
namespace {
QString title(ShellFeatureTools::Command command) {
  using C = ShellFeatureTools::Command;
  return command == C::PathCurve ? QStringLiteral("Pfad erstellen")
         : command == C::Sweep   ? QStringLiteral("Sweep")
                                 : QStringLiteral("Loft");
}
PathSegmentReference with_reversal(const PathSegmentReference& source, bool reversed) {
  if (source.source_kind() == PathSegmentSourceKind::PlanarSketchCurve)
    return PathSegmentReference::create_planar(*source.planar_sketch(), *source.entity(),
                                               *source.planar_curve_kind(), reversed)
        .value();
  if (source.source_kind() == PathSegmentSourceKind::ConstructionLine)
    return PathSegmentReference::create_construction_line(*source.construction_line(), reversed)
        .value();
  return PathSegmentReference::create_sketch_3d(*source.sketch_3d(), *source.entity(),
                                                *source.sketch_3d_curve_kind(), reversed)
      .value();
}
} // namespace

bool ShellFeatureTools::spatial() const noexcept {
  return command_ == Command::PathCurve || command_ == Command::Sweep || command_ == Command::Loft;
}
bool ShellFeatureTools::spatial_scalar() const noexcept {
  return command_ == Command::Sweep && sweep_twist_->isChecked();
}
QString ShellFeatureTools::spatial_summary() const {
  if (command_ == Command::PathCurve)
    return QStringLiteral("%1 verbundene Segmente. Pfadvorschau gültig.")
        .arg(spatial_order_->count());
  if (command_ == Command::Loft)
    return QStringLiteral("%1 Schnitte in Listenreihenfolge. Vorschau gültig.")
        .arg(spatial_order_->count());
  return QStringLiteral("Profil entlang %1. Vorschau gültig.").arg(spatial_path_->currentText());
}

std::vector<ShellFeatureTools::SpatialInput>
ShellFeatureTools::spatial_inputs_for(Command command) const {
  std::vector<SpatialInput> choices;
  const auto* part = session_.part_document();
  if (!part)
    return choices;
  for (const auto& sketch : part->sketches()) {
    if (command != Command::PathCurve) {
      const auto add_profiles = [&](const auto& profiles) {
        for (const auto& profile : profiles)
          choices.push_back({sketch.name() + " / " + profile.id().value(),
                             "sketch/" + sketch.id().value() + "/profile/" + profile.id().value(),
                             {},
                             std::pair{sketch.id(), profile.id()},
                             {}});
      };
      add_profiles(sketch.rectangle_profiles());
      add_profiles(sketch.circle_profiles());
      add_profiles(sketch.closed_profiles());
      add_profiles(sketch.arc_closed_profiles());
      add_profiles(sketch.composite_closed_profiles());
    } else {
      const auto add_segments = [&](const auto& entities, PlanarPathCurveKind kind) {
        for (const auto& entity : entities) {
          auto reference = PathSegmentReference::create_planar(sketch.id(), entity.id(), kind);
          if (reference)
            choices.push_back({sketch.name() + " / " + entity.id().value(),
                               "sketch/" + sketch.id().value() + "/entity/" + entity.id().value(),
                               reference.value(),
                               {},
                               {}});
        }
      };
      add_segments(sketch.line_segments(), PlanarPathCurveKind::Line);
      add_segments(sketch.arc_segments(), PlanarPathCurveKind::Arc);
      add_segments(sketch.spline_segments(), PlanarPathCurveKind::Spline);
    }
  }
  if (command == Command::PathCurve) {
    for (const auto& line : part->construction_lines())
      if (line.kind() == ConstructionLineKind::ThroughTwoPoints) {
        auto reference = PathSegmentReference::create_construction_line(line.id());
        if (reference)
          choices.push_back(
              {line.name(), "construction-line/" + line.id().value(), reference.value(), {}, {}});
      }
    for (const auto& sketch : part->sketches_3d()) {
      const auto add = [&](const auto& entities, Sketch3DPathCurveKind kind) {
        for (const auto& entity : entities) {
          auto reference = PathSegmentReference::create_sketch_3d(sketch.id(), entity.id(), kind);
          if (reference)
            choices.push_back({sketch.name() + " / " + entity.id().value(),
                               "sketch3d/" + sketch.id().value() + "/entity/" + entity.id().value(),
                               reference.value(),
                               {},
                               {}});
        }
      };
      add(sketch.lines(), Sketch3DPathCurveKind::Line);
      add(sketch.arcs(), Sketch3DPathCurveKind::Arc);
      add(sketch.polylines(), Sketch3DPathCurveKind::Polyline);
      add(sketch.splines(), Sketch3DPathCurveKind::Spline);
      add(sketch.helices(), Sketch3DPathCurveKind::Helix);
    }
  } else {
    for (const auto& path : part->path_curves())
      if (path.closure() == PathClosure::Open)
        choices.push_back(
            {path.name() + " (offener Pfad)", "path/" + path.id().value(), {}, {}, path.id()});
  }
  return choices;
}

void ShellFeatureTools::build_spatial_controls(QWidget* panel) {
  spatial_controls_ = new QWidget(panel);
  spatial_controls_->setMinimumWidth(280);
  auto* form = new QFormLayout(spatial_controls_);
  spatial_mode_ = new QComboBox(spatial_controls_);
  spatial_mode_->setObjectName("blcad.spatial.mode");
  spatial_mode_->addItems(
      {QStringLiteral("Volumenkörper"), QStringLiteral("Schnitt"), QStringLiteral("Fläche")});
  form->addRow(QStringLiteral("Ergebnis"), spatial_mode_);
  spatial_target_ = new QComboBox(spatial_controls_);
  spatial_target_->setObjectName("blcad.spatial.target");
  form->addRow(QStringLiteral("Zielkörper"), spatial_target_);
  spatial_pick_ = new QComboBox(spatial_controls_);
  spatial_pick_->setObjectName("blcad.spatial.pick_role");
  form->addRow(QStringLiteral("Modellklick wählt"), spatial_pick_);
  spatial_source_ = new QComboBox(spatial_controls_);
  spatial_source_->setObjectName("blcad.spatial.source");
  form->addRow(QStringLiteral("Profil / Element"), spatial_source_);
  spatial_add_ = new QPushButton(QStringLiteral("Zur Liste hinzufügen"), spatial_controls_);
  spatial_add_->setObjectName("blcad.spatial.add");
  form->addRow(spatial_add_);
  spatial_order_ = new QListWidget(spatial_controls_);
  spatial_order_->setObjectName("blcad.spatial.order");
  spatial_order_->setDragDropMode(QAbstractItemView::InternalMove);
  spatial_order_->setMinimumHeight(90);
  spatial_order_->setMaximumHeight(150);
  form->addRow(spatial_order_);
  spatial_order_buttons_ = new QWidget(spatial_controls_);
  auto* ordering = new QVBoxLayout(spatial_order_buttons_);
  ordering->setContentsMargins(0, 0, 0, 0);
  auto* buttons = new QHBoxLayout();
  ordering->addLayout(buttons);
  buttons->setContentsMargins(0, 0, 0, 0);
  for (int direction : {-1, 1, 0}) {
    auto* button = new QPushButton(direction < 0   ? QStringLiteral("Hoch")
                                   : direction > 0 ? QStringLiteral("Runter")
                                                   : QStringLiteral("Entfernen"),
                                   spatial_order_buttons_);
    button->setObjectName(direction < 0   ? "blcad.spatial.order.up"
                          : direction > 0 ? "blcad.spatial.order.down"
                                          : "blcad.spatial.order.remove");
    buttons->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, direction] {
      const int row = spatial_order_->currentRow();
      if (!spatial() || row < 0)
        return;
      if (direction == 0) {
        delete spatial_order_->takeItem(row);
        refresh_spatial_preview();
      } else
        (void)move_spatial_input(row, row + direction);
    });
  }
  form->addRow(spatial_order_buttons_);
  spatial_order_hint_ =
      new QLabel(QStringLiteral("Liste ziehen oder Hoch/Runter verwenden. Häkchen kehren "
                                "Segmentrichtung bzw. Schnittnormalen um."),
                 spatial_order_buttons_);
  spatial_order_hint_->setWordWrap(true);
  spatial_order_hint_->setMinimumHeight(64);
  spatial_order_hint_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  spatial_order_hint_->setObjectName("blcad.spatial.order_hint");
  ordering->addWidget(spatial_order_hint_);
  spatial_path_ = new QComboBox(spatial_controls_);
  spatial_path_->setObjectName("blcad.spatial.path");
  form->addRow(QStringLiteral("Pfad / Leitpfad"), spatial_path_);
  spatial_guide_ = new QComboBox(spatial_controls_);
  spatial_guide_->setObjectName("blcad.spatial.guide");
  form->addRow(QStringLiteral("Führung"), spatial_guide_);
  spatial_add_guide_ = new QPushButton(QStringLiteral("Führung hinzufügen"), spatial_controls_);
  spatial_add_guide_->setObjectName("blcad.spatial.add_guide");
  form->addRow(spatial_add_guide_);
  spatial_guides_ = new QListWidget(spatial_controls_);
  spatial_guides_->setObjectName("blcad.spatial.guides");
  spatial_guides_->setDragDropMode(QAbstractItemView::InternalMove);
  spatial_guides_->setMaximumHeight(75);
  form->addRow(spatial_guides_);
  spatial_remove_guide_ = new QPushButton(QStringLiteral("Führung entfernen"), spatial_controls_);
  form->addRow(spatial_remove_guide_);
  spatial_closed_ = new QCheckBox(QStringLiteral("Pfad geschlossen"), spatial_controls_);
  form->addRow(spatial_closed_);
  spatial_orientation_ = new QComboBox(spatial_controls_);
  spatial_orientation_->addItems({QStringLiteral("Profilnormal"),
                                  QStringLiteral("Minimale Verdrehung"),
                                  QStringLiteral("Fester Oben-Vektor")});
  form->addRow(QStringLiteral("Orientierung"), spatial_orientation_);
  for (std::size_t i = 0; i < spatial_up_.size(); ++i) {
    auto* input = new QDoubleSpinBox(spatial_controls_);
    input->setRange(-1e6, 1e6);
    input->setDecimals(6);
    input->setKeyboardTracking(false);
    spatial_up_[i] = input;
    form->addRow(QStringLiteral("Oben %1").arg(QStringLiteral("XYZ").at(i)), input);
    connect(input, &QDoubleSpinBox::valueChanged, this, [this] {
      if (spatial())
        refresh_spatial_preview();
    });
  }
  spatial_continuity_ = new QComboBox(spatial_controls_);
  spatial_continuity_->setObjectName("blcad.spatial.continuity");
  spatial_continuity_->addItems({QStringLiteral("C0"), QStringLiteral("G1"), QStringLiteral("G2")});
  form->addRow(QStringLiteral("Stetigkeit"), spatial_continuity_);
  sweep_twist_ = new QCheckBox(QStringLiteral("Verdrehung aktiv"), spatial_controls_);
  form->addRow(sweep_twist_);
  panel->layout()->addWidget(spatial_controls_);
  spatial_controls_->hide();
  connect(spatial_add_, &QPushButton::clicked, this, [this] {
    if (spatial()) {
      auto added = append_spatial_input(spatial_source_->currentData().toInt());
      if (!added)
        report(added.error());
    }
  });
  connect(spatial_add_guide_, &QPushButton::clicked, this, [this] {
    if (command_ != Command::Loft || spatial_guide_->currentData().toString().isEmpty())
      return;
    const auto id = spatial_guide_->currentData();
    for (int i = 0; i < spatial_guides_->count(); ++i)
      if (spatial_guides_->item(i)->data(Qt::UserRole) == id)
        return;
    auto* item = new QListWidgetItem(spatial_guide_->currentText(), spatial_guides_);
    item->setData(Qt::UserRole, id);
    refresh_spatial_preview();
  });
  connect(spatial_remove_guide_, &QPushButton::clicked, this, [this] {
    delete spatial_guides_->takeItem(spatial_guides_->currentRow());
    if (spatial())
      refresh_spatial_preview();
  });
  for (auto* input :
       {spatial_mode_, spatial_target_, spatial_path_, spatial_orientation_, spatial_continuity_})
    connect(input, &QComboBox::currentIndexChanged, this, [this] {
      if (spatial()) {
        update_spatial_controls();
        refresh_spatial_preview();
      }
    });
  connect(spatial_guide_, &QComboBox::currentIndexChanged, this, [this] {
    if (command_ == Command::Sweep)
      refresh_spatial_preview();
  });
  connect(spatial_pick_, &QComboBox::currentIndexChanged, this, [this] {
    if (spatial())
      update_spatial_pick_scope();
  });
  connect(spatial_source_, &QComboBox::currentIndexChanged, this, [this] {
    if (command_ == Command::Sweep)
      refresh_spatial_preview();
  });
  connect(spatial_order_, &QListWidget::itemChanged, this, [this] {
    if (spatial())
      refresh_spatial_preview();
  });
  for (auto* list : {spatial_order_, spatial_guides_})
    connect(list->model(), &QAbstractItemModel::rowsMoved, this, [this] {
      if (spatial())
        refresh_spatial_preview();
    });
  connect(spatial_closed_, &QCheckBox::toggled, this, [this] {
    if (spatial())
      refresh_spatial_preview();
  });
  connect(sweep_twist_, &QCheckBox::toggled, this, [this] {
    if (command_ == Command::Sweep) {
      update_spatial_controls();
      refresh_spatial_preview();
    }
  });
}

Result<std::size_t> ShellFeatureTools::begin_spatial(Command command) {
  const auto fail = [](std::string message) {
    return Result<std::size_t>::failure(Error::validation("gui.shell.spatial", std::move(message)));
  };
  const auto* part = session_.part_document();
  if (active() || session_.task().active() || window_.active_sketch() || !part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_shape_cache())
    return fail("Räumliche Operation benötigt ein Bauteil ohne aktiven Befehl");
  if (command != Command::PathCurve && command != Command::Sweep && command != Command::Loft)
    return fail("Unbekannter räumlicher Befehl");
  spatial_inputs_ = spatial_inputs_for(command);
  const auto profiles = std::count_if(spatial_inputs_.begin(), spatial_inputs_.end(),
                                      [](const auto& input) { return input.profile.has_value(); });
  const auto paths =
      std::count_if(part->path_curves().begin(), part->path_curves().end(),
                    [](const auto& path) { return path.closure() == PathClosure::Open; });
  if ((command == Command::PathCurve && spatial_inputs_.empty()) ||
      (command == Command::Sweep && (paths < 1 || (profiles < 1 && paths < 2))) ||
      (command == Command::Loft && profiles < 2 && paths < 2))
    return fail("Zuerst passende Profile und verbundene Pfade erstellen");
  const QSignalBlocker source(spatial_source_), mode(spatial_mode_), target(spatial_target_),
      path(spatial_path_), guide(spatial_guide_), order(spatial_order_), guides(spatial_guides_),
      closed(spatial_closed_), orientation(spatial_orientation_), continuity(spatial_continuity_),
      twist(sweep_twist_);
  spatial_source_->clear();
  spatial_path_->clear();
  spatial_guide_->clear();
  spatial_target_->clear();
  spatial_order_->clear();
  spatial_guides_->clear();
  spatial_pick_->clear();
  for (std::size_t i = 0; i < spatial_inputs_.size(); ++i)
    spatial_source_->addItem(QString::fromStdString(spatial_inputs_[i].label), static_cast<int>(i));
  if (command == Command::Loft)
    spatial_path_->addItem(QStringLiteral("Keiner"), QString());
  spatial_guide_->addItem(QStringLiteral("Keine"), QString());
  for (const auto& item : part->path_curves())
    if (item.closure() == PathClosure::Open) {
      spatial_path_->addItem(QString::fromStdString(item.name()),
                             QString::fromStdString(item.id().value()));
      spatial_guide_->addItem(QString::fromStdString(item.name()),
                              QString::fromStdString(item.id().value()));
    }
  for (const auto& body : part->bodies())
    if (body.kind() == BodyKind::Solid && body.visibility() == BodyVisibility::Visible &&
        session_.part_shape_cache()->find_body_shape(body.id()))
      spatial_target_->addItem(QString::fromStdString(body.name()),
                               QString::fromStdString(body.id().value()));
  if (command == Command::Sweep)
    spatial_pick_->addItems(
        {QStringLiteral("Profil"), QStringLiteral("Pfad"), QStringLiteral("Führung")});
  else
    spatial_pick_->addItems({QStringLiteral("Schnitt / Segment hinzufügen"),
                             QStringLiteral("Leitpfad"), QStringLiteral("Führung hinzufügen")});
  for (std::size_t n = 1;; ++n) {
    const auto suffix = std::to_string(n);
    spatial_feature_ = FeatureId("feature.spatial." + suffix);
    spatial_path_id_ = PathCurveId("path.spatial." + suffix);
    spatial_result_ = BodyId("body.spatial." + suffix);
    spatial_twist_parameter_ = ParameterId("parameter.spatial." + suffix + ".twist");
    spatial_name_ = title(command).toStdString() + " " + suffix;
    if (part->dependency_graph().has_node(spatial_feature_.value()) ||
        part->dependency_graph().has_node(spatial_path_id_.value()) ||
        part->find_body(spatial_result_) || part->find_parameter(spatial_twist_parameter_) ||
        std::any_of(part->parameters().begin(), part->parameters().end(),
                    [&](const auto& p) { return p.name() == spatial_name_ + " Verdrehung"; }) ||
        std::any_of(part->bodies().begin(), part->bodies().end(),
                    [&](const auto& b) { return b.name() == spatial_name_; }))
      continue;
    break;
  }
  spatial_mode_->setCurrentIndex(
      command != Command::PathCurve && profiles < (command == Command::Loft ? 2 : 1) ? 2 : 0);
  spatial_closed_->setChecked(false);
  spatial_orientation_->setCurrentIndex(1);
  spatial_continuity_->setCurrentIndex(0);
  for (std::size_t i = 0; i < 3; ++i) {
    const QSignalBlocker block(spatial_up_[i]);
    spatial_up_[i]->setValue(i == 1 ? 1 : 0);
  }
  sweep_twist_->setChecked(true);
  spatial_twist_ = 0;
  if (command == Command::Sweep && selected_profile_) {
    for (std::size_t i = 0; i < spatial_inputs_.size(); ++i)
      if (spatial_inputs_[i].profile == selected_profile_)
        spatial_source_->setCurrentIndex(i);
  }
  command_ = command;
  spatial_order_hint_->setText(
      command == Command::Loft
          ? QStringLiteral(
                "Schnitte mit Hoch/Runter ordnen. Häkchen kehren Normalen um. "
                "Profilsegmente werden automatisch angepasst; Skizzen bleiben unverändert.")
          : QStringLiteral("Liste ziehen oder Hoch/Runter verwenden. Häkchen kehren "
                           "Segmentrichtung bzw. Schnittnormalen um."));
  if (!session_.task().begin(std::string(command_id())) ||
      !session_.task().begin_parameter_editing() || !session_.task().show_preview()) {
    command_ = Command::None;
    if (session_.task().active())
      (void)session_.task().cancel();
    return fail("Räumliche Aufgabe konnte nicht gestartet werden");
  }
  previous_selection_mask_ = viewport_.selection_filter_mask();
  previous_semantic_scope_ = viewport_.selection_semantic_scope();
  update_spatial_pick_scope();
  show_task_panel(title(command), QStringLiteral("deg"), 0);
  update_spatial_controls();
  refresh_spatial_preview();
  window_.refresh();
  return Result<std::size_t>::success(1);
}

void ShellFeatureTools::update_spatial_controls() {
  auto* form = static_cast<QFormLayout*>(spatial_controls_->layout());
  const bool path = command_ == Command::PathCurve, sweep = command_ == Command::Sweep,
             loft = command_ == Command::Loft;
  form->setRowVisible(spatial_mode_, !path);
  form->setRowVisible(spatial_target_, !path && spatial_mode_->currentIndex() == 1);
  form->setRowVisible(spatial_pick_, !path);
  for (QWidget* widget :
       std::array<QWidget*, 3>{spatial_add_, spatial_order_, spatial_order_buttons_})
    form->setRowVisible(widget, !sweep);
  form->setRowVisible(spatial_path_, !path);
  form->setRowVisible(spatial_guide_, !path);
  form->setRowVisible(spatial_add_guide_, loft);
  form->setRowVisible(spatial_guides_, loft);
  form->setRowVisible(spatial_remove_guide_, loft);
  form->setRowVisible(spatial_closed_, path);
  form->setRowVisible(spatial_orientation_, path);
  for (auto* input : spatial_up_)
    form->setRowVisible(input, path && spatial_orientation_->currentIndex() == 2);
  form->setRowVisible(spatial_continuity_, loft);
  form->setRowVisible(sweep_twist_, sweep);
  numeric_editor_->setVisible(spatial_scalar());
  value_label_->setVisible(spatial_scalar());
  // QFormLayout can allocate less than its row widgets' minimum heights
  // inside a resizable scroll child. Keep its preferred row heights scrollable.
  spatial_controls_->setMinimumHeight(form->sizeHint().height());
  spatial_controls_->updateGeometry();
}

void ShellFeatureTools::update_spatial_pick_scope() {
  std::vector<std::string> scope;
  if (command_ == Command::PathCurve || spatial_pick_->currentIndex() == 0) {
    for (const auto& input : spatial_inputs_) {
      scope.push_back(input.semantic_id);
      if (input.profile)
        for (const char* family : {"rectangle-profile/", "circle-profile/", "closed-profile/",
                                   "arc-profile/", "composite-profile/"})
          scope.push_back("sketch/" + input.profile->first.value() + "/" + family +
                          input.profile->second.value());
    }
  } else {
    for (const auto& path : session_.part_document()->path_curves()) {
      if (path.closure() != PathClosure::Open)
        continue;
      scope.push_back("path/" + path.id().value());
      for (std::size_t i = 0; i < path.segments().size(); ++i)
        scope.push_back("path/" + path.id().value() + "/segment/" + std::to_string(i));
    }
  }
  viewport_.set_selection_filter_mask(
      selection_kind_bit(GuiSelectionKind::SketchEntity) |
      (command_ == Command::PathCurve ? selection_kind_bit(GuiSelectionKind::Datum) : 0));
  viewport_.set_selection_semantic_scope(std::move(scope));
}

Result<std::size_t> ShellFeatureTools::append_spatial_input(int choice) {
  if (!spatial() || command_ == Command::Sweep || choice < 0 ||
      static_cast<std::size_t>(choice) >= spatial_inputs_.size())
    return Result<std::size_t>::failure(Error::validation("spatial", "Ungültiges Listenelement"));
  for (int i = 0; i < spatial_order_->count(); ++i)
    if (spatial_order_->item(i)->data(Qt::UserRole).toInt() == choice)
      return Result<std::size_t>::failure(
          Error::validation("spatial", "Element ist bereits in der Liste"));
  {
    const QSignalBlocker block(spatial_order_);
    auto* item =
        new QListWidgetItem(QString::fromStdString(spatial_inputs_[choice].label), spatial_order_);
    item->setData(Qt::UserRole, choice);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    spatial_order_->setCurrentItem(item);
  }
  refresh_spatial_preview();
  return Result<std::size_t>::success(spatial_order_->count());
}
Result<std::size_t> ShellFeatureTools::move_spatial_input(int from, int to) {
  if (!spatial() || from < 0 || to < 0 || from >= spatial_order_->count() ||
      to >= spatial_order_->count())
    return Result<std::size_t>::failure(Error::validation("spatial", "Ungültige Listenposition"));
  auto* item = spatial_order_->takeItem(from);
  spatial_order_->insertItem(to, item);
  spatial_order_->setCurrentItem(item);
  refresh_spatial_preview();
  return Result<std::size_t>::success(spatial_order_->count());
}

Result<std::size_t> ShellFeatureTools::rebuild_spatial_controller() {
  if (viewport_.mouseGrabber() == &viewport_)
    viewport_.release_interaction_mouse();
  layer_.cancel();
  drag_start_value_.reset();
  coordinator_.cancel();
  auto& sweep = coordinator_.path_sweep();
  if (command_ == Command::PathCurve) {
    auto begun = sweep.begin_path_curve(
        session_, spatial_path_id_, spatial_name_,
        spatial_closed_->isChecked() ? PathClosure::Closed : PathClosure::Open,
        static_cast<PathOrientationRule>(spatial_orientation_->currentIndex()));
    if (!begun)
      return begun;
    if (spatial_orientation_->currentIndex() == 2)
      sweep.set_up_vector(
          Vector3{spatial_up_[0]->value(), spatial_up_[1]->value(), spatial_up_[2]->value()});
    for (int i = 0; i < spatial_order_->count(); ++i) {
      const auto* item = spatial_order_->item(i);
      auto added = sweep.add_segment(
          with_reversal(*spatial_inputs_.at(item->data(Qt::UserRole).toInt()).segment,
                        item->checkState() == Qt::Checked));
      if (!added)
        return added;
    }
  } else {
    const int mode = spatial_mode_->currentIndex();
    const BodyId result = mode == 1
                              ? BodyId(spatial_target_->currentData().toString().toStdString())
                              : spatial_result_;
    std::optional<Body> pending;
    if (mode != 1) {
      auto body =
          Body::create(result, spatial_name_, mode == 2 ? BodyKind::Surface : BodyKind::Solid);
      if (!body)
        return Result<std::size_t>::failure(body.error());
      pending = body.value();
    }
    if (command_ == Command::Sweep) {
      const auto& source = spatial_inputs_.at(spatial_source_->currentData().toInt());
      Result<SweepProfileReference> profile =
          source.profile
              ? SweepProfileReference::create_closed_region(
                    ProfileRegionReference::create(source.profile->first, source.profile->second,
                                                   PartFeatureInputRole::SweepProfile)
                        .value())
              : SweepProfileReference::create_open_path(*source.path);
      if (!profile)
        return Result<std::size_t>::failure(profile.error());
      std::optional<Parameter> twist;
      if (sweep_twist_->isChecked()) {
        auto parameter =
            Parameter::create_angle(spatial_twist_parameter_, spatial_name_ + " Verdrehung",
                                    Quantity::angle_deg(spatial_twist_).value());
        if (!parameter)
          return Result<std::size_t>::failure(parameter.error());
        twist = parameter.value();
      }
      auto begun = sweep.begin_sweep(
          session_, spatial_feature_, spatial_name_, static_cast<SweepFeatureKind>(mode),
          profile.value(), PathCurveId(spatial_path_->currentData().toString().toStdString()),
          result, pending, twist);
      if (!begun)
        return begun;
      if (twist)
        sweep.set_twist(spatial_twist_parameter_, spatial_twist_);
      const auto guide = spatial_guide_->currentData().toString();
      if (!guide.isEmpty())
        sweep.set_guide(PathCurveId(guide.toStdString()));
      GuiFeatureManipulatorFrame frame;
      auto segments = geometry::GeometryRecomputeExecutor{}.resolve_path_segments(
          *session_.part_document(),
          PathCurveId(spatial_path_->currentData().toString().toStdString()),
          *session_.part_shape_cache());
      if (segments && !segments.value().empty()) {
        const auto& first = segments.value().front();
        frame.origin = first.start;
        frame.plane_normal = normalized(Vector3{
            first.end.x - first.start.x, first.end.y - first.start.y, first.end.z - first.start.z});
        frame.reference_direction = normalized(
            cross(std::abs(frame.plane_normal.x) < .9 ? Vector3{1, 0, 0} : Vector3{0, 1, 0},
                  frame.plane_normal));
      }
      sweep.set_manipulator_frame(frame);
    } else {
      auto& loft = coordinator_.loft();
      auto begun = loft.begin_loft(session_, spatial_feature_, spatial_name_,
                                   static_cast<LoftFeatureKind>(mode), result, pending);
      if (!begun)
        return begun;
      for (int i = 0; i < spatial_order_->count(); ++i) {
        const auto* item = spatial_order_->item(i);
        const auto& source = spatial_inputs_.at(item->data(Qt::UserRole).toInt());
        auto added = source.profile
                         ? loft.add_closed_section(source.profile->first, source.profile->second,
                                                   item->checkState() == Qt::Checked)
                         : loft.add_open_section(*source.path, item->checkState() == Qt::Checked);
        if (!added)
          return added;
      }
      const auto path = spatial_path_->currentData().toString();
      if (!path.isEmpty())
        loft.set_path_curve(PathCurveId(path.toStdString()));
      for (int i = 0; i < spatial_guides_->count(); ++i) {
        auto added = loft.add_guide_curve(
            PathCurveId(spatial_guides_->item(i)->data(Qt::UserRole).toString().toStdString()));
        if (!added)
          return added;
      }
      loft.set_continuity(static_cast<LoftContinuity>(spatial_continuity_->currentIndex()));
    }
  }
  coordinator_.refresh_handles();
  return Result<std::size_t>::success(1);
}

void ShellFeatureTools::refresh_spatial_preview() {
  if (command_ == Command::Sweep && coordinator_.path_sweep().active())
    spatial_twist_ = coordinator_.path_sweep().twist();
  auto begun = rebuild_spatial_controller();
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

bool ShellFeatureTools::select_spatial_profile(SketchId sketch, ProfileId profile) {
  const auto* part = session_.part_document();
  const auto* source = part ? part->find_sketch(sketch) : nullptr;
  if ((command_ != Command::Sweep && command_ != Command::Loft) || !session_.task().active() ||
      session_.task().command_id() != command_id() || !part || !source ||
      !GuiModelingWorkspace::has_materialized_profile(*source, profile))
    return false;
  const auto selected = std::pair{std::move(sketch), std::move(profile)};
  for (std::size_t i = 0; i < spatial_inputs_.size(); ++i)
    if (spatial_inputs_[i].profile == selected) {
      if (command_ == Command::Loft)
        return append_spatial_input(static_cast<int>(i)).has_value();
      spatial_source_->setCurrentIndex(static_cast<int>(i));
      return true;
    }
  return false;
}

bool ShellFeatureTools::accept_spatial_selection(const GuiSelection& selection) {
  if (!spatial())
    return false;
  const auto matches = [&](const SpatialInput& input) {
    if (input.semantic_id == selection.semantic_id)
      return true;
    if (input.profile) {
      for (const char* family : {"profile/", "rectangle-profile/", "circle-profile/",
                                 "closed-profile/", "arc-profile/", "composite-profile/"})
        if (selection.semantic_id ==
            "sketch/" + input.profile->first.value() + "/" + family + input.profile->second.value())
          return true;
    }
    return false;
  };
  if (command_ == Command::PathCurve || spatial_pick_->currentIndex() == 0) {
    for (std::size_t i = 0; i < spatial_inputs_.size(); ++i)
      if (matches(spatial_inputs_[i])) {
        if (command_ == Command::Sweep)
          spatial_source_->setCurrentIndex(i);
        else
          (void)append_spatial_input(i);
        return true;
      }
  } else {
    const auto* part = session_.part_document();
    std::vector<PathCurveId> matches;
    for (const auto& path : part->path_curves()) {
      if (path.closure() != PathClosure::Open)
        continue;
      bool match = selection.semantic_id == "path/" + path.id().value() ||
                   selection.semantic_id.starts_with("path/" + path.id().value() + "/segment/");
      for (const auto& segment : path.segments()) {
        if (segment.planar_sketch() && segment.entity())
          match |= selection.semantic_id == "sketch/" + segment.planar_sketch()->value() +
                                                "/entity/" + segment.entity()->value();
        if (segment.sketch_3d() && segment.entity())
          match |= selection.semantic_id == "sketch3d/" + segment.sketch_3d()->value() +
                                                "/entity/" + segment.entity()->value();
      }
      if (match)
        matches.push_back(path.id());
    }
    if (matches.size() != 1)
      return false;
    auto* combo = spatial_pick_->currentIndex() == 1 ? spatial_path_ : spatial_guide_;
    combo->setCurrentIndex(combo->findData(QString::fromStdString(matches.front().value())));
    if (command_ == Command::Loft && spatial_pick_->currentIndex() == 2)
      spatial_add_guide_->click();
    return true;
  }
  return false;
}

void ShellFeatureTools::refresh_spatial_enablement(bool idle_part) {
  const auto choices = idle_part ? spatial_inputs_for(Command::Sweep) : std::vector<SpatialInput>{};
  const auto profiles = std::count_if(choices.begin(), choices.end(),
                                      [](const auto& input) { return input.profile.has_value(); });
  const auto paths = std::count_if(choices.begin(), choices.end(),
                                   [](const auto& input) { return input.path.has_value(); });
  for (const auto& [command, action] : spatial_actions_)
    action->setEnabled(idle_part && session_.part_shape_cache() &&
                       (command == Command::PathCurve ? !spatial_inputs_for(command).empty()
                        : command == Command::Sweep   ? paths >= 1 && (profiles >= 1 || paths >= 2)
                                                      : profiles >= 2 || paths >= 2));
}
} // namespace blcad::gui
