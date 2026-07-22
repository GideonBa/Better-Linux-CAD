#include "blcad/gui/occt_viewport.hpp"

#include "blcad/geometry/occt_shape_view.hpp"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_NeutralWindow.hxx>
#include <Graphic3d_Camera.hxx>
#include <OpenGl_Context.hxx>
#include <OpenGl_FrameBuffer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Quantity_Color.hxx>
#include <Standard_Failure.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <gp_Dir.hxx>

#include <QApplication>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QtGui/qguiapplication_platform.h>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSurfaceFormat>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <xcb/xcb.h> // X server round-trip before OCCT window queries; last to limit macro pollution
#include <cstdlib>

namespace blcad::gui {
namespace {

class SketchInteractionOverlay final : public QWidget {
public:
  explicit SketchInteractionOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
  }

  void set_grid(std::vector<GuiSketchScreenSegment> lines) {
    grid_ = std::move(lines);
    update();
  }

  void set_handles(std::vector<GuiSketchScreenPoint> handles) {
    handles_ = std::move(handles);
    update();
  }

  void set_hover(std::vector<GuiSketchScreenPoint> polyline,
                 std::optional<GuiSketchScreenPoint> point) {
    hover_polyline_ = std::move(polyline);
    hover_point_ = std::move(point);
    update();
  }

  void set_preview(std::vector<GuiSketchScreenPoint> polyline, bool closed) {
    preview_polyline_ = std::move(polyline);
    preview_closed_ = closed;
    update();
  }

  void set_snap(std::optional<GuiSketchScreenPoint> point) {
    snap_point_ = std::move(point);
    update();
  }

  void set_box(std::optional<GuiSketchScreenRect> rectangle) {
    box_ = std::move(rectangle);
    update();
  }

  void clear_transient() {
    hover_polyline_.clear();
    hover_point_.reset();
    snap_point_.reset();
    box_.reset();
    update();
  }

  [[nodiscard]] std::size_t grid_line_count() const noexcept { return grid_.size(); }
  [[nodiscard]] std::size_t handle_count() const noexcept { return handles_.size(); }

protected:
  void paintEvent(QPaintEvent* event) override {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const auto& line : grid_) {
      QPen pen(line.major ? QColor(116, 126, 139, 112) : QColor(104, 113, 125, 58));
      pen.setWidthF(line.major ? 1.0 : 0.6);
      painter.setPen(pen);
      painter.drawLine(QPointF(line.start.x, line.start.y), QPointF(line.end.x, line.end.y));
    }

    for (const auto& handle : handles_) {
      painter.setPen(QPen(QColor(84, 190, 255), 1.8));
      painter.setBrush(QColor(48, 52, 59));
      painter.drawEllipse(QPointF(handle.x, handle.y), 4.2, 4.2);
    }

    if (hover_polyline_.size() >= 2U) {
      QPen pen(QColor(255, 196, 61));
      pen.setWidthF(2.2);
      painter.setPen(pen);
      for (std::size_t index = 1; index < hover_polyline_.size(); ++index)
        painter.drawLine(QPointF(hover_polyline_[index - 1U].x, hover_polyline_[index - 1U].y),
                         QPointF(hover_polyline_[index].x, hover_polyline_[index].y));
    }

    if (!preview_polyline_.empty()) {
      QPen pen(QColor(120, 205, 255));
      pen.setWidthF(1.6);
      pen.setStyle(Qt::DashLine);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      for (std::size_t index = 1; index < preview_polyline_.size(); ++index)
        painter.drawLine(
            QPointF(preview_polyline_[index - 1U].x, preview_polyline_[index - 1U].y),
            QPointF(preview_polyline_[index].x, preview_polyline_[index].y));
      if (preview_closed_ && preview_polyline_.size() >= 3U)
        painter.drawLine(QPointF(preview_polyline_.back().x, preview_polyline_.back().y),
                         QPointF(preview_polyline_.front().x, preview_polyline_.front().y));
      painter.drawEllipse(
          QPointF(preview_polyline_.front().x, preview_polyline_.front().y), 3.0, 3.0);
    }

    if (hover_point_) {
      painter.setPen(QPen(QColor(255, 196, 61), 2.0));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(QPointF(hover_point_->x, hover_point_->y), 5.0, 5.0);
    }

    if (snap_point_) {
      painter.setPen(QPen(QColor(96, 220, 170), 1.8));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(QRectF(snap_point_->x - 4.0, snap_point_->y - 4.0, 8.0, 8.0));
    }

    if (box_) {
      const QRectF rectangle(QPointF(box_->left(), box_->top()),
                             QPointF(box_->right(), box_->bottom()));
      const bool window = box_->end.x >= box_->start.x;
      const QColor stroke = window ? QColor(89, 171, 255) : QColor(95, 210, 150);
      const QColor fill = window ? QColor(89, 171, 255, 36) : QColor(95, 210, 150, 36);
      QPen pen(stroke, 1.2);
      pen.setStyle(Qt::DashLine);
      painter.setPen(pen);
      painter.setBrush(fill);
      painter.drawRect(rectangle);
    }
  }

private:
  std::vector<GuiSketchScreenSegment> grid_;
  std::vector<GuiSketchScreenPoint> handles_;
  std::vector<GuiSketchScreenPoint> hover_polyline_;
  std::vector<GuiSketchScreenPoint> preview_polyline_;
  bool preview_closed_{false};
  std::optional<GuiSketchScreenPoint> hover_point_;
  std::optional<GuiSketchScreenPoint> snap_point_;
  std::optional<GuiSketchScreenRect> box_;
};

GuiSelectionKind selection_kind(geometry::ViewportSceneKind kind) {
  switch (kind) {
  case geometry::ViewportSceneKind::SolidBody:
  case geometry::ViewportSceneKind::SurfaceBody:
    return GuiSelectionKind::Body;
  case geometry::ViewportSceneKind::Face:
    return GuiSelectionKind::Face;
  case geometry::ViewportSceneKind::Edge:
    return GuiSelectionKind::Edge;
  case geometry::ViewportSceneKind::Vertex:
    return GuiSelectionKind::Vertex;
  case geometry::ViewportSceneKind::Datum:
    return GuiSelectionKind::Datum;
  case geometry::ViewportSceneKind::SketchEntity:
  case geometry::ViewportSceneKind::Path:
    return GuiSelectionKind::SketchEntity;
  case geometry::ViewportSceneKind::Component:
    return GuiSelectionKind::Component;
  case geometry::ViewportSceneKind::AssemblyTarget:
    return GuiSelectionKind::AssemblyTarget;
  }
  return GuiSelectionKind::Body;
}

