#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

enum class GuiViewportManipulatorKind {
  Linear,
  Angular,
  Radial,
  TranslateAxis,
  RotateAxis,
  PatternCount,
  PatternSpacing,
};

enum class GuiViewportManipulatorValueKind {
  Distance,
  Angle,
  Radius,
  Translation,
  Rotation,
  Count,
  Spacing,
};

enum class GuiViewportManipulatorInputSource { Drag, Numeric };

[[nodiscard]] constexpr std::string_view
manipulator_unit(GuiViewportManipulatorValueKind kind) noexcept {
  switch (kind) {
  case GuiViewportManipulatorValueKind::Angle:
  case GuiViewportManipulatorValueKind::Rotation: return "deg";
  case GuiViewportManipulatorValueKind::Count: return {};
  case GuiViewportManipulatorValueKind::Distance:
  case GuiViewportManipulatorValueKind::Radius:
  case GuiViewportManipulatorValueKind::Translation:
  case GuiViewportManipulatorValueKind::Spacing: return "mm";
  }
  return {};
}

struct GuiViewportManipulatorHandle {
  std::string id;
  GuiViewportManipulatorKind kind{GuiViewportManipulatorKind::Linear};
  Point3 origin;
  Vector3 axis{1.0, 0.0, 0.0};
  Vector3 plane_normal{0.0, 0.0, 1.0};
  Vector3 reference_direction{1.0, 0.0, 0.0};
  double reference_value{0.0};
  double minimum_value{-std::numeric_limits<double>::infinity()};
  double maximum_value{std::numeric_limits<double>::infinity()};
  double display_size_dip{68.0};
  double hit_tolerance_dip{9.0};
  double count_model_step{12.0};
};

struct GuiViewportManipulatorCandidate {
  std::string handle_id;
  GuiViewportManipulatorValueKind value_kind{GuiViewportManipulatorValueKind::Distance};
  GuiViewportManipulatorInputSource source{GuiViewportManipulatorInputSource::Drag};
  double scalar_value{0.0};
  Vector3 vector_value;
  int count_value{0};
  friend bool operator==(const GuiViewportManipulatorCandidate&,
                         const GuiViewportManipulatorCandidate&) = default;
};

struct GuiViewportManipulatorHit {
  std::string handle_id;
  double screen_distance_dip{0.0};
  friend bool operator==(const GuiViewportManipulatorHit&,
                         const GuiViewportManipulatorHit&) = default;
};

struct GuiViewportManipulatorPresentation {
  std::string handle_id;
  GuiViewportManipulatorKind kind{GuiViewportManipulatorKind::Linear};
  GuiSketchScreenPoint anchor;
  GuiSketchScreenPoint endpoint;
  double ring_radius_dip{0.0};
  bool active{false};
  std::string value_text;
};

