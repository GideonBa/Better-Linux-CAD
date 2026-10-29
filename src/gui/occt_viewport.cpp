#include "blcad/gui/gui_sketch_dimension_layout.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/gui_appearance.hpp"
#include "blcad/gui/gui_sketch_annotation_layout.hpp"

#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_NeutralWindow.hxx>
#include <Graphic3d_Camera.hxx>
#include <OpenGl_Context.hxx>
#include <OpenGl_FrameBuffer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Quantity_Color.hxx>
#include <SelectMgr_EntityOwner.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <V3d.hxx>
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
    setObjectName(QStringLiteral("blcad.sketch.overlay"));
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

  void set_selected(std::vector<std::vector<GuiSketchScreenPoint>> polylines) {
    selected_polylines_ = std::move(polylines);
    update();
  }

  void set_selected_annotations(std::unordered_set<std::string> ids) {
    selected_annotations_ = std::move(ids);
    update();
  }

  void set_hovered_annotation(std::string id) {
    hovered_annotation_ = std::move(id);
    update();
  }

  void set_operation_preview(std::vector<std::vector<GuiSketchScreenPoint>> curves) {
    operation_preview_ = std::move(curves);
    update();
  }

  void set_scene_points(std::vector<GuiSketchScreenPoint> points) {
    scene_points_ = std::move(points);
    update();
  }

  struct AnnotationBadge {
    GuiSketchScreenPoint point;
    QString label;
    bool dimension{true};
    std::optional<GuiDimensionScreenLayout> layout;
    std::string semantic_id;
  };

  void set_annotations(std::vector<AnnotationBadge> annotations) {
    annotations_ = std::move(annotations);
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
    selected_polylines_.clear();
    selected_annotations_.clear();
    hovered_annotation_.clear();
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
      // Inventor-style white plane grid; majors brighter than minors.
      QPen pen(line.major ? QColor(255, 255, 255, 122) : QColor(255, 255, 255, 52));
      pen.setWidthF(line.major ? 1.0 : 0.6);
      painter.setPen(pen);
      painter.drawLine(QPointF(line.start.x, line.start.y), QPointF(line.end.x, line.end.y));
    }

    // Selected entities: Inventor-style clear blue highlight.
    for (const auto& polyline : selected_polylines_) {
      QPen pen(QColor(66, 150, 250), 2.6);
      painter.setPen(pen);
      for (std::size_t index = 1; index < polyline.size(); ++index)
        painter.drawLine(QPointF(polyline[index - 1U].x, polyline[index - 1U].y),
                         QPointF(polyline[index].x, polyline[index].y));
    }

    // Sketch points (endpoints, centers): always-visible dots in the entity
    // line color, independent of the drag layer.
    for (const auto& point : scene_points_) {
      painter.setPen(QPen(QColor(77, 229, 140), 1.0));
      painter.setBrush(QColor(77, 229, 140));
      painter.drawEllipse(QPointF(point.x, point.y), 2.8, 2.8);
    }

    // Point handles: small filled dots in the sketch-entity line color.
    for (const auto& handle : handles_) {
      painter.setPen(QPen(QColor(77, 229, 140), 1.0));
      painter.setBrush(QColor(77, 229, 140));
      painter.drawEllipse(QPointF(handle.x, handle.y), 2.4, 2.4);
    }

    if (hover_polyline_.size() >= 2U) {
      QPen pen(QColor(255, 196, 61));
      pen.setWidthF(2.2);
      painter.setPen(pen);
      for (std::size_t index = 1; index < hover_polyline_.size(); ++index)
        painter.drawLine(QPointF(hover_polyline_[index - 1U].x, hover_polyline_[index - 1U].y),
                         QPointF(hover_polyline_[index].x, hover_polyline_[index].y));
    }

    painter.setPen(QPen(QColor(120, 205, 255), 2.0, Qt::DashLine));
    for (const auto& curve : operation_preview_)
      for (std::size_t i = 1; i < curve.size(); ++i)
        painter.drawLine(QPointF(curve[i-1].x, curve[i-1].y), QPointF(curve[i].x, curve[i].y));

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
      painter.setPen(QPen(QColor(96, 220, 170), 1.4));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(QRectF(snap_point_->x - 3.0, snap_point_->y - 3.0, 6.0, 6.0));
    }

    // The same resolved strokes and terminators are consumed by hit testing.
    for (const auto& annotation : annotations_) {
      if (!annotation.layout)
        continue;
      painter.setBrush(Qt::NoBrush);
      for (const auto& stroke : annotation.layout->strokes) {
        const bool main = stroke.role == GuiDimensionStrokeRole::Dimension;
        painter.setPen(QPen(QColor(150, 190, 235, main ? 220 : 160), main ? 1.2 : .8));
        for (std::size_t i = 1; i < stroke.points.size(); ++i) {
          const auto a = stroke.points[i - 1], b = stroke.points[i];
          painter.drawLine(QPointF(a.x, a.y), QPointF(b.x, b.y));
        }
      }
      painter.setPen(QPen(QColor(150, 190, 235, 220), 1));
      painter.setBrush(QColor(150, 190, 235, 220));
      for (const auto& arrow : annotation.layout->arrows) {
        const QPointF points[3]{{arrow.polygon[0].x, arrow.polygon[0].y},
                                {arrow.polygon[1].x, arrow.polygon[1].y},
                                {arrow.polygon[2].x, arrow.polygon[2].y}};
        painter.drawPolygon(points, 3);
      }
      painter.setBrush(Qt::NoBrush);
    }

    // Selected and hovered badges remain visible above overlapping labels.
    for (int layer = 0; layer < 3; ++layer) {
      for (const auto& annotation : annotations_) {
        const bool selected = selected_annotations_.contains(annotation.semantic_id);
        const bool hovered = hovered_annotation_ == annotation.semantic_id;
        if ((selected ? 2 : hovered ? 1 : 0) != layer)
          continue;
        if (annotation.label.isEmpty())
          continue;
        const auto layout = sketch_annotation_layout(annotation.label, annotation.dimension);
        painter.setFont(layout.font);
        const QRectF badge = layout.bounds.translated(annotation.point.x, annotation.point.y);
        painter.setPen(QPen(QColor::fromRgb(appearance::accent), 1.0));
        painter.setBrush(QColor::fromRgb(appearance::panel));
        if (selected || hovered) {
          painter.setPen(QPen(palette().color(QPalette::Highlight), selected ? 2.0 : 1.5));
          if (selected)
            painter.setBrush(palette().color(QPalette::Highlight));
        }
        painter.drawRect(badge);
        painter.setPen(QColor::fromRgb(appearance::text));
        if (selected)
          painter.setPen(palette().color(QPalette::HighlightedText));
        painter.setBrush(Qt::NoBrush);
        painter.drawText(badge, Qt::AlignCenter, annotation.label);
      }
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
  std::vector<std::vector<GuiSketchScreenPoint>> selected_polylines_;
  std::vector<std::vector<GuiSketchScreenPoint>> operation_preview_;
  std::vector<AnnotationBadge> annotations_;
  std::unordered_set<std::string> selected_annotations_;
  std::string hovered_annotation_;
  std::vector<GuiSketchScreenPoint> scene_points_;
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

Quantity_Color feature_preview_color() {
  return Quantity_Color(0.25, 0.75, 1.0, Quantity_TOC_RGB);
}

bool sketch_profile_face(const geometry::ViewportSceneItem& item) {
  if (item.kind != geometry::ViewportSceneKind::SketchEntity ||
      item.semantic_id.find("/profile/") == std::string::npos)
    return false;
  const TopoDS_Shape* native = geometry::OcctShapeView::get(item.shape);
  return native != nullptr && !native->IsNull() && native->ShapeType() == TopAbs_FACE;
}

void apply_sketch_profile_style(const geometry::ViewportSceneItem& item,
                                const Handle(AIS_Shape)& interactive) {
  if (interactive.IsNull() || !sketch_profile_face(item))
    return;
  // A faint fill provides an interior hit target without obscuring the
  // authored curves or the solids behind the sketch. Its boundary stays
  // visible in every shaded mode; source-curve presentations remain on top.
  interactive->SetTransparency(0.88);
  interactive->Attributes()->SetFaceBoundaryDraw(true);
}

void apply_selection_priority(const geometry::ViewportSceneItem& item,
                              const Handle(AIS_Shape)& interactive) {
  if (interactive.IsNull())
    return;
  const Handle(SelectMgr_EntityOwner) owner = interactive->GlobalSelOwner();
  if (owner.IsNull())
    return;
  // Coplanar datum planes and axes commonly cross a Sketch at the origin.
  // Prefer the bounded profile face there so an interior click selects the
  // feature input instead of an unrelated construction datum.
  owner->SetPriority(sketch_profile_face(item) ? 9 : 0);
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

// Sketch helper roles keep their dashed/dash-dot technical drawing style.
void apply_construction_line_style(const geometry::ViewportSceneItem& item,
                                   const Handle(AIS_Shape)& interactive) {
  if (interactive.IsNull() || (item.semantic_id.rfind("construction-line/", 0U) != 0U &&
                               item.line_role == SketchLineRole::Geometry))
    return;
  const Quantity_Color color = presentation_color(item.kind);
  Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(
      color, item.line_role == SketchLineRole::Construction ? Aspect_TOL_DASH : Aspect_TOL_DOTDASH,
      1.2);
  interactive->Attributes()->SetLineAspect(aspect);
  interactive->Attributes()->SetWireAspect(aspect);
}

Error viewport_error(std::string message) {
  return Error::geometry("gui.occt_viewport", std::move(message));
}

bool belongs_to_sketch(std::string_view semantic_id, std::string_view sketch_id) {
  const std::string prefix = "sketch/" + std::string(sketch_id) + "/";
  return semantic_id.size() >= prefix.size() && semantic_id.substr(0, prefix.size()) == prefix;
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
  GuiViewportCameraBookmark fallback_camera;
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
  std::vector<Presentation> feature_previews;
  std::vector<std::string> feature_preview_replaced_bodies;
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
  setMinimumSize(360, 260);
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

void OcctViewport::set_view_changed_callback(std::function<void()> callback) {
  view_changed_callback_ = std::move(callback);
}

void OcctViewport::render_update() {
  if (view_changed_callback_)
    view_changed_callback_();
  if (gl_surface_ != nullptr)
    gl_surface_->update();
  else
    update();
}

void OcctViewport::grab_interaction_mouse() {
  if (QWidget::mouseGrabber() != this) {
    QWidget::grabMouse();
    render_update();
  }
}

void OcctViewport::release_interaction_mouse() {
  if (QWidget::mouseGrabber() == this) {
    QWidget::releaseMouse();
    render_update();
  }
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
      // Source planes describe historical feature boundaries. Highlight their
      // outline so selection cannot paint over a modified Body or its preview.
      if (item.kind == geometry::ViewportSceneKind::Face &&
          item.semantic_id.starts_with("construction-source/"))
        interactive->SetHilightMode(0);
      apply_sketch_profile_style(item, interactive);
      apply_construction_line_style(item, interactive);
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
    {
      impl_->context->Display(presentation.interactive, false);
      apply_selection_priority(presentation.item, presentation.interactive);
    }
    for (const auto& preview : impl_->feature_previews)
      if (!preview.interactive.IsNull()) {
        impl_->context->Display(preview.interactive, false);
        impl_->context->SetSelectionModeActive(preview.interactive, 0, false,
                                               AIS_SelectionModesConcurrency_Single, true);
      }
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
  // Rebuilding the 3D presentations invalidates the 3D pick state only; the
  // sketch-mode selection must survive scene refreshes (they happen between
  // multi-select clicks whenever the presentation revision moves).
  if (!impl_->context.IsNull()) {
    impl_->context->ClearSelected(false);
    render_update();
  }
  publish_selection(std::nullopt);
  if (was_empty && !impl_->presentations.empty())
    fit_all();
  else
    render_update();
  update();
  return Result<std::size_t>::success(impl_->presentations.size());
}

Result<std::size_t> OcctViewport::set_feature_preview(geometry::GeometryShape shape,
                                                      std::optional<std::string> replaced_body) {
  std::vector<std::string> replaced;
  if (replaced_body)
    replaced.push_back(std::move(*replaced_body));
  return set_feature_previews({std::move(shape)}, std::move(replaced));
}

Result<std::size_t> OcctViewport::set_feature_previews(std::vector<geometry::GeometryShape> shapes,
                                                       std::vector<std::string> replaced_bodies) {
  if (shapes.empty() && replaced_bodies.empty())
    return Result<std::size_t>::failure(
        viewport_error("feature preview requires shapes or replacements"));
  std::vector<Impl::Presentation> candidates;
  for (auto& shape : shapes) {
    if (shape.empty())
      return Result<std::size_t>::failure(viewport_error("feature preview shape is empty"));
    Impl::Presentation candidate{
        {geometry::ViewportSceneKind::SolidBody,
         "preview/interactive-feature/" + std::to_string(candidates.size()), std::move(shape)},
        {}};
    if (!impl_->context.IsNull()) {
      const auto* native = geometry::OcctShapeView::get(candidate.item.shape);
      if (!native || native->IsNull())
        return Result<std::size_t>::failure(viewport_error("OCCT feature preview shape is null"));
      candidate.interactive = new AIS_Shape(*native);
      candidate.interactive->SetColor(feature_preview_color());
      candidate.interactive->SetTransparency(0.18);
      candidate.interactive->Attributes()->SetFaceBoundaryDraw(true);
    }
    candidates.push_back(std::move(candidate));
  }
  for (const auto& previous : impl_->feature_previews)
    if (!impl_->context.IsNull() && !previous.interactive.IsNull())
      impl_->context->Remove(previous.interactive, false);
  impl_->feature_previews = std::move(candidates);
  impl_->feature_preview_replaced_bodies = std::move(replaced_bodies);
  for (const auto& preview : impl_->feature_previews)
    if (!impl_->context.IsNull() && !preview.interactive.IsNull()) {
      impl_->context->Display(preview.interactive, false);
      impl_->context->SetDisplayMode(
          preview.interactive, display_mode_ == GuiViewportDisplayMode::Wireframe ? 0 : 1, false);
      impl_->context->SetSelectionModeActive(preview.interactive, 0, false,
                                             AIS_SelectionModesConcurrency_Single, true);
    }
  apply_sketch_focus();
  update();
  return Result<std::size_t>::success(impl_->feature_previews.size());
}

bool OcctViewport::feature_preview_replaces(std::string_view semantic_id) const noexcept {
  return std::find(impl_->feature_preview_replaced_bodies.begin(),
                   impl_->feature_preview_replaced_bodies.end(),
                   semantic_id) != impl_->feature_preview_replaced_bodies.end();
}

void OcctViewport::clear_feature_preview() {
  if (impl_->feature_previews.empty() && impl_->feature_preview_replaced_bodies.empty())
    return;
  for (const auto& preview : impl_->feature_previews)
    if (!impl_->context.IsNull() && !preview.interactive.IsNull())
      impl_->context->Remove(preview.interactive, false);
  impl_->feature_previews.clear();
  impl_->feature_preview_replaced_bodies.clear();
  apply_sketch_focus();
  update();
}

void OcctViewport::clear_scene() {
  if (!impl_->context.IsNull()) {
    impl_->context->RemoveAll(false);
    render_update();
  }
  impl_->presentations.clear();
  impl_->feature_previews.clear();
  impl_->feature_preview_replaced_bodies.clear();
  apply_sketch_focus();
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
  // An active planar Sketch owns the camera orientation and projection.  Keep
  // it orthographic even when a global Perspective action is triggered.
  if (sketch_focus_active() && projection == GuiViewportProjection::Perspective)
    return;
  projection_ = projection;
  if (!impl_->view.IsNull()) {
    impl_->view->Camera()->SetProjectionType(projection == GuiViewportProjection::Perspective
                                                 ? Graphic3d_Camera::Projection_Perspective
                                                 : Graphic3d_Camera::Projection_Orthographic);
  }
  render_update();
  rebuild_sketch_grid();
}

void OcctViewport::set_standard_view(GuiStandardView view) {
  // Standard model views would discard the normal-to-plane Sketch camera and
  // re-enable free rotation.  They become effective again after Sketch focus
  // is cleared.
  if (sketch_focus_active())
    return;
  auto camera = camera_bookmark();
  plane_camera_.reset();
  if (!impl_->view.IsNull()) {
    impl_->view->SetProj(occt_orientation(view));
    fit_all();
  } else {
    // Keep headless camera mappings consistent with OCCT's +Z-up views.
    // SetProj otherwise has no effect without a native viewer.
    const auto direction = V3d::GetProjAxis(occt_orientation(view));
    const double distance =
        std::hypot(camera.eye.x - camera.target.x, camera.eye.y - camera.target.y,
                   camera.eye.z - camera.target.z);
    camera.eye = {camera.target.x + distance * direction.X(),
                  camera.target.y + distance * direction.Y(),
                  camera.target.z + distance * direction.Z()};
    Vector3 up = view == GuiStandardView::Top      ? Vector3{0, 1, 0}
                 : view == GuiStandardView::Bottom ? Vector3{0, -1, 0}
                                                   : Vector3{0, 0, 1};
    const double alignment = up.x * direction.X() + up.y * direction.Y() + up.z * direction.Z();
    up = {up.x - alignment * direction.X(), up.y - alignment * direction.Y(),
          up.z - alignment * direction.Z()};
    const double length = std::hypot(up.x, up.y, up.z);
    camera.up_direction = {up.x / length, up.y / length, up.z / length};
    camera.plane_camera.reset();
    impl_->fallback_camera = camera;
    render_update();
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
  auto bookmark = impl_->fallback_camera;
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
      // Perspective SetScale changes eye distance. Restore the projection
      // first so an orthographic snapshot keeps its captured eye position.
      impl_->view->Camera()->SetProjectionType(projection_ == GuiViewportProjection::Perspective
                                                   ? Graphic3d_Camera::Projection_Perspective
                                                   : Graphic3d_Camera::Projection_Orthographic);
      impl_->view->SetEye(bookmark.eye.x, bookmark.eye.y, bookmark.eye.z);
      impl_->view->SetAt(bookmark.target.x, bookmark.target.y, bookmark.target.z);
      impl_->view->SetUp(bookmark.up_direction.x, bookmark.up_direction.y,
                         bookmark.up_direction.z);
      impl_->view->SetScale(bookmark.scale);
    }
    impl_->fallback_camera = bookmark;
    render_update();
    rebuild_sketch_grid();
    return true;
  } catch (const Standard_Failure&) {
    return false;
  }
}

Result<GuiSketchViewRay>
OcctViewport::screen_to_view_ray(GuiSketchScreenPoint point) const {
  if (!std::isfinite(point.x) || !std::isfinite(point.y))
    return Result<GuiSketchViewRay>::failure(
        Error::validation("gui.occt_viewport", "screen point must be finite"));
  if (!impl_->view.IsNull() && !impl_->view->Window().IsNull()) {
    try {
      const qreal ratio =
          gl_surface_ != nullptr ? gl_surface_->devicePixelRatioF() : devicePixelRatioF();
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
                             : "could not convert viewport pixel to view ray"));
    }
  }
  auto mapping = GuiViewportManipulatorMapping::create_camera(
      camera_bookmark(), static_cast<double>(std::max(1, width())),
      static_cast<double>(std::max(1, height())));
  if (mapping.has_error())
    return Result<GuiSketchViewRay>::failure(mapping.error());
  return mapping.value().screen_to_ray(point);
}