Quantity_Color presentation_color(geometry::ViewportSceneKind kind) {
  switch (kind) {
  case geometry::ViewportSceneKind::SurfaceBody:
    return Quantity_Color(0.35, 0.70, 0.95, Quantity_TOC_RGB);
  case geometry::ViewportSceneKind::Datum:
    return Quantity_Color(0.95, 0.75, 0.20, Quantity_TOC_RGB);
  case geometry::ViewportSceneKind::Face:
    return Quantity_Color(0.45, 0.70, 0.95, Quantity_TOC_RGB);
  case geometry::ViewportSceneKind::Edge:
  case geometry::ViewportSceneKind::Vertex:
    return Quantity_Color(0.95, 0.85, 0.25, Quantity_TOC_RGB);
  case geometry::ViewportSceneKind::SketchEntity:
  case geometry::ViewportSceneKind::Path:
    return Quantity_Color(0.30, 0.90, 0.55, Quantity_TOC_RGB);
  case geometry::ViewportSceneKind::AssemblyTarget:
    return Quantity_Color(0.95, 0.35, 0.25, Quantity_TOC_RGB);
  case geometry::ViewportSceneKind::SolidBody:
  case geometry::ViewportSceneKind::Component:
    return Quantity_Color(0.72, 0.76, 0.82, Quantity_TOC_RGB);
  }
  return Quantity_Color(0.72, 0.76, 0.82, Quantity_TOC_RGB);
}

V3d_TypeOfOrientation occt_orientation(GuiStandardView view) {
  switch (view) {
  case GuiStandardView::Front:
    return V3d_TypeOfOrientation_Zup_Front;
  case GuiStandardView::Back:
    return V3d_TypeOfOrientation_Zup_Back;
  case GuiStandardView::Left:
    return V3d_TypeOfOrientation_Zup_Left;
  case GuiStandardView::Right:
    return V3d_TypeOfOrientation_Zup_Right;
  case GuiStandardView::Top:
    return V3d_TypeOfOrientation_Zup_Top;
  case GuiStandardView::Bottom:
    return V3d_TypeOfOrientation_Zup_Bottom;
  case GuiStandardView::Isometric:
    return V3d_TypeOfOrientation_Zup_AxoRight;
  }
  return V3d_TypeOfOrientation_Zup_AxoRight;
}

Error viewport_error(std::string message) {
  return Error::geometry("gui.occt_viewport", std::move(message));
}

bool belongs_to_sketch(std::string_view semantic_id, std::string_view sketch_id) {
  const std::string prefix = "sketch/" + std::string(sketch_id) + "/";
  return semantic_id.size() >= prefix.size() && semantic_id.substr(0, prefix.size()) == prefix;
}

bool contains_selection(const std::vector<GuiSelection>& selections,
                        std::string_view semantic_id) {
  return std::any_of(selections.begin(), selections.end(), [semantic_id](const GuiSelection& item) {
    return item.kind == GuiSelectionKind::SketchEntity && item.semantic_id == semantic_id;
  });
}

void apply_selection_set(std::vector<GuiSelection>& current,
                         const std::vector<GuiSelection>& incoming,
                         Qt::KeyboardModifiers modifiers) {
  if (!(modifiers & Qt::ShiftModifier) && !(modifiers & Qt::ControlModifier))
    current.clear();
  for (const auto& selection : incoming) {
    const auto found = std::find(current.begin(), current.end(), selection);
    if (modifiers & Qt::ControlModifier) {
      if (found == current.end())
        current.push_back(selection);
      else
        current.erase(found);
    } else if (found == current.end()) {
      current.push_back(selection);
    }
  }
  std::sort(current.begin(), current.end(), [](const GuiSelection& first, const GuiSelection& second) {
    return first.semantic_id < second.semantic_id;
  });
}

} // namespace

struct OcctViewport::Impl {
  struct Presentation {
    geometry::ViewportSceneItem item;
    Handle(AIS_Shape) interactive;
  };

  Handle(Aspect_DisplayConnection) display_connection;
  Handle(OpenGl_GraphicDriver) graphic_driver;
  Handle(V3d_Viewer) viewer;
  Handle(AIS_InteractiveContext) context;
  Handle(V3d_View) view;
  Handle(Aspect_NeutralWindow) window;
  // Native X drawable of the top-level window, captured in showEvent BEFORE the
  // GL surface exists. Forcing native-window creation later (inside
  // initializeGL) would invalidate Qt's freshly created GL context.
  Aspect_Drawable native_drawable{0};
  std::vector<Presentation> presentations;
  std::unordered_map<const AIS_InteractiveObject*, std::size_t> owner_to_index;
  std::string initialization_error;
  bool initialization_attempted{false};
};

