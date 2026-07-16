#include "blcad/gui/gui_sketch_drag_binder.hpp"

#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/main_window.hpp"

#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {
namespace {

constexpr double kHandleToleranceDip = 9.0;

[[nodiscard]] bool drag_stage(GuiSketchInteractionStage stage) noexcept {
  return stage == GuiSketchInteractionStage::SelectedHandle ||
         stage == GuiSketchInteractionStage::DragCandidate;
}

[[nodiscard]] std::string solve_status_text(SketchSolveStatus status) {
  switch (status) {
  case SketchSolveStatus::FullyConstrained: return "Fully constrained";
  case SketchSolveStatus::UnderConstrained: return "Under constrained";
  case SketchSolveStatus::Redundant: return "Redundant";
  case SketchSolveStatus::Conflicting: return "Conflicting";
  case SketchSolveStatus::NonConvergent: return "Non-convergent";
  case SketchSolveStatus::InvalidReference: return "Invalid reference";
  }
  return "Not evaluated";
}

[[nodiscard]] double screen_distance(GuiSketchScreenPoint first,
                                     GuiSketchScreenPoint second) noexcept {
  return std::hypot(second.x - first.x, second.y - first.y);
}

class SketchDragBinder final : public QObject {
public:
  explicit SketchDragBinder(MainWindow& window)
      : QObject(&window), window_(window),
        viewport_(window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"))),
        diagnostics_(window.findChild<QTextEdit*>(QStringLiteral("blcad.diagnostics"))),
        dof_status_(window.findChild<QLabel*>(QStringLiteral("blcad.sketch.dof_status"))),
        solve_status_(window.findChild<QLabel*>(QStringLiteral("blcad.sketch.solve_status"))) {
    setObjectName(QStringLiteral("blcad.sketch.drag_binder"));
    if (viewport_ != nullptr) {
      viewport_->installEventFilter(this);
      bind_viewport();
    }
    window_.installEventFilter(this);
    bind_shell_actions();
    defer_sync();
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched == &window_ && event->type() == QEvent::KeyPress) {
      const auto* key = static_cast<QKeyEvent*>(event);
      if (key->key() == Qt::Key_Escape && controller_ && controller_->active() &&
          drag_stage(window_.sketch_workspace().stage())) {
        restore_source_preview();
        controller_->cancel();
        if (viewport_ != nullptr) {
          viewport_->set_sketch_inference_anchor(std::nullopt);
          viewport_->set_sketch_selection_enabled(true);
        }
        publish_baseline_feedback();
      }
    } else if (watched == viewport_ &&
               (event->type() == QEvent::UngrabMouse ||
                event->type() == QEvent::WindowDeactivate) &&
               controller_ && controller_->active() &&
               drag_stage(window_.sketch_workspace().stage())) {
      cancel_drag(true, "Sketch drag cancelled because pointer capture was lost");
    }
    return QObject::eventFilter(watched, event);
  }

private:
  void bind_viewport() {
    viewport_->set_sketch_drag_pointer_callback(
        [this](GuiSketchScreenPoint screen, Point2 raw, const GuiSketchSnapResult& snap,
               const std::optional<GuiSketchHit>& hit) {
          (void)screen;
          (void)raw;
          (void)hit;
          if (!controller_ || !controller_->active() ||
              !drag_stage(window_.sketch_workspace().stage()))
            return;
          if (window_.sketch_workspace().stage() == GuiSketchInteractionStage::SelectedHandle) {
            if (!window_.sketch_workspace().show_drag_candidate(window_.session())) {
              cancel_drag(true, "Sketch drag could not enter live preview");
              return;
            }
            window_.refresh_command_state();
          }
          auto queued = controller_->queue_pointer(snap.snapped_point);
          if (queued.has_error()) {
            cancel_drag(true, queued.error().message());
            return;
          }
          schedule_solve();
        });
    viewport_->set_sketch_pointer_phase_callback(
        [this](GuiSketchPointerPhase phase, GuiSketchScreenPoint screen, Point2 raw,
               const GuiSketchSnapResult& snap, const std::optional<GuiSketchHit>& hit) {
          (void)raw;
          (void)hit;
          if (phase == GuiSketchPointerPhase::Press)
            begin_drag(screen);
          else
            release_drag(snap.snapped_point);
        });
  }

  void bind_shell_actions() {
    const auto defer_for = [this](QStringView object_name) {
      if (QAction* action = window_.findChild<QAction*>(object_name.toString()))
        connect(action, &QAction::triggered, this, [this] { defer_sync(); });
    };
    defer_for(u"blcad.action.edit_sketch");
    defer_for(u"blcad.action.finish_sketch");
    defer_for(u"blcad.action.repair_sketch");
    defer_for(u"blcad.action.recompute");
    defer_for(u"blcad.action.sketch_topology_changed");
  }

  void defer_sync() {
    QTimer::singleShot(0, this, [this] { sync_controller(); });
  }

