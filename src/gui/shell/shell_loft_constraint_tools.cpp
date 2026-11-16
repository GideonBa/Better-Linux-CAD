#include "advanced_loft_controls.hpp"
#include "blcad/geometry/loft_adapter.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_reference_repair_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QFormLayout>
#include <QListWidget>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QUuid>
#include <algorithm>
#include <cmath>
namespace blcad::gui {
namespace {
Result<std::size_t> fail(std::string message) {
  return Result<std::size_t>::failure(Error::validation("loft", std::move(message)));
}
std::string face_key(const FaceReference& face) {
  return loft_support_key(face);
}
} // namespace
void ShellFeatureTools::build_advanced_loft_controls(QFormLayout& form) {
  advanced_loft_ = std::make_unique<AdvancedLoftControls>(spatial_controls_);
  form.addRow(advanced_loft_->panel);
  advanced_loft_->changed = [this] {
    if (command_ == Command::Loft)
      refresh_spatial_preview();
  };
  advanced_loft_->face_selected = [this](std::size_t index) {
    if (command_ != Command::Loft || index >= advanced_loft_->faces.size())
      return;
    const auto& id = advanced_loft_->faces[index].target.semantic_id;
    if (id.empty())
      return;
    const QScopedValueRollback guard(spatial_highlighting_, true);
    spatial_pick_->setCurrentIndex(3);
    update_spatial_pick_scope();
    (void)viewport_.select_semantic(id);
  };
  connect(advanced_loft_->include_hidden, &QCheckBox::toggled, this, [this] {
    if (command_ != Command::Loft || advanced_loft_->loading)
      return;
    auto result = refresh_loft_supports();
    if (!result)
      report(result.error());
    window_.refresh_feature_selection_scene();
    update_spatial_pick_scope();
    refresh_spatial_preview();
  });
  connect(advanced_loft_->repair, &QPushButton::clicked, this, [this] {
    cancel();
    auto result = window_.reference_repair_tools().begin();
    if (!result)
      report(result.error());
  });
}
Result<std::vector<ProfileSectionReference>> ShellFeatureTools::selected_loft_sections() const {
  std::vector<ProfileSectionReference> values;
  for (int i = 0; i < spatial_order_->count(); ++i) {
    const auto* item = spatial_order_->item(i);
    const auto& source = spatial_inputs_.at(item->data(Qt::UserRole).toInt());
    const ProfileSectionReference* saved = nullptr;
    for (const auto& s : spatial_saved_sections_) {
      if (source.profile && s.kind() == LoftSectionKind::ClosedRegion) {
        const auto& p = std::get<ProfileRegionReference>(s.source());
        if (p.sketch() == source.profile->first && p.profile() == source.profile->second)
          saved = &s;
      }
      if (source.path && s.kind() == LoftSectionKind::OpenPath &&
          std::get<PathCurveId>(s.source()) == source.path)
        saved = &s;
    }
    const bool flip = item->checkState() == Qt::Checked;
    auto section =
        source.profile
            ? ProfileSectionReference::create_closed_region(
                  ProfileRegionReference::create(source.profile->first, source.profile->second,
                                                 PartFeatureInputRole::LoftSection)
                      .value(),
                  saved ? saved->alignment_reference() : std::nullopt, flip,
                  saved ? saved->rotation_offset() : std::nullopt)
            : ProfileSectionReference::create_open_path(
                  *source.path, flip, saved ? saved->rotation_offset() : std::nullopt);
    if (!section)
      return Result<std::vector<ProfileSectionReference>>::failure(section.error());
    values.push_back(std::move(section.value()));
  }
  return Result<std::vector<ProfileSectionReference>>::success(std::move(values));
}
Result<std::size_t> ShellFeatureTools::refresh_loft_supports() {
  auto& ui = *advanced_loft_;
  const auto* part = session_.part_document();
  const auto* cache = spatial_input_cache_ ? &*spatial_input_cache_ : session_.part_shape_cache();
  if (!part || !cache)
    return fail("Loft inputs are unavailable");
  const auto prefix =
      spatial_input_cache_ ? spatial_input_prefix_ : session_.history_prefix_count();
  for (auto& face : ui.faces)
    face.available = false;
  auto resolved =
      query_with_step_import_context(session_, [&](auto* context) -> Result<std::size_t> {
        auto observed = geometry::observe_imported_part_sources(*part, context);
        if (!observed)
          return Result<std::size_t>::failure(observed.error());
        for (const auto& body : part->bodies()) {
          const bool hidden = body.visibility() == BodyVisibility::Hidden ||
                              !session_.presentation_visible("body/" + body.id().value());
          if ((hidden && !ui.include_hidden->isChecked()) || !cache->find_body_shape(body.id()))
            continue;
          auto targets = geometry::FinishingSelectionBuilder::build(
              *part, *cache, body.id(), PartFeatureInputRole::LoftBoundarySupport, context, prefix,
              ui.include_hidden->isChecked()
                  ? geometry::FinishingSelectionVisibility::IncludeHiddenInputs
                  : geometry::FinishingSelectionVisibility::VisibleOnly);
          if (!targets)
            return Result<std::size_t>::failure(targets.error());
          std::size_t number = 0;
          for (auto& target : targets.value()) {
            target.label = body.name() + " / Support Face " + std::to_string(++number);
            if (hidden || spatial_input_cache_)
              target.semantic_id = "loft-input/" + target.semantic_id;
            const auto key = face_key(std::get<FaceReference>(target.reference));
            auto previous = std::find_if(ui.faces.begin(), ui.faces.end(), [&](const auto& f) {
              return f.owner == body.id() &&
                     face_key(std::get<FaceReference>(f.target.reference)) == key;
            });
            AdvancedLoftControls::Support value{std::move(target), body.id(), true};
            if (previous == ui.faces.end())
              ui.faces.push_back(std::move(value));
            else
              *previous = std::move(value);
          }
        }
        return Result<std::size_t>::success(ui.faces.size());
      });
  if (!resolved)
    return resolved;
  const QScopedValueRollback loading(ui.loading, true);
  for (int row = 0; row < ui.supports->rowCount(); ++row) {
    auto* box = qobject_cast<QComboBox*>(ui.supports->cellWidget(row, 2));
    const int previous = box->currentData().toInt();
    const bool selected = box->currentIndex() >= 0;
    box->clear();
    for (std::size_t i = 0; i < ui.faces.size(); ++i)
      if (ui.faces[i].available)
        box->addItem(QString::fromStdString(ui.faces[i].target.label), int(i));
    if (selected && std::size_t(previous) < ui.faces.size() && !ui.faces[previous].available)
      box->addItem("Saved Support (outside input scope)", previous);
    box->setCurrentIndex(selected ? box->findData(previous) : -1);
  }
  return resolved;
}
Result<std::size_t> ShellFeatureTools::configure_loft_constraints() {
  auto& ui = *advanced_loft_;
  auto& loft = coordinator_.loft();
  std::vector<PathCurveId> guides;
  std::vector<bool> reversed;
  for (int i = 0; i < spatial_guides_->count(); ++i) {
    guides.emplace_back(spatial_guides_->item(i)->data(Qt::UserRole).toString().toStdString());
    reversed.push_back(spatial_guides_->item(i)->checkState() == Qt::Checked);
  }
  ui.sync(*session_.part_document(), loft.sections(), std::move(guides), std::move(reversed));
  ParameterId tolerance(ui.tolerance_binding->currentData().toString().toStdString());
  std::vector<Parameter> pending;
  if (ui.enabled->isChecked() && tolerance.empty()) {
    bool numeric = false;
    const double value = ui.linear->text().toDouble(&numeric);
    if (!numeric || !std::isfinite(value) || value < 1e-9 || value > 1)
      return fail("Enter a linear tolerance from 1e-9 to 1 mm");
    // Stable task-local ID; never overwrite a shared or formula-bound parameter.
    tolerance = ParameterId(spatial_independent_twist_.value() + ".loft_tolerance");
    auto name = spatial_name_ + " Tolerance";
    const auto root = name;
    for (int suffix = 2; std::any_of(session_.part_document()->parameters().begin(),
                                     session_.part_document()->parameters().end(),
                                     [&](const auto& p) { return p.name() == name; });
         ++suffix)
      name = root + " " + std::to_string(suffix);
    auto parameter = Parameter::create_length(tolerance, name, Quantity::length_mm(value).value());
    if (!parameter)
      return Result<std::size_t>::failure(parameter.error());
    pending.push_back(std::move(parameter.value()));
  }
  if (ui.enabled->isChecked() && ui.tolerance_binding->currentIndex() < 0)
    return fail("Choose an available tolerance parameter or an independent value");
  auto constraints = ui.collect(tolerance);
  if (!constraints)
    return Result<std::size_t>::failure(constraints.error());
  loft.set_pending_parameters(std::move(pending));
  loft.set_constraints(std::move(constraints.value()));
  return Result<std::size_t>::success(1);
}
std::vector<geometry::ViewportSceneItem> ShellFeatureTools::loft_selection_items() const {
  std::vector<geometry::ViewportSceneItem> result;
  if (!advanced_loft_)
    return result;
  for (const auto& face : advanced_loft_->faces)
    if (face.available && !face.target.shape.empty())
      result.push_back({geometry::ViewportSceneKind::Face, face.target.semantic_id,
                        face.target.shape, SketchLineRole::Geometry, face.owner.value()});
  return result;
}
void ShellFeatureTools::refresh_loft_feedback(const geometry::GeometryShape* shape) {
  auto& ui = *advanced_loft_;
  ui.feedback->clear();
  if (!shape)
    return;
  const auto report = geometry::LoftAdapter::verification(*shape);
  if (!report)
    return;
  const auto& r = *report;
  QString text = QString("Native Loft witnesses: %1 stations; G1 joins %2, G2 joins %3. Curve "
                         "deviation %4 / %5 mm; join position %6 mm; normal angle %7 / %8 rad.")
                     .arg(r.stations)
                     .arg(r.g1_joins)
                     .arg(r.g2_joins)
                     .arg(r.curve_error_mm, 0, 'g', 9)
                     .arg(r.linear_tolerance_mm, 0, 'g', 9)
                     .arg(r.position_error_mm, 0, 'g', 9)
                     .arg(r.normal_error_rad, 0, 'g', 9)
                     .arg(r.angular_tolerance_rad, 0, 'g', 9);
  if (r.g2_joins)
    text += QString(" Curvature error %1 / %2 1/mm.")
                .arg(r.curvature_error_per_mm, 0, 'g', 9)
                .arg(r.curvature_tolerance_per_mm, 0, 'g', 9);
  text += " Bounded native sampling and refinement.";
  if (r.before_body_operation)
    text += " Measured before Body operation.";
  ui.feedback->setText(text);
}
} // namespace blcad::gui