// QOpenGLWidget child that hosts the OCCT rendering. OCCT draws into Qt's GL
// context (framebuffer wrapping, driver buffersNoSwap), so Qt composites the
// 3D view like any other widget: no foreign native window, no erase/redraw
// races, and child overlays stack cleanly on top. Input stays on the parent
// OcctViewport; this surface is mouse-transparent.
class OcctViewportRenderSurface final : public QOpenGLWidget {
public:
  explicit OcctViewportRenderSurface(OcctViewport& owner)
      : QOpenGLWidget(&owner), owner_(owner) {
    setObjectName(QStringLiteral("blcad.occt_viewport.render_surface"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    setFormat(format);
  }

protected:
  void initializeGL() override { owner_.initialize_gl(); }
  void paintGL() override { owner_.paint_gl(); }

private:
  OcctViewport& owner_;
};

OcctViewport::OcctViewport(QWidget* parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
  setObjectName(QStringLiteral("blcad.occt_viewport"));
  setAccessibleName(QStringLiteral("BLCAD 3D viewport"));
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setMinimumSize(480, 320);
  setAutoFillBackground(false);
  sketch_overlay_ = new SketchInteractionOverlay(this);
  sketch_overlay_->setGeometry(rect());
}

OcctViewport::~OcctViewport() {
  // Release OCCT's GL resources while Qt's context is still current; the
  // context dies with the render surface during child destruction.
  if (gl_surface_ != nullptr && !impl_->view.IsNull()) {
    gl_surface_->makeCurrent();
    impl_.reset();
    gl_surface_->doneCurrent();
  }
}

void OcctViewport::render_update() {
  if (gl_surface_ != nullptr)
    gl_surface_->update();
  else
    update();
}

QPoint OcctViewport::to_render_point(QPoint point) const {
  // OCCT window/framebuffer coordinates are device pixels; Qt events are
  // logical pixels.
  const qreal scale =
      gl_surface_ != nullptr ? gl_surface_->devicePixelRatioF() : devicePixelRatioF();
  return {static_cast<int>(std::lround(point.x() * scale)),
          static_cast<int>(std::lround(point.y() * scale))};
}

Result<std::size_t> OcctViewport::set_scene(std::vector<geometry::ViewportSceneItem> items) {
  std::unordered_set<std::string> semantic_ids;
  std::vector<Impl::Presentation> candidate;
  candidate.reserve(items.size());
  for (auto& item : items) {
    if (item.semantic_id.empty() || item.shape.empty())
      return Result<std::size_t>::failure(
          viewport_error("presentation requires a semantic id and non-empty Geometry shape"));
    if (!semantic_ids.insert(item.semantic_id).second)
      return Result<std::size_t>::failure(
          viewport_error("duplicate presentation semantic id: " + item.semantic_id));

    Handle(AIS_Shape) interactive;
    if (!impl_->context.IsNull()) {
      const TopoDS_Shape* native = geometry::OcctShapeView::get(item.shape);
      if (native == nullptr || native->IsNull())
        return Result<std::size_t>::failure(viewport_error("OCCT presentation shape is null"));
      interactive = new AIS_Shape(*native);
      interactive->SetColor(presentation_color(item.kind));
      if (item.kind == geometry::ViewportSceneKind::Datum) {
        interactive->SetTransparency(0.72);
        // Highlight datum planes in wireframe (edges only). Otherwise hovering or
        // selecting a large plane redraws its filled face over the whole viewport,
        // flashing it grey.
        interactive->SetHilightMode(0);
      }
    }
    candidate.push_back({std::move(item), std::move(interactive)});
  }

  // Only auto-fit when content first appears. Re-fitting on every scene refresh
  // (each edit, sketch pointer update, or selection change bumps the
  // presentation revision) would reset the user's zoom/pan on every interaction
  // and make the view jump, especially while sketching. After first content the
  // camera is preserved; the user re-frames with Fit/Home or a standard view.
  const bool was_empty = impl_->presentations.empty();
  if (!impl_->context.IsNull()) {
    impl_->context->RemoveAll(false);
    for (auto& presentation : candidate)
      impl_->context->Display(presentation.interactive, false);
  }
  impl_->presentations = std::move(candidate);
  impl_->owner_to_index.clear();
  for (std::size_t index = 0; index < impl_->presentations.size(); ++index) {
    const auto& interactive = impl_->presentations[index].interactive;
    if (!interactive.IsNull())
      impl_->owner_to_index.emplace(interactive.get(), index);
  }
  apply_display_mode();
  apply_selection_filters();
  apply_sketch_focus();
  clear_selection();
  if (was_empty && !impl_->presentations.empty())
    fit_all();
  else
    render_update();
  update();
  return Result<std::size_t>::success(impl_->presentations.size());
}

void OcctViewport::clear_scene() {
  if (!impl_->context.IsNull()) {
    impl_->context->RemoveAll(false);
    render_update();
  }
  impl_->presentations.clear();
  impl_->owner_to_index.clear();
  clear_sketch_interaction();
  publish_selection(std::nullopt);
  update();
}

void OcctViewport::set_display_mode(GuiViewportDisplayMode mode) {
  display_mode_ = mode;
  apply_display_mode();
}

void OcctViewport::set_projection(GuiViewportProjection projection) {
  projection_ = projection;
  if (!impl_->view.IsNull()) {
    impl_->view->Camera()->SetProjectionType(projection == GuiViewportProjection::Perspective
                                                 ? Graphic3d_Camera::Projection_Perspective
                                                 : Graphic3d_Camera::Projection_Orthographic);
    render_update();
  }
  rebuild_sketch_grid();
}

void OcctViewport::set_standard_view(GuiStandardView view) {
  plane_camera_.reset();
  if (!impl_->view.IsNull()) {
    impl_->view->SetProj(occt_orientation(view));
    fit_all();
  }
  rebuild_sketch_grid();
}

bool OcctViewport::set_plane_camera(Point3 target, Vector3 normal, Vector3 up) {
  try {
    const gp_Dir projection(normal.x, normal.y, normal.z);
    const gp_Dir upward(up.x, up.y, up.z);
    if (std::abs(projection.Dot(upward)) > 1.0e-9)
      return false;
    plane_camera_ = GuiPlaneCamera{target, normal, up};
    if (!impl_->view.IsNull()) {
      impl_->view->SetAt(target.x, target.y, target.z);
      impl_->view->SetProj(projection.X(), projection.Y(), projection.Z());
      impl_->view->SetUp(upward.X(), upward.Y(), upward.Z());
      fit_all();
    }
    rebuild_sketch_grid();
    return true;
  } catch (const Standard_Failure&) {
    return false;
  }
}

GuiViewportCameraBookmark OcctViewport::camera_bookmark() const noexcept {
  GuiViewportCameraBookmark bookmark;
  bookmark.projection = projection_;
  bookmark.plane_camera = plane_camera_;
  if (plane_camera_) {
    bookmark.target = plane_camera_->target;
    bookmark.eye = {plane_camera_->target.x + plane_camera_->view_direction.x,
                    plane_camera_->target.y + plane_camera_->view_direction.y,
                    plane_camera_->target.z + plane_camera_->view_direction.z};
    bookmark.up_direction = plane_camera_->up_direction;
  }
  if (impl_->view.IsNull())
    return bookmark;
  try {
    impl_->view->Eye(bookmark.eye.x, bookmark.eye.y, bookmark.eye.z);
    impl_->view->At(bookmark.target.x, bookmark.target.y, bookmark.target.z);
    impl_->view->Up(bookmark.up_direction.x, bookmark.up_direction.y, bookmark.up_direction.z);
    bookmark.scale = impl_->view->Scale();
  } catch (const Standard_Failure&) {
  }
  return bookmark;
}

bool OcctViewport::restore_camera_bookmark(const GuiViewportCameraBookmark& bookmark) noexcept {
  if (bookmark.scale <= 0.0)
    return false;
  try {
    projection_ = bookmark.projection;
    plane_camera_ = bookmark.plane_camera;
    if (!impl_->view.IsNull()) {
      impl_->view->SetEye(bookmark.eye.x, bookmark.eye.y, bookmark.eye.z);
      impl_->view->SetAt(bookmark.target.x, bookmark.target.y, bookmark.target.z);
      impl_->view->SetUp(bookmark.up_direction.x, bookmark.up_direction.y,
                         bookmark.up_direction.z);
      impl_->view->SetScale(bookmark.scale);
      impl_->view->Camera()->SetProjectionType(
          projection_ == GuiViewportProjection::Perspective
              ? Graphic3d_Camera::Projection_Perspective
              : Graphic3d_Camera::Projection_Orthographic);
      render_update();
    }
    rebuild_sketch_grid();
    return true;
  } catch (const Standard_Failure&) {
    return false;
  }
}

void OcctViewport::set_sketch_focus(std::string sketch_id, GuiSketchSurroundingsMode mode) {
  sketch_focus_id_ = std::move(sketch_id);
  sketch_surroundings_mode_ = mode;
  apply_sketch_focus();
}

void OcctViewport::clear_sketch_focus() {
  sketch_focus_id_.clear();
  sketch_surroundings_mode_ = GuiSketchSurroundingsMode::Dim;
  apply_sketch_focus();
}

Result<std::size_t>
OcctViewport::set_sketch_interaction(GuiSketchPlaneView plane,
                                     GuiSketchInteractionScene scene,
                                     GuiSketchInteractionConfig config) {
  Result<GuiSketchPlaneMapping> mapping = [&]() -> Result<GuiSketchPlaneMapping> {
    if (!impl_->view.IsNull()) {
      GuiSketchPlaneMapping::ScreenToRay screen_to_ray = [this](GuiSketchScreenPoint point) {
        try {
          const double ratio = devicePixelRatioF();
          const int x = static_cast<int>(std::lround(point.x * ratio));
          const int y = static_cast<int>(std::lround(point.y * ratio));
          double px{}, py{}, pz{}, vx{}, vy{}, vz{};
          impl_->view->ConvertWithProj(x, y, px, py, pz, vx, vy, vz);
          return Result<GuiSketchViewRay>::success(
              GuiSketchViewRay{{px, py, pz}, {vx, vy, vz}});
        } catch (const Standard_Failure& failure) {
          return Result<GuiSketchViewRay>::failure(
              viewport_error(failure.GetMessageString() != nullptr
                                 ? failure.GetMessageString()
                                 : "could not convert Sketch pixel to view ray"));
        }
      };
      GuiSketchPlaneMapping::ModelToScreen model_to_screen = [this](Point3 point) {
        try {
          int x{}, y{};
          impl_->view->Convert(point.x, point.y, point.z, x, y);
          const double ratio = devicePixelRatioF();
          return Result<GuiSketchScreenPoint>::success(
              GuiSketchScreenPoint{static_cast<double>(x) / ratio,
                                   static_cast<double>(y) / ratio});
        } catch (const Standard_Failure& failure) {
          return Result<GuiSketchScreenPoint>::failure(
              viewport_error(failure.GetMessageString() != nullptr
                                 ? failure.GetMessageString()
                                 : "could not convert Sketch model point to pixel"));
        }
      };
      return GuiSketchPlaneMapping::create(std::move(plane), std::move(screen_to_ray),
                                           std::move(model_to_screen));
    }
    const Point2 center = plane.model_to_plane(plane_camera_ ? plane_camera_->target : plane.origin);
    return GuiSketchPlaneMapping::create_orthographic(
        std::move(plane), std::max(1, width()), std::max(1, height()), center, 1.0);
  }();
  if (mapping.has_error())
    return Result<std::size_t>::failure(mapping.error());
  auto controller = GuiSketchInteractionController::create(
      std::move(mapping.value()), std::move(scene), std::move(config));
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());

  const std::size_t primitive_count = controller.value().scene().curves.size() +
                                      controller.value().scene().points.size() +
                                      controller.value().scene().annotations.size();
  sketch_interaction_ = std::make_unique<GuiSketchInteractionController>(
      std::move(controller.value()));
  sketch_snap_result_.reset();
  hovered_sketch_hit_.reset();
  sketch_box_selection_.reset();
  sketch_box_active_ = false;
  sketch_overlay_->show();
  sketch_overlay_->raise();
  rebuild_sketch_grid();
  rebuild_sketch_drag_handles();
  rebuild_sketch_preview_polyline();
  return Result<std::size_t>::success(primitive_count);
}

void OcctViewport::clear_sketch_interaction() {
  sketch_interaction_.reset();
  sketch_selections_.clear();
  sketch_inference_anchor_.reset();
  sketch_snap_result_.reset();
  hovered_sketch_hit_.reset();
  sketch_box_selection_.reset();
  sketch_box_active_ = false;
  sketch_drag_handles_.clear();
  sketch_preview_polyline_.clear();
  sketch_preview_closed_ = false;
  if (auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_)) {
    overlay->set_grid({});
    overlay->set_handles({});
    overlay->set_preview({}, false);
    overlay->clear_transient();
    overlay->hide();
  }
}

