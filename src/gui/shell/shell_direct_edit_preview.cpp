#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "direct_edit_tools_impl.hpp"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
namespace blcad::gui {
namespace {
Error error(const std::string& text) {
  return Error::validation("part.direct_edit", text);
}
} // namespace
Result<std::size_t> ShellDirectEditTools::Impl::show_inputs(const geometry::ShapeCache* result) {
  if (!inputs || !cache)
    return Result<std::size_t>::failure(error("Input history is unavailable."));
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  for (const auto& b : inputs->bodies()) {
    replaced.push_back("body/" + b.id().value());
    if (b.visibility() == BodyVisibility::Visible)
      if (const auto* shape = (result ? result : &*cache)->find_body_shape(b.id()))
        shapes.push_back(*shape);
  }
  return window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
}
void ShellDirectEditTools::Impl::overlay_refresh() {
  auto mapping = GuiViewportManipulatorMapping::create(
      [this](GuiSketchScreenPoint p) { return window.viewport().screen_to_view_ray(p); },
      [this](Point3 p) { return window.viewport().model_to_screen(p); });
  if (mapping)
    (void)layer.set_mapping(std::move(mapping.value()));
  overlay->setGeometry(window.viewport().rect());
  overlay->setVisible(active && !layer.handles().empty());
  overlay->raise();
  overlay->update();
}
void ShellDirectEditTools::Impl::grips() {
  if (!layer.active_handle_id().empty())
    return;
  std::vector<GuiViewportManipulatorHandle> handles;
  grip_fields.clear();
  const auto f = frame();
  const Vector3 z = cross(f.x_axis, f.y_axis);
  const auto world = [&](Vector3 v) {
    return Vector3{f.x_axis.x * v.x + f.y_axis.x * v.y + z.x * v.z,
                   f.x_axis.y * v.x + f.y_axis.y * v.y + z.y * v.z,
                   f.x_axis.z * v.x + f.y_axis.z * v.y + z.z * v.z};
  };
  const auto add = [&](std::string id, QLineEdit* field, Point3 origin, Vector3 axis,
                       bool angular = false, double factor = 1) {
    GuiViewportManipulatorHandle h;
    h.id = "direct_edit/" + id;
    h.origin = origin;
    h.axis = axis;
    h.reference_value = field->text().toDouble();
    h.kind = angular ? GuiViewportManipulatorKind::Angular : GuiViewportManipulatorKind::Linear;
    h.minimum_value = mode->currentIndex() == 3 ? 1e-6 : angular ? -89.999 : -1e9;
    h.maximum_value = angular ? 89.999 : 1e9;
    const auto unit = normalized(axis);
    h.reference_direction =
        normalized(cross(std::abs(unit.x) < .8 ? Vector3{1, 0, 0} : Vector3{0, 1, 0}, unit));
    h.model_units_per_value = factor;
    h.interactive = !field->isReadOnly();
    h.label = id;
    grip_fields[h.id] = field;
    handles.push_back(std::move(h));
  };
  if (mode->currentIndex() == 0) {
    add("x", move[0].value, f.origin_mm, f.x_axis);
    add("y", move[1].value, f.origin_mm, f.y_axis);
    add("z", move[2].value, f.origin_mm, z);
  } else if (mode->currentIndex() == 1) {
    add("angle", angle.value, f.origin_mm,
        world({axis[0]->text().toDouble(), axis[1]->text().toDouble(), axis[2]->text().toDouble()}),
        true);
  } else if (mode->currentIndex() == 2 || mode->currentIndex() == 3) {
    for (const auto& target : targets)
      if (selected->count() && QString::fromStdString(interactive_face_identity(
                                   std::get<FaceReference>(target.selection.reference))) ==
                                   selected->item(0)->data(Qt::UserRole).toString()) {
        add("size", size.value, target.anchor, target.normal, false,
            mode->currentIndex() == 3 && measurement->currentIndex() == 1 ? .5 : 1);
        break;
      }
  }
  (void)layer.set_handles(std::move(handles));
  overlay_refresh();
}
void ShellDirectEditTools::Impl::fail(const Error& e) {
  valid = false;
  apply->setEnabled(false);
  status->setText(QString::fromStdString(e.message()));
  (void)show_inputs();
  if (!layer.active_handle_id().empty())
    return;
  std::vector<GuiViewportManipulatorHandle> markers;
  for (const auto& target : targets)
    for (int row = 0; row < selected->count(); ++row)
      if (QString::fromStdString(interactive_face_identity(std::get<FaceReference>(
              target.selection.reference))) == selected->item(row)->data(Qt::UserRole).toString()) {
        GuiViewportManipulatorHandle marker;
        marker.id = "direct_edit/failure/" + std::to_string(row);
        marker.origin = target.anchor;
        marker.axis = target.normal;
        marker.interactive = false;
        marker.label = "Failed edit / selected face";
        markers.push_back(marker);
      }
  (void)layer.set_handles(std::move(markers));
  overlay_refresh();
}
Result<std::size_t> ShellDirectEditTools::preview() {
  auto& p = *impl_;
  if (!p.current())
    return Result<std::size_t>::failure(error("The model changed. Reopen Direct Edit."));
  p.valid = false;
  p.apply->setEnabled(false);
  auto candidate = p.candidate();
  if (!candidate) {
    p.fail(candidate.error());
    return Result<std::size_t>::failure(candidate.error());
  }
  auto cache = geometry::ShapeCache::create(ShapeCacheId("direct_edit.preview"));
  if (!cache)
    return Result<std::size_t>::failure(cache.error());
  auto built = query_with_step_import_context(p.window.session(), [&](auto* context) {
    return geometry::GeometryRecomputeExecutor(context).execute_document(candidate.value(),
                                                                         cache.value());
  });
  if (!built) {
    p.fail(built.error());
    return Result<std::size_t>::failure(built.error());
  }
  auto shown = p.show_inputs(&cache.value());
  if (!shown) {
    p.fail(shown.error());
    return shown;
  }
  p.valid = true;
  p.apply->setEnabled(true);
  p.grips();
  (void)p.window.session().task().show_preview();
  auto history = candidate.value().executable_history();
  p.status->setText(QString("Valid solid and complete history. %1 downstream feature(s) "
                            "checked. Apply publishes one Undo step.")
                        .arg(history ? history.value().size() - p.prefix - 1 : 0));
  return Result<std::size_t>::success(1);
}
void ShellDirectEditTools::Impl::sample(const GuiViewportManipulatorCandidate& value) {
  auto at = grip_fields.find(value.handle_id);
  if (at == grip_fields.end())
    return;
  loading = true;
  at->second->setText(QString::number(value.scalar_value, 'g', 17));
  loading = false;
  (void)owner.preview();
}
void ShellDirectEditTools::Impl::restore_drag() {
  if (layer.active_handle_id().empty())
    return;
  layer.cancel();
  if (drag_field) {
    loading = true;
    drag_field->setText(drag_start);
    loading = false;
  }
  drag_field = nullptr;
  scroll->setEnabled(true);
  numeric->setEnabled(false);
  window.viewport().release_interaction_mouse();
  (void)owner.preview();
}
Result<std::size_t> ShellDirectEditTools::set_drag_text(const QString& text) {
  auto& p = *impl_;
  p.numeric_valid = p.current() && !p.layer.active_handle_id().empty() &&
                    p.layer.set_numeric_input(text.toStdString()) &&
                    p.layer.candidate().has_value();
  if (!p.numeric_valid) {
    p.valid = false;
    p.apply->setEnabled(false);
    return Result<std::size_t>::failure(error("Enter a valid active grip value in mm or deg."));
  }
  p.sample(*p.layer.candidate());
  return p.valid
             ? Result<std::size_t>::success(1)
             : Result<std::size_t>::failure(error("The direct edit grip candidate is invalid."));
}
bool ShellDirectEditTools::eventFilter(QObject* watched, QEvent* event) {
  auto& p = *impl_;
  if (!p.active)
    return QObject::eventFilter(watched, event);
  if (!p.layer.active_handle_id().empty() &&
      (event->type() == QEvent::WindowDeactivate ||
       (watched == &p.window.viewport() &&
        (event->type() == QEvent::UngrabMouse || event->type() == QEvent::Resize ||
         event->type() == QEvent::Wheel)))) {
    p.restore_drag();
    return QObject::eventFilter(watched, event);
  }
  if (event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
      if (!p.layer.active_handle_id().empty())
        p.restore_drag();
      else
        cancel();
      return true;
    }
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
      if (key->modifiers() == Qt::ControlModifier && p.layer.active_handle_id().empty()) {
        (void)apply();
        return true;
      }
      if (!p.layer.active_handle_id().empty()) {
        if (!p.numeric_valid)
          return true;
        auto last = p.layer.end_drag(p.pointer);
        p.scroll->setEnabled(true);
        p.numeric->setEnabled(false);
        p.window.viewport().release_interaction_mouse();
        if (last)
          p.sample(last.value());
        p.drag_field = nullptr;
        return true;
      }
    }
  }
  if (watched != &p.window.viewport())
    return QObject::eventFilter(watched, event);
  if (event->type() == QEvent::Resize) {
    p.overlay_refresh();
    return false;
  }
  if (event->type() == QEvent::MouseButtonPress) {
    const auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() == Qt::LeftButton && p.valid) {
      p.overlay_refresh();
      if (p.layer.begin_drag({mouse->position().x(), mouse->position().y()})) {
        auto at = p.grip_fields.find(std::string(p.layer.active_handle_id()));
        if (at == p.grip_fields.end()) {
          p.layer.cancel();
          return false;
        }
        p.drag_field = at->second;
        p.drag_start = p.drag_field->text();
        p.numeric_valid = true;
        p.pointer = {mouse->position().x(), mouse->position().y()};
        p.numeric->setEnabled(true);
        p.numeric->setText(p.drag_start + (p.mode->currentIndex() == 1 ? " deg" : " mm"));
        p.numeric->setFocus();
        p.numeric->selectAll();
        p.scroll->setEnabled(false);
        p.window.viewport().grab_interaction_mouse();
        return true;
      }
    }
  }
  if (event->type() == QEvent::MouseMove && !p.layer.active_handle_id().empty()) {
    const auto* mouse = static_cast<QMouseEvent*>(event);
    p.pointer = {mouse->position().x(), mouse->position().y()};
    if (p.layer.update_drag(p.pointer) && p.layer.candidate())
      p.sample(*p.layer.candidate());
    return true;
  }
  if (event->type() == QEvent::MouseButtonRelease && !p.layer.active_handle_id().empty()) {
    const auto* mouse = static_cast<QMouseEvent*>(event);
    if (!p.numeric_valid) {
      p.restore_drag();
      return true;
    }
    auto last = p.layer.end_drag({mouse->position().x(), mouse->position().y()});
    p.scroll->setEnabled(true);
    p.numeric->setEnabled(false);
    p.window.viewport().release_interaction_mouse();
    if (last)
      p.sample(last.value());
    p.drag_field = nullptr;
    return true;
  }
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
