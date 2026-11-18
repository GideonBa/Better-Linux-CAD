#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QComboBox>
#include <QSignalBlocker>
#include <algorithm>

namespace blcad::gui {
bool ShellWindow::enter_project_part_context(const ProjectDesignOccurrence& occurrence) {
  if (session_.task().active() || active_sketch_) {
    report_error(
        Error::validation("gui.project.context",
                          "Apply or cancel the active command before editing a Part in context"));
    return false;
  }
  const auto filter = model_selection_filter_mask_;
  const auto scope = viewport_->selection_semantic_scope();
  const auto camera = viewport_->camera_bookmark();
  auto entered = session_.begin_project_part_context(occurrence);
  if (!entered) {
    report_error(entered.error());
    return false;
  }
  project_context_filter_ = filter;
  project_context_scope_ = scope;
  project_context_camera_ = camera;
  reset_document_interaction(false);
  reset_selection_filter();
  refresh();
  viewport_->fit_all();
  ribbon_->set_current_tab(tab_model_);
  return true;
}
bool ShellWindow::finish_project_part_context(bool apply) {
  if (!session_.project_part_context_active())
    return false;
  if (apply && active_sketch_ && !finish_active_sketch())
    return false;
  if (apply && session_.task().active()) {
    report_error(Error::validation("gui.project.context",
                                   "Apply or cancel the child command before finishing this Part"));
    return false;
  }
  reset_document_interaction(false);
  if (apply) {
    auto finished = session_.finish_project_part_context("Edit Part in Assembly");
    if (!finished) {
      report_error(finished.error());
      refresh();
      return false;
    }
  } else
    session_.cancel_project_part_context();
  model_selection_filter_mask_ = project_context_filter_.value_or(0xFFFFFFFFU);
  {
    const QSignalBlocker blocker(selection_filter_);
    selection_filter_->setCurrentIndex(
        std::max(0, selection_filter_->findData(model_selection_filter_mask_)));
  }
  session_.selection().set_filter_mask(model_selection_filter_mask_);
  viewport_->set_selection_filter_mask(model_selection_filter_mask_);
  viewport_->set_selection_semantic_scope(project_context_scope_);
  if (project_context_camera_)
    (void)viewport_->restore_camera_bookmark(*project_context_camera_);
  project_context_filter_.reset();
  project_context_scope_.reset();
  project_context_camera_.reset();
  refresh();
  ribbon_->set_current_tab(tab_assembly_);
  return true;
}
} // namespace blcad::gui
