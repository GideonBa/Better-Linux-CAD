#include "blcad/gui/shell/shell_feature_tools.hpp"

#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <charconv>
#include <cmath>

namespace blcad::gui {
namespace {
std::optional<double> scalar(const QString& text, std::string_view unit) {
  const auto input = text.trimmed().toStdString();
  double value = 0;
  const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
  if (end == input.data() || error != std::errc() || !std::isfinite(value))
    return std::nullopt;
  const auto suffix =
      QString::fromStdString(std::string(end, input.data() + input.size())).trimmed();
  if (!suffix.isEmpty() && suffix.toStdString() != unit)
    return std::nullopt;
  return value;
}
bool distance_extent(ExtrudeExtentMode mode) {
  return mode == ExtrudeExtentMode::Distance || mode == ExtrudeExtentMode::Symmetric ||
         mode == ExtrudeExtentMode::TwoSided;
}
} // namespace

void ShellFeatureTools::clear_extrude_diagnostics() {
  for (auto* group : {extrude_geometry_group_, extrude_behavior_group_, extrude_advanced_group_})
    group->set_diagnostic({});
  extrude_profile_selector_->set_diagnostic({});
  extrude_first_selector_->set_diagnostic({});
  extrude_second_selector_->set_diagnostic({});
}

void ShellFeatureTools::build_extrude_controls(QWidget* panel) {
  extrude_controls_ = new QWidget(panel);
  extrude_controls_->setObjectName(QStringLiteral("blcad.extrude.inputs"));
  auto* column = new QVBoxLayout(extrude_controls_);
  column->setContentsMargins(0, 0, 0, 0);
  extrude_geometry_group_ = new ShellPropertyGroup(
      "blcad.extrude.geometry_group", QStringLiteral("Input Geometry"), extrude_controls_);
  extrude_behavior_group_ = new ShellPropertyGroup("blcad.extrude.behavior_group",
                                                   QStringLiteral("Behavior"), extrude_controls_);
  extrude_advanced_group_ = new ShellPropertyGroup("blcad.extrude.advanced_group",
                                                   QStringLiteral("Advanced"), extrude_controls_);
  for (auto* group : {extrude_geometry_group_, extrude_behavior_group_, extrude_advanced_group_})
    column->addWidget(group);
  extrude_profile_ = new QComboBox(&extrude_geometry_group_->content());
  extrude_profile_->setObjectName("blcad.extrude.profile");
  extrude_profile_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  extrude_profile_->setMinimumContentsLength(15);
  extrude_profile_selector_ = new ShellPropertySelector(
      "blcad.extrude.profile.selector", QStringLiteral("Profile / Open Line"), *extrude_profile_,
      *this, [this](bool active) { set_extrude_profile_picking(active); },
      &extrude_geometry_group_->content());
  extrude_geometry_group_->content_layout().addWidget(extrude_profile_selector_);
  connect(extrude_profile_, &QComboBox::currentIndexChanged, this,
          [this] { change_extrude_profile(); });
  extrude_profile_summary_ = new QLabel(&extrude_geometry_group_->content());
  extrude_profile_summary_->setObjectName("blcad.extrude.profile_summary");
  extrude_profile_summary_->setTextFormat(Qt::PlainText);
  extrude_profile_summary_->setWordWrap(true);
  extrude_geometry_group_->content_layout().addWidget(extrude_profile_summary_);
  auto* rows = &extrude_behavior_group_->content_layout();
  const auto row = [&](const QString& text, QWidget* field) {
    auto* container = new QWidget(extrude_controls_);
    auto* form = new QFormLayout(container);
    form->setContentsMargins(0, 0, 0, 0);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto* label = new QLabel(text, container);
    label->setMinimumWidth(126);
    label->setBuddy(field);
    form->addRow(label, field);
    rows->addWidget(container);
  };
  const auto combo = [&](const char* id, const QString& label) {
    auto* field = new QComboBox(extrude_controls_);
    field->setObjectName(QString::fromLatin1(id));
    field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    field->setMinimumContentsLength(15);
    row(label, field);
    return field;
  };
  extrude_extent_ = combo("blcad.extrude.extent", QStringLiteral("Boundary"));
  for (const auto& [label, mode] : {std::pair{"Distance", ExtrudeExtentMode::Distance},
                                    {"Symmetric", ExtrudeExtentMode::Symmetric},
                                    {"Two Sides", ExtrudeExtentMode::TwoSided},
                                    {"Through All", ExtrudeExtentMode::ThroughAll},
                                    {"To Next", ExtrudeExtentMode::ToNext},
                                    {"To Face", ExtrudeExtentMode::ToFace},
                                    {"Between Faces", ExtrudeExtentMode::Between}})
    extrude_extent_->addItem(QString::fromUtf8(label), static_cast<int>(mode));
  extrude_operation_ = combo("blcad.extrude.operation", QStringLiteral("Operation"));
  for (const auto& [label, mode] : {std::pair{"New Body", FeatureBodyOperationMode::NewBody},
                                    {"Union", FeatureBodyOperationMode::Join},
                                    {"Cut", FeatureBodyOperationMode::Cut},
                                    {"Intersect", FeatureBodyOperationMode::Intersect}})
    extrude_operation_->addItem(QString::fromUtf8(label), static_cast<int>(mode));
  extrude_target_ = combo("blcad.extrude.target", QStringLiteral("Target Body"));
  extrude_direction_ = combo("blcad.extrude.direction", QStringLiteral("Direction"));
  extrude_direction_->addItem(QStringLiteral("Sketch Normal"));
  extrude_direction_->addItem(QStringLiteral("Opposite"));
  extrude_direction_->addItem(QStringLiteral("Along Path"));
  extrude_path_ = combo("blcad.extrude.path", QStringLiteral("Open Path"));
  extrude_second_distance_ = new QLineEdit(QStringLiteral("5"), extrude_controls_);
  extrude_second_distance_->setObjectName(QStringLiteral("blcad.extrude.second_distance"));
  row(QStringLiteral("Second Depth (mm)"), extrude_second_distance_);
  const auto face_selector = [&](const char* id, const QString& title, int role,
                                 ShellPropertySelector*& selector) {
    auto* field = new QComboBox(extrude_controls_);
    field->setObjectName(QString::fromLatin1(id));
    field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    field->setMinimumContentsLength(15);
    selector = new ShellPropertySelector(
        QString::fromLatin1(id) + ".selector", title, *field, *this,
        [this, role](bool active) {
          if (active)
            extrude_pick_role_->setCurrentIndex(role);
          else if (extrude_pick_role_->currentIndex() == role)
            extrude_pick_role_->setCurrentIndex(0);
        },
        &extrude_geometry_group_->content());
    extrude_geometry_group_->content_layout().addWidget(selector);
    return field;
  };
  extrude_first_face_ = face_selector("blcad.extrude.first_face", QStringLiteral("First Face"), 1,
                                      extrude_first_selector_);
  extrude_second_face_ = face_selector("blcad.extrude.second_face", QStringLiteral("Second Face"),
                                       2, extrude_second_selector_);
  rows = &extrude_geometry_group_->content_layout();
  extrude_pick_role_ = combo("blcad.extrude.pick_role", QStringLiteral("Viewport Selection"));
  extrude_pick_role_->addItem(QStringLiteral("Finish Selection"));
  extrude_pick_role_->addItem(QStringLiteral("First Face"));
  rows = &extrude_advanced_group_->content_layout();
  extrude_taper_ = new QLineEdit(extrude_controls_);
  extrude_taper_->setObjectName(QStringLiteral("blcad.extrude.taper"));
  extrude_taper_->setPlaceholderText(QStringLiteral("Optional, -90 < Angle < 90"));
  row(QStringLiteral("Draft (deg)"), extrude_taper_);
  extrude_thin_mode_ = combo("blcad.extrude.thin_mode", QStringLiteral("Wall Layout"));
  for (const auto& [label, mode] : {std::pair{"One Side Left", ExtrudeThinMode::OneSided},
                                    {"Centered", ExtrudeThinMode::MidPlane},
                                    {"Two Sides", ExtrudeThinMode::TwoSided}})
    extrude_thin_mode_->addItem(QString::fromUtf8(label), static_cast<int>(mode));
  extrude_thickness_ = new QLineEdit(QStringLiteral("1"), extrude_controls_);
  extrude_thickness_->setObjectName(QStringLiteral("blcad.extrude.thickness"));
  row(QStringLiteral("Wall Thickness (mm)"), extrude_thickness_);
  extrude_second_thickness_ = new QLineEdit(QStringLiteral("1"), extrude_controls_);
  extrude_second_thickness_->setObjectName(QStringLiteral("blcad.extrude.second_thickness"));
  row(QStringLiteral("Right Wall (mm)"), extrude_second_thickness_);
  extrude_hint_ = new QLabel(extrude_controls_);
  extrude_hint_->setObjectName(QStringLiteral("blcad.extrude.hint"));
  extrude_hint_->setWordWrap(true);
  rows->addWidget(extrude_hint_);
  qobject_cast<QVBoxLayout*>(panel->layout())->addWidget(extrude_controls_);
  for (auto* field : {extrude_extent_, extrude_operation_, extrude_target_, extrude_direction_,
                      extrude_first_face_, extrude_second_face_, extrude_path_, extrude_thin_mode_})
    connect(field, &QComboBox::currentIndexChanged, this, [this] { refresh_extrude_preview(); });
  connect(extrude_pick_role_, &QComboBox::currentIndexChanged, this, [this] {
    update_extrude_pick_mode();
    synchronize_overlay();
  });
  for (auto* field :
       {extrude_second_distance_, extrude_taper_, extrude_thickness_, extrude_second_thickness_}) {
    connect(field, &QLineEdit::textEdited, this, [this] { invalidate_extrude_preview(); });
    connect(field, &QLineEdit::editingFinished, this, [this] { refresh_extrude_preview(); });
  }
  extrude_controls_->hide();
}

void ShellFeatureTools::reset_extrude_controls(GuiFeatureManipulatorFrame frame) {
  extrude_frame_ = frame;
  clear_extrude_diagnostics();
  for (auto* group : {extrude_geometry_group_, extrude_behavior_group_, extrude_advanced_group_})
    group->set_expanded(true);
  const auto profile = profile_from_selection();
  const auto line = open_line_from_selection();
  const bool open = coordinator_.extrude().uses_open_line();
  const auto* sketch =
      session_.part_document() && (open ? line.has_value() : bool(profile))
          ? session_.part_document()->find_sketch(open ? *line : profile.value().first)
          : nullptr;
  extrude_profile_summary_->setText(
      sketch ? QStringLiteral("%1 · %2").arg(QString::fromStdString(sketch->name()),
                                             open ? QStringLiteral("Open thin profile")
                                                  : QStringLiteral("1 profile"))
             : QStringLiteral("Profile selected"));
  const QSignalBlocker b1(extrude_extent_), b2(extrude_operation_), b3(extrude_target_),
      b4(extrude_direction_), b5(extrude_first_face_), b6(extrude_second_face_), b7(extrude_path_),
      b8(extrude_thin_mode_);
  extrude_extent_->setCurrentIndex(0);
  extrude_operation_->setCurrentIndex(0);
  extrude_direction_->setCurrentIndex(0);
  if (!awaiting_extrude_source() && coordinator_.extrude().uses_open_line())
    extrude_direction_->removeItem(2);
  else if (extrude_direction_->count() < 3)
    extrude_direction_->addItem(QStringLiteral("Along Path"));
  extrude_thin_mode_->setCurrentIndex(0);
  extrude_thickness_->setText(QStringLiteral("1"));
  extrude_second_thickness_->setText(QStringLiteral("1"));
  extrude_second_distance_->setText(QStringLiteral("5"));
  extrude_taper_->clear();
  refresh_extrude_reference_choices(false);
  update_extrude_controls();
}

void ShellFeatureTools::refresh_extrude_reference_choices(bool preserve) {
  const auto target = extrude_target_->currentData();
  const auto path = extrude_path_->currentData();
  const auto first = extrude_first_face_->currentData();
  const auto second = extrude_second_face_->currentData();
  const QSignalBlocker b1(extrude_target_), b2(extrude_path_), b3(extrude_first_face_),
      b4(extrude_second_face_);
  extrude_target_->clear();
  extrude_path_->clear();
  extrude_first_face_->clear();
  extrude_second_face_->clear();
  extrude_faces_.clear();
  extrude_face_items_.clear();
  {
    const QSignalBlocker blocker(extrude_pick_role_);
    extrude_pick_role_->setCurrentIndex(0);
  }
  const auto& part = *session_.part_document();
  for (const auto& path : part.path_curves())
    if (path.closure() == PathClosure::Open)
      extrude_path_->addItem(QString::fromStdString(path.name()),
                             QString::fromStdString(path.id().value()));
  const auto* cache = session_.part_shape_cache();
  if (cache) {
    for (const auto& body : part.bodies())
      if (body.kind() == BodyKind::Solid && body.visibility() == BodyVisibility::Visible &&
          cache->find_body_shape(body.id()))
        extrude_target_->addItem(QString::fromStdString(body.name()),
                                 QString::fromStdString(body.id().value()));
    auto sources = geometry::ViewportSceneBuilder{}.build_construction_sources(
        part, *cache, session_.history_prefix_count());
    if (sources)
      for (const auto& source : sources.value())
        if (const auto* face = std::get_if<SemanticFaceReference>(&source.reference)) {
          auto reference =
              FaceReference::create_planar(PartFeatureInputRole::ExtrudeLimitFace, *face);
          if (!reference)
            continue;
          const auto* feature = part.find_feature(face->source_feature());
          const auto face_label = [&] {
            switch (face->face()) {
            case SemanticFace::Top:
              return QStringLiteral("Top");
            case SemanticFace::Bottom:
              return QStringLiteral("Bottom");
            case SemanticFace::Left:
              return QStringLiteral("Left");
            case SemanticFace::Right:
              return QStringLiteral("Right");
            case SemanticFace::Front:
              return QStringLiteral("Front");
            case SemanticFace::Back:
              return QStringLiteral("Back");
            }
            return QString::fromUtf8(to_string(face->face()).data());
          }();
          const auto label =
              QString::fromStdString(feature->name()) + QStringLiteral(" / ") + face_label;
          const auto identity = QString::fromStdString(face->source_feature().value() + "/" +
                                                       std::string(to_string(face->face())));
          extrude_faces_.push_back(reference.value());
          extrude_face_items_.push_back(source.item);
          extrude_first_face_->addItem(label, identity);
          extrude_second_face_->addItem(label, identity);
        }
  }
  if (preserve) {
    extrude_target_->setCurrentIndex(target.isValid() ? extrude_target_->findData(target) : -1);
    extrude_path_->setCurrentIndex(path.isValid() ? extrude_path_->findData(path) : -1);
    extrude_first_face_->setCurrentIndex(first.isValid() ? extrude_first_face_->findData(first)
                                                         : -1);
    extrude_second_face_->setCurrentIndex(second.isValid() ? extrude_second_face_->findData(second)
                                                           : -1);
  } else if (extrude_second_face_->count() > 1) {
    extrude_second_face_->setCurrentIndex(1);
  }
}

void ShellFeatureTools::update_extrude_controls() {
  const auto mode = static_cast<ExtrudeExtentMode>(extrude_extent_->currentData().toInt());
  const bool path = extrude_direction_->currentIndex() == 2;
  const bool thin = coordinator_.extrude().uses_open_line();
  const bool cut = static_cast<FeatureBodyOperationMode>(
                       extrude_operation_->currentData().toInt()) == FeatureBodyOperationMode::Cut;
  extrude_hint_->setText(
      (cut ? QStringLiteral("Cut must remove material from the selected target body. Check the "
                            "target, Normal/Opposite direction and depth or Through All extent. ")
           : QString{}) +
      (thin ? QStringLiteral(
                  "Left and right follow the line direction in the sketch. With Two Sides, "
                  "the first thickness acts left, the second right. Centered uses the total "
                  "wall thickness. Through All and To Next require a target body. "
                  "All input remains local until Apply.")
            : QStringLiteral("Through All and To Next require a target body. "
                             "Limit faces must be perpendicular to the extrusion direction. "
                             "Along Path uses the orientation of an existing open path. "
                             "All input remains local until Apply.")));
  const bool distance = !path && distance_extent(mode);
  numeric_editor_->setVisible(distance);
  value_label_->setVisible(distance);
  extrude_path_->setEnabled(path);
  extrude_extent_->setEnabled(!path);
  extrude_taper_->setEnabled(!path);
  // Each field and label share an independent row container. Qt 6.4's shared
  // form geometry can otherwise occlude another row during mode transitions.
  const auto show_row = [](QWidget* field, bool visible) {
    field->parentWidget()->setVisible(visible);
  };
  show_row(extrude_second_distance_, !path && mode == ExtrudeExtentMode::TwoSided);
  show_row(extrude_first_face_,
           !path && (mode == ExtrudeExtentMode::ToFace || mode == ExtrudeExtentMode::Between));
  show_row(extrude_second_face_, !path && mode == ExtrudeExtentMode::Between);
  const bool faces =
      !path && (mode == ExtrudeExtentMode::ToFace || mode == ExtrudeExtentMode::Between);
  {
    const QSignalBlocker blocker(extrude_pick_role_);
    if (mode == ExtrudeExtentMode::Between && extrude_pick_role_->count() < 3)
      extrude_pick_role_->addItem(QStringLiteral("Second Face"));
    if (mode != ExtrudeExtentMode::Between)
      extrude_pick_role_->removeItem(2);
    if (!faces)
      extrude_pick_role_->setCurrentIndex(0);
  }
  show_row(extrude_pick_role_, faces);
  extrude_pick_role_->setEnabled(!extrude_faces_.empty());
  show_row(extrude_target_, extrude_operation_->currentIndex() != 0);
  show_row(extrude_path_, !thin);
  show_row(extrude_thin_mode_, thin);
  show_row(extrude_thickness_, thin);
  show_row(extrude_second_thickness_,
           thin && static_cast<ExtrudeThinMode>(extrude_thin_mode_->currentData().toInt()) ==
                       ExtrudeThinMode::TwoSided);
  update_extrude_pick_mode();
}

void ShellFeatureTools::update_extrude_pick_mode() {
  if (command_ != Command::Extrude || editing_source_sketch_ || synchronizing_extrude_selection_)
    return;
  const QScopedValueRollback guard(synchronizing_extrude_selection_, true);
  const bool picking = extrude_pick_role_->currentIndex() > 0;
  if (picking)
    extrude_profile_picking_ = false;
  extrude_profile_selector_->set_active(extrude_profile_picking_);
  extrude_first_selector_->set_active(extrude_pick_role_->currentIndex() == 1);
  extrude_second_selector_->set_active(extrude_pick_role_->currentIndex() == 2);
  const bool changed = picking != extrude_reference_picking_;
  extrude_reference_picking_ = picking;
  if (extrude_profile_picking_) {
    viewport_.set_selection_semantic_scope(std::nullopt);
    viewport_.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::SketchEntity));
    if (changed)
      window_.refresh_feature_selection_scene();
    return;
  }
  if (picking) {
    std::vector<std::string> scope;
    for (const auto& item : extrude_face_items_)
      scope.push_back(item.semantic_id);
    viewport_.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Face));
    viewport_.set_selection_semantic_scope(std::move(scope));
  } else {
    viewport_.set_selection_semantic_scope(previous_semantic_scope_);
    viewport_.set_selection_filter_mask(previous_selection_mask_);
  }
  if (changed)
    window_.refresh_feature_selection_scene();
  if (picking) {
    const auto* field =
        extrude_pick_role_->currentIndex() == 1 ? extrude_first_face_ : extrude_second_face_;
    if (field->currentIndex() >= 0)
      (void)viewport_.select_semantic(
          extrude_face_items_.at(static_cast<std::size_t>(field->currentIndex())).semantic_id);
  } else if (changed && extrude_previous_selection_) {
    (void)viewport_.select_semantic(extrude_previous_selection_->semantic_id);
  }
}