  [[nodiscard]] GuiSketchInteractionConfig interaction_config() const {
    GuiSketchInteractionConfig config;
    if (const QAction* grid = window_.findChild<QAction*>(QStringLiteral("blcad.action.sketch_grid")))
      config.grid.visible = grid->isChecked();
    if (const QAction* snap =
            window_.findChild<QAction*>(QStringLiteral("blcad.action.sketch_grid_snap")))
      config.grid.snap_enabled = snap->isChecked();
    return config;
  }

  void sync_controller() {
    if (!window_.sketch_workspace().active() || !window_.active_sketch() ||
        window_.session().part_document() == nullptr) {
      controller_.reset();
      if (viewport_ != nullptr)
        viewport_->set_sketch_drag_handles({});
      return;
    }
    if (controller_ && controller_->active())
      return;
    auto controller = GuiSketchDragController::create(*window_.session().part_document(),
                                                       *window_.active_sketch());
    if (controller.has_error()) {
      append_diagnostic(controller.error());
      controller_.reset();
      if (viewport_ != nullptr)
        viewport_->set_sketch_drag_handles({});
      return;
    }
    controller_.emplace(std::move(controller.value()));
    publish_baseline_feedback();
    publish_handles(controller_->handles());
  }

  void begin_drag(GuiSketchScreenPoint screen) {
    if (!controller_ || controller_->active() || !window_.sketch_workspace().active() ||
        window_.session().task().active())
      return;
    const auto handle = handle_at(screen);
    if (!handle)
      return;
    auto begun = controller_->begin(handle->id);
    if (begun.has_error()) {
      append_diagnostic(begun.error());
      publish_baseline_feedback();
      return;
    }
    if (!window_.sketch_workspace().select_handle(window_.session(), handle->id)) {
      controller_->cancel();
      append_message("selected Sketch handle could not enter the workspace drag lifecycle");
      return;
    }
    viewport_->set_sketch_selection_enabled(false);
    viewport_->set_sketch_inference_anchor(handle->position);
    window_.refresh_command_state();
  }

  void release_drag(Point2 final_pointer) {
    if (!controller_ || !controller_->active() || !drag_stage(window_.sketch_workspace().stage()))
      return;
    if (window_.sketch_workspace().stage() == GuiSketchInteractionStage::SelectedHandle &&
        !controller_->has_pending() && !controller_->processed_pointer()) {
      restore_source_preview();
      controller_->cancel();
      (void)window_.sketch_workspace().escape(window_.session());
      viewport_->set_sketch_inference_anchor(std::nullopt);
      viewport_->set_sketch_selection_enabled(true);
      publish_baseline_feedback();
      window_.refresh_command_state();
      return;
    }

    if (window_.sketch_workspace().stage() == GuiSketchInteractionStage::SelectedHandle &&
        !window_.sketch_workspace().show_drag_candidate(window_.session())) {
      cancel_drag(true, "Sketch drag could not enter release preview");
      return;
    }
    auto preview = controller_->flush(final_pointer);
    solve_scheduled_ = false;
    if (preview.has_error()) {
      cancel_drag(true, preview.error().message());
      return;
    }
    publish_preview(preview.value());

    auto committed = controller_->commit(window_.session());
    if (committed.has_error()) {
      cancel_drag(true, committed.error().message());
      return;
    }
    if (!window_.sketch_workspace().commit_drag(window_.session())) {
      append_message("Sketch drag document committed but workspace lifecycle could not close");
      return;
    }
    viewport_->set_sketch_inference_anchor(std::nullopt);
    viewport_->set_sketch_selection_enabled(true);
    window_.refresh_command_state();
    sync_controller();
    publish_current_document_scene();
  }

  [[nodiscard]] std::optional<GuiSketchDragHandle>
  handle_at(GuiSketchScreenPoint pointer) const {
    if (!controller_ || viewport_ == nullptr)
      return std::nullopt;
    const auto handles = controller_->latest_preview()
                             ? controller_->handles_for_topology(
                                   controller_->latest_preview()->topology())
                             : controller_->handles();
    struct Candidate {
      GuiSketchDragHandle handle;
      double distance{0.0};
    };
    std::vector<Candidate> candidates;
    for (const auto& handle : handles) {
      auto screen = viewport_->sketch_plane_to_screen(handle.position);
      if (screen.has_error())
        continue;
      const double distance = screen_distance(pointer, screen.value());
      if (distance <= kHandleToleranceDip)
        candidates.push_back({handle, distance});
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
      if (std::abs(left.distance - right.distance) > 1.0e-9)
        return left.distance < right.distance;
      return left.handle.id < right.handle.id;
    });
    return candidates.empty() ? std::nullopt
                              : std::optional<GuiSketchDragHandle>{candidates.front().handle};
  }