void OcctViewport::set_sketch_selection_enabled(bool enabled) noexcept {
  sketch_selection_enabled_ = enabled;
  if (!enabled) {
    sketch_box_active_ = false;
    sketch_box_selection_.reset();
    if (auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_))
      overlay->set_box(std::nullopt);
  }
}

void OcctViewport::set_sketch_inference_anchor(std::optional<Point2> anchor) noexcept {
  sketch_inference_anchor_ = std::move(anchor);
}

void OcctViewport::set_sketch_grid_config(GuiSketchGridConfig config) {
  if (!sketch_interaction_)
    return;
  auto interaction_config = sketch_interaction_->config();
  interaction_config.grid = std::move(config);
  sketch_interaction_->set_config(std::move(interaction_config));
  rebuild_sketch_grid();
  update_sketch_pointer({static_cast<double>(last_mouse_position_.x()),
                         static_cast<double>(last_mouse_position_.y())});
}

void OcctViewport::set_sketch_drag_handles(std::vector<Point2> handles) {
  sketch_drag_handles_ = std::move(handles);
  rebuild_sketch_drag_handles();
}

void OcctViewport::set_sketch_preview_polyline(std::vector<Point2> polyline, bool closed) {
  sketch_preview_polyline_ = std::move(polyline);
  sketch_preview_closed_ = closed;
  rebuild_sketch_preview_polyline();
}

void OcctViewport::set_sketch_pointer_callback(SketchPointerCallback callback) {
  sketch_pointer_callback_ = std::move(callback);
}

void OcctViewport::set_sketch_drag_pointer_callback(SketchDragPointerCallback callback) {
  sketch_drag_pointer_callback_ = std::move(callback);
}

void OcctViewport::set_sketch_pointer_phase_callback(SketchPointerPhaseCallback callback) {
  sketch_pointer_phase_callback_ = std::move(callback);
}

void OcctViewport::set_sketch_selection_callback(SketchSelectionCallback callback) {
  sketch_selection_callback_ = std::move(callback);
}

void OcctViewport::set_context_menu_callback(
    std::function<void(QPoint)> callback) {
  context_menu_callback_ = std::move(callback);
}

