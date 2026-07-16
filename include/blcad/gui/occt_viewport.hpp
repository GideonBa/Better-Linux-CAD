#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"
#include "blcad/gui/gui_types.hpp"

#include <QPoint>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class QMouseEvent;
class QPaintEngine;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;

namespace blcad::gui {

enum class GuiViewportDisplayMode { Shaded, ShadedWithEdges, Wireframe };
enum class GuiViewportProjection { Perspective, Orthographic };
enum class GuiStandardView { Isometric, Front, Back, Left, Right, Top, Bottom };
enum class GuiSketchSurroundingsMode { Dim, Isolate };
enum class GuiSketchPointerPhase { Press, Release };

struct GuiPlaneCamera {
  Point3 target;
  Vector3 view_direction;
  Vector3 up_direction;
  friend bool operator==(const GuiPlaneCamera&, const GuiPlaneCamera&) = default;
};

struct GuiViewportCameraBookmark {
  Point3 eye{1.0, 1.0, 1.0};
  Point3 target{0.0, 0.0, 0.0};
  Vector3 up_direction{0.0, 0.0, 1.0};
  double scale{1.0};
  GuiViewportProjection projection{GuiViewportProjection::Perspective};
  std::optional<GuiPlaneCamera> plane_camera;
};

class OcctViewport final : public QWidget {
public:
  using SketchPointerCallback =
      std::function<void(Point2, const GuiSketchSnapResult&, const std::optional<GuiSketchHit>&)>;
  using SketchDragPointerCallback =
      std::function<void(GuiSketchScreenPoint, Point2, const GuiSketchSnapResult&,
                         const std::optional<GuiSketchHit>&)>;
  using SketchPointerPhaseCallback =
      std::function<void(GuiSketchPointerPhase, GuiSketchScreenPoint, Point2,
                         const GuiSketchSnapResult&, const std::optional<GuiSketchHit>&)>;
  using SketchSelectionCallback = std::function<void(const std::vector<GuiSelection>&)>;

  explicit OcctViewport(QWidget* parent = nullptr);
  ~OcctViewport() override;

  [[nodiscard]] Result<std::size_t> set_scene(std::vector<geometry::ViewportSceneItem> items);
  void clear_scene();
  void set_display_mode(GuiViewportDisplayMode mode);
  void set_projection(GuiViewportProjection projection);
  void set_standard_view(GuiStandardView view);
  [[nodiscard]] bool set_plane_camera(Point3 target, Vector3 normal, Vector3 up);
  [[nodiscard]] GuiViewportCameraBookmark camera_bookmark() const noexcept;
  [[nodiscard]] bool restore_camera_bookmark(const GuiViewportCameraBookmark& bookmark) noexcept;
  void set_sketch_focus(std::string sketch_id,
                        GuiSketchSurroundingsMode mode = GuiSketchSurroundingsMode::Dim);
  void clear_sketch_focus();
  [[nodiscard]] Result<std::size_t>
  set_sketch_interaction(GuiSketchPlaneView plane, GuiSketchInteractionScene scene,
                         GuiSketchInteractionConfig config = {});
  void clear_sketch_interaction();
  void set_sketch_selection_enabled(bool enabled) noexcept;
  void set_sketch_inference_anchor(std::optional<Point2> anchor) noexcept;
  void set_sketch_grid_config(GuiSketchGridConfig config);
  void set_sketch_drag_handles(std::vector<Point2> handles);
  void set_sketch_preview_polyline(std::vector<Point2> polyline, bool closed);
  void set_sketch_pointer_callback(SketchPointerCallback callback);
  void set_sketch_drag_pointer_callback(SketchDragPointerCallback callback);
  void set_sketch_pointer_phase_callback(SketchPointerPhaseCallback callback);
  void set_sketch_selection_callback(SketchSelectionCallback callback);
  void set_context_menu_callback(std::function<void(QPoint)> callback);
  void fit_all();

  void set_selection_filter_mask(std::uint32_t mask);
  void set_selection_callback(std::function<void(std::optional<GuiSelection>)> callback);
  [[nodiscard]] bool select_semantic(std::string_view semantic_id);
  void clear_selection();

