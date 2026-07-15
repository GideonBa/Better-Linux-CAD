#include "blcad/gui/occt_viewport.hpp"

#include "blcad/geometry/occt_shape_view.hpp"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Graphic3d_Camera.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Quantity_Color.hxx>
#include <Standard_Failure.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <Xw_Window.hxx>
#include <gp_Dir.hxx>

#include <QApplication>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace blcad::gui {
namespace {

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
  Handle(Xw_Window) window;
  std::vector<Presentation> presentations;
  std::unordered_map<const AIS_InteractiveObject*, std::size_t> owner_to_index;
  std::string initialization_error;
  bool initialization_attempted{false};
};

OcctViewport::OcctViewport(QWidget* parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
  setObjectName(QStringLiteral("blcad.occt_viewport"));
  setAccessibleName(QStringLiteral("BLCAD 3D viewport"));
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setMinimumSize(480, 320);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_NoSystemBackground);
  setAutoFillBackground(false);
}

OcctViewport::~OcctViewport() = default;

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
      if (item.kind == geometry::ViewportSceneKind::Datum)
        interactive->SetTransparency(0.72);
    }
    candidate.push_back({std::move(item), std::move(interactive)});
  }

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
  clear_selection();
  fit_all();
  update();
  return Result<std::size_t>::success(impl_->presentations.size());
}

void OcctViewport::clear_scene() {
  if (!impl_->context.IsNull())
    impl_->context->RemoveAll(true);
  impl_->presentations.clear();
  impl_->owner_to_index.clear();
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
    impl_->view->Redraw();
  }
}

void OcctViewport::set_standard_view(GuiStandardView view) {
  plane_camera_.reset();
  if (!impl_->view.IsNull()) {
    impl_->view->SetProj(occt_orientation(view));
    fit_all();
  }
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
      impl_->view->Redraw();
    }
    return true;
  } catch (const Standard_Failure&) {
    return false;
  }
}

void OcctViewport::fit_all() {
  if (!impl_->view.IsNull() && !impl_->presentations.empty()) {
    impl_->view->FitAll(0.05, false);
    impl_->view->ZFitAll();
    impl_->view->Redraw();
  }
}

void OcctViewport::set_selection_filter_mask(std::uint32_t mask) {
  selection_filter_mask_ = mask;
  apply_selection_filters();
  if (selected_semantic_.has_value() && (mask & selection_kind_bit(selected_semantic_->kind)) == 0U)
    clear_selection();
}

void OcctViewport::set_selection_callback(
    std::function<void(std::optional<GuiSelection>)> callback) {
  selection_callback_ = std::move(callback);
}

bool OcctViewport::select_semantic(std::string_view semantic_id) {
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
    impl_->context->AddOrRemoveSelected(found->interactive, true);
  }
  publish_selection(std::move(selection));
  return true;
}

void OcctViewport::clear_selection() {
  if (!impl_->context.IsNull())
    impl_->context->ClearSelected(true);
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

bool OcctViewport::native_viewer_available() const noexcept {
  return !impl_->view.IsNull();
}

const std::string& OcctViewport::initialization_error() const noexcept {
  return impl_->initialization_error;
}

QPaintEngine* OcctViewport::paintEngine() const {
  return impl_->view.IsNull() ? QWidget::paintEngine() : nullptr;
}

void OcctViewport::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  initialize_native_viewer();
}

void OcctViewport::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)
  if (!impl_->view.IsNull()) {
    impl_->view->Redraw();
    return;
  }
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
  if (!impl_->view.IsNull())
    impl_->view->MustBeResized();
}

void OcctViewport::mousePressEvent(QMouseEvent* event) {
  last_mouse_position_ = event->position().toPoint();
  if (event->button() == Qt::MiddleButton)
    panning_ = true;
  if (event->button() == Qt::RightButton && !impl_->view.IsNull()) {
    rotating_ = true;
    impl_->view->StartRotation(last_mouse_position_.x(), last_mouse_position_.y());
  }
  QWidget::mousePressEvent(event);
}

