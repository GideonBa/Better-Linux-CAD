#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QToolButton>
#include <cmath>

namespace blcad::gui {

void ShellFeatureTools::refresh_extrude_frame(const GuiSketchPlaneView& plane, SketchId sketch) {
  extrude_frame_ = {plane.origin, plane.normal, plane.normal, plane.x_axis};
  coordinator_.extrude().set_manipulator_frame(extrude_frame_);
  if (!coordinator_.extrude().uses_open_line())
    return;
  const auto* source = session_.part_document()->find_sketch(sketch);
  const auto* line = source ? interactive_thin_line(*source) : nullptr;
  if (!line)
    return;
  const double dx = line->end().x - line->start().x;
  const double dy = line->end().y - line->start().y;
  const double length = std::hypot(dx, dy);
  const Point2 middle{(line->start().x + line->end().x) / 2, (line->start().y + line->end().y) / 2};
  const Point3 origin{plane.origin.x + middle.x * plane.x_axis.x + middle.y * plane.y_axis.x,
                      plane.origin.y + middle.x * plane.x_axis.y + middle.y * plane.y_axis.y,
                      plane.origin.z + middle.x * plane.x_axis.z + middle.y * plane.y_axis.z};
  const Vector3 left{(-dy * plane.x_axis.x + dx * plane.y_axis.x) / length,
                     (-dy * plane.x_axis.y + dx * plane.y_axis.y) / length,
                     (-dy * plane.x_axis.z + dx * plane.y_axis.z) / length};
  coordinator_.extrude().set_thin_manipulator_frame({origin, left, plane.normal, plane.x_axis});
}

void ShellFeatureTools::refresh_extrude_breadcrumbs() {
  if (!feature_breadcrumbs_)
    return;
  const bool extrude = command_ == Command::Extrude;
  feature_breadcrumbs_->setVisible(extrude);
  numeric_editor_->setEnabled(!editing_source_sketch_ && !awaiting_extrude_source());
  const bool inputs = !editing_source_sketch_ && !awaiting_extrude_source();
  extrude_behavior_group_->setEnabled(inputs);
  extrude_advanced_group_->setEnabled(inputs);
  extrude_first_selector_->setEnabled(inputs);
  extrude_second_selector_->setEnabled(inputs);
  extrude_pick_role_->setEnabled(inputs && !extrude_faces_.empty());
  extrude_profile_selector_->setEnabled(!editing_source_sketch_ &&
                                        layer_.active_handle_id().empty());
  feature_context_->setText(QString::fromStdString(std::string(session_.display_name())) +
                            QStringLiteral(" · Draft"));
  feature_context_->setToolTip(feature_context_->text());
  const auto* part = session_.part_document();
  const auto* sketch =
      part && extrude_source_ ? part->find_sketch(extrude_source_->sketch) : nullptr;
  if (extrude && sketch)
    extrude_profile_summary_->setText(QStringLiteral("%1 · %2").arg(
        QString::fromStdString(sketch->name()), coordinator_.extrude().uses_open_line()
                                                    ? QStringLiteral("Open thin profile")
                                                    : QStringLiteral("1 profile")));
  else if (extrude)
    extrude_profile_summary_->setText(QStringLiteral("Select a profile or single open line."));
  const auto source_name = sketch ? QString::fromStdString(sketch->name())
                           : awaiting_extrude_source()
                               ? QStringLiteral("Select Profile")
                               : QStringLiteral("Source Sketch unavailable");
  source_breadcrumb_->setText(
      source_breadcrumb_->fontMetrics().elidedText(source_name, Qt::ElideMiddle, 180));
  source_breadcrumb_->setAccessibleName(source_name);
  source_breadcrumb_->setToolTip(
      QStringLiteral("Edit this source Sketch inside the Extrude draft"));
  source_breadcrumb_->setEnabled(extrude && sketch && !awaiting_extrude_source() &&
                                 !editing_source_sketch_ && layer_.active_handle_id().empty());
  feature_breadcrumb_->setEnabled(extrude && editing_source_sketch_);
  feature_breadcrumb_->setToolTip(QStringLiteral("Finish Sketch and return to Extrude"));
}

Result<std::size_t> ShellFeatureTools::edit_extrude_source_sketch() {
  if (command_ != Command::Extrude || editing_source_sketch_ || !extrude_source_ ||
      !session_.part_workflow_active() || !layer_.active_handle_id().empty() ||
      session_.task().command_id() != "part.extrude")
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.feature", "Finish the active input before editing the source Sketch."));
  extrude_pick_role_->setCurrentIndex(0);
  update_extrude_pick_mode();
  (void)session_.task().cancel();
  editing_source_sketch_ = true;
  task_summary_->setText(
      QStringLiteral("Editing source Sketch; Finish Sketch to return to Extrude."));
  preview_valid_ = false;
  viewport_.clear_feature_preview();
  (void)layer_.set_handles({});
  synchronize_overlay();
  refresh_enablement();
  if (!window_.enter_feature_source_sketch(extrude_source_->sketch)) {
    editing_source_sketch_ = false;
    (void)session_.task().begin("part.extrude");
    (void)session_.task().begin_parameter_editing();
    (void)session_.task().show_preview();
    coordinator_.refresh_handles();
    (void)preview_and_publish();
    return Result<std::size_t>::failure(
        Error::validation(extrude_source_->sketch.value(), "The source Sketch cannot be entered."));
  }
  return Result<std::size_t>::success(1U);
}

void ShellFeatureTools::resume_extrude_source_sketch(SketchId finished) {
  if (!editing_source_sketch_ || !extrude_source_ || finished != extrude_source_->sketch)
    return;
  editing_source_sketch_ = false;
  (void)session_.task().begin("part.extrude");
  (void)session_.task().begin_parameter_editing();
  (void)session_.task().show_preview();
  const auto source = coordinator_.extrude().refresh_source(session_);
  if (source) {
    auto plane = sketch_workbench_.plane_view(session_, finished);
    if (plane)
      refresh_extrude_frame(plane.value(), finished);
    else
      report(plane.error());
  }
  refresh_extrude_profile_choices();
  refresh_extrude_reference_choices(true);
  update_extrude_controls();
  coordinator_.refresh_handles();
  const auto preview = preview_and_publish();
  if (!source) {
    extrude_geometry_group_->set_diagnostic(QString::fromStdString(source.error().message()));
    report(source.error());
  } else if (!preview)
    report(preview.error());
  window_.refresh();
}

void ShellFeatureTools::refresh_extrude_profile_choices() {
  const QSignalBlocker guard(extrude_profile_);
  extrude_profile_->clear();
  std::string current_sketch;
  int ordinal = 0;
  for (const auto& source : extrude_profile_choices()) {
    const auto* sketch = session_.part_document()->find_sketch(source.sketch);
    if (current_sketch != source.sketch.value()) {
      current_sketch = source.sketch.value();
      ordinal = 0;
    }
    const auto label =
        source.profile ? QStringLiteral("Profile %1").arg(++ordinal) : QStringLiteral("Open Line");
    extrude_profile_->addItem(
        QString::fromStdString(sketch->name()) + QStringLiteral(" · ") + label,
        QStringList{QString::fromStdString(source.sketch.value()),
                    source.profile ? QString::fromStdString(source.profile->value()) : QString{}});
  }
  const QStringList key =
      extrude_source_ ? QStringList{QString::fromStdString(extrude_source_->sketch.value()),
                                    extrude_source_->profile
                                        ? QString::fromStdString(extrude_source_->profile->value())
                                        : QString{}}
                      : QStringList{};
  extrude_profile_->setCurrentIndex(key.isEmpty() ? -1 : extrude_profile_->findData(key));
  extrude_profile_selector_->set_active(extrude_profile_picking_);
}

void ShellFeatureTools::change_extrude_profile() {
  if (command_ != Command::Extrude || editing_source_sketch_ || synchronizing_extrude_source_)
    return;
  if (!layer_.active_handle_id().empty()) {
    refresh_extrude_profile_choices();
    return;
  }
  const QScopedValueRollback guard(synchronizing_extrude_source_, true);
  clear_extrude_diagnostics();
  if (extrude_profile_->currentIndex() < 0) {
    coordinator_.cancel();
    extrude_source_.reset();
    preview_valid_ = false;
    viewport_.clear_feature_preview();
    if (session_.task().stage() == GuiTaskStage::Preview)
      (void)session_.task().return_to_editing();
    if (session_.task().stage() == GuiTaskStage::EditingParameters)
      (void)session_.task().return_to_selection();
    task_summary_->setText(QStringLiteral("Select a profile or single open line."));
    set_extrude_profile_picking(true);
  } else {
    const auto key = extrude_profile_->currentData().toStringList();
    if (key.size() != 2)
      return;
    const auto result = activate_extrude_source(
        {SketchId(key[0].toStdString()),
         key[1].isEmpty() ? std::nullopt : std::optional(ProfileId(key[1].toStdString()))});
    if (!result) {
      task_summary_->setText(QString::fromStdString(result.error().message()));
      extrude_profile_selector_->set_diagnostic(QString::fromStdString(result.error().message()));
    }
  }
  refresh_enablement();
  synchronize_overlay();
}

void ShellFeatureTools::set_extrude_profile_picking(bool active) {
  if (command_ != Command::Extrude || editing_source_sketch_)
    return;
  if (active) {
    const QSignalBlocker guard(extrude_pick_role_);
    extrude_pick_role_->setCurrentIndex(0);
  }
  extrude_profile_picking_ = active || awaiting_extrude_source();
  extrude_profile_selector_->set_active(extrude_profile_picking_);
  update_extrude_pick_mode();
  synchronize_overlay();
}

void ShellFeatureTools::apply_extrude_settings(const ExtrudeSettings& settings) {
  const QSignalBlocker b1(extrude_extent_), b2(extrude_operation_), b3(extrude_direction_),
      b4(extrude_thin_mode_), b5(extrude_target_), b6(extrude_path_), b7(extrude_first_face_),
      b8(extrude_second_face_);
  extrude_extent_->setCurrentIndex(settings.extent);
  extrude_operation_->setCurrentIndex(settings.operation);
  extrude_direction_->setCurrentIndex(settings.direction);
  extrude_thin_mode_->setCurrentIndex(settings.thin_mode);
  numeric_editor_->setText(settings.depth);
  extrude_second_distance_->setText(settings.second_depth);
  extrude_taper_->setText(settings.taper);
  extrude_thickness_->setText(settings.thickness);
  extrude_second_thickness_->setText(settings.second_thickness);
  for (auto* reference :
       {extrude_target_, extrude_path_, extrude_first_face_, extrude_second_face_})
    reference->setCurrentIndex(-1);
  update_extrude_controls();
}

Result<std::size_t> ShellFeatureTools::apply_create_another() {
  if (command_ != Command::Extrude || editing_source_sketch_ || awaiting_extrude_source())
    return Result<std::size_t>::failure(
        Error::validation("extrude", "Complete the current Extrude first."));
  if (numeric_editor_->isVisible()) {
    const auto numeric = set_numeric_text(numeric_editor_->text());
    if (!numeric)
      return Result<std::size_t>::failure(numeric.error());
  }
  const ExtrudeSettings settings{extrude_extent_->currentIndex(),
                                 extrude_operation_->currentIndex(),
                                 extrude_direction_->currentIndex(),
                                 extrude_thin_mode_->currentIndex(),
                                 numeric_editor_->text(),
                                 extrude_second_distance_->text(),
                                 extrude_taper_->text(),
                                 extrude_thickness_->text(),
                                 extrude_second_thickness_->text()};
  const auto accepted = apply();
  if (!accepted)
    return accepted;
  selected_profile_.reset();
  selected_open_line_.reset();
  viewport_.clear_selection();
  session_.selection().clear();
  const auto next = begin_extrude_workflow(settings);
  if (!next)
    report(next.error()); // The preceding Apply already succeeded; never report it as uncommitted.
  return accepted;
}

} // namespace blcad::gui