namespace manipulator_detail {
inline constexpr double epsilon = 1.0e-9;
inline constexpr double pi = 3.141592653589793238462643383279502884;

[[nodiscard]] inline bool finite(Point3 p) noexcept {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
[[nodiscard]] inline bool finite(Vector3 v) noexcept {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
[[nodiscard]] inline bool finite(GuiSketchScreenPoint p) noexcept {
  return std::isfinite(p.x) && std::isfinite(p.y);
}
[[nodiscard]] inline Vector3 subtract(Point3 a, Point3 b) noexcept {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
[[nodiscard]] inline Point3 add(Point3 p, Vector3 v) noexcept {
  return {p.x + v.x, p.y + v.y, p.z + v.z};
}
[[nodiscard]] inline Vector3 add(Vector3 a, Vector3 b) noexcept {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
[[nodiscard]] inline Vector3 scale(Vector3 v, double s) noexcept {
  return {v.x * s, v.y * s, v.z * s};
}
[[nodiscard]] inline double dot(Vector3 a, Vector3 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
[[nodiscard]] inline Vector3 cross(Vector3 a, Vector3 b) noexcept {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
          a.x * b.y - a.y * b.x};
}
[[nodiscard]] inline double length(Vector3 v) noexcept {
  return std::sqrt(dot(v, v));
}
[[nodiscard]] inline Vector3 normalized(Vector3 v) noexcept {
  const double l = length(v);
  return l <= epsilon ? Vector3{} : scale(v, 1.0 / l);
}
[[nodiscard]] inline Vector3 project_to_plane(Vector3 v, Vector3 normal) noexcept {
  normal = normalized(normal);
  return add(v, scale(normal, -dot(v, normal)));
}
[[nodiscard]] inline double screen_length(GuiSketchScreenPoint v) noexcept {
  return std::hypot(v.x, v.y);
}
[[nodiscard]] inline GuiSketchScreenPoint screen_normalized(GuiSketchScreenPoint v) noexcept {
  const double l = screen_length(v);
  return l <= epsilon ? GuiSketchScreenPoint{1.0, 0.0}
                      : GuiSketchScreenPoint{v.x / l, v.y / l};
}
[[nodiscard]] inline double screen_distance(GuiSketchScreenPoint a,
                                            GuiSketchScreenPoint b) noexcept {
  return std::hypot(a.x - b.x, a.y - b.y);
}
[[nodiscard]] inline double point_segment_distance(GuiSketchScreenPoint p,
                                                    GuiSketchScreenPoint a,
                                                    GuiSketchScreenPoint b) noexcept {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double squared = dx * dx + dy * dy;
  if (squared <= epsilon) return screen_distance(p, a);
  const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / squared,
                              0.0, 1.0);
  return screen_distance(p, {a.x + t * dx, a.y + t * dy});
}
[[nodiscard]] inline bool is_ring(GuiViewportManipulatorKind kind) noexcept {
  return kind == GuiViewportManipulatorKind::Angular ||
         kind == GuiViewportManipulatorKind::RotateAxis;
}
[[nodiscard]] inline GuiViewportManipulatorValueKind
value_kind(GuiViewportManipulatorKind kind) noexcept {
  switch (kind) {
  case GuiViewportManipulatorKind::Linear:
    return GuiViewportManipulatorValueKind::Distance;
  case GuiViewportManipulatorKind::Angular:
    return GuiViewportManipulatorValueKind::Angle;
  case GuiViewportManipulatorKind::Radial:
    return GuiViewportManipulatorValueKind::Radius;
  case GuiViewportManipulatorKind::TranslateAxis:
    return GuiViewportManipulatorValueKind::Translation;
  case GuiViewportManipulatorKind::RotateAxis:
    return GuiViewportManipulatorValueKind::Rotation;
  case GuiViewportManipulatorKind::PatternCount:
    return GuiViewportManipulatorValueKind::Count;
  case GuiViewportManipulatorKind::PatternSpacing:
    return GuiViewportManipulatorValueKind::Spacing;
  }
  return GuiViewportManipulatorValueKind::Distance;
}
[[nodiscard]] inline GuiSketchScreenPoint fallback_screen_direction(Vector3 axis) noexcept {
  axis = normalized(axis);
  const double ax = std::abs(axis.x), ay = std::abs(axis.y), az = std::abs(axis.z);
  if (ax >= ay && ax >= az) return {axis.x < 0.0 ? -1.0 : 1.0, 0.0};
  if (ay >= ax && ay >= az) return {0.0, axis.y < 0.0 ? 1.0 : -1.0};
  const double sign = axis.z < 0.0 ? -1.0 : 1.0;
  return {0.7071067811865475 * sign, -0.7071067811865475 * sign};
}
} // namespace manipulator_detail

class GuiViewportManipulatorMapping {
public:
  using ScreenToRay = std::function<Result<GuiSketchViewRay>(GuiSketchScreenPoint)>;
  using ModelToScreen = std::function<Result<GuiSketchScreenPoint>(Point3)>;

  [[nodiscard]] static Result<GuiViewportManipulatorMapping>
  create(ScreenToRay screen_to_ray, ModelToScreen model_to_screen) {
    if (!screen_to_ray || !model_to_screen)
      return Result<GuiViewportManipulatorMapping>::failure(
          Error::validation("gui.viewport_manipulator.mapping",
                            "screen-to-ray and model-to-screen providers are required"));
    return Result<GuiViewportManipulatorMapping>::success(
        GuiViewportManipulatorMapping(std::move(screen_to_ray), std::move(model_to_screen)));
  }

  [[nodiscard]] static Result<GuiViewportManipulatorMapping>
  create_camera(const GuiViewportCameraBookmark& camera, double width_dip,
                double height_dip) {
    using namespace manipulator_detail;
    if (!finite(camera.eye) || !finite(camera.target) || !finite(camera.up_direction) ||
        !std::isfinite(camera.scale) || camera.scale <= 0.0 ||
        !std::isfinite(width_dip) || width_dip <= 0.0 ||
        !std::isfinite(height_dip) || height_dip <= 0.0)
      return Result<GuiViewportManipulatorMapping>::failure(
          Error::validation("gui.viewport_manipulator.mapping",
                            "camera and viewport values must be finite and positive"));

    const Vector3 view = normalized(subtract(camera.target, camera.eye));
    const Vector3 right = normalized(cross(view, camera.up_direction));
    if (length(view) <= epsilon || length(right) <= epsilon)
      return Result<GuiViewportManipulatorMapping>::failure(
          Error::validation("gui.viewport_manipulator.mapping",
                            "camera frame is degenerate"));
    const Vector3 up = normalized(cross(right, view));
    const double target_distance = length(subtract(camera.target, camera.eye));
    const double model_per_dip = camera.scale / height_dip;
    if (!std::isfinite(model_per_dip) || model_per_dip <= epsilon)
      return Result<GuiViewportManipulatorMapping>::failure(
          Error::validation("gui.viewport_manipulator.mapping",
                            "camera scale does not define a usable mapping"));

    auto model_to_screen = [camera, view, right, up, target_distance, model_per_dip,
                            width_dip, height_dip](Point3 point)
        -> Result<GuiSketchScreenPoint> {
      using namespace manipulator_detail;
      if (!finite(point))
        return Result<GuiSketchScreenPoint>::failure(
            Error::validation("gui.viewport_manipulator.mapping",
                              "model point must be finite"));
      double horizontal{}, vertical{};
      if (camera.projection == GuiViewportProjection::Perspective) {
        const Vector3 eye_to_point = subtract(point, camera.eye);
        const double depth = dot(eye_to_point, view);
        if (depth <= epsilon)
          return Result<GuiSketchScreenPoint>::failure(
              Error::validation("gui.viewport_manipulator.mapping",
                                "model point is behind the camera"));
        const double perspective = target_distance / depth;
        horizontal = dot(eye_to_point, right) * perspective;
        vertical = dot(eye_to_point, up) * perspective;
      } else {
        const Vector3 delta = subtract(point, camera.target);
        horizontal = dot(delta, right);
        vertical = dot(delta, up);
      }
      return Result<GuiSketchScreenPoint>::success(
          {width_dip * 0.5 + horizontal / model_per_dip,
           height_dip * 0.5 - vertical / model_per_dip});
    };

    auto screen_to_ray = [camera, view, right, up, target_distance, model_per_dip,
                          width_dip, height_dip](GuiSketchScreenPoint screen)
        -> Result<GuiSketchViewRay> {
      using namespace manipulator_detail;
      if (!finite(screen))
        return Result<GuiSketchViewRay>::failure(
            Error::validation("gui.viewport_manipulator.mapping",
                              "screen point must be finite"));
      const double h = (screen.x - width_dip * 0.5) * model_per_dip;
      const double v = (height_dip * 0.5 - screen.y) * model_per_dip;
      const Point3 target_plane = add(camera.target, add(scale(right, h), scale(up, v)));
      if (camera.projection == GuiViewportProjection::Perspective) {
        const Vector3 direction = normalized(subtract(target_plane, camera.eye));
        if (length(direction) <= epsilon)
          return Result<GuiSketchViewRay>::failure(
              Error::validation("gui.viewport_manipulator.mapping",
                                "screen ray is degenerate"));
        return Result<GuiSketchViewRay>::success({camera.eye, direction});
      }
      return Result<GuiSketchViewRay>::success(
          {add(target_plane, scale(view, -target_distance)), view});
    };
    return create(std::move(screen_to_ray), std::move(model_to_screen));
  }

  [[nodiscard]] Result<GuiSketchViewRay> screen_to_ray(GuiSketchScreenPoint p) const {
    return screen_to_ray_(p);
  }
  [[nodiscard]] Result<GuiSketchScreenPoint> model_to_screen(Point3 p) const {
    return model_to_screen_(p);
  }

private:
  GuiViewportManipulatorMapping(ScreenToRay screen_to_ray, ModelToScreen model_to_screen)
      : screen_to_ray_(std::move(screen_to_ray)),
        model_to_screen_(std::move(model_to_screen)) {}
  ScreenToRay screen_to_ray_;
  ModelToScreen model_to_screen_;
};

class GuiViewportManipulatorLayer {
public:
  [[nodiscard]] Result<std::size_t> set_mapping(GuiViewportManipulatorMapping mapping) {
    mapping_ = std::move(mapping);
    ++revision_;
    return Result<std::size_t>::success(revision_);
  }

  [[nodiscard]] Result<std::size_t>
  set_handles(std::vector<GuiViewportManipulatorHandle> handles) {
    using namespace manipulator_detail;
    for (auto& handle : handles) {
      const auto valid = validate_handle(handle);
      if (valid.has_error()) return Result<std::size_t>::failure(valid.error());
      handle.axis = normalized(handle.axis);
      handle.plane_normal = normalized(handle.plane_normal);
      handle.reference_direction = normalized(handle.reference_direction);
    }
    std::sort(handles.begin(), handles.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    for (std::size_t i = 1; i < handles.size(); ++i)
      if (handles[i - 1].id == handles[i].id)
        return Result<std::size_t>::failure(
            Error::validation(handles[i].id, "manipulator handle ids must be unique"));
    handles_ = std::move(handles);
    reset_interaction();
    ++revision_;
    return Result<std::size_t>::success(handles_.size());
  }

  void clear_handles() noexcept {
    handles_.clear();
    reset_interaction();
    ++revision_;
  }

  [[nodiscard]] static std::vector<GuiViewportManipulatorHandle>
  translation_triad(std::string_view prefix, Point3 origin, double display_size_dip = 68.0) {
    return triad(prefix, origin, GuiViewportManipulatorKind::TranslateAxis,
                 display_size_dip);
  }
  [[nodiscard]] static std::vector<GuiViewportManipulatorHandle>
  rotation_triad(std::string_view prefix, Point3 origin, double display_size_dip = 48.0) {
    return triad(prefix, origin, GuiViewportManipulatorKind::RotateAxis,
                 display_size_dip);
  }

  [[nodiscard]] std::vector<GuiViewportManipulatorPresentation> presentations() const {
    std::vector<GuiViewportManipulatorPresentation> result;
    if (!mapping_) return result;
    result.reserve(handles_.size());
    for (const auto& handle : handles_)
      if (auto presentation = presentation_for(handle))
        result.push_back(std::move(*presentation));
    return result;
  }

  [[nodiscard]] std::vector<GuiViewportManipulatorHit>
  hits_at(GuiSketchScreenPoint pointer) const {
    using namespace manipulator_detail;
    std::vector<GuiViewportManipulatorHit> hits;
    if (!finite(pointer)) return hits;
    for (const auto& presentation : presentations()) {
      const auto* handle = find_handle(presentation.handle_id);
      if (!handle) continue;
      const double distance = is_ring(handle->kind)
                                  ? std::abs(screen_distance(pointer, presentation.anchor) -
                                             presentation.ring_radius_dip)
                                  : point_segment_distance(pointer, presentation.anchor,
                                                           presentation.endpoint);
      if (distance <= handle->hit_tolerance_dip)
        hits.push_back({handle->id, distance});
    }
    std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) {
      if (std::abs(a.screen_distance_dip - b.screen_distance_dip) >
          manipulator_detail::epsilon)
        return a.screen_distance_dip < b.screen_distance_dip;
      return a.handle_id < b.handle_id;
    });
    return hits;
  }

  [[nodiscard]] bool begin_drag(GuiSketchScreenPoint pointer) {
    if (!mapping_ || active_handle_id_ || !manipulator_detail::finite(pointer)) return false;
    const auto hits = hits_at(pointer);
    if (hits.empty()) return false;
    const auto* handle = find_handle(hits.front().handle_id);
    if (!handle) return false;
    const auto measurement = pointer_measurement(*handle, pointer);
    if (!measurement) return false;
    active_handle_id_ = handle->id;
    drag_start_pointer_ = pointer;
    latest_pointer_ = pointer;
    drag_start_measurement_ = *measurement;
    numeric_override_ = false;
    candidate_ = make_candidate(*handle, handle->reference_value,
                                GuiViewportManipulatorInputSource::Drag);
    numeric_text_ = format_candidate(*candidate_);
    ++revision_;
    return true;
  }

  [[nodiscard]] bool update_drag(GuiSketchScreenPoint pointer) {
    if (!active_handle_id_ || !manipulator_detail::finite(pointer)) return false;
    latest_pointer_ = pointer;
    if (numeric_override_) {
      ++revision_;
      return true;
    }
    const auto* handle = find_handle(*active_handle_id_);
    if (!handle) return false;
    const auto measurement = pointer_measurement(*handle, pointer);
    if (!measurement || !drag_start_measurement_) return false;
    candidate_ = make_candidate(*handle, drag_value(*handle, *measurement),
                                GuiViewportManipulatorInputSource::Drag);
    numeric_text_ = format_candidate(*candidate_);
    ++revision_;
    return true;
  }

  [[nodiscard]] bool set_numeric_input(std::string text) {
    if (!active_handle_id_) return false;
    const auto* handle = find_handle(*active_handle_id_);
    if (!handle) return false;
    const auto parsed = parse_numeric(*handle, std::move(text));
    if (!parsed) return false;
    numeric_override_ = true;
    candidate_ = make_candidate(*handle, *parsed,
                                GuiViewportManipulatorInputSource::Numeric);
    numeric_text_ = format_candidate(*candidate_);
    ++revision_;
    return true;
  }

  [[nodiscard]] bool clear_numeric_override() {
    if (!active_handle_id_ || !numeric_override_) return false;
    numeric_override_ = false;
    return update_drag(latest_pointer_.value_or(
        drag_start_pointer_.value_or(GuiSketchScreenPoint{})));
  }

  [[nodiscard]] std::optional<GuiViewportManipulatorCandidate>
  end_drag(GuiSketchScreenPoint final_pointer) {
    if (!active_handle_id_) return std::nullopt;
    if (!numeric_override_ && !update_drag(final_pointer)) {
      cancel();
      return std::nullopt;
    }
    active_handle_id_.reset();
    drag_start_pointer_.reset();
    latest_pointer_.reset();
    drag_start_measurement_.reset();
    numeric_override_ = false;
    ++revision_;
    return candidate_;
  }

  void cancel() noexcept {
    reset_interaction();
    ++revision_;
  }

  [[nodiscard]] const std::vector<GuiViewportManipulatorHandle>& handles() const noexcept {
    return handles_;
  }
  [[nodiscard]] const std::optional<GuiViewportManipulatorCandidate>& candidate() const noexcept {
    return candidate_;
  }
  [[nodiscard]] std::string_view active_handle_id() const noexcept {
    return active_handle_id_ ? std::string_view(*active_handle_id_) : std::string_view{};
  }
  [[nodiscard]] std::string_view numeric_text() const noexcept { return numeric_text_; }
  [[nodiscard]] bool numeric_override_active() const noexcept { return numeric_override_; }
  [[nodiscard]] std::size_t revision() const noexcept { return revision_; }

private:
  [[nodiscard]] static Result<std::size_t>
  validate_handle(const GuiViewportManipulatorHandle& h) {
    using namespace manipulator_detail;
    if (h.id.empty())
      return Result<std::size_t>::failure(
          Error::validation("gui.viewport_manipulator", "handle id must not be empty"));
    if (!finite(h.origin) || !finite(h.axis) || !finite(h.plane_normal) ||
        !finite(h.reference_direction) || !std::isfinite(h.reference_value) ||
        !std::isfinite(h.display_size_dip) || h.display_size_dip <= 0.0 ||
        !std::isfinite(h.hit_tolerance_dip) || h.hit_tolerance_dip <= 0.0 ||
        !std::isfinite(h.count_model_step) || h.count_model_step <= 0.0 ||
        std::isnan(h.minimum_value) || std::isnan(h.maximum_value) ||
        h.minimum_value > h.maximum_value || length(h.axis) <= epsilon)
      return Result<std::size_t>::failure(
          Error::validation(h.id, "manipulator handle values are invalid"));
    if ((is_ring(h.kind) || h.kind == GuiViewportManipulatorKind::Radial) &&
        (length(h.plane_normal) <= epsilon ||
         length(project_to_plane(h.reference_direction, h.plane_normal)) <= epsilon))
      return Result<std::size_t>::failure(
          Error::validation(h.id,
                            "radial and angular handles require a valid plane frame"));
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] static std::vector<GuiViewportManipulatorHandle>
  triad(std::string_view prefix, Point3 origin, GuiViewportManipulatorKind kind,
        double display_size_dip) {
    const std::array<std::pair<std::string_view, Vector3>, 3> axes{{
        {"x", {1.0, 0.0, 0.0}}, {"y", {0.0, 1.0, 0.0}}, {"z", {0.0, 0.0, 1.0}}}};
    std::vector<GuiViewportManipulatorHandle> result;
    for (const auto& [suffix, axis] : axes) {
      GuiViewportManipulatorHandle h;
      h.id = std::string(prefix) + "." + std::string(suffix);
      h.kind = kind;
      h.origin = origin;
      h.axis = axis;
      h.plane_normal = axis;
      h.reference_direction = axis.z == 0.0 ? Vector3{0.0, 0.0, 1.0}
                                            : Vector3{1.0, 0.0, 0.0};
      h.display_size_dip = display_size_dip;
      result.push_back(std::move(h));
    }
    return result;
  }

  [[nodiscard]] const GuiViewportManipulatorHandle*
  find_handle(std::string_view id) const noexcept {
    const auto found = std::lower_bound(
        handles_.begin(), handles_.end(), id,
        [](const auto& handle, std::string_view value) { return handle.id < value; });
    return found != handles_.end() && found->id == id ? &*found : nullptr;
  }

  [[nodiscard]] std::optional<GuiViewportManipulatorPresentation>
  presentation_for(const GuiViewportManipulatorHandle& h) const {
    using namespace manipulator_detail;
    const auto anchor = mapping_->model_to_screen(h.origin);
    if (anchor.has_error()) return std::nullopt;
    const Vector3 model_direction =
        is_ring(h.kind) || h.kind == GuiViewportManipulatorKind::Radial
            ? normalized(project_to_plane(h.reference_direction, h.plane_normal))
            : h.axis;
    const auto projected = mapping_->model_to_screen(add(h.origin, model_direction));
    GuiSketchScreenPoint direction{};
    if (!projected.has_error())
      direction = {projected.value().x - anchor.value().x,
                   projected.value().y - anchor.value().y};
    if (screen_length(direction) <= epsilon)
      direction = fallback_screen_direction(model_direction);
    direction = screen_normalized(direction);

    GuiViewportManipulatorPresentation p;
    p.handle_id = h.id;
    p.kind = h.kind;
    p.anchor = anchor.value();
    p.endpoint = {p.anchor.x + direction.x * h.display_size_dip,
                  p.anchor.y + direction.y * h.display_size_dip};
    p.ring_radius_dip = is_ring(h.kind) ? h.display_size_dip : 0.0;
    p.active = active_handle_id_ && *active_handle_id_ == h.id;
    if (p.active && candidate_) p.value_text = format_candidate(*candidate_);
    return p;
  }

  [[nodiscard]] std::optional<double>
  pointer_measurement(const GuiViewportManipulatorHandle& h,
                      GuiSketchScreenPoint pointer) const {
    using namespace manipulator_detail;
    const auto ray = mapping_->screen_to_ray(pointer);
    if (ray.has_error() || !finite(ray.value().origin) || !finite(ray.value().direction) ||
        length(ray.value().direction) <= epsilon)
      return std::nullopt;
    if (is_ring(h.kind)) return angular_measurement(h, ray.value());
    if (h.kind == GuiViewportManipulatorKind::Radial)
      return radial_measurement(h, ray.value());
    return linear_measurement(h, ray.value(), pointer);
  }

  [[nodiscard]] std::optional<double>
  linear_measurement(const GuiViewportManipulatorHandle& h,
                     const GuiSketchViewRay& ray,
                     GuiSketchScreenPoint pointer) const {
    using namespace manipulator_detail;
    const Vector3 axis = normalized(h.axis);
    const Vector3 direction = normalized(ray.direction);
    const Vector3 w0 = subtract(h.origin, ray.origin);
    const double a = dot(axis, axis), b = dot(axis, direction), c = dot(direction, direction);
    const double d = dot(axis, w0), e = dot(direction, w0);
    const double denominator = a * c - b * b;
    if (std::abs(denominator) > epsilon)
      return (b * e - c * d) / denominator;

    const auto anchor = mapping_->model_to_screen(h.origin);
    const auto one_unit = mapping_->model_to_screen(add(h.origin, axis));
    if (anchor.has_error() || one_unit.has_error()) return std::nullopt;
    GuiSketchScreenPoint projected{one_unit.value().x - anchor.value().x,
                                   one_unit.value().y - anchor.value().y};
    double pixels_per_model = screen_length(projected);
    if (pixels_per_model <= epsilon) {
      projected = fallback_screen_direction(axis);
      pixels_per_model = 1.0;
    }
    projected = screen_normalized(projected);
    return ((pointer.x - anchor.value().x) * projected.x +
            (pointer.y - anchor.value().y) * projected.y) /
           pixels_per_model;
  }

  [[nodiscard]] static std::optional<Point3>
  intersect_plane(const GuiSketchViewRay& ray, Point3 origin, Vector3 normal) {
    using namespace manipulator_detail;
    normal = normalized(normal);
    const double denominator = dot(ray.direction, normal);
    if (std::abs(denominator) <= epsilon) return std::nullopt;
    const double t = dot(subtract(origin, ray.origin), normal) / denominator;
    return std::isfinite(t) ? std::optional<Point3>(add(ray.origin, scale(ray.direction, t)))
                            : std::nullopt;
  }

  [[nodiscard]] static std::optional<double>
  radial_measurement(const GuiViewportManipulatorHandle& h,
                     const GuiSketchViewRay& ray) {
    using namespace manipulator_detail;
    const auto point = intersect_plane(ray, h.origin, h.plane_normal);
    return point ? std::optional<double>(
                       length(project_to_plane(subtract(*point, h.origin), h.plane_normal)))
                 : std::nullopt;
  }

  [[nodiscard]] static std::optional<double>
  angular_measurement(const GuiViewportManipulatorHandle& h,
                      const GuiSketchViewRay& ray) {
    using namespace manipulator_detail;
    const auto point = intersect_plane(ray, h.origin, h.plane_normal);
    if (!point) return std::nullopt;
    const Vector3 normal = normalized(h.plane_normal);
    const Vector3 reference = normalized(project_to_plane(h.reference_direction, normal));
    const Vector3 radial = normalized(project_to_plane(subtract(*point, h.origin), normal));
    if (length(reference) <= epsilon || length(radial) <= epsilon) return std::nullopt;
    return std::atan2(dot(cross(reference, radial), normal),
                      std::clamp(dot(reference, radial), -1.0, 1.0)) *
           180.0 / pi;
  }

  [[nodiscard]] double drag_value(const GuiViewportManipulatorHandle& h,
                                  double current) const noexcept {
    double delta = current - drag_start_measurement_.value_or(current);
    if (manipulator_detail::is_ring(h.kind)) {
      while (delta > 180.0) delta -= 360.0;
      while (delta < -180.0) delta += 360.0;
    }
    if (h.kind == GuiViewportManipulatorKind::PatternCount)
      delta /= h.count_model_step;
    return clamp_value(h, h.reference_value + delta);
  }

  [[nodiscard]] static double clamp_value(const GuiViewportManipulatorHandle& h,
                                          double value) noexcept {
    if (h.kind == GuiViewportManipulatorKind::Radial ||
        h.kind == GuiViewportManipulatorKind::PatternSpacing)
      value = std::max(0.0, value);
    return std::clamp(value, h.minimum_value, h.maximum_value);
  }

  [[nodiscard]] static GuiViewportManipulatorCandidate
  make_candidate(const GuiViewportManipulatorHandle& h, double value,
                 GuiViewportManipulatorInputSource source) {
    using namespace manipulator_detail;
    GuiViewportManipulatorCandidate c;
    c.handle_id = h.id;
    c.value_kind = value_kind(h.kind);
    c.source = source;
    c.scalar_value = clamp_value(h, value);
    if (h.kind == GuiViewportManipulatorKind::PatternCount) {
      c.count_value = std::max(1, static_cast<int>(std::llround(c.scalar_value)));
      c.scalar_value = static_cast<double>(c.count_value);
    }
    if (h.kind == GuiViewportManipulatorKind::TranslateAxis ||
        h.kind == GuiViewportManipulatorKind::RotateAxis)
      c.vector_value = scale(normalized(h.axis), c.scalar_value);
    return c;
  }

  [[nodiscard]] static std::optional<double>
  parse_numeric(const GuiViewportManipulatorHandle& h, std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::nullopt;
    text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || errno == ERANGE || !std::isfinite(value))
      return std::nullopt;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    const std::string suffix(end);
    const auto kind = manipulator_detail::value_kind(h.kind);
    if (!suffix.empty() && suffix != manipulator_unit(kind)) return std::nullopt;
    if (kind == GuiViewportManipulatorValueKind::Count &&
        std::abs(value - std::round(value)) > manipulator_detail::epsilon)
      return std::nullopt;
    return clamp_value(h, value);
  }

  [[nodiscard]] static std::string
  format_candidate(const GuiViewportManipulatorCandidate& c) {
    std::ostringstream stream;
    if (c.value_kind == GuiViewportManipulatorValueKind::Count) {
      stream << c.count_value;
    } else {
      stream << std::fixed << std::setprecision(3) << c.scalar_value;
      std::string number = stream.str();
      while (number.size() > 1 && number.back() == '0') number.pop_back();
      if (!number.empty() && number.back() == '.') number.pop_back();
      stream.str({});
      stream.clear();
      stream << number;
    }
    const auto unit = manipulator_unit(c.value_kind);
    if (!unit.empty()) stream << ' ' << unit;
    return stream.str();
  }

  void reset_interaction() noexcept {
    active_handle_id_.reset();
    drag_start_pointer_.reset();
    latest_pointer_.reset();
    drag_start_measurement_.reset();
    candidate_.reset();
    numeric_text_.clear();
    numeric_override_ = false;
  }

  std::optional<GuiViewportManipulatorMapping> mapping_;
  std::vector<GuiViewportManipulatorHandle> handles_;
  std::optional<std::string> active_handle_id_;
  std::optional<GuiSketchScreenPoint> drag_start_pointer_;
  std::optional<GuiSketchScreenPoint> latest_pointer_;
  std::optional<double> drag_start_measurement_;
  std::optional<GuiViewportManipulatorCandidate> candidate_;
  std::string numeric_text_;
  bool numeric_override_{false};
  std::size_t revision_{0};
};

} // namespace blcad::gui