  [[nodiscard]] std::size_t presentation_count() const noexcept;
  [[nodiscard]] GuiViewportDisplayMode display_mode() const noexcept;
  [[nodiscard]] GuiViewportProjection projection() const noexcept;
  [[nodiscard]] std::uint32_t selection_filter_mask() const noexcept;
  [[nodiscard]] const std::optional<GuiSelection>& selected_semantic() const noexcept;
  [[nodiscard]] const std::optional<GuiPlaneCamera>& plane_camera() const noexcept;
  [[nodiscard]] bool sketch_focus_active() const noexcept;
  [[nodiscard]] std::string_view sketch_focus_id() const noexcept;
  [[nodiscard]] GuiSketchSurroundingsMode sketch_surroundings_mode() const noexcept;
  [[nodiscard]] bool sketch_interaction_active() const noexcept;
  [[nodiscard]] bool sketch_selection_enabled() const noexcept;
  [[nodiscard]] const std::vector<GuiSelection>& sketch_selections() const noexcept;
  [[nodiscard]] const std::optional<GuiSketchSnapResult>& sketch_snap_result() const noexcept;
  [[nodiscard]] const std::optional<GuiSketchHit>& hovered_sketch_hit() const noexcept;
  [[nodiscard]] const std::optional<GuiSketchScreenRect>& sketch_box_selection() const noexcept;
  [[nodiscard]] std::size_t sketch_grid_line_count() const noexcept;
  [[nodiscard]] std::size_t sketch_drag_handle_count() const noexcept;
  [[nodiscard]] std::size_t sketch_preview_point_count() const noexcept {
    return sketch_preview_polyline_.size();
  }
  [[nodiscard]] Result<GuiSketchScreenPoint> sketch_plane_to_screen(Point2 point) const {
    if (!sketch_interaction_)
      return Result<GuiSketchScreenPoint>::failure(
          Error::validation("gui.occt_viewport", "Sketch interaction is not active"));
    return sketch_interaction_->mapping().plane_to_screen(point);
  }
  [[nodiscard]] bool native_viewer_available() const noexcept;
  [[nodiscard]] const std::string& initialization_error() const noexcept;

protected:
  [[nodiscard]] QPaintEngine* paintEngine() const override;
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  struct Impl;

  void initialize_native_viewer();
  void apply_display_mode();
  void apply_selection_filters();
  void apply_sketch_focus();
  void rebuild_sketch_grid();
  void rebuild_sketch_drag_handles();
  void rebuild_sketch_preview_polyline();
  void update_sketch_pointer(GuiSketchScreenPoint screen_point);
  void publish_sketch_pointer_phase(GuiSketchPointerPhase phase,
                                    GuiSketchScreenPoint screen_point);
  void publish_sketch_selection();
  void publish_selection(std::optional<GuiSelection> selection);
  [[nodiscard]] std::optional<GuiSelection> detected_selection() const;

  std::unique_ptr<Impl> impl_;
  std::unique_ptr<GuiSketchInteractionController> sketch_interaction_;
  std::function<void(std::optional<GuiSelection>)> selection_callback_;
  SketchPointerCallback sketch_pointer_callback_;
  SketchDragPointerCallback sketch_drag_pointer_callback_;
  SketchPointerPhaseCallback sketch_pointer_phase_callback_;
  SketchSelectionCallback sketch_selection_callback_;
  std::function<void(QPoint)> context_menu_callback_;
  std::optional<GuiSelection> selected_semantic_;
  std::vector<GuiSelection> sketch_selections_;
  std::optional<GuiPlaneCamera> plane_camera_;
  std::optional<Point2> sketch_inference_anchor_;
  std::optional<GuiSketchSnapResult> sketch_snap_result_;
  std::optional<GuiSketchHit> hovered_sketch_hit_;
  std::optional<GuiSketchScreenRect> sketch_box_selection_;
  std::vector<Point2> sketch_drag_handles_;
  std::vector<Point2> sketch_preview_polyline_;
  bool sketch_preview_closed_{false};
  GuiViewportDisplayMode display_mode_{GuiViewportDisplayMode::ShadedWithEdges};
  GuiViewportProjection projection_{GuiViewportProjection::Perspective};
  GuiSketchSurroundingsMode sketch_surroundings_mode_{GuiSketchSurroundingsMode::Dim};
  std::uint32_t selection_filter_mask_{0xFFFFFFFFU};
  std::string sketch_focus_id_;
  QWidget* sketch_overlay_{nullptr};
  QPoint last_mouse_position_;
  QPoint right_press_position_;
  QPoint sketch_press_position_;
  bool panning_{false};
  bool rotating_{false};
  bool sketch_box_active_{false};
  bool sketch_selection_enabled_{true};
};

} // namespace blcad::gui