void OcctViewport::fit_all() {
  if (!impl_->view.IsNull() && !impl_->presentations.empty()) {
    impl_->view->FitAll(0.05, false);
    impl_->view->ZFitAll();
    render_update();
  }
  rebuild_sketch_grid();
}

void OcctViewport::set_selection_filter_mask(std::uint32_t mask) {
  selection_filter_mask_ = mask;
  apply_selection_filters();
  if (selected_semantic_.has_value() &&
      (mask & selection_kind_bit(selected_semantic_->kind)) == 0U)
    clear_selection();
}

void OcctViewport::set_selection_callback(
    std::function<void(std::optional<GuiSelection>)> callback) {
  selection_callback_ = std::move(callback);
}

bool OcctViewport::select_semantic(std::string_view semantic_id) {
  if (sketch_interaction_ &&
      (selection_filter_mask_ & selection_kind_bit(GuiSelectionKind::SketchEntity)) != 0U) {
    const auto& scene = sketch_interaction_->scene();
    const bool known = std::any_of(scene.curves.begin(), scene.curves.end(),
                                   [semantic_id](const auto& item) {
                                     return item.semantic_id == semantic_id;
                                   }) ||
                       std::any_of(scene.points.begin(), scene.points.end(),
                                   [semantic_id](const auto& item) {
                                     return item.semantic_id == semantic_id;
                                   }) ||
                       std::any_of(scene.annotations.begin(), scene.annotations.end(),
                                   [semantic_id](const auto& item) {
                                     return item.semantic_id == semantic_id;
                                   });
    if (known) {
      sketch_selections_ = {{GuiSelectionKind::SketchEntity, std::string(semantic_id)}};
      publish_sketch_selection();
      return true;
    }
  }

  const auto found = std::find_if(impl_->presentations.begin(), impl_->presentations.end(),
                                  [semantic_id](const Impl::Presentation& presentation) {
                                    return presentation.item.semantic_id == semantic_id;
                                  });
  if (found == impl_->presentations.end())
    return false;
  GuiSelection selection{selection_kind(found->item.kind), found->item.semantic_id};
  if ((selection_filter_mask_ & selection_kind_bit(selection.kind)) == 0U)
    return false;
  if (!impl_->context.IsNull() && !found->interactive.IsNull()) {
    impl_->context->ClearSelected(false);
    impl_->context->AddOrRemoveSelected(found->interactive, false);
    render_update();
  }
  publish_selection(std::move(selection));
  return true;
}

void OcctViewport::clear_selection() {
  if (!impl_->context.IsNull()) {
    impl_->context->ClearSelected(false);
    render_update();
  }
  sketch_selections_.clear();
  publish_sketch_selection();
  publish_selection(std::nullopt);
}

std::size_t OcctViewport::presentation_count() const noexcept {
  return impl_->presentations.size();
}

GuiViewportDisplayMode OcctViewport::display_mode() const noexcept {
  return display_mode_;
}

GuiViewportProjection OcctViewport::projection() const noexcept {
  return projection_;
}

std::uint32_t OcctViewport::selection_filter_mask() const noexcept {
  return selection_filter_mask_;
}

const std::optional<GuiSelection>& OcctViewport::selected_semantic() const noexcept {
  return selected_semantic_;
}

const std::optional<GuiPlaneCamera>& OcctViewport::plane_camera() const noexcept {
  return plane_camera_;
}

bool OcctViewport::sketch_focus_active() const noexcept {
  return !sketch_focus_id_.empty();
}

std::string_view OcctViewport::sketch_focus_id() const noexcept {
  return sketch_focus_id_;
}

GuiSketchSurroundingsMode OcctViewport::sketch_surroundings_mode() const noexcept {
  return sketch_surroundings_mode_;
}

bool OcctViewport::sketch_interaction_active() const noexcept {
  return sketch_interaction_ != nullptr;
}

bool OcctViewport::sketch_selection_enabled() const noexcept {
  return sketch_selection_enabled_;
}

const std::vector<GuiSelection>& OcctViewport::sketch_selections() const noexcept {
  return sketch_selections_;
}

const std::optional<GuiSketchSnapResult>& OcctViewport::sketch_snap_result() const noexcept {
  return sketch_snap_result_;
}

const std::optional<GuiSketchHit>& OcctViewport::hovered_sketch_hit() const noexcept {
  return hovered_sketch_hit_;
}

const std::optional<GuiSketchScreenRect>& OcctViewport::sketch_box_selection() const noexcept {
  return sketch_box_selection_;
}

std::size_t OcctViewport::sketch_grid_line_count() const noexcept {
  const auto* overlay = static_cast<const SketchInteractionOverlay*>(sketch_overlay_);
  return overlay == nullptr ? 0U : overlay->grid_line_count();
}

std::size_t OcctViewport::sketch_drag_handle_count() const noexcept {
  const auto* overlay = static_cast<const SketchInteractionOverlay*>(sketch_overlay_);
  return overlay == nullptr ? 0U : overlay->handle_count();
}

bool OcctViewport::native_viewer_available() const noexcept {
  return !impl_->view.IsNull();
}

const std::string& OcctViewport::initialization_error() const noexcept {
  return impl_->initialization_error;
}

void OcctViewport::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  initialize_native_viewer();
}

void OcctViewport::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)
  if (!impl_->view.IsNull())
    return; // The render surface child paints the 3D view.
  QPainter painter(this);
  painter.fillRect(rect(), QColor(48, 52, 59));
  painter.setPen(QColor(215, 220, 226));
  const QString message = impl_->initialization_error.empty()
                              ? QStringLiteral("OCCT 3D Viewport")
                              : QString::fromStdString(impl_->initialization_error);
  painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, message);
}

void OcctViewport::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (gl_surface_ != nullptr)
    gl_surface_->setGeometry(rect());
  if (sketch_overlay_)
    sketch_overlay_->setGeometry(rect());
  if (sketch_interaction_ && impl_->view.IsNull()) {
    const auto plane = sketch_interaction_->mapping().plane();
    const auto scene = sketch_interaction_->scene();
    const auto config = sketch_interaction_->config();
    (void)set_sketch_interaction(plane, scene, config);
  } else {
    rebuild_sketch_grid();
  }
  rebuild_sketch_drag_handles();
  rebuild_sketch_preview_polyline();
}

