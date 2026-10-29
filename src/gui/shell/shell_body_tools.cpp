#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/gui_icons.hpp"

#include "blcad/geometry/body_rotation_axis_resolver.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>

namespace blcad::gui {
namespace {
QString title(ShellFeatureTools::Command command) {
  using C = ShellFeatureTools::Command;
  if (command == C::BodyBoolean)
    return QStringLiteral("Combine Bodies");
  if (command == C::BodyTranslate)
    return QStringLiteral("Move Bodies");
  if (command == C::BodyRotate)
    return QStringLiteral("Rotate Bodies");
  return QStringLiteral("Scale Bodies");
}
} // namespace

bool ShellFeatureTools::body_operation() const noexcept {
  return command_ == Command::BodyBoolean || body_transform();
}
bool ShellFeatureTools::body_transform() const noexcept {
  return command_ == Command::BodyTranslate || command_ == Command::BodyRotate ||
         command_ == Command::BodyScale;
}

QString ShellFeatureTools::body_summary() const {
  return command_ == Command::BodyBoolean
             ? QStringLiteral("%1: %2 tool bodies. Preview valid.")
                   .arg(boolean_operation_->currentText())
                   .arg(body_sources_->selectedItems().size())
             : QStringLiteral("%1 bodies. Preview valid; apply together as one step.")
                   .arg(body_sources_->selectedItems().size());
}

void ShellFeatureTools::build_body_controls(QWidget* panel) {
  body_controls_ = new QWidget(panel);
  body_controls_->setMinimumWidth(280);
  auto* column = new QVBoxLayout(body_controls_);
  column->setContentsMargins(0, 0, 0, 0);
  auto* form = new QFormLayout;
  body_form_ = form;
  column->addLayout(form);
  boolean_operation_ = new QComboBox(body_controls_);
  boolean_operation_->setObjectName("blcad.boolean.operation");
  boolean_operation_->addItems(
      {QStringLiteral("Union"), QStringLiteral("Subtract"), QStringLiteral("Intersect")});
  form->addRow(QStringLiteral("Operation"), boolean_operation_);
  boolean_target_ = new QComboBox(body_controls_);
  boolean_target_->setObjectName("blcad.boolean.target");
  form->addRow(QStringLiteral("Target Body"), boolean_target_);
  body_axis_ = new QComboBox(body_controls_);
  body_axis_->setObjectName("blcad.body.axis");
  form->addRow(QStringLiteral("Rotation Axis"), body_axis_);
  body_reference_button_ = make_gui_button(body_controls_);
  body_reference_button_->setObjectName("blcad.body.pick_reference");
  body_reference_button_->setCheckable(true);
  form->addRow(body_reference_button_);
  body_sources_ = new QListWidget(body_controls_);
  body_sources_->setObjectName("blcad.body.sources");
  body_sources_->setSelectionMode(QAbstractItemView::MultiSelection);
  body_sources_->setMinimumHeight(95);
  form->addRow(new QLabel(QStringLiteral("Body Selection"), body_controls_));
  form->addRow(body_sources_);
  boolean_result_ = new QComboBox(body_controls_);
  boolean_result_->setObjectName("blcad.boolean.result");
  boolean_result_->addItems({QStringLiteral("Modify Target Body"), QStringLiteral("New Body")});
  form->addRow(QStringLiteral("Result"), boolean_result_);
  boolean_keep_tools_ = new QCheckBox(QStringLiteral("Keep Tool Bodies"), body_controls_);
  boolean_keep_tools_->setObjectName("blcad.boolean.keep_tools");
  form->addRow(boolean_keep_tools_);
  for (std::size_t i = 0; i < body_translation_editors_.size(); ++i) {
    auto* editor = new QLineEdit(body_controls_);
    body_translation_editors_[i] = editor;
    editor->setObjectName(QStringLiteral("blcad.body.translation.%1").arg(i + 1));
    form->addRow(i == 0 ? QStringLiteral("Y (mm)") : QStringLiteral("Z (mm)"), editor);
    editor->installEventFilter(this);
    connect(editor, &QLineEdit::editingFinished, this, [this, editor, i] {
      if (command_ != Command::BodyTranslate || synchronizing_numeric_)
        return;
      auto edited = set_translation_text(i + 1, editor->text());
      if (!edited)
        report(edited.error());
    });
  }
  for (std::size_t i = 0; i < body_center_editors_.size(); ++i) {
    auto* editor = new QLineEdit(body_controls_);
    body_center_editors_[i] = editor;
    editor->setObjectName(QStringLiteral("blcad.body.center.%1").arg(i));
    form->addRow(QStringLiteral("Center %1 (mm)").arg(QStringLiteral("XYZ").at(i)), editor);
    editor->installEventFilter(this);
    connect(editor, &QLineEdit::editingFinished, this, [this, editor, i] {
      if (command_ != Command::BodyScale || synchronizing_numeric_)
        return;
      auto edited = set_scale_center_text(i, editor->text());
      if (!edited)
        report(edited.error());
    });
  }
  body_owned_sketches_ = new QCheckBox(QStringLiteral("Move Owned Sketches"), body_controls_);
  body_owned_construction_ =
      new QCheckBox(QStringLiteral("Move Owned Construction Geometry"), body_controls_);
  body_owned_sketches_->setToolTip(
      QStringLiteral("Transform already owned sketches with the bodies."));
  body_owned_construction_->setToolTip(
      QStringLiteral("Transform already owned construction geometry with the bodies."));
  form->addRow(body_owned_sketches_);
  form->addRow(body_owned_construction_);
  auto* hint =
      new QLabel(QStringLiteral("Click again to add or remove bodies. "
                                "The list remains available when source bodies are hidden."),
                 body_controls_);
  hint->setWordWrap(true);
  // Keep wrapping guidance out of the dynamic form: Qt 6.4's height-for-width
  // pass can reuse hidden rows' indices and collapse unrelated visible fields.
  column->addWidget(hint);
  panel->layout()->addWidget(body_controls_);
  body_controls_->hide();
  connect(body_sources_, &QListWidget::itemSelectionChanged, this, [this] {
    if (body_operation())
      refresh_body_preview();
  });
  for (auto* combo : {boolean_operation_, boolean_target_, boolean_result_, body_axis_})
    connect(combo, &QComboBox::currentIndexChanged, this, [this] {
      if (!body_operation())
        return;
      body_reference_picking_ = false;
      update_body_pick_mode();
      refresh_body_preview();
    });
  for (auto* checkbox : {boolean_keep_tools_, body_owned_sketches_, body_owned_construction_})
    connect(checkbox, &QCheckBox::toggled, this, [this] {
      if (body_operation())
        refresh_body_preview();
    });
  connect(body_reference_button_, &QPushButton::toggled, this, [this](bool checked) {
    if (!body_operation())
      return;
    if (!layer_.active_handle_id().empty()) {
      layer_.cancel();
      restore_drag_start_value();
      if (viewport_.mouseGrabber() == &viewport_)
        viewport_.release_interaction_mouse();
    }
    body_reference_picking_ = checked;
    update_body_pick_mode();
  });
}

QAction* ShellFeatureTools::body_action(Command command) const noexcept {
  for (const auto& [kind, action] : body_actions_)
    if (kind == command)
      return action;
  return nullptr;
}

bool ShellFeatureTools::body_context_available(const BodyContext& context) const {
  if (!body_action(context.command) || session_.document_kind() != GuiDocumentKind::Part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_document() ||
      session_.part_document()->is_reference_part() || !session_.part_shape_cache() ||
      !session_.derived_results_fresh())
    return false;
  const auto* body = session_.part_document()->find_body(context.body);
  const auto* shape = session_.part_shape_cache()->find_body_shape(context.body);
  return body && body->visibility() == BodyVisibility::Visible &&
         (context.command != Command::BodyBoolean || body->kind() == BodyKind::Solid) && shape &&
         !shape->empty();
}

std::optional<ShellFeatureTools::BodyContext>
ShellFeatureTools::body_context(Command command) const {
  if (!body_action(command) || active() || session_.task().active() || window_.active_sketch() ||
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
        body_context_available({command, body.id()}))
      return BodyContext{command, body.id()};
  return std::nullopt;
}

bool ShellFeatureTools::select_body_context(const BodyContext& context) {
  if (!body_operation() || command_ != context.command || !session_.task().active() ||
      session_.task().command_id() != command_id() || !body_context_available(context))
    return false;
  const auto id = QString::fromStdString(context.body.value());
  for (int i = 0; i < body_sources_->count(); ++i)
    if (body_sources_->item(i)->data(Qt::UserRole).toString() == id) {
      {
        const QSignalBlocker sources(body_sources_), target(boolean_target_);
        body_sources_->clearSelection();
        if (command_ == Command::BodyBoolean)
          boolean_target_->setCurrentIndex(boolean_target_->findData(id));
        else
          body_sources_->item(i)->setSelected(true);
      }
      body_reference_picking_ = false;
      update_body_pick_mode();
      refresh_body_preview();
      return true;
    }
  return false;
}

Result<std::size_t> ShellFeatureTools::begin_body_operation(Command command) {
  const auto fail = [](std::string message) {
    return Result<std::size_t>::failure(Error::validation("gui.shell.body", std::move(message)));
  };
  const auto* part = session_.part_document();
  if (active() || session_.task().active() || window_.active_sketch() || !part ||
      session_.workspace() != GuiWorkspace::Part || !session_.part_shape_cache())
    return fail("Body operation requires a part without an active command");
  if (command != Command::BodyBoolean && command != Command::BodyTranslate &&
      command != Command::BodyRotate && command != Command::BodyScale)
    return fail("Unknown body operation");
  const QSignalBlocker sources(body_sources_), target(boolean_target_),
      operation(boolean_operation_), result(boolean_result_), axis(body_axis_),
      keep(boolean_keep_tools_), sketches(body_owned_sketches_),
      construction(body_owned_construction_);
  body_sources_->clear();
  boolean_target_->clear();
  body_axis_->clear();
  body_axes_.clear();
  for (const auto& body : part->bodies()) {
    if (body.visibility() != BodyVisibility::Visible ||
        !session_.part_shape_cache()->find_body_shape(body.id()) ||
        (command == Command::BodyBoolean && body.kind() != BodyKind::Solid))
      continue;
    auto* item = new QListWidgetItem(QString::fromStdString(body.name()), body_sources_);
    item->setData(Qt::UserRole, QString::fromStdString(body.id().value()));
    boolean_target_->addItem(QString::fromStdString(body.name()), item->data(Qt::UserRole));
    for (const auto& selection : session_.selection().entries())
      if (selection.semantic_id == "body/" + body.id().value())
        item->setSelected(true);
  }
  if (body_sources_->count() < (command == Command::BodyBoolean ? 2 : 1))
    return fail("Not enough visible recomputed bodies available");
  if (command == Command::BodyBoolean && !body_sources_->selectedItems().empty())
    boolean_target_->setCurrentIndex(body_sources_->row(body_sources_->selectedItems().front()));
  if (command == Command::BodyRotate) {
    QStringList unavailable;
    const auto append = [&](Result<BodyTransformRotationAxis> reference, const QString& label,
                            std::string identity) {
      if (!reference) {
        unavailable.push_back(QString::fromStdString(reference.error().message()));
        return;
      }
      auto resolved = geometry::BodyRotationAxisResolver{}.resolve(*part, reference.value(),
                                                                   *session_.part_shape_cache());
      if (!resolved) {
        unavailable.push_back(label + ": " + QString::fromStdString(resolved.error().message()));
        return;
      }
      body_axis_->addItem(label, QString::fromStdString(identity));
      body_axes_.push_back({std::move(reference.value()), std::move(identity)});
    };
    for (const auto& datum : part->datum_axes())
      append(BodyTransformRotationAxis::create_datum_axis(datum.id()),
             (datum.kind() == DatumAxisKind::Explicit ? QStringLiteral("Datum Axis: ")
                                                      : QStringLiteral("Derived Datum Axis: ")) +
                 QString::fromStdString(datum.name()),
             "datum-axis/" + datum.id().value());
    for (const auto& line : part->construction_lines())
      append(BodyTransformRotationAxis::create_construction_line(line.id()),
             QStringLiteral("Construction Line: ") + QString::fromStdString(line.name()),
             "construction-line/" + line.id().value());
    for (const auto& sketch : part->sketches())
      for (const auto& line : sketch.line_segments()) {
        auto reference = SketchLineReference::create(sketch.id(), line.id());
        if (!reference)
          continue;
        const auto role = line.role() == SketchLineRole::Centerline ? "Centerline"
                          : line.construction()                     ? "Construction"
                                                                    : "Geometry";
        append(BodyTransformRotationAxis::create_sketch_line(reference.value()),
               QStringLiteral("Sketch Line (%1): %2 / %3")
                   .arg(QString::fromLatin1(role), QString::fromStdString(sketch.name()),
                        QString::fromStdString(line.id().value())),
               reference.value().identity());
      }
    auto generated = geometry::ViewportSceneBuilder{}.build_body_rotation_sources(
        *part, *session_.part_shape_cache());
    if (generated) {
      for (const auto& source : generated.value())
        if (const auto* edge = std::get_if<SemanticEdgeReference>(&source.reference)) {
          const auto* producer = part->find_feature(edge->source_feature());
          if (!producer)
            continue;
          append(BodyTransformRotationAxis::create_semantic_edge(*edge),
                 QStringLiteral("Edge: ") +
                     QString::fromStdString(producer->name() + " / " +
                                            std::string(to_string(edge->edge()))),
                 source.item.semantic_id);
        }
    } else
      unavailable.push_back(QString::fromStdString(generated.error().message()));
    body_axis_->setToolTip(unavailable.empty()
                               ? QStringLiteral("Choose a saved datum, construction line, Sketch "
                                                "line or supported straight feature edge. "
                                                "Sketch lines are selected in this list.")
                               : QStringLiteral("Unavailable axis sources:\n") +
                                     unavailable.join('\n'));
    if (body_axes_.empty())
      return fail("Create a resolvable rotation axis first");
    for (std::size_t i = 0; i < body_axes_.size(); ++i)
      if (body_axes_[i].semantic_id == "datum-axis/datum.axis.z")
        body_axis_->setCurrentIndex(i);
  }
  for (std::size_t n = 1;; ++n) {
    const auto suffix = std::to_string(n);
    body_feature_ = FeatureId("feature.bodyop." + suffix);
    body_result_ = BodyId("body.boolean." + suffix);
    body_result_name_ = "Boolean Result " + suffix;
    if (part->dependency_graph().has_node(body_feature_.value()) || part->find_body(body_result_) ||
        std::any_of(part->bodies().begin(), part->bodies().end(),
                    [&](const auto& b) { return b.name() == body_result_name_; }))
      continue;
    body_transform_ids_.clear();
    bool collision = false;
    for (int i = 0; i < body_sources_->count(); ++i) {
      BodyTransformId id("transform.bodyop." + suffix + "." + std::to_string(i + 1));
      collision |= part->dependency_graph().has_node(id.value());
      body_transform_ids_.push_back(id);
    }
    if (!collision)
      break;
  }
  command_ = command;
  if (!session_.task().begin(std::string(command_id())) ||
      !session_.task().begin_parameter_editing() || !session_.task().show_preview()) {
    command_ = Command::None;
    if (session_.task().active())
      (void)session_.task().cancel();
    return fail("Could not start body task");
  }
  previous_selection_mask_ = viewport_.selection_filter_mask();
  previous_semantic_scope_ = viewport_.selection_semantic_scope();
  body_translation_ = {};
  body_scale_center_ = {};
  body_value_ = command == Command::BodyScale ? 1 : 0;
  body_reference_picking_ = false;
  boolean_operation_->setCurrentIndex(0);
  boolean_result_->setCurrentIndex(0);
  boolean_keep_tools_->setChecked(false);
  body_owned_sketches_->setChecked(false);
  body_owned_construction_->setChecked(false);
  auto* form = body_form_;
  for (QWidget* widget : std::array<QWidget*, 4>{boolean_operation_, boolean_target_,
                                                 boolean_result_, boolean_keep_tools_})
    form->setRowVisible(widget, command == Command::BodyBoolean);
  form->setRowVisible(body_axis_, command == Command::BodyRotate);
  form->setRowVisible(body_reference_button_,
                      command == Command::BodyBoolean || command == Command::BodyRotate);
  body_reference_button_->setText(command == Command::BodyBoolean
                                      ? QStringLiteral("Pick Target Body in Model")
                                      : QStringLiteral("Pick Rotation Axis in Model"));
  apply_gui_button_icon(*body_reference_button_);
  for (auto* editor : body_translation_editors_)
    form->setRowVisible(editor, command == Command::BodyTranslate);
  for (auto* editor : body_center_editors_)
    form->setRowVisible(editor, command == Command::BodyScale);
  form->setRowVisible(body_owned_sketches_, body_transform());
  form->setRowVisible(body_owned_construction_, body_transform());
  show_task_panel(title(command),
                  command == Command::BodyScale    ? QString()
                  : command == Command::BodyRotate ? QStringLiteral("deg")
                                                   : QStringLiteral("mm"),
                  body_value_);
  update_body_pick_mode();
  refresh_body_preview();
  window_.refresh();
  return Result<std::size_t>::success(1);
}

Result<std::size_t> ShellFeatureTools::rebuild_body_controller() {
  if (viewport_.mouseGrabber() == &viewport_)
    viewport_.release_interaction_mouse();
  layer_.cancel();
  drag_start_value_.reset();
  coordinator_.cancel();
  auto& controller = coordinator_.body_operation();
  if (command_ == Command::BodyBoolean) {
    const QString target = boolean_target_->currentData().toString();
    {
      const QSignalBlocker block(body_sources_);
      for (int i = 0; i < body_sources_->count(); ++i) {
        auto* item = body_sources_->item(i);
        const bool is_target = item->data(Qt::UserRole).toString() == target;
        if (is_target)
          item->setSelected(false);
        item->setFlags(is_target ? Qt::NoItemFlags : Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      }
    }
    const bool new_body = boolean_result_->currentIndex() == 1;
    std::optional<Body> pending;
    if (new_body) {
      auto body = Body::create(body_result_, body_result_name_, BodyKind::Solid);
      if (!body)
        return Result<std::size_t>::failure(body.error());
      pending = body.value();
    }
    auto begun = controller.begin_boolean(
        session_, body_feature_,
        static_cast<BodyBooleanOperation>(boolean_operation_->currentIndex()),
        BodyId(target.toStdString()),
        new_body ? BodyBooleanResultMode::NewBody : BodyBooleanResultMode::ModifyTarget,
        new_body ? std::optional<BodyId>{body_result_} : std::nullopt, pending);
    if (!begun)
      return begun;
    for (auto* item : body_sources_->selectedItems()) {
      auto added = controller.add_tool_body(
          session_, BodyId(item->data(Qt::UserRole).toString().toStdString()));
      if (!added)
        return added;
    }
    controller.set_keep_tool_bodies(boolean_keep_tools_->isChecked());
  } else {
    bool first = true;
    GuiFeatureManipulatorFrame frame;
    if (command_ == Command::BodyRotate) {
      const int selected = body_axis_->currentIndex();
      if (selected < 0 || static_cast<std::size_t>(selected) >= body_axes_.size())
        return Result<std::size_t>::failure(
            Error::validation("body.rotation.axis", "Choose a resolvable rotation axis"));
      const auto& axis = body_axes_[selected];
      auto resolved = geometry::BodyRotationAxisResolver{}.resolve(
          *session_.part_document(), axis.reference, *session_.part_shape_cache());
      if (!resolved)
        return Result<std::size_t>::failure(resolved.error());
      const auto& [origin, direction] = resolved.value();
      frame.origin = origin;
      frame.plane_normal = direction;
      frame.reference_direction = normalized(
          cross(std::abs(direction.x) < .9 ? Vector3{1, 0, 0} : Vector3{0, 1, 0}, direction));
    } else if (command_ == Command::BodyScale)
      frame.origin = body_scale_center_;
    for (int i = 0; i < body_sources_->count(); ++i) {
      auto* item = body_sources_->item(i);
      if (!item->isSelected())
        continue;
      BodyId body(item->data(Qt::UserRole).toString().toStdString());
      const auto id = body_transform_ids_.at(i);
      auto begun =
          first ? command_ == Command::BodyTranslate
                      ? controller.begin_translate(session_, id, body)
                  : command_ == Command::BodyRotate
                      ? controller.begin_rotate(session_, id, body,
                                                body_axes_.at(body_axis_->currentIndex()).reference)
                      : controller.begin_uniform_scale(session_, id, body, body_scale_center_)
                : controller.add_transform_body(session_, id, body);
      if (!begun)
        return begun;
      first = false;
    }
    if (first)
      return Result<std::size_t>::failure(Error::validation("body", "Select at least one body."));
    if (command_ == Command::BodyTranslate)
      controller.set_translation(body_translation_);
    else if (command_ == Command::BodyRotate)
      controller.set_angle(body_value_);
    else
      controller.set_scale_factor(body_value_);
    controller.set_apply_to_owned_sketches(body_owned_sketches_->isChecked());
    controller.set_apply_to_owned_construction_geometry(body_owned_construction_->isChecked());
    controller.set_manipulator_frame(frame);
  }
  coordinator_.refresh_handles();
  return Result<std::size_t>::success(1);
}

void ShellFeatureTools::refresh_body_preview() {
  synchronize_body_editors();
  auto begun = rebuild_body_controller();
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

void ShellFeatureTools::synchronize_body_editors() {
  if (body_transform() && coordinator_.body_operation().active()) {
    body_translation_ = coordinator_.body_operation().translation();
    body_value_ = current_value();
  }
  const std::array<double, 2> translation{body_translation_.y, body_translation_.z};
  const std::array<double, 3> center{body_scale_center_.x, body_scale_center_.y,
                                     body_scale_center_.z};
  for (std::size_t i = 0; i < translation.size(); ++i) {
    const QSignalBlocker block(body_translation_editors_[i]);
    body_translation_editors_[i]->setText(QString::number(translation[i], 'g', 12));
  }
  for (std::size_t i = 0; i < center.size(); ++i) {
    const QSignalBlocker block(body_center_editors_[i]);
    body_center_editors_[i]->setText(QString::number(center[i], 'g', 12));
  }
}

void ShellFeatureTools::update_body_pick_mode() {
  const QSignalBlocker block(body_reference_button_);
  body_reference_button_->setChecked(body_reference_picking_);
  const auto mask =
      body_reference_picking_ && command_ == Command::BodyRotate
          ? selection_kind_bit(GuiSelectionKind::Datum) | selection_kind_bit(GuiSelectionKind::Edge)
          : selection_kind_bit(GuiSelectionKind::Body);
  viewport_.set_selection_filter_mask(mask);
  std::vector<std::string> scope;
  if (body_reference_picking_ && command_ == Command::BodyRotate) {
    for (const auto& axis : body_axes_)
      scope.push_back(axis.semantic_id);
  } else {
    for (int i = 0; i < body_sources_->count(); ++i) {
      const auto id = body_sources_->item(i)->data(Qt::UserRole).toString();
      if (command_ != Command::BodyBoolean || body_reference_picking_ ||
          id != boolean_target_->currentData().toString())
        scope.push_back("body/" + id.toStdString());
    }
  }
  viewport_.set_selection_semantic_scope(std::move(scope));
  if (command_ == Command::BodyRotate)
    window_.refresh_feature_selection_scene();
  synchronize_overlay();
}

bool ShellFeatureTools::accept_body_selection(const GuiSelection& selection) {
  if (!body_operation())
    return false;
  if (body_reference_picking_ && command_ == Command::BodyRotate) {
    for (std::size_t i = 0; i < body_axes_.size(); ++i)
      if ((selection.kind == GuiSelectionKind::Datum || selection.kind == GuiSelectionKind::Edge) &&
          body_axes_[i].semantic_id == selection.semantic_id) {
        const QSignalBlocker block(body_axis_);
        body_axis_->setCurrentIndex(i);
        body_reference_picking_ = false;
        update_body_pick_mode();
        refresh_body_preview();
        return true;
      }
    return false;
  }
  if (selection.kind != GuiSelectionKind::Body || !selection.semantic_id.starts_with("body/"))
    return false;
  const QString id = QString::fromStdString(selection.semantic_id.substr(5));
  if (body_reference_picking_ && command_ == Command::BodyBoolean) {
    const int index = boolean_target_->findData(id);
    if (index < 0)
      return false;
    const QSignalBlocker block(boolean_target_);
    boolean_target_->setCurrentIndex(index);
    body_reference_picking_ = false;
    update_body_pick_mode();
    refresh_body_preview();
    return true;
  }
  for (int i = 0; i < body_sources_->count(); ++i) {
    auto* item = body_sources_->item(i);
    if (item->data(Qt::UserRole).toString() == id && (item->flags() & Qt::ItemIsSelectable)) {
      item->setSelected(!item->isSelected());
      return true;
    }
  }
  return false;
}

std::vector<std::string> ShellFeatureTools::body_preview_replacements() const {
  std::vector<std::string> result;
  if (command_ == Command::BodyBoolean && boolean_result_->currentIndex() == 0)
    result.push_back("body/" + boolean_target_->currentData().toString().toStdString());
  if (body_transform() || !boolean_keep_tools_->isChecked())
    for (auto* item : body_sources_->selectedItems())
      result.push_back("body/" + item->data(Qt::UserRole).toString().toStdString());
  return result;
}

void ShellFeatureTools::refresh_body_enablement(bool idle_part) {
  std::size_t visible = 0, solids = 0;
  if (idle_part && session_.part_shape_cache())
    for (const auto& body : session_.part_document()->bodies())
      if (body.visibility() == BodyVisibility::Visible &&
          session_.part_shape_cache()->find_body_shape(body.id())) {
        ++visible;
        if (body.kind() == BodyKind::Solid)
          ++solids;
      }
  for (const auto& [command, action] : body_actions_) {
    const auto* part = session_.part_document();
    const bool enabled = command == Command::BodyBoolean ? solids >= 2 : visible >= 1;
    action->setEnabled(enabled && (command != Command::BodyRotate ||
                                   (part && (!part->datum_axes().empty() ||
                                             !part->construction_lines().empty()))));
  }
}
} // namespace blcad::gui