  void schedule_solve() {
    if (solve_scheduled_)
      return;
    solve_scheduled_ = true;
    QTimer::singleShot(0, this, [this] {
      solve_scheduled_ = false;
      if (!controller_ || !controller_->active() || !controller_->has_pending())
        return;
      auto preview = controller_->process_pending();
      if (preview.has_error()) {
        cancel_drag(true, preview.error().message());
        return;
      }
      publish_preview(preview.value());
    });
  }

  void publish_preview(const GuiSketchDragPreview& preview) {
    publish_scene(preview.preview_sketch());
    publish_handles(controller_->handles_for_topology(preview.topology()));
    const auto& solve = preview.solve();
    window_.sketch_workspace().set_solve_feedback(solve.remaining_dof,
                                                   solve_status_text(solve.status));
    refresh_status_labels();
  }

  void publish_baseline_feedback() {
    if (!controller_)
      return;
    const auto& solve = controller_->baseline_solve();
    window_.sketch_workspace().set_solve_feedback(solve.remaining_dof,
                                                   solve_status_text(solve.status));
    refresh_status_labels();
  }

  void publish_handles(const std::vector<GuiSketchDragHandle>& handles) {
    if (viewport_ == nullptr)
      return;
    std::vector<Point2> positions;
    positions.reserve(handles.size());
    for (const auto& handle : handles)
      positions.push_back(handle.position);
    viewport_->set_sketch_drag_handles(std::move(positions));
  }

  void publish_scene(const Sketch& sketch) {
    const PartDocument* part = window_.session().part_document();
    if (viewport_ == nullptr || part == nullptr)
      return;
    auto scene = window_.session().part_shape_cache() != nullptr
                     ? GuiSketchInteractionSceneBuilder{}.build(
                           *part, sketch, *window_.session().part_shape_cache())
                     : GuiSketchInteractionSceneBuilder{}.build(*part, sketch);
    if (scene.has_error()) {
      append_diagnostic(scene.error());
      return;
    }
    const auto plane = window_.sketch_workbench().plane_view(window_.session(), sketch.id());
    if (plane.has_error()) {
      append_diagnostic(plane.error());
      return;
    }
    auto published = viewport_->set_sketch_interaction(plane.value(), std::move(scene.value()),
                                                       interaction_config());
    if (published.has_error())
      append_diagnostic(published.error());
  }

  void publish_current_document_scene() {
    if (!window_.active_sketch() || window_.session().part_document() == nullptr)
      return;
    const Sketch* sketch = window_.session().part_document()->find_sketch(*window_.active_sketch());
    if (sketch != nullptr)
      publish_scene(*sketch);
    if (controller_)
      publish_handles(controller_->handles());
  }

  void restore_source_preview() {
    if (!controller_)
      return;
    publish_scene(controller_->source_sketch());
    publish_handles(controller_->handles());
  }

  void cancel_drag(bool cancel_workspace, std::string message) {
    if (!message.empty())
      append_message(message);
    restore_source_preview();
    if (controller_)
      controller_->cancel();
    if (cancel_workspace && drag_stage(window_.sketch_workspace().stage()))
      (void)window_.sketch_workspace().escape(window_.session());
    if (viewport_ != nullptr) {
      viewport_->set_sketch_inference_anchor(std::nullopt);
      viewport_->set_sketch_selection_enabled(true);
    }
    publish_baseline_feedback();
    window_.refresh_command_state();
  }

  void refresh_status_labels() const {
    const auto& status = window_.sketch_workspace().status();
    if (dof_status_ != nullptr)
      dof_status_->setText(status.remaining_dof
                               ? QStringLiteral("DOF: %1").arg(*status.remaining_dof)
                               : QStringLiteral("DOF: —"));
    if (solve_status_ != nullptr)
      solve_status_->setText(
          QStringLiteral("Solve: %1").arg(QString::fromStdString(status.solve_status)));
  }

  void append_diagnostic(const Error& error) const {
    if (diagnostics_ != nullptr)
      diagnostics_->append(QStringLiteral("[%1] %2: %3")
                               .arg(QString::fromStdString(std::string(to_string(error.category()))),
                                    QString::fromStdString(error.object_id()),
                                    QString::fromStdString(error.message())));
  }

  void append_message(const std::string& message) const {
    if (diagnostics_ != nullptr)
      diagnostics_->append(QStringLiteral("[Validation] gui.sketch_drag: %1")
                               .arg(QString::fromStdString(message)));
  }

  MainWindow& window_;
  OcctViewport* viewport_{nullptr};
  QTextEdit* diagnostics_{nullptr};
  QLabel* dof_status_{nullptr};
  QLabel* solve_status_{nullptr};
  std::optional<GuiSketchDragController> controller_;
  bool solve_scheduled_{false};
};

} // namespace

void install_sketch_drag_binder(MainWindow& window) {
  if (window.findChild<QObject*>(QStringLiteral("blcad.sketch.drag_binder")) != nullptr)
    return;
  (void)new SketchDragBinder(window);
}

} // namespace blcad::gui