void OcctViewport::mousePressEvent(QMouseEvent* event) {
  last_mouse_position_ = event->position().toPoint();
  if (event->button() == Qt::MiddleButton)
    panning_ = true;
  if (event->button() == Qt::RightButton) {
    right_press_position_ = last_mouse_position_;
    if (!impl_->view.IsNull() && (!sketch_focus_active() || !plane_camera_.has_value())) {
      rotating_ = true;
      const QPoint start = to_render_point(last_mouse_position_);
      impl_->view->StartRotation(start.x(), start.y());
    }
  }
  if (event->button() == Qt::LeftButton && sketch_interaction_) {
    const GuiSketchScreenPoint current{event->position().x(), event->position().y()};
    update_sketch_pointer(current);
    publish_sketch_pointer_phase(GuiSketchPointerPhase::Press, current);
    if (sketch_selection_enabled_) {
      sketch_press_position_ = last_mouse_position_;
      auto hits = sketch_interaction_->hits_at(current);
      if (hits && hits.value().empty()) {
        sketch_box_active_ = true;
        sketch_box_selection_ = GuiSketchScreenRect{current, current};
        static_cast<SketchInteractionOverlay*>(sketch_overlay_)->set_box(sketch_box_selection_);
      }
    }
  }
  QWidget::mousePressEvent(event);
}

void OcctViewport::mouseMoveEvent(QMouseEvent* event) {
  const QPoint current = event->position().toPoint();
  if (!impl_->view.IsNull() && panning_) {
    const QPoint delta = to_render_point(current) - to_render_point(last_mouse_position_);
    impl_->view->Pan(delta.x(), -delta.y(), 1.0, false);
    render_update();
    rebuild_sketch_grid();
  } else if (!impl_->view.IsNull() && rotating_) {
    const QPoint point = to_render_point(current);
    impl_->view->Rotation(point.x(), point.y());
    render_update();
    rebuild_sketch_grid();
  } else if (sketch_interaction_) {
    if (sketch_box_active_) {
      sketch_box_selection_->end = {event->position().x(), event->position().y()};
      static_cast<SketchInteractionOverlay*>(sketch_overlay_)->set_box(sketch_box_selection_);
    }
    update_sketch_pointer({event->position().x(), event->position().y()});
  } else if (!impl_->context.IsNull()) {
    const QPoint point = to_render_point(current);
    impl_->context->MoveTo(point.x(), point.y(), impl_->view, false);
    render_update();
  }
  last_mouse_position_ = current;
  QWidget::mouseMoveEvent(event);
}

void OcctViewport::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton)
    panning_ = false;
  if (event->button() == Qt::RightButton) {
    rotating_ = false;
    const QPoint current = event->position().toPoint();
    if (sketch_focus_active() && (current - right_press_position_).manhattanLength() <= 3 &&
        context_menu_callback_)
      context_menu_callback_(mapToGlobal(current));
  }
  if (event->button() == Qt::LeftButton && sketch_interaction_) {
    const GuiSketchScreenPoint current{event->position().x(), event->position().y()};
    update_sketch_pointer(current);
    publish_sketch_pointer_phase(GuiSketchPointerPhase::Release, current);
    if (sketch_selection_enabled_) {
      if (sketch_box_active_ &&
          (event->position().toPoint() - sketch_press_position_).manhattanLength() > 3) {
        sketch_box_selection_->end = current;
        const auto mode = current.x >= sketch_box_selection_->start.x
                              ? GuiSketchBoxSelectionMode::Window
                              : GuiSketchBoxSelectionMode::Crossing;
        auto selections = sketch_interaction_->box_select(*sketch_box_selection_, mode);
        if (selections)
          apply_selection_set(sketch_selections_, selections.value(), event->modifiers());
      } else if (!sketch_box_active_) {
        auto hit = sketch_interaction_->cycle_hit(current);
        if (hit && hit.value()) {
          apply_selection_set(sketch_selections_,
                              {{GuiSelectionKind::SketchEntity,
                                hit.value()->semantic_id}},
                              event->modifiers());
        } else if (!(event->modifiers() & Qt::ShiftModifier) &&
                   !(event->modifiers() & Qt::ControlModifier)) {
          sketch_selections_.clear();
        }
      } else if (!(event->modifiers() & Qt::ShiftModifier) &&
                 !(event->modifiers() & Qt::ControlModifier)) {
        sketch_selections_.clear();
      }
      publish_sketch_selection();
    }
    sketch_box_active_ = false;
    sketch_box_selection_.reset();
    static_cast<SketchInteractionOverlay*>(sketch_overlay_)->set_box(std::nullopt);
    update_sketch_pointer(current);
  } else if (event->button() == Qt::LeftButton && !impl_->context.IsNull()) {
    const QPoint current = to_render_point(event->position().toPoint());
    impl_->context->MoveTo(current.x(), current.y(), impl_->view, false);
    if (impl_->context->HasDetected()) {
      impl_->context->SelectDetected(AIS_SelectionScheme_Replace);
      publish_selection(detected_selection());
    } else {
      clear_selection();
    }
    render_update();
  }
  QWidget::mouseReleaseEvent(event);
}

void OcctViewport::wheelEvent(QWheelEvent* event) {
  if (!impl_->view.IsNull() && event->angleDelta().y() != 0) {
    // Cursor-anchored zoom. OCCT's Zoom(x1,y1,x2,y2) derives the factor from the
    // (x2,y2)-(x1,y1) delta, so both axes must move with an explicit sign:
    // wheel up (positive delta) zooms in, wheel down zooms out. Anchoring at the
    // cursor keeps the point under the pointer stable.
    const QPoint point = to_render_point(event->position().toPoint());
    const int factor = event->angleDelta().y() > 0 ? 15 : -15;
    impl_->view->StartZoomAtPoint(point.x(), point.y());
    impl_->view->ZoomAtPoint(point.x(), point.y(), point.x() + factor, point.y() + factor);
    render_update();
    rebuild_sketch_grid();
    if (sketch_interaction_)
      update_sketch_pointer({event->position().x(), event->position().y()});
    event->accept();
    return;
  }
  QWidget::wheelEvent(event);
}

