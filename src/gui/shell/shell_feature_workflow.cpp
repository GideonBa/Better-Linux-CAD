#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
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
  numeric_editor_->setEnabled(!editing_source_sketch_);
  feature_context_->setText(QString::fromStdString(std::string(session_.display_name())) +
                            QStringLiteral(" · Draft"));
  const auto* part = session_.part_document();
  const auto* sketch =
      part && extrude_source_ ? part->find_sketch(extrude_source_->sketch) : nullptr;
  if (extrude && sketch)
    extrude_profile_summary_->setText(QStringLiteral("%1 · %2").arg(
        QString::fromStdString(sketch->name()), coordinator_.extrude().uses_open_line()
                                                    ? QStringLiteral("Open thin profile")
                                                    : QStringLiteral("1 profile")));
  const auto source_name =
      sketch ? QString::fromStdString(sketch->name()) : QStringLiteral("Source Sketch unavailable");
  source_breadcrumb_->setText(
      source_breadcrumb_->fontMetrics().elidedText(source_name, Qt::ElideMiddle, 180));
  source_breadcrumb_->setAccessibleName(source_name);
  source_breadcrumb_->setToolTip(
      QStringLiteral("Edit this source Sketch inside the Extrude draft"));
  source_breadcrumb_->setEnabled(extrude && sketch && !editing_source_sketch_ &&
                                 layer_.active_handle_id().empty());
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

} // namespace blcad::gui
