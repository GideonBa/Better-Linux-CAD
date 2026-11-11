#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/gui_icons.hpp"

#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace blcad::gui {
namespace {
QString pattern_title(ShellFeatureTools::Command command) {
  if (command == ShellFeatureTools::Command::LinearPattern)
    return QStringLiteral("Linear Pattern");
  if (command == ShellFeatureTools::Command::CircularPattern)
    return QStringLiteral("Circular Pattern");
  return QStringLiteral("Mirror Bodies");
}
} // namespace

bool ShellFeatureTools::pattern() const noexcept {
  return command_ == Command::LinearPattern || command_ == Command::CircularPattern ||
         command_ == Command::BodyMirror;
}
QAction* ShellFeatureTools::pattern_action(Command command) const noexcept {
  for (const auto& [kind, action] : pattern_actions_)
    if (kind == command)
      return action;
  return nullptr;
}

bool ShellFeatureTools::pattern_context_available(const PatternContext& context) const {
  if (session_.document_kind() != GuiDocumentKind::Part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_document() ||
      !session_.part_shape_cache() || !session_.part_modeling_ready())
    return false;
  const auto* body = session_.part_document()->find_body(context.body);
  const auto* shape = session_.part_shape_cache()->find_body_shape(context.body);
  return body && body->kind() == BodyKind::Solid && body->visibility() == BodyVisibility::Visible &&
         shape && !shape->empty();
}

std::optional<ShellFeatureTools::PatternContext> ShellFeatureTools::pattern_context() const {
  if (active() || session_.task().active() || window_.active_sketch() ||
      session_.reference_repair_mode() || session_.selection().size() != 1 ||
      !session_.part_document() || !session_.selection().allows(GuiSelectionKind::Body) ||
      !(viewport_.selection_filter_mask() & selection_kind_bit(GuiSelectionKind::Body)))
    return std::nullopt;
  const auto& selected = session_.selection().entries().front();
  if (selected.kind != GuiSelectionKind::Body)
    return std::nullopt;
  const auto& scope = viewport_.selection_semantic_scope();
  if (scope && std::find(scope->begin(), scope->end(), selected.semantic_id) == scope->end())
    return std::nullopt;
  for (const auto& body : session_.part_document()->bodies())
    if (selected.semantic_id == "body/" + body.id().value() &&
        pattern_context_available({body.id()}))
      return PatternContext{body.id()};
  return std::nullopt;
}

bool ShellFeatureTools::select_pattern_context(const PatternContext& context) {
  if (!pattern() || pattern_holes_mode() || !session_.task().active() ||
      session_.task().command_id() != command_id() || !pattern_context_available(context))
    return false;
  for (int i = 0; i < pattern_sources_->count(); ++i)
    if (pattern_sources_->item(i)->data(Qt::UserRole).toString() ==
        QString::fromStdString(context.body.value())) {
      {
        const QSignalBlocker block(pattern_sources_);
        pattern_sources_->clearSelection();
        pattern_sources_->item(i)->setSelected(true);
      }
      refresh_pattern_preview();
      return true;
    }
  return false;
}

QString ShellFeatureTools::pattern_summary() const {
  const auto sources = pattern_sources_->selectedItems().size();
  if (command_ == Command::BodyMirror)
    return QStringLiteral("%1 bodies selected. Mirror preview valid.").arg(sources);
  if (pattern_holes_mode())
    return QStringLiteral(
               "%1 Hole features; %2 × %3 occurrences including source. Cut preview valid.")
        .arg(sources)
        .arg(coordinator_.pattern_mirror().count())
        .arg(command_ == Command::LinearPattern && pattern_second_group_->isChecked()
                 ? pattern_second_count_->value()
                 : 1);
  if (command_ == Command::LinearPattern && pattern_second_group_->isChecked())
    return QStringLiteral("%1 bodies; %2 × %3 occurrences including source. Preview valid.")
        .arg(sources)
        .arg(coordinator_.pattern_mirror().count())
        .arg(pattern_second_count_->value());
  return QStringLiteral("%1 bodies, %2 instances including source. Preview valid.")
      .arg(sources)
      .arg(coordinator_.pattern_mirror().count());
}