void OcctViewport::initialize_native_viewer() {
  if (impl_->initialization_attempted)
    return;
  impl_->initialization_attempted = true;
  if (QApplication::platformName() != QStringLiteral("xcb")) {
    impl_->initialization_error = "OCCT viewport requires the Qt xcb platform";
    update();
    return;
  }
  try {
    impl_->display_connection = new Aspect_DisplayConnection();
    // Deferred driver init: the GL context is Qt's and only becomes available
    // inside the render surface's initializeGL. Qt swaps buffers, not OCCT.
    impl_->graphic_driver = new OpenGl_GraphicDriver(impl_->display_connection, false);
    impl_->graphic_driver->ChangeOptions().buffersNoSwap = true;
    impl_->graphic_driver->ChangeOptions().buffersOpaqueAlpha = true;
    impl_->graphic_driver->ChangeOptions().useSystemBuffer = false;
    impl_->viewer = new V3d_Viewer(impl_->graphic_driver);
    impl_->viewer->SetDefaultLights();
    impl_->viewer->SetLightOn();
    impl_->context = new AIS_InteractiveContext(impl_->viewer);
    impl_->view = impl_->viewer->CreateView();
    impl_->view->SetImmediateUpdate(false);
    impl_->view->SetBackgroundColor(Quantity_Color(0.08, 0.10, 0.13, Quantity_TOC_RGB));
    impl_->owner_to_index.clear();
    for (std::size_t index = 0; index < impl_->presentations.size(); ++index) {
      auto& presentation = impl_->presentations[index];
      const TopoDS_Shape* native = geometry::OcctShapeView::get(presentation.item.shape);
      if (native == nullptr || native->IsNull())
        continue;
      presentation.interactive = new AIS_Shape(*native);
      presentation.interactive->SetColor(presentation_color(presentation.item.kind));
      if (presentation.item.kind == geometry::ViewportSceneKind::Datum) {
        presentation.interactive->SetTransparency(0.72);
        presentation.interactive->SetHilightMode(0);
      }
      impl_->context->Display(presentation.interactive, false);
      impl_->owner_to_index.emplace(presentation.interactive.get(), index);
    }
    set_projection(projection_);
    apply_display_mode();
    apply_selection_filters();
    apply_sketch_focus();
    gl_surface_ = new OcctViewportRenderSurface(*this);
    gl_surface_->setGeometry(rect());
    // Keep the render surface below every overlay child (sketch, inference,
    // manipulators), regardless of creation order.
    gl_surface_->lower();
    gl_surface_->show();
  } catch (const Standard_Failure& failure) {
    impl_->initialization_error = failure.GetMessageString() != nullptr
                                      ? failure.GetMessageString()
                                      : "OCCT viewport initialization failed";
    impl_->view.Nullify();
    impl_->context.Nullify();
  }
  update();
}

void OcctViewport::initialize_gl() {
  if (impl_->view.IsNull() || gl_surface_ == nullptr)
    return;
  try {
    Handle(OpenGl_Context) gl_context = new OpenGl_Context();
    if (!gl_context->Init()) {
      throw Standard_Failure("OCCT could not wrap the Qt OpenGL context");
    }
    Handle(Aspect_NeutralWindow) neutral_window =
        Handle(Aspect_NeutralWindow)::DownCast(impl_->view->Window());
    if (neutral_window.IsNull()) {
      neutral_window = new Aspect_NeutralWindow();
      impl_->window = neutral_window;
    }
    // OCCT's GLX path needs a real X drawable for its visual query even though
    // rendering goes to the wrapped Qt framebuffer. Query it here: earlier the
    // top-level may still get recreated (adding the GL child switches its
    // surface type), which would invalidate a captured handle. The top-level is
    // native by now, so winId() forces nothing.
    impl_->native_drawable = static_cast<Aspect_Drawable>(window()->winId());
    // Qt batches window creation on its xcb connection; round-trip so the X
    // server knows the window before OCCT queries it over its own connection
    // (otherwise XGetWindowAttributes fails and OCCT crashes).
    if (auto* x11_app = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        x11_app != nullptr && x11_app->connection() != nullptr)
      std::free(xcb_get_input_focus_reply(x11_app->connection(),
                                          xcb_get_input_focus(x11_app->connection()), nullptr));
    neutral_window->SetNativeHandle(impl_->native_drawable);
    const QPoint size = to_render_point({gl_surface_->width(), gl_surface_->height()});
    neutral_window->SetSize(size.x(), size.y());
    impl_->view->SetWindow(neutral_window, gl_context->RenderingContext());
  } catch (const Standard_Failure& failure) {
    impl_->initialization_error = failure.GetMessageString() != nullptr
                                      ? failure.GetMessageString()
                                      : "OCCT viewport initialization failed";
    for (auto& presentation : impl_->presentations)
      presentation.interactive.Nullify();
    impl_->owner_to_index.clear();
    impl_->view.Nullify();
    impl_->context.Nullify();
    update();
  }
}

void OcctViewport::paint_gl() {
  if (impl_->view.IsNull() || impl_->view->Window().IsNull() ||
      impl_->graphic_driver.IsNull())
    return;
  const Handle(OpenGl_Context)& gl_context = impl_->graphic_driver->GetSharedContext();
  if (gl_context.IsNull())
    return;
  Handle(OpenGl_FrameBuffer) default_fbo = gl_context->DefaultFrameBuffer();
  if (default_fbo.IsNull()) {
    default_fbo = new OpenGl_FrameBuffer();
    gl_context->SetDefaultFrameBuffer(default_fbo);
  }
  // Redirect OCCT's default framebuffer to the QOpenGLWidget framebuffer for
  // this frame; Qt composites and swaps.
  if (!default_fbo->InitWrapper(gl_context))
    return;
  const Graphic3d_Vec2i fbo_size(default_fbo->GetVPSizeX(), default_fbo->GetVPSizeY());
  Standard_Integer width = 0;
  Standard_Integer height = 0;
  impl_->window->Size(width, height);
  if (fbo_size.x() != width || fbo_size.y() != height) {
    impl_->window->SetSize(fbo_size.x(), fbo_size.y());
    impl_->view->MustBeResized();
    impl_->view->Invalidate();
  }
  impl_->view->InvalidateImmediate();
  impl_->view->Redraw();
}

void OcctViewport::apply_display_mode() {
  if (impl_->context.IsNull())
    return;
  const int mode = display_mode_ == GuiViewportDisplayMode::Wireframe ? 0 : 1;
  for (auto& presentation : impl_->presentations) {
    if (presentation.interactive.IsNull())
      continue;
    // Datum planes are drawn as a wireframe border (Inventor-style), never as a
    // shaded face. A large shaded plane fills the whole viewport grey the moment
    // it is hovered or selected; a wireframe border highlights only its edges.
    const bool datum = presentation.item.kind == geometry::ViewportSceneKind::Datum;
    presentation.interactive->Attributes()->SetFaceBoundaryDraw(
        display_mode_ == GuiViewportDisplayMode::ShadedWithEdges);
    impl_->context->SetDisplayMode(presentation.interactive, datum ? 0 : mode, false);
  }
  render_update();
}

