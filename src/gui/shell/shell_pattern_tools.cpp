#include "blcad/gui/shell/shell_feature_tools.hpp"

#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QWidget>

#include <algorithm>

namespace blcad::gui {
namespace {
QString pattern_title(ShellFeatureTools::Command command) {
  if (command == ShellFeatureTools::Command::LinearPattern)
    return QStringLiteral("Lineares Muster");
  if (command == ShellFeatureTools::Command::CircularPattern)
    return QStringLiteral("Kreismuster");
  return QStringLiteral("Körper spiegeln");
}
} // namespace

bool ShellFeatureTools::pattern() const noexcept {
  return command_ == Command::LinearPattern || command_ == Command::CircularPattern ||
         command_ == Command::BodyMirror;
}
QString ShellFeatureTools::pattern_summary() const {
  const auto sources = pattern_sources_->selectedItems().size();
  return command_ == Command::BodyMirror
             ? QStringLiteral("%1 Körper ausgewählt. Spiegelvorschau gültig.").arg(sources)
             : QStringLiteral(
                   "%1 Körper, %2 Instanzen einschließlich Ausgangslage. Vorschau gültig.")
                   .arg(sources)
                   .arg(coordinator_.pattern_mirror().count());
}

void ShellFeatureTools::build_pattern_controls(QWidget* panel) {
  pattern_controls_ = new QWidget(panel);
  pattern_controls_->setMinimumWidth(280);
  auto* form = new QFormLayout(pattern_controls_);
  pattern_reference_ = new QComboBox(pattern_controls_);
  pattern_reference_->setObjectName(QStringLiteral("blcad.pattern.reference"));
  form->addRow(QStringLiteral("Achse / Ebene"), pattern_reference_);
  pattern_reference_button_ =
      new QPushButton(QStringLiteral("Referenz im Modell wählen"), pattern_controls_);
  pattern_reference_button_->setObjectName(QStringLiteral("blcad.pattern.pick_reference"));
  pattern_reference_button_->setCheckable(true);
  form->addRow(pattern_reference_button_);
  form->addRow(new QLabel(QStringLiteral("Ausgangskörper auswählen"), pattern_controls_));
  pattern_sources_ = new QListWidget(pattern_controls_);
  pattern_sources_->setObjectName(QStringLiteral("blcad.pattern.sources"));
  pattern_sources_->setSelectionMode(QAbstractItemView::MultiSelection);
  pattern_sources_->setMinimumHeight(110);
  form->addRow(pattern_sources_);
  auto* hint = new QLabel(
      QStringLiteral(
          "Körper im Modell oder in der Liste anklicken. "
          "Weitere Klicks ergänzen oder entfernen Körper. Das Ergebnis entsteht als neuer Körper."),
      pattern_controls_);
  hint->setWordWrap(true);
  form->addRow(hint);
  pattern_count_editor_ = new QSpinBox(pattern_controls_);
  pattern_count_editor_->setObjectName(QStringLiteral("blcad.pattern.count"));
  pattern_count_editor_->setRange(2, 1000);
  pattern_count_editor_->setKeyboardTracking(false);
  form->addRow(QStringLiteral("Anzahl"), pattern_count_editor_);
  pattern_extent_ = new QComboBox(pattern_controls_);
  pattern_extent_->setObjectName(QStringLiteral("blcad.pattern.extent"));
  pattern_extent_->addItems({QStringLiteral("Abstand je Instanz"), QStringLiteral("Gesamtlänge")});
  form->addRow(QStringLiteral("Verteilung"), pattern_extent_);
  pattern_direction_ = new QComboBox(pattern_controls_);
  pattern_direction_->setObjectName(QStringLiteral("blcad.pattern.direction"));
  pattern_direction_->addItems(
      {QStringLiteral("Positive Richtung"), QStringLiteral("Negative Richtung")});
  form->addRow(QStringLiteral("Richtung"), pattern_direction_);
  panel->layout()->addWidget(pattern_controls_);
  pattern_controls_->hide();
  connect(pattern_sources_, &QListWidget::itemSelectionChanged, this, [this] {
    if (pattern())
      refresh_pattern_preview();
  });
  for (auto* input : {pattern_reference_, pattern_extent_, pattern_direction_})
    connect(input, &QComboBox::currentIndexChanged, this, [this] {
      if (!pattern())
        return;
      pattern_reference_picking_ = false;
      update_pattern_pick_mode();
      refresh_pattern_preview();
    });
  connect(pattern_reference_button_, &QPushButton::toggled, this, [this](bool checked) {
    if (!pattern())
      return;
    if (!layer_.active_handle_id().empty()) {
      layer_.cancel();
      restore_drag_start_value();
      if (viewport_.mouseGrabber() == &viewport_)
        viewport_.release_interaction_mouse();
    }
    pattern_reference_picking_ = checked;
    update_pattern_pick_mode();
  });
  connect(pattern_count_editor_, &QSpinBox::valueChanged, this, [this](int value) {
    if (!pattern())
      return;
    auto preview = set_pattern_count(value);
    if (!preview && !pattern_sources_->selectedItems().empty())
      report(preview.error());
  });
}

Result<std::size_t> ShellFeatureTools::begin_pattern(Command command) {
  const auto fail = [](std::string message) {
    return Result<std::size_t>::failure(Error::validation("gui.shell.pattern", std::move(message)));
  };
  const auto* part = session_.part_document();
  if (active() || session_.task().active() || window_.active_sketch() || !part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_shape_cache())
    return fail("Muster benötigen ein Bauteil ohne aktiven Befehl");
  if (command != Command::LinearPattern && command != Command::CircularPattern &&
      command != Command::BodyMirror)
    return fail("Unbekannter Musterbefehl");
  const auto& cache = *session_.part_shape_cache();
  const QSignalBlocker reference_block(pattern_reference_), sources_block(pattern_sources_),
      count_block(pattern_count_editor_), extent_block(pattern_extent_),
      direction_block(pattern_direction_);
  pattern_sources_->clear();
  pattern_reference_->clear();
  pattern_axes_.clear();
  pattern_planes_.clear();
  for (const auto& body : part->bodies())
    if (body.kind() == BodyKind::Solid && body.visibility() == BodyVisibility::Visible &&
        cache.find_body_shape(body.id())) {
      auto* item = new QListWidgetItem(QString::fromStdString(body.name()), pattern_sources_);
      item->setData(Qt::UserRole, QString::fromStdString(body.id().value()));
      for (const auto& selected : session_.selection().entries())
        if (selected.kind == GuiSelectionKind::Body &&
            selected.semantic_id == "body/" + body.id().value())
          item->setSelected(true);
    }
  if (pattern_sources_->count() == 0)
    return fail("Keine sichtbaren, berechneten Volumenkörper vorhanden");
  if (command == Command::BodyMirror) {
    const auto add_plane = [&](auto reference, const std::string& semantic,
                               const std::string& name) {
      if (!reference)
        return;
      pattern_reference_->addItem(QString::fromStdString(name));
      pattern_planes_.push_back({reference.value(), semantic});
    };
    for (const auto& plane : part->datum_planes())
      add_plane(PlaneReference::create_datum_plane(PartFeatureInputRole::MirrorPlane, plane.id()),
                "datum-plane/" + plane.id().value(), plane.name());
    for (const auto& plane : part->construction_planes())
      if (geometry::WorkplaneResolver{}.resolve(*part, DatumPlaneId(plane.id().value())))
        add_plane(PlaneReference::create_construction_plane(PartFeatureInputRole::MirrorPlane,
                                                            plane.id()),
                  "construction-plane/" + plane.id().value(), plane.name());
    for (std::size_t i = 0; i < pattern_planes_.size(); ++i)
      if (pattern_planes_[i].semantic_id == "datum-plane/datum.yz")
        pattern_reference_->setCurrentIndex(static_cast<int>(i));
  } else {
    for (const auto& axis : part->datum_axes()) {
      if (axis.kind() != DatumAxisKind::Explicit)
        continue;
      auto reference =
          AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, axis.id());
      if (!reference)
        continue;
      pattern_reference_->addItem(QString::fromStdString(axis.name()));
      pattern_axes_.push_back({reference.value(), axis.origin(), normalized(axis.direction()),
                               "datum-axis/" + axis.id().value()});
    }
    for (const auto& line : part->construction_lines()) {
      auto resolved = geometry::ConstructionLineResolver{}.resolve(*part, line.id());
      auto reference = AxisReference::create_construction_line(
          PartFeatureInputRole::PatternAxis, line.id(), PartFeatureInputCapability::Axis);
      if (!resolved || !reference)
        continue;
      pattern_reference_->addItem(QString::fromStdString(line.name()));
      pattern_axes_.push_back({reference.value(), resolved.value().point,
                               normalized(resolved.value().direction),
                               "construction-line/" + line.id().value()});
    }
    const std::string preferred =
        command == Command::LinearPattern ? "datum-axis/datum.axis.x" : "datum-axis/datum.axis.z";
    for (std::size_t i = 0; i < pattern_axes_.size(); ++i)
      if (pattern_axes_[i].semantic_id == preferred)
        pattern_reference_->setCurrentIndex(static_cast<int>(i));
  }
  if (pattern_reference_->count() == 0)
    return fail("Zuerst eine passende Achse oder Spiegelebene erstellen");
  const auto title = pattern_title(command);
  for (std::size_t n = 1;; ++n) {
    const std::string suffix = std::to_string(n), base = "pattern." + suffix;
    const FeatureId id("feature." + base);
    const std::string name = title.toStdString() + " " + suffix;
    const bool feature_taken =
        part->find_feature(id) || part->find_revolve_feature(id) || part->find_sweep_feature(id) ||
        part->find_loft_feature(id) || part->find_surface_feature(id) ||
        part->find_linear_pattern_feature(id) || part->find_circular_pattern_feature(id) ||
        part->find_mirror_feature(id) || part->find_fillet_feature(id) ||
        part->find_chamfer_feature(id) || part->find_shell_feature(id) ||
        part->find_draft_feature(id) || part->find_body_boolean_feature(id);
    if (feature_taken || part->find_body(BodyId("body." + base)) ||
        part->find_parameter(ParameterId("parameter." + base + ".count")) ||
        part->find_parameter(ParameterId("parameter." + base + ".extent")) ||
        std::any_of(part->parameters().begin(), part->parameters().end(),
                    [&](const auto& p) {
                      return p.name() == name + " Anzahl" || p.name() == name + " Abstand";
                    }) ||
        std::any_of(part->bodies().begin(), part->bodies().end(),
                    [&](const auto& b) { return b.name() == name; }))
      continue;
    pattern_feature_ = id;
    pattern_result_ = BodyId("body." + base);
    pattern_name_ = name;
    pattern_count_parameter_ = ParameterId("parameter." + base + ".count");
    pattern_extent_parameter_ = ParameterId("parameter." + base + ".extent");
    break;
  }
  command_ = command;
  if (!session_.task().begin(std::string(command_id())) ||
      !session_.task().begin_parameter_editing() || !session_.task().show_preview()) {
    command_ = Command::None;
    if (session_.task().active())
      (void)session_.task().cancel();
    return fail("Musteraufgabe konnte nicht gestartet werden");
  }
  previous_selection_mask_ = viewport_.selection_filter_mask();
  pattern_reference_picking_ = false;
  pattern_count_ = 3;
  pattern_value_ = command == Command::CircularPattern ? 360 : 40;
  pattern_extent_->setCurrentIndex(0);
  pattern_direction_->setCurrentIndex(0);
  pattern_count_editor_->setValue(pattern_count_);
  auto* form = static_cast<QFormLayout*>(pattern_controls_->layout());
  form->setRowVisible(pattern_count_editor_, command != Command::BodyMirror);
  form->setRowVisible(pattern_extent_, command == Command::LinearPattern);
  form->setRowVisible(pattern_direction_, command == Command::LinearPattern);
  show_task_panel(title, angular_value() ? QStringLiteral("deg") : QStringLiteral("mm"),
                  pattern_value_);
  update_pattern_pick_mode();
  refresh_pattern_preview();
  window_.refresh();
  return Result<std::size_t>::success(1);
}