bool ShellFeatureTools::accept_extrude_selection(const GuiSelection& selection) {
  if (!extrude_reference_picking_ || synchronizing_extrude_selection_ ||
      command_ != Command::Extrude || selection.kind != GuiSelectionKind::Face)
    return false;
  for (std::size_t row = 0; row < extrude_face_items_.size(); ++row)
    if (extrude_face_items_[row].semantic_id == selection.semantic_id) {
      auto* field =
          extrude_pick_role_->currentIndex() == 1 ? extrude_first_face_ : extrude_second_face_;
      {
        const QSignalBlocker blocker(field);
        field->setCurrentIndex(static_cast<int>(row));
      }
      refresh_extrude_preview();
      return true;
    }
  return false;
}

std::vector<geometry::ViewportSceneItem> ShellFeatureTools::extrude_selection_items() const {
  if (command_ == Command::BodyRotate && body_reference_picking_ && session_.part_document() &&
      session_.part_shape_cache()) {
    std::vector<geometry::ViewportSceneItem> items;
    auto sources = geometry::ViewportSceneBuilder{}.build_body_rotation_sources(
        *session_.part_document(), *session_.part_shape_cache(), session_.history_prefix_count());
    if (sources)
      for (const auto& source : sources.value())
        if (std::any_of(body_axes_.begin(), body_axes_.end(), [&](const auto& axis) {
              return axis.semantic_id == source.item.semantic_id;
            }))
          items.push_back(source.item);
    return items;
  }
  return command_ == Command::Extrude && extrude_reference_picking_
             ? extrude_face_items_
             : std::vector<geometry::ViewportSceneItem>{};
}