void OcctViewport::apply_selection_filters() {
  if (impl_->context.IsNull())
    return;
  for (auto& presentation : impl_->presentations) {
    if (presentation.interactive.IsNull())
      continue;
    const GuiSelectionKind kind = selection_kind(presentation.item.kind);
    impl_->context->SetSelectionModeActive(
        presentation.interactive, 0,
        (selection_filter_mask_ & selection_kind_bit(kind)) != 0U,
        AIS_SelectionModesConcurrency_Single, true);
  }
  render_update();
}

void OcctViewport::apply_sketch_focus() {
  if (impl_->context.IsNull())
    return;
  for (auto& presentation : impl_->presentations) {
    if (presentation.interactive.IsNull())
      continue;

    if (!sketch_focus_active()) {
      impl_->context->Display(presentation.interactive, false);
      presentation.interactive->SetTransparency(
          presentation.item.kind == geometry::ViewportSceneKind::Datum ? 0.72 : 0.0);
      continue;
    }

    const bool active_sketch = belongs_to_sketch(presentation.item.semantic_id, sketch_focus_id_);
    if (active_sketch) {
      impl_->context->Display(presentation.interactive, false);
      presentation.interactive->SetTransparency(0.0);
    } else if (sketch_surroundings_mode_ == GuiSketchSurroundingsMode::Isolate) {
      impl_->context->Erase(presentation.interactive, false);
    } else {
      impl_->context->Display(presentation.interactive, false);
      presentation.interactive->SetTransparency(
          presentation.item.kind == geometry::ViewportSceneKind::Datum ? 0.88 : 0.72);
    }
  }
  render_update();
}

void OcctViewport::rebuild_sketch_grid() {
  auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_);
  if (!sketch_interaction_ || overlay == nullptr)
    return;
  auto lines = sketch_interaction_->grid_lines(std::max(1, width()), std::max(1, height()));
  if (lines)
    overlay->set_grid(std::move(lines.value()));
  else
    overlay->set_grid({});
}

void OcctViewport::rebuild_sketch_drag_handles() {
  auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_);
  if (!sketch_interaction_ || overlay == nullptr)
    return;
  std::vector<GuiSketchScreenPoint> handles;
  handles.reserve(sketch_drag_handles_.size());
  for (const auto point : sketch_drag_handles_) {
    auto screen = sketch_interaction_->mapping().plane_to_screen(point);
    if (screen)
      handles.push_back(screen.value());
  }
  overlay->set_handles(std::move(handles));
}

void OcctViewport::rebuild_sketch_preview_polyline() {
  auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_);
  if (overlay == nullptr)
    return;
  if (!sketch_interaction_) {
    overlay->set_preview({}, false);
    return;
  }
  std::vector<GuiSketchScreenPoint> screen_points;
  screen_points.reserve(sketch_preview_polyline_.size());
  for (const auto point : sketch_preview_polyline_) {
    auto screen = sketch_interaction_->mapping().plane_to_screen(point);
    if (screen)
      screen_points.push_back(screen.value());
  }
  overlay->set_preview(std::move(screen_points), sketch_preview_closed_);
}

void OcctViewport::update_sketch_pointer(GuiSketchScreenPoint screen_point) {
  if (!sketch_interaction_)
    return;
  auto snap = sketch_interaction_->snap(screen_point, sketch_inference_anchor_);
  auto hits = sketch_interaction_->hits_at(screen_point);
  if (snap.has_error() || hits.has_error())
    return;

  sketch_snap_result_ = snap.value();
  hovered_sketch_hit_ = hits.value().empty()
                            ? std::optional<GuiSketchHit>{}
                            : std::optional<GuiSketchHit>{hits.value().front()};
  auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_);
  if (overlay != nullptr) {
    std::vector<GuiSketchScreenPoint> hover_polyline;
    std::optional<GuiSketchScreenPoint> hover_point;
    if (hovered_sketch_hit_) {
      const auto curve = std::find_if(
          sketch_interaction_->scene().curves.begin(),
          sketch_interaction_->scene().curves.end(), [this](const auto& item) {
            return item.candidate_id == hovered_sketch_hit_->candidate_id;
          });
      if (curve != sketch_interaction_->scene().curves.end()) {
        auto screen = sketch_interaction_->screen_polyline(*curve);
        if (screen)
          hover_polyline = std::move(screen.value());
      } else {
        auto screen = sketch_interaction_->mapping().plane_to_screen(
            hovered_sketch_hit_->plane_point);
        if (screen)
          hover_point = screen.value();
      }
    }
    overlay->set_hover(std::move(hover_polyline), hover_point);
    auto snap_screen = sketch_interaction_->mapping().plane_to_screen(
        sketch_snap_result_->snapped_point);
    overlay->set_snap(snap_screen ? std::optional<GuiSketchScreenPoint>{snap_screen.value()}
                                  : std::nullopt);
  }
  if (sketch_pointer_callback_)
    sketch_pointer_callback_(sketch_snap_result_->raw_point, *sketch_snap_result_,
                             hovered_sketch_hit_);
  if (sketch_drag_pointer_callback_)
    sketch_drag_pointer_callback_(screen_point, sketch_snap_result_->raw_point,
                                  *sketch_snap_result_, hovered_sketch_hit_);
}

void OcctViewport::publish_sketch_pointer_phase(GuiSketchPointerPhase phase,
                                                GuiSketchScreenPoint screen_point) {
  if (sketch_pointer_phase_callback_ && sketch_snap_result_)
    sketch_pointer_phase_callback_(phase, screen_point, sketch_snap_result_->raw_point,
                                   *sketch_snap_result_, hovered_sketch_hit_);
}

void OcctViewport::publish_sketch_selection() {
  selected_semantic_ = sketch_selections_.size() == 1U
                           ? std::optional<GuiSelection>{sketch_selections_.front()}
                           : std::nullopt;
  if (sketch_selection_callback_)
    sketch_selection_callback_(sketch_selections_);
}

void OcctViewport::publish_selection(std::optional<GuiSelection> selection) {
  selected_semantic_ = std::move(selection);
  if (selection_callback_)
    selection_callback_(selected_semantic_);
}

std::optional<GuiSelection> OcctViewport::detected_selection() const {
  if (impl_->context.IsNull() || !impl_->context->HasDetected())
    return std::nullopt;
  const Handle(AIS_InteractiveObject) detected = impl_->context->DetectedInteractive();
  if (detected.IsNull())
    return std::nullopt;
  const auto found = impl_->owner_to_index.find(detected.get());
  if (found == impl_->owner_to_index.end())
    return std::nullopt;
  const auto& item = impl_->presentations[found->second].item;
  GuiSelection selection{selection_kind(item.kind), item.semantic_id};
  if ((selection_filter_mask_ & selection_kind_bit(selection.kind)) == 0U)
    return std::nullopt;
  return selection;
}

} // namespace blcad::gui