void OcctViewport::mouseMoveEvent(QMouseEvent* event) {
  const QPoint current = event->position().toPoint();
  if (!impl_->view.IsNull() && panning_) {
    const QPoint delta = current - last_mouse_position_;
    impl_->view->Pan(delta.x(), -delta.y(), 1.0, false);
  } else if (!impl_->view.IsNull() && rotating_) {
    impl_->view->Rotation(current.x(), current.y());
  } else if (!impl_->context.IsNull()) {
    impl_->context->MoveTo(current.x(), current.y(), impl_->view, true);
  }
  last_mouse_position_ = current;
  QWidget::mouseMoveEvent(event);
}

void OcctViewport::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton)
    panning_ = false;
  if (event->button() == Qt::RightButton)
    rotating_ = false;
  if (event->button() == Qt::LeftButton && !impl_->context.IsNull()) {
    const QPoint current = event->position().toPoint();
    impl_->context->MoveTo(current.x(), current.y(), impl_->view, true);
    if (impl_->context->HasDetected()) {
      impl_->context->SelectDetected(AIS_SelectionScheme_Replace);
      publish_selection(detected_selection());
    } else {
      clear_selection();
    }
  }
  QWidget::mouseReleaseEvent(event);
}

void OcctViewport::wheelEvent(QWheelEvent* event) {
  if (!impl_->view.IsNull()) {
    const QPoint point = event->position().toPoint();
    const int step = std::clamp(event->angleDelta().y() / 4, -120, 120);
    impl_->view->Zoom(point.x(), point.y(), point.x(), point.y() + step);
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
    impl_->graphic_driver = new OpenGl_GraphicDriver(impl_->display_connection);
    impl_->viewer = new V3d_Viewer(impl_->graphic_driver);
    impl_->viewer->SetDefaultLights();
    impl_->viewer->SetLightOn();
    impl_->context = new AIS_InteractiveContext(impl_->viewer);
    impl_->view = impl_->viewer->CreateView();
    impl_->window = new Xw_Window(impl_->display_connection, static_cast<Aspect_Drawable>(winId()));
    impl_->view->SetWindow(impl_->window);
    if (!impl_->window->IsMapped())
      impl_->window->Map();
    impl_->view->SetBackgroundColor(Quantity_Color(0.08, 0.10, 0.13, Quantity_TOC_RGB));
    impl_->owner_to_index.clear();
    for (std::size_t index = 0; index < impl_->presentations.size(); ++index) {
      auto& presentation = impl_->presentations[index];
      const TopoDS_Shape* native = geometry::OcctShapeView::get(presentation.item.shape);
      if (native == nullptr || native->IsNull())
        continue;
      presentation.interactive = new AIS_Shape(*native);
      presentation.interactive->SetColor(presentation_color(presentation.item.kind));
      if (presentation.item.kind == geometry::ViewportSceneKind::Datum)
        presentation.interactive->SetTransparency(0.72);
      impl_->context->Display(presentation.interactive, false);
      impl_->owner_to_index.emplace(presentation.interactive.get(), index);
    }
    set_projection(projection_);
    apply_display_mode();
    apply_selection_filters();
    impl_->view->MustBeResized();
  } catch (const Standard_Failure& failure) {
    impl_->initialization_error = failure.GetMessageString() != nullptr
                                      ? failure.GetMessageString()
                                      : "OCCT viewport initialization failed";
    impl_->view.Nullify();
    impl_->context.Nullify();
  }
  update();
}

void OcctViewport::apply_display_mode() {
  if (impl_->context.IsNull())
    return;
  const int mode = display_mode_ == GuiViewportDisplayMode::Wireframe ? 0 : 1;
  for (auto& presentation : impl_->presentations) {
    if (presentation.interactive.IsNull())
      continue;
    presentation.interactive->Attributes()->SetFaceBoundaryDraw(
        display_mode_ == GuiViewportDisplayMode::ShadedWithEdges);
    impl_->context->SetDisplayMode(presentation.interactive, mode, false);
  }
  impl_->context->UpdateCurrentViewer();
}

void OcctViewport::apply_selection_filters() {
  if (impl_->context.IsNull())
    return;
  for (auto& presentation : impl_->presentations) {
    if (presentation.interactive.IsNull())
      continue;
    const GuiSelectionKind kind = selection_kind(presentation.item.kind);
    impl_->context->SetSelectionModeActive(
        presentation.interactive, 0, (selection_filter_mask_ & selection_kind_bit(kind)) != 0U,
        AIS_SelectionModesConcurrency_Single, true);
  }
  impl_->context->UpdateCurrentViewer();
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