void ShellFeatureTools::restore_extrude_selection() {
  const QScopedValueRollback guard(synchronizing_extrude_selection_, true);
  const auto previous_selection = session_.selection();
  extrude_reference_picking_ = false;
  extrude_face_items_.clear();
  extrude_faces_.clear();
  viewport_.set_selection_semantic_scope(previous_semantic_scope_);
  viewport_.set_selection_filter_mask(previous_selection_mask_);
  // A complete workflow restore advances the scene revision. Rebuild before
  // restoring selection, so the subsequent window refresh cannot clear it.
  window_.refresh_feature_selection_scene();
  if (extrude_previous_selection_)
    (void)viewport_.select_semantic(extrude_previous_selection_->semantic_id);
  else {
    viewport_.clear_selection();
    session_.selection() = previous_selection;
    selected_profile_ = extrude_previous_profile_;
    selected_open_line_ = extrude_previous_open_line_;
  }
  extrude_previous_selection_.reset();
  extrude_previous_profile_.reset();
  extrude_previous_open_line_.reset();
}

void ShellFeatureTools::invalidate_extrude_preview() {
  if (command_ != Command::Extrude)
    return;
  preview_valid_ = false;
  viewport_.clear_feature_preview();
  refresh_enablement();
  synchronize_overlay();
}

