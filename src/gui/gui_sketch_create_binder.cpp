#include "blcad/gui/gui_sketch_create_binder.hpp"

#include "blcad/gui/gui_sketch_create.hpp"
#include "blcad/gui/main_window.hpp"

#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QTextEdit>
#include <QTimer>

#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {
namespace {

struct ToolActionBinding {
  const char* action_name;
  const char* label;
  GuiSketchCreateTool tool;
  const char* hint;
};

constexpr std::array<ToolActionBinding, 21> kToolActions{{
    {"blcad.action.sketch_create_point", "Point", GuiSketchCreateTool::Point,
     "Pick the construction point position"},
    {"blcad.action.sketch_create_line", "Line", GuiSketchCreateTool::Line,
     "Pick the line start and end"},
    {"blcad.action.sketch_create_polyline", "Polyline", GuiSketchCreateTool::Polyline,
     "Pick polyline points; Enter or double-click finishes"},
    {"blcad.action.sketch_create_corner_rectangle", "Corner rectangle",
     GuiSketchCreateTool::CornerRectangle, "Pick two opposite rectangle corners"},
    {"blcad.action.sketch_create_center_rectangle", "Center rectangle",
     GuiSketchCreateTool::CenterRectangle, "Pick the rectangle center and one corner"},
    {"blcad.action.sketch_create_three_point_rectangle", "Three-point rectangle",
     GuiSketchCreateTool::ThreePointRectangle, "Pick the base edge and one height point"},
    {"blcad.action.sketch_create_parallelogram", "Parallelogram",
     GuiSketchCreateTool::Parallelogram, "Pick three parallelogram corners"},
    {"blcad.action.sketch_create_polygon", "Regular polygon",
     GuiSketchCreateTool::RegularPolygon, "Type the side count, then pick center and vertex"},
    {"blcad.action.sketch_create_centerline", "Centerline", GuiSketchCreateTool::Centerline,
     "Pick the centerline start and end"},
    {"blcad.action.sketch_create_center_radius_circle", "Circle: center/radius",
     GuiSketchCreateTool::CenterRadiusCircle, "Pick the circle center and one radius point"},
    {"blcad.action.sketch_create_center_diameter_circle", "Circle: center/diameter",
     GuiSketchCreateTool::CenterDiameterCircle,
     "Pick the circle center and one point on the diameter"},
    {"blcad.action.sketch_create_two_point_circle", "Circle: two-point diameter",
     GuiSketchCreateTool::TwoPointCircle, "Pick the two diameter endpoints"},
    {"blcad.action.sketch_create_three_point_circle", "Circle: three points",
     GuiSketchCreateTool::ThreePointCircle, "Pick three non-collinear points on the circle"},
    {"blcad.action.sketch_create_tangent_circle", "Circle: tangent three-point",
     GuiSketchCreateTool::TangentCircle,
     "Pick three tangent candidates; tangent constraints remain preview-only"},
    {"blcad.action.sketch_create_center_start_end_arc", "Arc: center/start/end",
     GuiSketchCreateTool::CenterStartEndArc, "Pick the arc center, start, and end direction"},
    {"blcad.action.sketch_create_three_point_arc", "Arc: three points",
     GuiSketchCreateTool::ThreePointArc, "Pick the arc start, through-point, and end"},
    {"blcad.action.sketch_create_tangent_arc", "Arc: tangent three-point",
     GuiSketchCreateTool::TangentArc,
     "Pick start, through-point, and end; tangent constraints remain preview-only"},
    {"blcad.action.sketch_create_ellipse", "Ellipse", GuiSketchCreateTool::Ellipse,
     "Pick center, major-axis point, and minor-axis distance"},
    {"blcad.action.sketch_create_elliptical_arc", "Elliptical arc",
     GuiSketchCreateTool::EllipticalArc,
     "Pick center, major axis, minor radius, start, and end"},
    {"blcad.action.sketch_create_center_slot", "Slot: center-to-center",
     GuiSketchCreateTool::CenterSlot, "Pick both cap centers and the slot half-width"},
    {"blcad.action.sketch_create_overall_slot", "Slot: overall length",
     GuiSketchCreateTool::OverallSlot, "Pick overall endpoints and the slot half-width"},
}};

[[nodiscard]] bool collecting_stage(GuiSketchInteractionStage stage) noexcept {
  return stage == GuiSketchInteractionStage::CollectingPicks ||
         stage == GuiSketchInteractionStage::NumericInput ||
         stage == GuiSketchInteractionStage::Preview;
}

[[nodiscard]] bool numeric_character(QChar character) noexcept {
  return character.isDigit() || character == QChar('.') || character == QChar('-') ||
         character == QChar('<') || character == QChar(';');
}

class SketchCreateBinder final : public QObject {
public:
  explicit SketchCreateBinder(MainWindow& window)
      : QObject(&window), window_(window),
        viewport_(window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"))),
        diagnostics_(window.findChild<QTextEdit*>(QStringLiteral("blcad.diagnostics"))) {
    setObjectName(QStringLiteral("blcad.sketch.create_binder"));
    topology_changed_action_ = new QAction(&window);
    topology_changed_action_->setObjectName(
        QStringLiteral("blcad.action.sketch_topology_changed"));
    if (viewport_ != nullptr)
      viewport_->installEventFilter(this);
    window_.installEventFilter(this);

    QMenu* create_menu =
        window_.findChild<QMenu*>(QStringLiteral("blcad.menu.sketch_create"));
    QAction* finish_action =
        window_.findChild<QAction*>(QStringLiteral("blcad.action.finish_sketch"));
    bool inserted_conic_separator = false;
    for (std::size_t index = 0U; index < kToolActions.size(); ++index) {
      const auto& binding = kToolActions[index];
      QAction* action =
          window_.findChild<QAction*>(QString::fromUtf8(binding.action_name));
      if (action == nullptr && create_menu != nullptr) {
        if (!inserted_conic_separator && index >= 9U) {
          create_menu->addSeparator();
          inserted_conic_separator = true;
        }
        action = create_menu->addAction(QString::fromUtf8(binding.label));
        action->setObjectName(QString::fromUtf8(binding.action_name));
        action->setEnabled(finish_action != nullptr && finish_action->isEnabled());
        dynamic_actions_.push_back(action);
      }
      if (action != nullptr)
        connect(action, &QAction::triggered, this,
                [this, binding] { begin_tool(binding.tool, binding.hint, binding.action_name); });
    }
    if (finish_action != nullptr)
      connect(finish_action, &QAction::changed, this, [this, finish_action] {
        for (QAction* action : dynamic_actions_)
          if (action != nullptr)
            action->setEnabled(finish_action->isEnabled());
      });

    for (const auto* cancel_action :
         {"blcad.action.finish_sketch", "blcad.action.edit_sketch", "blcad.action.recompute"})
      if (QAction* action = window_.findChild<QAction*>(QString::fromUtf8(cancel_action)))
        connect(action, &QAction::triggered, this, [this] { cancel_tool(false); });
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched == viewport_ && tool_active()) {
      if (event->type() == QEvent::MouseButtonPress) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton)
          schedule_pick_capture();
      } else if (event->type() == QEvent::MouseButtonDblClick) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton &&
            controller_->tool() == GuiSketchCreateTool::Polyline && controller_->complete_ready()) {
          schedule_completion();
          return true;
        }
      } else if (event->type() == QEvent::MouseMove) {
        schedule_preview_update();
      }
    } else if (watched == &window_ && event->type() == QEvent::KeyPress && tool_active()) {
      auto* key = static_cast<QKeyEvent*>(event);
      if (key->key() == Qt::Key_Escape) {
        if (!numeric_buffer_.empty()) {
          clear_numeric_buffer();
          return true;
        }
        if (!controller_->picks().empty()) {
          (void)controller_->pop_pick();
          update_preview();
          return true;
        }
        cancel_tool(false);
        return false;
      }
      if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        if (!numeric_buffer_.empty()) {
          apply_numeric_buffer();
          return true;
        }
        if (controller_->tool() == GuiSketchCreateTool::Polyline &&
            controller_->complete_ready()) {
          complete_tool();
          return true;
        }
        return true;
      }
      if (key->key() == Qt::Key_Backspace && !numeric_buffer_.empty()) {
        numeric_buffer_.pop_back();
        mirror_numeric_buffer();
        return true;
      }
      const QString text = key->text();
      if (text.size() == 1 && numeric_character(text.front())) {
        if (numeric_buffer_.empty())
          (void)window_.sketch_workspace().begin_numeric_input(window_.session());
        numeric_buffer_ += text.toStdString();
        mirror_numeric_buffer();
        return true;
      }
    }
    return QObject::eventFilter(watched, event);
  }