Result<std::size_t> ShellFeatureTools::rebuild_pattern_controller() {
  if (viewport_.mouseGrabber() == &viewport_)
    viewport_.release_interaction_mouse();
  layer_.cancel();
  drag_start_value_.reset();
  coordinator_.cancel();
  auto body = Body::create(pattern_result_, pattern_name_, BodyKind::Solid);
  if (!body)
    return Result<std::size_t>::failure(body.error());
  const auto index = static_cast<std::size_t>(pattern_reference_->currentIndex());
  Result<std::size_t> begun = Result<std::size_t>::success(1);
  if (command_ == Command::BodyMirror) {
    if (index >= pattern_planes_.size())
      return Result<std::size_t>::failure(Error::validation("pattern", "Spiegelebene auswählen"));
    begun = coordinator_.pattern_mirror().begin_mirror(session_, pattern_feature_, pattern_name_,
                                                       pattern_planes_[index].reference,
                                                       pattern_result_, body.value());
  } else {
    if (index >= pattern_axes_.size())
      return Result<std::size_t>::failure(Error::validation("pattern", "Musterachse auswählen"));
    auto quantity = Quantity::count(pattern_count_);
    if (!quantity)
      return Result<std::size_t>::failure(quantity.error());
    auto count = Parameter::create_count(pattern_count_parameter_, pattern_name_ + " Anzahl",
                                         quantity.value());
    if (!count)
      return Result<std::size_t>::failure(count.error());
    std::vector<Parameter> pending{count.value()};
    if (command_ == Command::LinearPattern) {
      auto length = Quantity::length_mm(pattern_value_);
      if (!length)
        return Result<std::size_t>::failure(length.error());
      auto extent = Parameter::create_length(pattern_extent_parameter_, pattern_name_ + " Abstand",
                                             length.value());
      if (!extent)
        return Result<std::size_t>::failure(extent.error());
      pending.push_back(extent.value());
      begun = coordinator_.pattern_mirror().begin_linear_pattern(
          session_, pattern_feature_, pattern_name_, pattern_axes_[index].reference,
          pattern_count_parameter_,
          pattern_extent_->currentIndex() == 0 ? LinearPatternExtentMode::Spacing
                                               : LinearPatternExtentMode::TotalExtent,
          pattern_extent_parameter_, pattern_result_, pending, body.value());
      coordinator_.pattern_mirror().set_direction_sign(pattern_direction_->currentIndex() == 0
                                                           ? PatternDirectionSign::Positive
                                                           : PatternDirectionSign::Negative);
    } else {
      begun = coordinator_.pattern_mirror().begin_circular_pattern(
          session_, pattern_feature_, pattern_name_, pattern_axes_[index].reference,
          pattern_count_parameter_, pattern_result_, true, pending, body.value());
      coordinator_.pattern_mirror().set_angle(pattern_value_);
    }
    GuiFeatureManipulatorFrame frame;
    const auto& choice = pattern_axes_[index];
    frame.origin = choice.origin;
    if (command_ == Command::LinearPattern) {
      const double sign = pattern_direction_->currentIndex() == 0 ? 1 : -1;
      frame.axis = {choice.direction.x * sign, choice.direction.y * sign,
                    choice.direction.z * sign};
      frame.plane_normal = std::abs(frame.axis.z) < 0.9 ? Vector3{0, 0, 1} : Vector3{0, 1, 0};
    } else {
      frame.plane_normal = choice.direction;
      const Vector3 helper =
          std::abs(choice.direction.x) < 0.9 ? Vector3{1, 0, 0} : Vector3{0, 1, 0};
      frame.reference_direction = normalized(cross(helper, choice.direction));
      frame.axis = frame.reference_direction;
    }
    coordinator_.pattern_mirror().set_manipulator_frame(frame);
  }
  if (!begun)
    return begun;
  for (int i = 0; i < pattern_sources_->count(); ++i)
    if (pattern_sources_->item(i)->isSelected()) {
      auto added = coordinator_.pattern_mirror().add_source_body(
          BodyId(pattern_sources_->item(i)->data(Qt::UserRole).toString().toStdString()));
      if (!added)
        return added;
    }
  coordinator_.refresh_handles();
  return begun;
}