void ShellFeatureTools::build_pattern_controls(QWidget* panel) {
  pattern_controls_ = new QWidget(panel);
  pattern_controls_->setMinimumWidth(280);
  auto* column = new QVBoxLayout(pattern_controls_);
  auto* sources = new QGroupBox(QStringLiteral("Sources"), pattern_controls_);
  auto* source_form = pattern_sources_form_ = new QFormLayout(sources);
  pattern_source_mode_ = new QComboBox(sources);
  pattern_source_mode_->setObjectName("blcad.pattern.source_mode");
  pattern_source_mode_->addItems({QStringLiteral("Bodies"), QStringLiteral("Hole Features")});
  source_form->addRow(QStringLiteral("Pattern"), pattern_source_mode_);
  pattern_sources_ = new QListWidget(sources);
  pattern_sources_->setObjectName(QStringLiteral("blcad.pattern.sources"));
  pattern_sources_->setSelectionMode(QAbstractItemView::MultiSelection);
  pattern_sources_->setMinimumHeight(100);
  pattern_sources_->setMaximumHeight(140);
  source_form->addRow(pattern_sources_);
  pattern_hint_ = new QLabel(sources);
  pattern_hint_->setWordWrap(true);
  source_form->addRow(pattern_hint_);
  column->addWidget(sources);
  pattern_first_group_ = new QGroupBox(QStringLiteral("Direction 1"), pattern_controls_);
  auto* form = pattern_form_ = new QFormLayout(pattern_first_group_);
  pattern_reference_ = new QComboBox(pattern_first_group_);
  pattern_reference_->setObjectName(QStringLiteral("blcad.pattern.reference"));
  form->addRow(QStringLiteral("Axis / Plane"), pattern_reference_);
  pattern_reference_button_ =
      make_gui_button(QStringLiteral("Pick Reference in Model"), pattern_first_group_);
  pattern_reference_button_->setObjectName(QStringLiteral("blcad.pattern.pick_reference"));
  pattern_reference_button_->setCheckable(true);
  form->addRow(pattern_reference_button_);
  pattern_count_editor_ = new QSpinBox(pattern_first_group_);
  pattern_count_editor_->setObjectName(QStringLiteral("blcad.pattern.count"));
  pattern_count_editor_->setRange(1, 1000);
  pattern_count_editor_->setKeyboardTracking(false);
  form->addRow(QStringLiteral("Count"), pattern_count_editor_);
  pattern_extent_ = new QComboBox(pattern_first_group_);
  pattern_extent_->setObjectName(QStringLiteral("blcad.pattern.extent"));
  pattern_extent_->addItems(
      {QStringLiteral("Spacing per Instance"), QStringLiteral("Overall Length")});
  form->addRow(QStringLiteral("Distribution"), pattern_extent_);
  pattern_direction_ = new QComboBox(pattern_first_group_);
  pattern_direction_->setObjectName(QStringLiteral("blcad.pattern.direction"));
  pattern_direction_->addItems(
      {QStringLiteral("Positive Direction"), QStringLiteral("Negative Direction")});
  form->addRow(QStringLiteral("Direction"), pattern_direction_);
  column->addWidget(pattern_first_group_);
  pattern_second_group_ = new QGroupBox(QStringLiteral("Direction 2"), pattern_controls_);
  pattern_second_group_->setObjectName("blcad.pattern.second.enabled");
  pattern_second_group_->setCheckable(true);
  pattern_second_group_->setChecked(false);
  auto* second = new QFormLayout(pattern_second_group_);
  pattern_second_reference_ = new QComboBox(pattern_second_group_);
  pattern_second_reference_->setObjectName("blcad.pattern.second.reference");
  second->addRow(QStringLiteral("Axis"), pattern_second_reference_);
  pattern_second_reference_button_ =
      make_gui_button(QStringLiteral("Pick Reference in Model"), pattern_second_group_);
  pattern_second_reference_button_->setCheckable(true);
  pattern_second_reference_button_->setObjectName("blcad.pattern.second.pick_reference");
  second->addRow(pattern_second_reference_button_);
  pattern_second_count_ = new QSpinBox(pattern_second_group_);
  pattern_second_count_->setObjectName("blcad.pattern.second.count");
  pattern_second_count_->setRange(1, 1000);
  pattern_second_count_->setKeyboardTracking(false);
  second->addRow(QStringLiteral("Count"), pattern_second_count_);
  pattern_second_value_ = new QDoubleSpinBox(pattern_second_group_);
  pattern_second_value_->setObjectName("blcad.pattern.second.value");
  pattern_second_value_->setDecimals(6);
  pattern_second_value_->setRange(.000001, 1e6);
  pattern_second_value_->setSuffix(QStringLiteral(" mm"));
  pattern_second_value_->setKeyboardTracking(false);
  second->addRow(QStringLiteral("Distance / Length"), pattern_second_value_);
  pattern_second_extent_ = new QComboBox(pattern_second_group_);
  pattern_second_extent_->setObjectName("blcad.pattern.second.extent");
  pattern_second_extent_->addItems(
      {QStringLiteral("Spacing per Instance"), QStringLiteral("Overall Length")});
  second->addRow(QStringLiteral("Distribution"), pattern_second_extent_);
  pattern_second_direction_ = new QComboBox(pattern_second_group_);
  pattern_second_direction_->setObjectName("blcad.pattern.second.direction");
  pattern_second_direction_->addItems(
      {QStringLiteral("Positive Direction"), QStringLiteral("Negative Direction")});
  second->addRow(QStringLiteral("Direction"), pattern_second_direction_);
  column->addWidget(pattern_second_group_);
  pattern_skip_group_ = new QGroupBox(QStringLiteral("Occurrences to Skip"), pattern_controls_);
  auto* skip_column = new QVBoxLayout(pattern_skip_group_);
  auto* skip_hint = new QLabel(
      QStringLiteral(
          "The source is always included. Exclusions outside the current counts remain stored."),
      pattern_skip_group_);
  skip_hint->setWordWrap(true);
  skip_column->addWidget(skip_hint);
  pattern_skip_editor_ = new QListWidget(pattern_skip_group_);
  pattern_skip_editor_->setObjectName("blcad.pattern.skipped");
  pattern_skip_editor_->setMinimumHeight(100);
  pattern_skip_editor_->setMaximumHeight(140);
  skip_column->addWidget(pattern_skip_editor_);
  column->addWidget(pattern_skip_group_);
  panel->layout()->addWidget(pattern_controls_);
  pattern_controls_->hide();
  connect(pattern_source_mode_, &QComboBox::currentIndexChanged, this, [this] {
    if (!pattern())
      return;
    pattern_reference_picking_ = false;
    pattern_second_reference_picking_ = false;
    populate_pattern_sources();
    update_pattern_pick_mode();
    refresh_pattern_preview();
  });
  connect(pattern_sources_, &QListWidget::itemSelectionChanged, this, [this] {
    if (pattern())
      refresh_pattern_preview();
  });
  for (auto* input : {pattern_reference_, pattern_extent_, pattern_direction_,
                      pattern_second_reference_, pattern_second_extent_, pattern_second_direction_})
    connect(input, &QComboBox::currentIndexChanged, this, [this] {
      if (!pattern())
        return;
      pattern_reference_picking_ = false;
      pattern_second_reference_picking_ = false;
      update_pattern_pick_mode();
      refresh_pattern_preview();
    });
  const auto pick = [this](bool checked, bool second) {
    if (!pattern())
      return;
    if (!layer_.active_handle_id().empty()) {
      layer_.cancel();
      restore_drag_start_value();
      if (viewport_.mouseGrabber() == &viewport_)
        viewport_.release_interaction_mouse();
    }
    pattern_reference_picking_ = checked;
    pattern_second_reference_picking_ = checked && second;
    update_pattern_pick_mode();
  };
  connect(pattern_reference_button_, &QPushButton::toggled, this,
          [pick](bool checked) { pick(checked, false); });
  connect(pattern_second_reference_button_, &QPushButton::toggled, this,
          [pick](bool checked) { pick(checked, true); });
  connect(pattern_count_editor_, &QSpinBox::valueChanged, this, [this](int value) {
    if (!pattern())
      return;
    auto preview = set_pattern_count(value);
    if (!preview && !pattern_sources_->selectedItems().empty())
      report(preview.error());
    rebuild_pattern_skip_list();
  });
  connect(pattern_second_group_, &QGroupBox::toggled, this, [this](bool enabled) {
    if (!pattern())
      return;
    if (!enabled && pattern_second_reference_picking_) {
      pattern_reference_picking_ = false;
      pattern_second_reference_picking_ = false;
      update_pattern_pick_mode();
    }
    rebuild_pattern_skip_list();
    refresh_pattern_preview();
  });
  connect(pattern_second_count_, &QSpinBox::valueChanged, this, [this] {
    if (pattern()) {
      rebuild_pattern_skip_list();
      refresh_pattern_preview();
    }
  });
  connect(pattern_second_value_, &QDoubleSpinBox::valueChanged, this, [this] {
    if (pattern())
      refresh_pattern_preview();
  });
  connect(pattern_skip_editor_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
    if (!pattern())
      return;
    const PatternOccurrenceIndex index{item->data(Qt::UserRole).toUInt(),
                                       item->data(Qt::UserRole + 1).toUInt()};
    if (item->checkState() == Qt::Checked)
      pattern_skipped_.insert(index);
    else
      pattern_skipped_.erase(index);
    refresh_pattern_preview();
  });
}

