#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/gui/shell/shell_construction_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <cmath>
#include <numbers>

namespace blcad::gui {
namespace {
Vector3 read(const std::array<QDoubleSpinBox*, 3>& controls) {
  return {controls[0]->value(), controls[1]->value(), controls[2]->value()};
}
Vector3 normalized_or_zero(Vector3 value) {
  const auto magnitude = length(value);
  return magnitude > 1e-12 ? Vector3{value.x / magnitude, value.y / magnitude, value.z / magnitude}
                           : Vector3{};
}
Vector3 rotated(Vector3 value, Vector3 axis, double angle) {
  const double radians = angle * std::numbers::pi / 180;
  using manipulator_detail::add;
  using manipulator_detail::scale;
  return add(add(scale(value, std::cos(radians)), scale(cross(axis, value), std::sin(radians))),
             scale(axis, dot(axis, value) * (1 - std::cos(radians))));
}
} // namespace

ShellConstructionTools::~ShellConstructionTools() {
  active_ = false;
  layer_.clear_handles();
  if (window_.viewport().mouseGrabber() == &window_.viewport())
    window_.viewport().release_interaction_mouse();
  delete overlay_;
}

std::array<Vector3, 3> ShellConstructionTools::definition_values() const {
  std::array result{read(origin_), read(direction_), read(y_direction_)};
  // Preserve unchanged full-precision components rather than round-tripping
  // rotated frames through the six-decimal numeric presentation.
  const auto coordinate = [&](double ui, double displayed, double precise, double initial,
                              double original) {
    if (precise_values_ && ui == displayed)
      return precise;
    return editing_ && ui == initial ? original : ui;
  };
  const auto precise_origin = precise_values_ ? (*precise_values_)[0] : Vector3{};
  result[0] = {coordinate(result[0].x, precise_display_[0].x, precise_origin.x,
                          initial_values_[0].x, original_values_[0].x),
               coordinate(result[0].y, precise_display_[0].y, precise_origin.y,
                          initial_values_[0].y, original_values_[0].y),
               coordinate(result[0].z, precise_display_[0].z, precise_origin.z,
                          initial_values_[0].z, original_values_[0].z)};
  for (std::size_t i = 1; i < result.size(); ++i) {
    if (precise_values_ && result[i] == precise_display_[i])
      result[i] = (*precise_values_)[i];
    else if (editing_ && result[i] == initial_values_[i])
      result[i] = original_values_[i];
    else
      result[i] = normalized_or_zero(result[i]);
  }
  return result;
}

double ShellConstructionTools::offset_value() const {
  return precise_offset_ && offset_->value() == precise_offset_display_ ? *precise_offset_
                                                                        : offset_->value();
}

void ShellConstructionTools::write_values(const std::array<Vector3, 3>& values) {
  updating_ = true;
  const std::array controls{origin_, direction_, y_direction_};
  for (std::size_t i = 0; i < controls.size(); ++i) {
    controls[i][0]->setValue(values[i].x);
    controls[i][1]->setValue(values[i].y);
    controls[i][2]->setValue(values[i].z);
    precise_display_[i] = read(controls[i]);
  }
  precise_values_ = values;
  updating_ = false;
}

void ShellConstructionTools::update_manipulator_mode() {
  const auto previous = manipulation_->currentData();
  {
    const QSignalBlocker blocker(manipulation_);
    manipulation_->clear();
    const auto kind = static_cast<Kind>(kind_->currentIndex());
    if (kind == Kind::Point || kind == Kind::Line || kind == Kind::Axis || kind == Kind::Plane)
      manipulation_->addItem(QStringLiteral("Position (XYZ)"), 0);
    if (kind == Kind::Line || kind == Kind::Axis || kind == Kind::Plane)
      manipulation_->addItem(kind == Kind::Plane ? QStringLiteral("Rotate Plane Frame")
                                                 : QStringLiteral("Rotate Direction"),
                             1);
    if (kind == Kind::OffsetPlane && parameter_->currentData().toString().isEmpty())
      manipulation_->addItem(QStringLiteral("Offset"), 2);
    const int retained = manipulation_->findData(previous);
    if (retained >= 0)
      manipulation_->setCurrentIndex(retained);
  }
  show_row(manipulation_, manipulation_->count() > 0);
  show_row(grip_numeric_, manipulation_->count() > 0);
}

void ShellConstructionTools::refresh_handles() {
  if (drag_) {
    synchronize_overlay();
    return;
  }
  std::vector<GuiViewportManipulatorHandle> handles;
  if (current() && manipulation_->count() && apply_->isEnabled()) {
    const auto values = definition_values();
    const Point3 origin{values[0].x, values[0].y, values[0].z};
    const int mode = manipulation_->currentData().toInt();
    if (mode == 0) {
      handles = GuiViewportManipulatorLayer::translation_triad("construction.position", origin);
      const std::array coordinates{origin.x, origin.y, origin.z};
      for (std::size_t i = 0; i < handles.size(); ++i) {
        handles[i].reference_value = coordinates[i];
        handles[i].minimum_value = -1e6;
        handles[i].maximum_value = 1e6;
      }
    } else if (mode == 1) {
      handles = GuiViewportManipulatorLayer::rotation_triad("construction.rotation", origin);
      // Separate fixed-DIP rings so each world axis has an unambiguous hit target.
      for (std::size_t i = 0; i < handles.size(); ++i)
        handles[i].display_size_dip = 46 + 20 * i;
    } else {
      auto plane = geometry::WorkplaneResolver{}.resolve(
          *window_.session().part_document(),
          DatumPlaneId(plane_->currentData().toString().toStdString()));
      if (plane) {
        const auto& frame = plane.value();
        const double offset = offset_value();
        GuiViewportManipulatorHandle handle;
        handle.id = "construction.offset";
        handle.origin = {frame.origin.x + frame.normal.x * offset,
                         frame.origin.y + frame.normal.y * offset,
                         frame.origin.z + frame.normal.z * offset};
        handle.axis = frame.normal;
        handle.reference_value = offset;
        handle.minimum_value = .000001;
        handle.maximum_value = 1e6;
        handles.push_back(handle);
      }
    }
  }
  (void)layer_.set_handles(std::move(handles));
  grip_numeric_->setReadOnly(true);
  synchronize_overlay();
}

void ShellConstructionTools::synchronize_overlay() {
  overlay_->setGeometry(window_.viewport().rect());
  const bool visible = current() && !layer_.handles().empty();
  overlay_->setVisible(visible);
  if (visible) {
    overlay_->raise();
    overlay_->update();
  }
}

void ShellConstructionTools::refresh_presentation() {
  auto& viewport = window_.viewport();
  const auto camera = viewport.camera_bookmark();
  const std::array size{viewport.width(), viewport.height()};
  const bool changed =
      !mapping_camera_ || size != mapping_size_ || camera.eye != mapping_camera_->eye ||
      camera.target != mapping_camera_->target ||
      camera.up_direction != mapping_camera_->up_direction ||
      camera.scale != mapping_camera_->scale || camera.projection != mapping_camera_->projection;
  mapping_camera_ = camera;
  mapping_size_ = size;
  if (changed && drag_)
    cancel_drag();
  auto mapping = GuiViewportManipulatorMapping::create(
      [&viewport](GuiSketchScreenPoint p) { return viewport.screen_to_view_ray(p); },
      [&viewport](Point3 p) { return viewport.model_to_screen(p); });
  if (mapping)
    (void)layer_.set_mapping(std::move(mapping.value()));
  refresh_handles();
}

void ShellConstructionTools::accept_candidate(const GuiViewportManipulatorCandidate& candidate) {
  if (!drag_ || !current())
    return;
  auto values = drag_->values;
  if (candidate.handle_id == "construction.offset") {
    updating_ = true;
    offset_->setValue(candidate.scalar_value);
    precise_offset_ = candidate.scalar_value;
    precise_offset_display_ = offset_->value();
    updating_ = false;
  } else if (candidate.value_kind == GuiViewportManipulatorValueKind::Translation) {
    auto& origin = values[0];
    if (candidate.handle_id.ends_with(".x"))
      origin.x = candidate.scalar_value;
    if (candidate.handle_id.ends_with(".y"))
      origin.y = candidate.scalar_value;
    if (candidate.handle_id.ends_with(".z"))
      origin.z = candidate.scalar_value;
    write_values(values);
  } else {
    const Vector3 axis = candidate.handle_id.ends_with(".x")   ? Vector3{1, 0, 0}
                         : candidate.handle_id.ends_with(".y") ? Vector3{0, 1, 0}
                                                               : Vector3{0, 0, 1};
    values[1] = rotated(values[1], axis, candidate.scalar_value);
    if (static_cast<Kind>(kind_->currentIndex()) == Kind::Plane)
      values[2] = rotated(values[2], axis, candidate.scalar_value);
    write_values(values);
  }
  update_preview();
}

void ShellConstructionTools::edit_grip_numeric() {
  if (!drag_)
    return;
  numeric_invalid_ = !layer_.set_numeric_input(grip_numeric_->text().toStdString());
  if (numeric_invalid_) {
    apply_->setEnabled(false);
    summary_->setText(
        QStringLiteral("Invalid handle value. Enter a number in mm or deg, or press Escape."));
    return;
  }
  accept_candidate(*layer_.candidate());
}

void ShellConstructionTools::cancel_drag() {
  if (!drag_)
    return;
  const auto start = *drag_;
  drag_.reset();
  layer_.cancel();
  numeric_invalid_ = false;
  updating_ = true;
  const std::array controls{origin_, direction_, y_direction_};
  for (std::size_t i = 0; i < controls.size(); ++i) {
    controls[i][0]->setValue(start.display[i].x);
    controls[i][1]->setValue(start.display[i].y);
    controls[i][2]->setValue(start.display[i].z);
  }
  precise_values_ = start.values;
  precise_display_ = start.display;
  offset_->setValue(start.displayed_offset);
  precise_offset_ = start.offset;
  precise_offset_display_ = start.displayed_offset;
  updating_ = false;
  if (window_.viewport().mouseGrabber() == &window_.viewport())
    window_.viewport().release_interaction_mouse();
  grip_numeric_->clear();
  grip_numeric_->setReadOnly(true);
  update_preview();
}

bool ShellConstructionTools::manipulator_event(QObject* watched, QEvent* event) {
  auto& viewport = window_.viewport();
  if (watched == &window_ && event->type() == QEvent::WindowDeactivate && drag_)
    cancel_drag();
  if (watched != &viewport)
    return false;
  if (event->type() == QEvent::Resize) {
    refresh_presentation();
    return false;
  }
  if (!current())
    return false;
  if (event->type() == QEvent::MouseButtonPress) {
    const auto& mouse = *static_cast<QMouseEvent*>(event);
    if (mouse.button() != Qt::LeftButton || !apply_->isEnabled())
      return false;
    refresh_presentation();
    if (!layer_.begin_drag({mouse.position().x(), mouse.position().y()}))
      return false;
    drag_ = DragState{definition_values(),
                      {read(origin_), read(direction_), read(y_direction_)},
                      offset_value(),
                      offset_->value()};
    viewport.grab_interaction_mouse();
    numeric_invalid_ = false;
    grip_numeric_->setReadOnly(false);
    grip_numeric_->setText(QString::fromStdString(std::string(layer_.numeric_text())));
    grip_numeric_->setFocus();
    grip_numeric_->selectAll();
    synchronize_overlay();
    return true;
  }
  if (event->type() == QEvent::MouseMove && drag_) {
    const auto& mouse = *static_cast<QMouseEvent*>(event);
    if (layer_.update_drag({mouse.position().x(), mouse.position().y()})) {
      accept_candidate(*layer_.candidate());
      if (!layer_.numeric_override_active())
        grip_numeric_->setText(QString::fromStdString(std::string(layer_.numeric_text())));
    } else
      cancel_drag();
    return true;
  }
  if (event->type() == QEvent::MouseButtonRelease && drag_) {
    const auto& mouse = *static_cast<QMouseEvent*>(event);
    if (mouse.button() != Qt::LeftButton)
      return false;
    if (numeric_invalid_) {
      cancel_drag();
      return true;
    }
    const auto candidate = layer_.end_drag({mouse.position().x(), mouse.position().y()});
    if (!candidate) {
      cancel_drag();
      return true;
    }
    accept_candidate(*candidate);
    drag_.reset();
    viewport.release_interaction_mouse();
    grip_numeric_->setReadOnly(true);
    refresh_handles();
    return true;
  }
  return false;
}
} // namespace blcad::gui