void ShellFeatureTools::refresh_pattern_preview() {
  if (coordinator_.pattern_mirror().active()) {
    pattern_value_ = current_value();
    pattern_count_ = coordinator_.pattern_mirror().count();
  }
  auto begun = rebuild_pattern_controller();
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

void ShellFeatureTools::synchronize_pattern_editors() {
  if (coordinator_.pattern_mirror().active()) {
    pattern_count_ = coordinator_.pattern_mirror().count();
    pattern_value_ = current_value();
  }
  const QSignalBlocker block(pattern_count_editor_);
  pattern_count_editor_->setValue(pattern_count_);
}

Result<GuiPartFeaturePreview> ShellFeatureTools::set_pattern_count(int value) {
  if (!pattern() || command_ == Command::BodyMirror || value < 2 || value > 1000)
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("pattern", "Anzahl muss zwischen 2 und 1000 liegen"));
  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = std::string(kPatternCountHandleId);
  candidate.value_kind = GuiViewportManipulatorValueKind::Count;
  candidate.scalar_value = value;
  candidate.count_value = value;
  candidate.source = GuiViewportManipulatorInputSource::Numeric;
  if (!layer_.active_handle_id().empty()) {
    if (layer_.active_handle_id() != candidate.handle_id ||
        !layer_.set_numeric_input(std::to_string(value)) || !layer_.candidate())
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("pattern", "zuerst den aktiven Manipulatorzug abschließen"));
    candidate = *layer_.candidate();
  }
  return accept_manipulator_candidate(candidate);
}