bool ShellFeatureTools::pattern_holes_mode() const noexcept {
  return command_ != Command::BodyMirror && pattern_source_mode_->currentIndex() == 1;
}
void ShellFeatureTools::populate_pattern_sources() {
  const QSignalBlocker block(pattern_sources_);
  pattern_sources_->clear();
  const auto* part = session_.part_document();
  const auto* cache = session_.part_shape_cache();
  if (!part || !cache)
    return;
  std::string selected_feature;
  if (const auto* item = window_.model_browser().currentItem())
    if (const auto* node =
            window_.browser_model().find(item->data(0, Qt::UserRole).toString().toStdString());
        node && node->kind == GuiBrowserNodeKind::Feature)
      selected_feature = node->semantic_id;
  if (pattern_holes_mode()) {
    for (const auto& hole : part->hole_features()) {
      const auto* body = part->find_body(hole.target_body());
      if (!body || body->visibility() != BodyVisibility::Visible ||
          !cache->find_feature_shape(hole.id()))
        continue;
      auto* item = new QListWidgetItem(QString::fromStdString(hole.name() + " / " + body->name()),
                                       pattern_sources_);
      item->setData(Qt::UserRole, QString::fromStdString(hole.id().value()));
      item->setSelected(selected_feature == hole.id().value());
    }
    pattern_hint_->setText(QStringLiteral("Select Hole features in the list or model. Every owned "
                                          "position is repeated as a cut in its current body."));
  } else {
    for (const auto& body : part->bodies()) {
      if (body.kind() != BodyKind::Solid || body.visibility() != BodyVisibility::Visible ||
          !cache->find_body_shape(body.id()))
        continue;
      auto* item = new QListWidgetItem(QString::fromStdString(body.name()), pattern_sources_);
      item->setData(Qt::UserRole, QString::fromStdString(body.id().value()));
      for (const auto& selected : session_.selection().entries())
        if (selected.kind == GuiSelectionKind::Body &&
            selected.semantic_id == "body/" + body.id().value())
          item->setSelected(true);
    }
    pattern_hint_->setText(
        QStringLiteral("Select bodies in the list or model. The result is created as a new body."));
  }
}
void ShellFeatureTools::rebuild_pattern_skip_list() {
  const QSignalBlocker block(pattern_skip_editor_);
  const unsigned first = pattern_count_editor_->value();
  const unsigned second = command_ == Command::LinearPattern && pattern_second_group_->isChecked()
                              ? pattern_second_count_->value()
                              : 1;
  std::set<PatternOccurrenceIndex> indices = pattern_skipped_;
  if (first <= pattern_placement_limit / second)
    for (unsigned i = 0; i < first; ++i)
      for (unsigned j = 0; j < second; ++j)
        indices.insert({i, j});
  const bool over_limit = first > pattern_placement_limit / second;
  // Preview publication also synchronizes these controls during itemChanged.
  // Keep unchanged rows alive until Qt has finished dispatching the input event.
  bool same_rows =
      pattern_skip_editor_->count() == static_cast<int>(indices.size()) + (over_limit ? 1 : 0);
  int row = over_limit ? 1 : 0;
  for (const auto index : indices) {
    if (!same_rows)
      break;
    const auto* item = pattern_skip_editor_->item(row++);
    same_rows = item->data(Qt::UserRole).toUInt() == index.first &&
                item->data(Qt::UserRole + 1).toUInt() == index.second;
  }
  if (!same_rows)
    pattern_skip_editor_->clear();
  if (over_limit && !same_rows) {
    auto* message = new QListWidgetItem(
        QStringLiteral("Reduce counts to at most 1024 occurrences."), pattern_skip_editor_);
    message->setFlags(Qt::NoItemFlags);
  }
  row = over_limit ? 1 : 0;
  for (const auto index : indices) {
    const bool dormant = index.first >= first || index.second >= second;
    auto text = QStringLiteral("Occurrence (%1, %2)").arg(index.first + 1).arg(index.second + 1);
    if (index == PatternOccurrenceIndex{})
      text += QStringLiteral(" — Source");
    else if (dormant)
      text += QStringLiteral(" — Outside current counts");
    auto* item =
        same_rows ? pattern_skip_editor_->item(row++) : new QListWidgetItem(pattern_skip_editor_);
    item->setText(text);
    item->setData(Qt::UserRole, static_cast<unsigned>(index.first));
    item->setData(Qt::UserRole + 1, static_cast<unsigned>(index.second));
    item->setFlags(index == PatternOccurrenceIndex{} ? Qt::NoItemFlags
                                                     : Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    item->setCheckState(pattern_skipped_.contains(index) ? Qt::Checked : Qt::Unchecked);
  }
}

Result<std::size_t> ShellFeatureTools::begin_pattern(Command command) {
  const auto fail = [](std::string message) {
    return Result<std::size_t>::failure(Error::validation("gui.shell.pattern", std::move(message)));
  };
  const auto* part = session_.part_document();
  if (active() || session_.task().active() || window_.active_sketch() || !part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_shape_cache())
    return fail("Patterns require a part without an active command");
  if (command != Command::LinearPattern && command != Command::CircularPattern &&
      command != Command::BodyMirror)
    return fail("Unknown pattern command");
  const QSignalBlocker reference_block(pattern_reference_), sources_block(pattern_sources_),
      count_block(pattern_count_editor_), extent_block(pattern_extent_),
      direction_block(pattern_direction_), mode_block(pattern_source_mode_),
      second_reference_block(pattern_second_reference_), second_group_block(pattern_second_group_),
      second_count_block(pattern_second_count_), second_extent_block(pattern_second_extent_),
      second_direction_block(pattern_second_direction_), second_value_block(pattern_second_value_);
  pattern_sources_->clear();
  pattern_reference_->clear();
  pattern_axes_.clear();
  pattern_planes_.clear();
  bool preselected_hole = false;
  if (command != Command::BodyMirror)
    if (const auto* item = window_.model_browser().currentItem())
      if (const auto* node =
              window_.browser_model().find(item->data(0, Qt::UserRole).toString().toStdString());
          node && node->kind == GuiBrowserNodeKind::Feature)
        preselected_hole = part->find_hole_feature(FeatureId(node->semantic_id));
  pattern_source_mode_->setCurrentIndex(preselected_hole ? 1 : 0);
  populate_pattern_sources();
  if (pattern_sources_->count() == 0)
    return fail("No visible recomputed solid sources available");
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
    return fail("First create a suitable axis or mirror plane");
  pattern_second_reference_->clear();
  for (int i = 0; i < pattern_reference_->count(); ++i)
    pattern_second_reference_->addItem(pattern_reference_->itemText(i));
  if (command != Command::BodyMirror) {
    const auto first = static_cast<std::size_t>(pattern_reference_->currentIndex());
    for (std::size_t i = 0; i < pattern_axes_.size(); ++i)
      if (length(cross(pattern_axes_[first].direction, pattern_axes_[i].direction)) > 1e-7) {
        pattern_second_reference_->setCurrentIndex(static_cast<int>(i));
        break;
      }
  }
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
        part->find_draft_feature(id) || part->find_body_boolean_feature(id) ||
        part->find_hole_feature(id) || part->find_thread_feature(id);
    if (feature_taken || part->find_body(BodyId("body." + base)) ||
        part->find_parameter(ParameterId("parameter." + base + ".count")) ||
        part->find_parameter(ParameterId("parameter." + base + ".extent")) ||
        part->find_parameter(ParameterId("parameter." + base + ".second_count")) ||
        part->find_parameter(ParameterId("parameter." + base + ".second_extent")) ||
        std::any_of(part->parameters().begin(), part->parameters().end(),
                    [&](const auto& p) {
                      return p.name() == name + " Count" || p.name() == name + " Spacing" ||
                             p.name() == name + " Count 2" || p.name() == name + " Spacing 2";
                    }) ||
        std::any_of(part->bodies().begin(), part->bodies().end(),
                    [&](const auto& b) { return b.name() == name; }))
      continue;
    pattern_feature_ = id;
    pattern_result_ = BodyId("body." + base);
    pattern_name_ = name;
    pattern_count_parameter_ = ParameterId("parameter." + base + ".count");
    pattern_extent_parameter_ = ParameterId("parameter." + base + ".extent");
    pattern_second_count_parameter_ = ParameterId("parameter." + base + ".second_count");
    pattern_second_extent_parameter_ = ParameterId("parameter." + base + ".second_extent");
    break;
  }
  command_ = command;
  if (!session_.task().begin(std::string(command_id())) ||
      !session_.task().begin_parameter_editing() || !session_.task().show_preview()) {
    command_ = Command::None;
    if (session_.task().active())
      (void)session_.task().cancel();
    return fail("Could not start pattern task");
  }
  previous_selection_mask_ = viewport_.selection_filter_mask();
  previous_semantic_scope_ = viewport_.selection_semantic_scope();
  pattern_reference_picking_ = false;
  pattern_second_reference_picking_ = false;
  pattern_skipped_.clear();
  pattern_second_group_->setChecked(false);
  pattern_second_count_->setValue(2);
  pattern_second_value_->setValue(15);
  pattern_second_extent_->setCurrentIndex(0);
  pattern_second_direction_->setCurrentIndex(0);
  pattern_count_ = 3;
  pattern_value_ = command == Command::CircularPattern ? 360 : 40;
  pattern_extent_->setCurrentIndex(0);
  pattern_direction_->setCurrentIndex(0);
  pattern_count_editor_->setValue(pattern_count_);
  pattern_first_group_->setTitle(command == Command::BodyMirror ? QStringLiteral("Mirror Plane")
                                 : command == Command::CircularPattern
                                     ? QStringLiteral("Axis")
                                     : QStringLiteral("Direction 1"));
  pattern_sources_form_->setRowVisible(pattern_source_mode_, command != Command::BodyMirror);
  pattern_second_group_->setVisible(command == Command::LinearPattern);
  pattern_skip_group_->setVisible(command != Command::BodyMirror);
  rebuild_pattern_skip_list();
  auto* form = pattern_form_;
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
  std::optional<PatternPlacementOptions> placements;
  if (command_ != Command::BodyMirror &&
      (!pattern_skipped_.empty() ||
       (command_ == Command::LinearPattern && pattern_second_group_->isChecked()))) {
    PatternPlacementOptions options;
    options.skipped.assign(pattern_skipped_.begin(), pattern_skipped_.end());
    if (command_ == Command::LinearPattern && pattern_second_group_->isChecked()) {
      const auto second = static_cast<std::size_t>(pattern_second_reference_->currentIndex());
      if (second >= pattern_axes_.size())
        return Result<std::size_t>::failure(Error::validation("pattern", "Select direction 2"));
      options.second_direction = LinearPatternDirection{
          pattern_axes_[second].reference, pattern_second_count_parameter_,
          pattern_second_extent_->currentIndex() == 0 ? LinearPatternExtentMode::Spacing
                                                      : LinearPatternExtentMode::TotalExtent,
          pattern_second_extent_parameter_,
          pattern_second_direction_->currentIndex() == 0 ? PatternDirectionSign::Positive
                                                         : PatternDirectionSign::Negative};
    }
    placements = std::move(options);
  }
  Result<std::size_t> begun = Result<std::size_t>::success(1);
  if (command_ == Command::BodyMirror) {
    if (index >= pattern_planes_.size())
      return Result<std::size_t>::failure(Error::validation("pattern", "Select Mirror Plane"));
    begun = coordinator_.pattern_mirror().begin_mirror(session_, pattern_feature_, pattern_name_,
                                                       pattern_planes_[index].reference,
                                                       pattern_result_, body.value());
  } else {
    if (index >= pattern_axes_.size())
      return Result<std::size_t>::failure(Error::validation("pattern", "Select Pattern Axis"));
    auto quantity = Quantity::count(pattern_count_);
    if (!quantity)
      return Result<std::size_t>::failure(quantity.error());
    auto count = Parameter::create_count(pattern_count_parameter_, pattern_name_ + " Count",
                                         quantity.value());
    if (!count)
      return Result<std::size_t>::failure(count.error());
    std::vector<Parameter> pending{count.value()};
    if (command_ == Command::LinearPattern) {
      auto length = Quantity::length_mm(pattern_value_);
      if (!length)
        return Result<std::size_t>::failure(length.error());
      auto extent = Parameter::create_length(pattern_extent_parameter_, pattern_name_ + " Spacing",
                                             length.value());
      if (!extent)
        return Result<std::size_t>::failure(extent.error());
      pending.push_back(extent.value());
      if (pattern_second_group_->isChecked()) {
        auto count2 =
            Parameter::create_count(pattern_second_count_parameter_, pattern_name_ + " Count 2",
                                    Quantity::count(pattern_second_count_->value()).value());
        auto spacing2 =
            Parameter::create_length(pattern_second_extent_parameter_, pattern_name_ + " Spacing 2",
                                     Quantity::length_mm(pattern_second_value_->value()).value());
        if (!count2)
          return Result<std::size_t>::failure(count2.error());
        if (!spacing2)
          return Result<std::size_t>::failure(spacing2.error());
        pending.push_back(count2.value());
        pending.push_back(spacing2.value());
      }
      begun = coordinator_.pattern_mirror().begin_linear_pattern(
          session_, pattern_feature_, pattern_name_, pattern_axes_[index].reference,
          pattern_count_parameter_,
          pattern_extent_->currentIndex() == 0 ? LinearPatternExtentMode::Spacing
                                               : LinearPatternExtentMode::TotalExtent,
          pattern_extent_parameter_, pattern_result_, pending, body.value(), placements);
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
  if (placements && command_ == Command::CircularPattern)
    coordinator_.pattern_mirror().set_placements(*placements);
  for (int i = 0; i < pattern_sources_->count(); ++i)
    if (pattern_sources_->item(i)->isSelected()) {
      const auto id = pattern_sources_->item(i)->data(Qt::UserRole).toString().toStdString();
      auto added = pattern_holes_mode()
                       ? coordinator_.pattern_mirror().add_source_feature(FeatureId(id))
                       : coordinator_.pattern_mirror().add_source_body(BodyId(id));
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
    window_.statusBar()->showMessage(task_summary_->text());
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
  rebuild_pattern_skip_list();
}

Result<GuiPartFeaturePreview> ShellFeatureTools::set_pattern_count(int value) {
  if (!pattern() || command_ == Command::BodyMirror || value < 1 || value > 1000)
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("pattern", "Count must be between 1 and 1000"));
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
          Error::validation("pattern", "finish the active manipulator drag first"));
    candidate = *layer_.candidate();
  }
  return accept_manipulator_candidate(candidate);
}