Result<GuiSketchScreenPoint> OcctViewport::model_to_screen(Point3 point) const {
  if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
    return Result<GuiSketchScreenPoint>::failure(
        Error::validation("gui.occt_viewport", "model point must be finite"));
  if (!impl_->view.IsNull() && !impl_->view->Window().IsNull()) {
    try {
      // Preserve subpixel axis differences for manipulator directions. The
      // integer V3d_View::Convert overload can collapse distinct model points.
      const auto projected = impl_->view->Camera()->Project(gp_Pnt(point.x, point.y, point.z));
      int width{}, height{};
      impl_->view->Window()->Size(width, height);
      const qreal ratio =
          gl_surface_ != nullptr ? gl_surface_->devicePixelRatioF() : devicePixelRatioF();
      return Result<GuiSketchScreenPoint>::success(
          {(projected.X() + 1) * width / (2 * ratio), (1 - projected.Y()) * height / (2 * ratio)});
    } catch (const Standard_Failure& failure) {
      return Result<GuiSketchScreenPoint>::failure(
          viewport_error(failure.GetMessageString() != nullptr
                             ? failure.GetMessageString()
                             : "could not convert model point to viewport pixel"));
    }
  }
  auto mapping = GuiViewportManipulatorMapping::create_camera(
      camera_bookmark(), static_cast<double>(std::max(1, width())),
      static_cast<double>(std::max(1, height())));
  if (mapping.has_error())
    return Result<GuiSketchScreenPoint>::failure(mapping.error());
  return mapping.value().model_to_screen(point);
}