void ShellFeatureTools::update_pattern_pick_mode() {
  const QSignalBlocker block(pattern_reference_button_);
  pattern_reference_button_->setChecked(pattern_reference_picking_);
  viewport_.set_selection_filter_mask(selection_kind_bit(
      pattern_reference_picking_ ? GuiSelectionKind::Datum : GuiSelectionKind::Body));
  synchronize_overlay();
}

bool ShellFeatureTools::accept_pattern_selection(const GuiSelection& selection) {
  if (!pattern())
    return false;
  if (pattern_reference_picking_) {
    const auto match = [&](const auto& choices) {
      for (std::size_t i = 0; i < choices.size(); ++i)
        if (choices[i].semantic_id == selection.semantic_id) {
          pattern_reference_picking_ = false;
          {
            const QSignalBlocker block(pattern_reference_);
            pattern_reference_->setCurrentIndex(static_cast<int>(i));
          }
          update_pattern_pick_mode();
          refresh_pattern_preview();
          return true;
        }
      return false;
    };
    return command_ == Command::BodyMirror ? match(pattern_planes_) : match(pattern_axes_);
  }
  if (selection.kind != GuiSelectionKind::Body || !selection.semantic_id.starts_with("body/"))
    return false;
  const QString id = QString::fromStdString(selection.semantic_id.substr(5));
  for (int i = 0; i < pattern_sources_->count(); ++i)
    if (pattern_sources_->item(i)->data(Qt::UserRole).toString() == id) {
      auto* item = pattern_sources_->item(i);
      item->setSelected(!item->isSelected());
      return true;
    }
  return false;
}
} // namespace blcad::gui