private:
  [[nodiscard]] bool tool_active() const {
    return controller_.has_value() && window_.sketch_workspace().active() &&
           collecting_stage(window_.sketch_workspace().stage());
  }

  void begin_tool(GuiSketchCreateTool tool, const char* hint, const char* action_name) {
    if (!window_.sketch_workspace().active() || !window_.active_sketch() ||
        window_.session().part_document() == nullptr || window_.session().task().active())
      return;
    auto plane = window_.sketch_workbench().plane_view(window_.session(), *window_.active_sketch());
    if (plane.has_error()) {
      append_diagnostic(plane.error());
      return;
    }
    if (!window_.sketch_workspace().begin_command(window_.session(), action_name, hint)) {
      append_message("Sketch creation command could not enter the workspace lifecycle");
      return;
    }
    auto controller = GuiSketchCreateController::begin(tool, *window_.active_sketch(),
                                                       std::move(plane.value()));
    if (controller.has_error()) {
      append_diagnostic(controller.error());
      (void)window_.sketch_workspace().escape(window_.session());
      return;
    }
    controller_.emplace(std::move(controller.value()));
    numeric_buffer_.clear();
    if (viewport_ != nullptr)
      viewport_->set_sketch_selection_enabled(false);
    window_.refresh_command_state();
  }

  void schedule_pick_capture() {
    QTimer::singleShot(0, this, [this] {
      if (!tool_active() || viewport_ == nullptr)
        return;
      const auto& snap = viewport_->sketch_snap_result();
      if (!snap)
        return;
      auto added = controller_->add_pick({snap->snapped_point, snap->kind});
      if (added.has_error()) {
        append_diagnostic(added.error());
        return;
      }
      if (viewport_ != nullptr)
        viewport_->set_sketch_inference_anchor(snap->snapped_point);
      after_pick();
    });
  }

  void schedule_completion() {
    QTimer::singleShot(0, this, [this] {
      if (tool_active())
        complete_tool();
    });
  }

  void schedule_preview_update() {
    if (preview_scheduled_)
      return;
    preview_scheduled_ = true;
    QTimer::singleShot(0, this, [this] {
      preview_scheduled_ = false;
      update_preview();
    });
  }

  void after_pick() {
    clear_numeric_buffer();
    update_preview();
    if (!controller_->accepts_more_picks() && controller_->complete_ready())
      complete_tool();
  }

  void update_preview() {
    if (!tool_active() || viewport_ == nullptr)
      return;
    std::optional<GuiSketchCreatePick> hover;
    if (const auto& snap = viewport_->sketch_snap_result())
      hover = GuiSketchCreatePick{snap->snapped_point, snap->kind};
    const auto preview = controller_->preview(hover);
    viewport_->set_sketch_preview_polyline(preview.rubber_band, preview.closed);
    if (!preview.constraints.empty())
      window_.sketch_workspace().set_snap_inference(
          "Constraint preview: " + preview.constraints.back().kind);
  }

  void apply_numeric_buffer() {
    if (!tool_active())
      return;
    const std::string text = numeric_buffer_;
    std::optional<Point2> hover;
    if (viewport_ != nullptr)
      if (const auto& snap = viewport_->sketch_snap_result())
        hover = snap->snapped_point;
    auto numeric = controller_->apply_numeric(text, hover);
    if (numeric.has_error()) {
      append_diagnostic(numeric.error());
      return;
    }
    if (!numeric.value().has_value()) {
      std::size_t sides{};
      const auto [parsed, error] =
          std::from_chars(text.data(), text.data() + text.size(), sides);
      if (error == std::errc{} && parsed == text.data() + text.size()) {
        auto updated = controller_->set_polygon_sides(sides);
        if (updated.has_error())
          append_diagnostic(updated.error());
      }
      clear_numeric_buffer();
      return;
    }
    auto added = controller_->add_pick(*numeric.value());
    if (added.has_error()) {
      append_diagnostic(added.error());
      return;
    }
    after_pick();
  }

  void complete_tool() {
    if (!controller_)
      return;
    if (window_.sketch_workspace().stage() == GuiSketchInteractionStage::CollectingPicks)
      (void)window_.sketch_workspace().begin_numeric_input(window_.session());
    if (window_.sketch_workspace().stage() == GuiSketchInteractionStage::NumericInput)
      (void)window_.sketch_workspace().show_preview(window_.session());
    auto committed = controller_->commit(window_.session(), window_.sketch_workbench());
    if (committed.has_error()) {
      append_diagnostic(committed.error());
      cancel_tool(true);
      return;
    }
    (void)window_.sketch_workspace().commit_command(window_.session());
    controller_.reset();
    numeric_buffer_.clear();
    if (viewport_ != nullptr) {
      viewport_->set_sketch_preview_polyline({}, false);
      viewport_->set_sketch_inference_anchor(std::nullopt);
      viewport_->set_sketch_selection_enabled(true);
    }
    if (topology_changed_action_ != nullptr)
      topology_changed_action_->trigger();
    window_.refresh_command_state();
    publish_current_document_scene();
  }

  void cancel_tool(bool escape_workspace) {
    if (!controller_)
      return;
    controller_.reset();
    numeric_buffer_.clear();
    if (viewport_ != nullptr) {
      viewport_->set_sketch_preview_polyline({}, false);
      viewport_->set_sketch_inference_anchor(std::nullopt);
      viewport_->set_sketch_selection_enabled(true);
    }
    if (escape_workspace && collecting_stage(window_.sketch_workspace().stage()))
      while (collecting_stage(window_.sketch_workspace().stage()) &&
             window_.sketch_workspace().escape(window_.session())) {
      }
    window_.refresh_command_state();
  }

  void publish_current_document_scene() {
    if (viewport_ == nullptr || !window_.active_sketch() ||
        window_.session().part_document() == nullptr)
      return;
    const Sketch* sketch =
        window_.session().part_document()->find_sketch(*window_.active_sketch());
    if (sketch == nullptr)
      return;
    auto scene = window_.session().part_shape_cache() != nullptr
                     ? GuiSketchInteractionSceneBuilder{}.build(
                           *window_.session().part_document(), *sketch,
                           *window_.session().part_shape_cache())
                     : GuiSketchInteractionSceneBuilder{}.build(
                           *window_.session().part_document(), *sketch);
    if (scene.has_error()) {
      append_diagnostic(scene.error());
      return;
    }
    auto plane = window_.sketch_workbench().plane_view(window_.session(), sketch->id());
    if (plane.has_error()) {
      append_diagnostic(plane.error());
      return;
    }
    auto published =
        viewport_->set_sketch_interaction(plane.value(), std::move(scene.value()));
    if (published.has_error())
      append_diagnostic(published.error());
  }

  void mirror_numeric_buffer() {
    (void)window_.sketch_workspace().set_numeric_input(numeric_buffer_);
    window_.refresh_command_state();
  }

  void clear_numeric_buffer() {
    numeric_buffer_.clear();
    (void)window_.sketch_workspace().set_numeric_input({});
  }

  void append_diagnostic(const Error& error) const {
    if (diagnostics_ != nullptr)
      diagnostics_->append(
          QStringLiteral("[%1] %2: %3")
              .arg(QString::fromStdString(std::string(to_string(error.category()))),
                   QString::fromStdString(error.object_id()),
                   QString::fromStdString(error.message())));
  }

  void append_message(const std::string& message) const {
    if (diagnostics_ != nullptr)
      diagnostics_->append(QStringLiteral("[Validation] gui.sketch_create: %1")
                               .arg(QString::fromStdString(message)));
  }

  MainWindow& window_;
  OcctViewport* viewport_{nullptr};
  QTextEdit* diagnostics_{nullptr};
  QAction* topology_changed_action_{nullptr};
  std::vector<QAction*> dynamic_actions_;
  std::optional<GuiSketchCreateController> controller_;
  std::string numeric_buffer_;
  bool preview_scheduled_{false};
};

} // namespace

void install_sketch_create_binder(MainWindow& window) {
  if (window.findChild<QObject*>(QStringLiteral("blcad.sketch.create_binder")) != nullptr)
    return;
  (void)new SketchCreateBinder(window);
}

} // namespace blcad::gui