void OcctViewport::set_sketch_focus(std::string sketch_id, GuiSketchSurroundingsMode mode) {
  sketch_focus_id_ = std::move(sketch_id);
  sketch_surroundings_mode_ = mode;
  if (sketch_focus_active())
    set_projection(GuiViewportProjection::Orthographic);
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
    if (!impl_->view.IsNull() && !impl_->view->Window().IsNull()) {
      GuiSketchPlaneMapping::ScreenToRay screen_to_ray =
          [this](GuiSketchScreenPoint point) { return this->screen_to_view_ray(point); };
      GuiSketchPlaneMapping::ModelToScreen model_to_screen =
          [this](Point3 point) { return this->model_to_screen(point); };
      return GuiSketchPlaneMapping::create(std::move(plane), std::move(screen_to_ray),
                                           std::move(model_to_screen));
    }
    const Point2 center = plane.model_to_plane(plane_camera_ ? plane_camera_->target : plane.origin);
    return GuiSketchPlaneMapping::create_orthographic(
        std::move(plane), std::max(1, width()), std::max(1, height()), center, 1.0);
  }();
  if (mapping.has_error())
    return Result<std::size_t>::failure(mapping.error());
  for (auto& annotation : scene.annotations) {
    if (annotation.label.empty())
      continue;
    const auto bounds = sketch_annotation_layout(QString::fromStdString(annotation.label),
                                                 annotation.hit_kind == GuiSketchHitKind::Dimension)
                            .bounds.adjusted(-1.0, -1.0, 1.0, 1.0);
    annotation.label_bounds_dip =
        GuiSketchScreenRect{{bounds.left(), bounds.top()}, {bounds.right(), bounds.bottom()}};
  }
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

