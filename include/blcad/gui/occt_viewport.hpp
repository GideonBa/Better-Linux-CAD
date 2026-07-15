#pragma once

#include "blcad/core/result.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_types.hpp"
#include "blcad/core/spatial.hpp"

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
  void publish_selection(std::optional<GuiSelection> selection);
  [[nodiscard]] std::optional<GuiSelection> detected_selection() const;

  std::unique_ptr<Impl> impl_;
  std::function<void(std::optional<GuiSelection>)> selection_callback_;
  std::function<void(QPoint)> context_menu_callback_;
  std::optional<GuiSelection> selected_semantic_;
  std::optional<GuiPlaneCamera> plane_camera_;
  GuiViewportDisplayMode display_mode_{GuiViewportDisplayMode::ShadedWithEdges};
  GuiViewportProjection projection_{GuiViewportProjection::Perspective};
  GuiSketchSurroundingsMode sketch_surroundings_mode_{GuiSketchSurroundingsMode::Dim};
  std::uint32_t selection_filter_mask_{0xFFFFFFFFU};
  std::string sketch_focus_id_;
  QPoint last_mouse_position_;
  QPoint right_press_position_;
  bool panning_{false};
  bool rotating_{false};
};

} // namespace blcad::gui