void ShellFeatureTools::update_pattern_pick_mode() {
  const QSignalBlocker first_block(pattern_reference_button_),
      second_block(pattern_second_reference_button_);
  pattern_reference_button_->setChecked(pattern_reference_picking_ &&
                                        !pattern_second_reference_picking_);
  pattern_second_reference_button_->setChecked(pattern_reference_picking_ &&
                                               pattern_second_reference_picking_);
  pattern_hole_picks_.clear();
  viewport_.clear_preview_selection_targets();
  std::vector<std::string> scope;
  if (pattern_reference_picking_) {
    if (command_ == Command::BodyMirror)
      for (const auto& plane : pattern_planes_)
        scope.push_back(plane.semantic_id);
    else
      for (const auto& axis : pattern_axes_)
        scope.push_back(axis.semantic_id);
  } else if (pattern_holes_mode() && session_.part_document() && session_.part_shape_cache()) {
    std::vector<geometry::ViewportSceneItem> targets;
    const auto& part = *session_.part_document();
    for (int i = 0; i < pattern_sources_->count(); ++i) {
      const auto* hole = part.find_hole_feature(
          FeatureId(pattern_sources_->item(i)->data(Qt::UserRole).toString().toStdString()));
      if (!hole)
        continue;
      for (const auto& placement : hole->placements()) {
        auto ref = HoleInstanceReference::create(hole->id(), placement.id, HoleReferenceRole::Axis);
        if (!ref)
          continue;
        auto resolved = geometry::HoleInstanceGeometry::resolve(part, ref.value(),
                                                                *session_.part_shape_cache());
        if (!resolved)
          continue;
        const auto& h = hole->id().value();
        const auto& p = placement.id.value();
        const auto id = "pattern-source/hole/" + std::to_string(h.size()) + ":" + h + "/" +
                        std::to_string(p.size()) + ":" + p;
        scope.push_back(id);
        pattern_hole_picks_.emplace(id, hole->id());
        targets.push_back(
            {geometry::ViewportSceneKind::Face, id, std::move(resolved.value().support)});
      }
    }
    auto shown = viewport_.set_preview_selection_targets(std::move(targets));
    if (!shown)
      report(shown.error());
  } else
    for (int i = 0; i < pattern_sources_->count(); ++i)
      scope.push_back("body/" +
                      pattern_sources_->item(i)->data(Qt::UserRole).toString().toStdString());
  viewport_.set_selection_semantic_scope(std::move(scope));
  viewport_.set_selection_filter_mask(
      selection_kind_bit(pattern_reference_picking_ ? GuiSelectionKind::Datum
                         : pattern_holes_mode()     ? GuiSelectionKind::Face
                                                    : GuiSelectionKind::Body));
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
            auto* editor =
                pattern_second_reference_picking_ ? pattern_second_reference_ : pattern_reference_;
            const QSignalBlocker block(editor);
            editor->setCurrentIndex(static_cast<int>(i));
            pattern_second_reference_picking_ = false;
          }
          update_pattern_pick_mode();
          refresh_pattern_preview();
          return true;
        }
      return false;
    };
    return command_ == Command::BodyMirror ? match(pattern_planes_) : match(pattern_axes_);
  }
  std::string source;
  if (pattern_holes_mode()) {
    const auto found = pattern_hole_picks_.find(selection.semantic_id);
    if (selection.kind != GuiSelectionKind::Face || found == pattern_hole_picks_.end())
      return false;
    source = found->second.value();
  } else {
    if (selection.kind != GuiSelectionKind::Body || !selection.semantic_id.starts_with("body/"))
      return false;
    source = selection.semantic_id.substr(5);
  }
  const QString id = QString::fromStdString(source);
  for (int i = 0; i < pattern_sources_->count(); ++i)
    if (pattern_sources_->item(i)->data(Qt::UserRole).toString() == id) {
      auto* item = pattern_sources_->item(i);
      item->setSelected(!item->isSelected());
      return true;
    }
  return false;
}
} // namespace blcad::gui