bool OcctViewport::select_sketch_dimension_at(GuiSketchScreenPoint point) {
  if (!sketch_interaction_ || !sketch_selection_enabled_)
    return false;
  auto hits = sketch_interaction_->hits_at(point);
  if (!hits || hits.value().empty() || hits.value().front().kind != GuiSketchHitKind::Dimension ||
      !hits.value().front().label_hit)
    return false;
  sketch_box_active_ = false;
  sketch_box_selection_.reset();
  static_cast<SketchInteractionOverlay*>(sketch_overlay_)->set_box(std::nullopt);
  sketch_selections_ = {{GuiSelectionKind::SketchEntity, hits.value().front().semantic_id}};
  publish_sketch_selection();
  return true;
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
  dimension_preview_.reset();
  sketch_operation_preview_.clear();
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

void OcctViewport::set_sketch_operation_preview(std::vector<std::vector<Point2>> curves) {
  sketch_operation_preview_ = std::move(curves);
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

bool OcctViewport::selection_allowed(const GuiSelection& selection) const noexcept {
  return (selection_filter_mask_ & selection_kind_bit(selection.kind)) != 0U &&
         (!selection_semantic_scope_ ||
          std::find(selection_semantic_scope_->begin(), selection_semantic_scope_->end(),
                    selection.semantic_id) != selection_semantic_scope_->end());
}

void OcctViewport::set_selection_semantic_scope(std::optional<std::vector<std::string>> scope) {
  selection_semantic_scope_ = std::move(scope);
  apply_selection_filters();
  if (selected_semantic_ && !selection_allowed(*selected_semantic_))
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
    if (known && selection_allowed({GuiSelectionKind::SketchEntity, std::string(semantic_id)})) {
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
  if (!selection_allowed(selection))
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

std::size_t OcctViewport::feature_preview_count() const noexcept {
  return impl_->feature_previews.size();
}

bool OcctViewport::feature_preview_active() const noexcept {
  if (impl_->feature_previews.empty())
    return false;
  if (impl_->context.IsNull())
    return true;
  return std::all_of(
      impl_->feature_previews.begin(), impl_->feature_previews.end(), [&](const auto& preview) {
        return !preview.interactive.IsNull() && impl_->context->IsDisplayed(preview.interactive);
      });
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

Result<std::vector<GuiSketchHit>> OcctViewport::sketch_curve_hits_along(
    GuiSketchScreenPoint start, GuiSketchScreenPoint end) const {
  if (!sketch_interaction_)
    return Result<std::vector<GuiSketchHit>>::failure(
        Error::validation("gui.occt_viewport", "Sketch interaction is not active"));
  return sketch_interaction_->curve_hits_along(start, end);
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
  painter.fillRect(rect(), QColor::fromRgb(appearance::canvas));
  painter.setPen(Qt::white);
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
          apply_selection_set(sketch_selections_, selections.value(),
                              event->modifiers() | (sketch_additive_selection_ ? Qt::ControlModifier
                                                                               : Qt::NoModifier));
      } else if (!sketch_box_active_) {
        auto hit = sketch_interaction_->cycle_hit(current);
        if (hit && hit.value()) {
          // hit.semantic_id already carries the endpoint/midpoint role via the
          // hit-role marker; the GuiSelection constructor splits it back into
          // semantic_id + candidate_id. (Passing candidate_id here instead would
          // suppress that split and leave the marker embedded.)
          apply_selection_set(sketch_selections_,
                              {{GuiSelectionKind::SketchEntity, hit.value()->semantic_id}},
                              event->modifiers() | (sketch_additive_selection_ ? Qt::ControlModifier
                                                                               : Qt::NoModifier));
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
      // OCCT may rank a datum plane/axis ahead of a coplanar translucent
      // Sketch face, especially where all origin planes cross. Inspect the
      // complete hit stack and prefer a bounded profile face whenever one is
      // under the pointer; otherwise preserve OCCT's normal nearest-hit rule.
      Handle(SelectMgr_EntityOwner) profile_owner;
      std::optional<GuiSelection> profile_selection;
      impl_->context->InitDetected();
      for (; impl_->context->MoreDetected(); impl_->context->NextDetected()) {
        const Handle(SelectMgr_EntityOwner) owner =
            impl_->context->DetectedCurrentOwner();
        if (owner.IsNull())
          continue;
        const Handle(AIS_InteractiveObject) interactive =
            Handle(AIS_InteractiveObject)::DownCast(owner->Selectable());
        if (interactive.IsNull())
          continue;
        const auto found = impl_->owner_to_index.find(interactive.get());
        if (found == impl_->owner_to_index.end())
          continue;
        const auto& item = impl_->presentations[found->second].item;
        if (!sketch_profile_face(item))
          continue;
        GuiSelection selection{selection_kind(item.kind), item.semantic_id};
        if (!selection_allowed(selection))
          continue;
        profile_owner = owner;
        profile_selection = std::move(selection);
        break;
      }
      if (!profile_owner.IsNull()) {
        impl_->context->SetSelected(profile_owner, false);
        publish_selection(std::move(profile_selection));
      } else {
        impl_->context->SelectDetected(AIS_SelectionScheme_Replace);
        publish_selection(detected_selection());
      }
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
    const QColor background = QColor::fromRgb(appearance::canvas);
    impl_->view->SetBackgroundColor(Quantity_Color(background.redF(), background.greenF(),
                                                   background.blueF(), Quantity_TOC_RGB));
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
      apply_sketch_profile_style(presentation.item, presentation.interactive);
      apply_construction_line_style(presentation.item, presentation.interactive);
      impl_->context->Display(presentation.interactive, false);
      apply_selection_priority(presentation.item, presentation.interactive);
      impl_->owner_to_index.emplace(presentation.interactive.get(), index);
    }
    for (auto& preview : impl_->feature_previews) {
      const TopoDS_Shape* native = geometry::OcctShapeView::get(preview.item.shape);
      if (native != nullptr && !native->IsNull()) {
        preview.interactive = new AIS_Shape(*native);
        preview.interactive->SetColor(feature_preview_color());
        preview.interactive->SetTransparency(0.18);
        preview.interactive->Attributes()->SetFaceBoundaryDraw(true);
        impl_->context->Display(preview.interactive, false);
        impl_->context->SetSelectionModeActive(preview.interactive, 0, false,
                                               AIS_SelectionModesConcurrency_Single, true);
      }
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
    // A Sketch may have been entered before the top-level window was shown
    // (tests, restored sessions, or deferred docks). Apply the stored plane
    // camera now that OCCT has a real window, then replace the temporary
    // 1 mm/DIP mapping with callbacks backed by the authoritative view.
    if (plane_camera_.has_value()) {
      const GuiPlaneCamera camera = *plane_camera_;
      (void)set_plane_camera(camera.target, camera.view_direction, camera.up_direction);
    }
    if (sketch_interaction_) {
      const auto plane = sketch_interaction_->mapping().plane();
      const auto scene = sketch_interaction_->scene();
      const auto config = sketch_interaction_->config();
      (void)set_sketch_interaction(plane, scene, config);
    }
  } catch (const Standard_Failure& failure) {
    impl_->initialization_error = failure.GetMessageString() != nullptr
                                      ? failure.GetMessageString()
                                      : "OCCT viewport initialization failed";
    for (auto& presentation : impl_->presentations)
      presentation.interactive.Nullify();
    for (auto& preview : impl_->feature_previews)
      preview.interactive.Nullify();
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
    const bool profile_face = sketch_profile_face(presentation.item);
    presentation.interactive->Attributes()->SetFaceBoundaryDraw(
        profile_face || display_mode_ == GuiViewportDisplayMode::ShadedWithEdges);
    impl_->context->SetDisplayMode(presentation.interactive, datum ? 0 : mode, false);
  }
  for (const auto& preview : impl_->feature_previews)
    if (!preview.interactive.IsNull()) {
      preview.interactive->Attributes()->SetFaceBoundaryDraw(
          display_mode_ == GuiViewportDisplayMode::ShadedWithEdges);
      impl_->context->SetDisplayMode(preview.interactive, mode, false);
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
    impl_->context->SetSelectionModeActive(presentation.interactive, 0,
                                           selection_allowed({kind, presentation.item.semantic_id}),
                                           AIS_SelectionModesConcurrency_Single, true);
    apply_selection_priority(presentation.item, presentation.interactive);
  }
  render_update();
}

void OcctViewport::apply_sketch_focus() {
  if (impl_->context.IsNull())
    return;
  for (auto& presentation : impl_->presentations) {
    if (presentation.interactive.IsNull())
      continue;
    const bool profile_face = sketch_profile_face(presentation.item);
    if (feature_preview_replaces(presentation.item.semantic_id)) {
      impl_->context->Erase(presentation.interactive, false);
      continue;
    }

    if (!sketch_focus_active()) {
      impl_->context->Display(presentation.interactive, false);
      presentation.interactive->SetTransparency(
          presentation.item.kind == geometry::ViewportSceneKind::Datum ? 0.72
          : profile_face || presentation.item.kind == geometry::ViewportSceneKind::AssemblyTarget ||
                  (presentation.item.kind == geometry::ViewportSceneKind::Face &&
                   (presentation.item.semantic_id.starts_with("finishing/") ||
                    presentation.item.semantic_id.starts_with("measure/") ||
                    presentation.item.semantic_id.starts_with("construction-source/")))
              ? 0.88
              : 0.0);
      continue;
    }

    const bool active_sketch = belongs_to_sketch(presentation.item.semantic_id, sketch_focus_id_);
    if (active_sketch) {
      impl_->context->Display(presentation.interactive, false);
      presentation.interactive->SetTransparency(profile_face ? 0.88 : 0.0);
    } else if (sketch_surroundings_mode_ == GuiSketchSurroundingsMode::Isolate) {
      impl_->context->Erase(presentation.interactive, false);
    } else {
      impl_->context->Display(presentation.interactive, false);
      presentation.interactive->SetTransparency(
          presentation.item.kind == geometry::ViewportSceneKind::Datum
              ? 0.88
              : profile_face ? 0.96 : 0.72);
    }
  }
  for (const auto& preview : impl_->feature_previews)
    if (!preview.interactive.IsNull()) {
      if (sketch_focus_active())
        impl_->context->Erase(preview.interactive, false);
      else
        impl_->context->Display(preview.interactive, false);
      // Visibility changes must never turn a disposable result into a pick owner.
      impl_->context->SetSelectionModeActive(preview.interactive, 0, false,
                                             AIS_SelectionModesConcurrency_Single, true);
    }
  // Display() can reactivate an object's default selection mode. Apply the
  // capability filter after every visibility change, including preview updates.
  apply_selection_filters();
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
  // Every screen-space overlay must follow camera changes (zoom/pan/rotate);
  // this is called on each of them, so re-project the rest here too.
  rebuild_sketch_drag_handles();
  rebuild_sketch_preview_polyline();
  rebuild_sketch_selection_highlight();
  rebuild_sketch_annotations();
  rebuild_sketch_scene_points();
}

void OcctViewport::rebuild_sketch_scene_points() {
  auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_);
  if (overlay == nullptr)
    return;
  std::vector<GuiSketchScreenPoint> points;
  if (sketch_interaction_) {
    for (const auto& point : sketch_interaction_->scene().points) {
      // Endpoints and centers are permanent visual anchors (Inventor look);
      // midpoints/quadrants stay snap-only to avoid clutter.
      if (point.snap_kind != GuiSketchSnapKind::Endpoint &&
          point.snap_kind != GuiSketchSnapKind::Center)
        continue;
      auto screen = sketch_interaction_->mapping().plane_to_screen(point.point);
      if (screen)
        points.push_back(screen.value());
    }
  }
  overlay->set_scene_points(std::move(points));
}

void OcctViewport::set_sketch_dimension_preview(
    std::optional<GuiSketchAnnotationPrimitive> annotation) {
  dimension_preview_ = std::move(annotation);
  rebuild_sketch_annotations();
}

Result<GuiDimensionScreenLayout>
OcctViewport::sketch_dimension_screen_layout(const GuiSketchAnnotationPrimitive& annotation) const {
  if (!sketch_interaction_)
    return Result<GuiDimensionScreenLayout>::failure(Error::validation(
        annotation.semantic_id, "Dimension projection requires an active Sketch mapping"));
  auto displayed = annotation;
  if (!displayed.label_bounds_dip) {
    const auto bounds = sketch_annotation_layout(QString::fromStdString(annotation.label), true)
                            .bounds.adjusted(-1, -1, 1, 1);
    displayed.label_bounds_dip =
        GuiSketchScreenRect{{bounds.left(), bounds.top()}, {bounds.right(), bounds.bottom()}};
  }
  return resolve_dimension_screen_layout(displayed, sketch_interaction_->mapping());
}

void OcctViewport::rebuild_sketch_annotations() {
  auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_);
  if (overlay == nullptr)
    return;
  std::vector<SketchInteractionOverlay::AnnotationBadge> badges;
  if (sketch_interaction_) {
    auto annotations = sketch_interaction_->scene().annotations;
    if (dimension_preview_)
      annotations.push_back(*dimension_preview_);
    for (const auto& annotation : annotations) {
      if (annotation.label.empty())
        continue;
      auto screen = sketch_interaction_->mapping().plane_to_screen(annotation.point);
      if (!screen)
        continue;
      std::optional<GuiDimensionScreenLayout> layout;
      if (annotation.hit_kind == GuiSketchHitKind::Dimension) {
        auto resolved = sketch_dimension_screen_layout(annotation);
        if (resolved) {
          screen.value() = resolved.value().label_anchor;
          layout = std::move(resolved.value());
        }
      }
      badges.push_back({screen.value(), QString::fromStdString(annotation.label),
                        annotation.hit_kind == GuiSketchHitKind::Dimension, std::move(layout),
                        annotation.semantic_id});
    }
  }
  overlay->set_annotations(std::move(badges));
}

void OcctViewport::rebuild_sketch_selection_highlight() {
  auto* overlay = static_cast<SketchInteractionOverlay*>(sketch_overlay_);
  if (overlay == nullptr)
    return;
  std::vector<std::vector<GuiSketchScreenPoint>> polylines;
  std::unordered_set<std::string> annotations;
  if (sketch_interaction_) {
    for (const auto& selection : sketch_selections_) {
      annotations.insert(selection.semantic_id);
      const auto curve = std::find_if(
          sketch_interaction_->scene().curves.begin(), sketch_interaction_->scene().curves.end(),
          [&selection](const auto& item) { return item.semantic_id == selection.semantic_id; });
      if (curve == sketch_interaction_->scene().curves.end())
        continue;
      auto screen = sketch_interaction_->screen_polyline(*curve);
      if (screen)
        polylines.push_back(std::move(screen.value()));
    }
  }
  overlay->set_selected(std::move(polylines));
  overlay->set_selected_annotations(std::move(annotations));
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
    overlay->set_operation_preview({});
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
  std::vector<std::vector<GuiSketchScreenPoint>> curves;
  for (const auto& curve : sketch_operation_preview_) {
    std::vector<GuiSketchScreenPoint> points;
    for (auto point : curve) {
      auto screen = sketch_interaction_->mapping().plane_to_screen(point);
      if (screen) points.push_back(screen.value());
    }
    curves.push_back(std::move(points));
  }
  overlay->set_operation_preview(std::move(curves));
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
    overlay->set_hovered_annotation(hovered_sketch_hit_ && hovered_sketch_hit_->label_hit
                                        ? hovered_sketch_hit_->semantic_id
                                        : std::string{});
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
    // Show the snap marker only for meaningful geometric snaps; a marker that
    // permanently follows the cursor (grid snap is almost always active) reads
    // as a "dragging square" instead of a snap cue.
    const bool meaningful_snap = sketch_snap_result_->kind != GuiSketchSnapKind::None &&
                                 sketch_snap_result_->kind != GuiSketchSnapKind::Grid;
    auto snap_screen = sketch_interaction_->mapping().plane_to_screen(
        sketch_snap_result_->snapped_point);
    overlay->set_snap(meaningful_snap && snap_screen
                          ? std::optional<GuiSketchScreenPoint>{snap_screen.value()}
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
  rebuild_sketch_selection_highlight();
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
  if (!selection_allowed(selection))
    return std::nullopt;
  return selection;
}

} // namespace blcad::gui