void ShellFeatureTools::refresh_extrude_preview() {
  if (editing_source_sketch_ || awaiting_extrude_source())
    return;
  if (command_ != Command::Extrude)
    return;
  update_extrude_controls();
  auto preview = preview_and_publish();
  if (!preview)
    report(preview.error());
  if (extrude_reference_picking_)
    window_.refresh();
}

Result<std::size_t> ShellFeatureTools::configure_extrude_candidate() {
  clear_extrude_diagnostics();
  const auto fail = [&](std::string message, ShellPropertyGroup* group = nullptr) {
    (group ? group : extrude_behavior_group_)->set_diagnostic(QString::fromStdString(message));
    return Result<std::size_t>::failure(Error::validation("extrude", std::move(message)));
  };
  if (editing_source_sketch_)
    return fail("Finish the source Sketch before applying Extrude.", extrude_geometry_group_);
  if (awaiting_extrude_source())
    return fail("Select a profile or single open line.", extrude_geometry_group_);
  if (extrude_direction_->currentIndex() < 0)
    return fail("Choose a compatible extrusion direction.");
  if (extrude_source_) {
    const auto plane = sketch_workbench_.plane_view(session_, extrude_source_->sketch);
    if (!plane)
      return fail(plane.error().message(), extrude_geometry_group_);
  }
  if (!layer_.active_handle_id().empty())
    return Result<std::size_t>::success(0);
  auto& controller = coordinator_.extrude();
  const auto mode = static_cast<ExtrudeExtentMode>(extrude_extent_->currentData().toInt());
  const auto operation =
      static_cast<FeatureBodyOperationMode>(extrude_operation_->currentData().toInt());
  const bool path = extrude_direction_->currentIndex() == 2;
  if (!path && distance_extent(mode)) {
    const auto depth = scalar(numeric_editor_->text(), "mm");
    if (!depth || *depth <= 0)
      return fail("Extrude depth must be a positive length.");
  }
  if (!path && operation == FeatureBodyOperationMode::NewBody &&
      (mode == ExtrudeExtentMode::ThroughAll || mode == ExtrudeExtentMode::ToNext))
    return fail("This extent requires an operation with a target body.");
  if (operation != FeatureBodyOperationMode::NewBody && extrude_target_->currentIndex() < 0)
    return fail("Select a visible solid body as the target.");
  controller.set_operation(
      operation,
      operation == FeatureBodyOperationMode::NewBody
          ? std::nullopt
          : std::optional<BodyId>{BodyId(extrude_target_->currentData().toString().toStdString())});
  if (path) {
    if (extrude_path_->currentIndex() < 0)
      return fail("Select an open path; create new paths with Create Path.");
    controller.set_path_curve(PathCurveId(extrude_path_->currentData().toString().toStdString()));
    controller.set_taper(std::nullopt);
    controller.clear_thin();
    controller.set_limit_faces(std::nullopt);
    coordinator_.refresh_handles();
    return Result<std::size_t>::success(1);
  }
  controller.set_extent_mode(mode);
  if (mode == ExtrudeExtentMode::TwoSided) {
    const auto depth = scalar(extrude_second_distance_->text(), "mm");
    if (!depth || *depth <= 0)
      return fail("Second depth must be a positive length.");
    const auto id = controller.distance_parameter_id().value() + ".second";
    auto parameter =
        Parameter::create_length(ParameterId(id), controller.feature_id().value() + " Second Depth",
                                 Quantity::length_mm(*depth, id).value());
    if (!parameter)
      return Result<std::size_t>::failure(parameter.error());
    controller.set_two_sided_new_parameter(std::move(parameter.value()));
  }
  std::optional<double> taper;
  if (!extrude_taper_->text().trimmed().isEmpty()) {
    taper = scalar(extrude_taper_->text(), "deg");
    if (!taper || *taper <= -90 || *taper >= 90)
      return fail("Draft must be between -90 and 90 degrees.", extrude_advanced_group_);
  }
  controller.set_taper(taper);
  if (controller.uses_open_line()) {
    const auto thin_mode = static_cast<ExtrudeThinMode>(extrude_thin_mode_->currentData().toInt());
    const auto thickness = [&](QLineEdit* field, const std::string& suffix,
                               const char* label) -> Result<Parameter> {
      const auto value = scalar(field->text(), "mm");
      if (!value || *value <= 0)
        return Result<Parameter>::failure(
            Error::validation("extrude", "Wall thickness must be a positive length."));
      return Parameter::create_length(
          ParameterId(controller.distance_parameter_id().value() + suffix),
          controller.feature_id().value() + label, Quantity::length_mm(*value).value());
    };
    auto first = thickness(extrude_thickness_, ".wall", " Wall Thickness");
    if (!first) {
      extrude_advanced_group_->set_diagnostic(QString::fromStdString(first.error().message()));
      return Result<std::size_t>::failure(first.error());
    }
    std::optional<Parameter> second;
    if (thin_mode == ExtrudeThinMode::TwoSided) {
      auto value = thickness(extrude_second_thickness_, ".wall.second", " Second Wall Thickness");
      if (!value) {
        extrude_advanced_group_->set_diagnostic(QString::fromStdString(value.error().message()));
        return Result<std::size_t>::failure(value.error());
      }
      second = std::move(value.value());
    }
    controller.set_thin_new_inputs(thin_mode, std::move(first.value()), std::move(second));
  } else {
    controller.clear_thin();
  }
  std::optional<FaceReference> first, second;
  if (mode == ExtrudeExtentMode::ToFace || mode == ExtrudeExtentMode::Between) {
    if (extrude_first_face_->currentIndex() < 0) {
      extrude_first_selector_->set_diagnostic(
          QStringLiteral("Select the first planar limit face."));
      return fail("Select the first planar limit face.", extrude_geometry_group_);
    }
    first = extrude_faces_.at(static_cast<std::size_t>(extrude_first_face_->currentIndex()));
  }
  if (mode == ExtrudeExtentMode::Between) {
    if (extrude_second_face_->currentIndex() < 0) {
      extrude_second_selector_->set_diagnostic(
          QStringLiteral("Select the second planar limit face."));
      return fail("Select the second planar limit face.", extrude_geometry_group_);
    }
    second = extrude_faces_.at(static_cast<std::size_t>(extrude_second_face_->currentIndex()));
  }
  controller.set_limit_faces(std::move(first), std::move(second));
  const bool opposite = extrude_direction_->currentIndex() == 1;
  controller.set_direction(opposite ? ExtrudeDirection::OppositeSketchNormal
                                    : ExtrudeDirection::SketchNormal);
  auto frame = extrude_frame_;
  if (opposite)
    frame.axis = {-frame.axis.x, -frame.axis.y, -frame.axis.z};
  controller.set_manipulator_frame(frame);
  coordinator_.refresh_handles();
  return Result<std::size_t>::success(1);
}
} // namespace blcad::gui
