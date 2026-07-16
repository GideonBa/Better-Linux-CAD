#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
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
  case GuiViewportManipulatorValueKind::Count: return "";
  case GuiViewportManipulatorValueKind::Distance:
  case GuiViewportManipulatorValueKind::Radius:
  case GuiViewportManipulatorValueKind::Translation:
  case GuiViewportManipulatorValueKind::Spacing: return "mm";
  }
  return "";
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
  create_camera(const GuiViewportCameraBookmark& camera, double viewport_width_dip,
                double viewport_height_dip) {
    if (!finite(camera.eye) || !finite(camera.target) || !finite(camera.up_direction) ||
        !std::isfinite(camera.scale) || camera.scale <= 0.0 ||
        !std::isfinite(viewport_width_dip) || viewport_width_dip <= 0.0 ||
        !std::isfinite(viewport_height_dip) || viewport_height_dip <= 0.0)
      return Result<GuiViewportManipulatorMapping>::failure(
          Error::validation("gui.viewport_manipulator.mapping",
                            "camera and viewport values must be finite and positive"));

    const Vector3 view = normalized(subtract(camera.target, camera.eye));
    if (length(view) <= kEpsilon)
      return Result<GuiViewportManipulatorMapping>::failure(
          Error::validation("gui.viewport_manipulator.mapping",
                            "camera eye and target must differ"));
    Vector3 right = normalized(cross(view, camera.up_direction));
    if (length(right) <= kEpsilon)
      return Result<GuiViewportManipulatorMapping>::failure(
          Error::validation("gui.viewport_manipulator.mapping",
                            "camera up direction must not be parallel to the view direction"));
    const Vector3 up = normalized(cross(right, view));
    const double target_distance = length(subtract(camera.target, camera.eye));
    const double model_units_per_dip = camera.scale / viewport_height_dip;
    if (!std::isfinite(model_units_per_dip) || model_units_per_dip <= kEpsilon)
      return Result<GuiViewportManipulatorMapping>::failure(
          Error::validation("gui.viewport_manipulator.mapping",
                            "camera scale does not define a usable interaction mapping"));

    auto model_to_screen = [camera, view, right, up, target_distance, model_units_per_dip,
                            viewport_width_dip,
                            viewport_height_dip](Point3 point) -> Result<GuiSketchScreenPoint> {
      if (!finite(point))
        return Result<GuiSketchScreenPoint>::failure(
            Error::validation("gui.viewport_manipulator.mapping",
                              "model point must be finite"));
      double horizontal{};
      double vertical{};
      if (camera.projection == GuiViewportProjection::Perspective) {
        const Vector3 eye_to_point = subtract(point, camera.eye);
        const double depth = dot(eye_to_point, view);
        if (depth <= kEpsilon)
          return Result<GuiSketchScreenPoint>::failure(
              Error::validation("gui.viewport_manipulator.mapping",
                                "model point is behind the perspective camera"));
        const double perspective_scale = target_distance / depth;
        horizontal = dot(eye_to_point, right) * perspective_scale;
        vertical = dot(eye_to_point, up) * perspective_scale;
      } else {
        const Vector3 target_to_point = subtract(point, camera.target);
        horizontal = dot(target_to_point, right);
        vertical = dot(target_to_point, up);
      }
      return Result<GuiSketchScreenPoint>::success(
          {viewport_width_dip * 0.5 + horizontal / model_units_per_dip,
           viewport_height_dip * 0.5 - vertical / model_units_per_dip});
    };

    auto screen_to_ray = [camera, view, right, up, target_distance, model_units_per_dip,
                          viewport_width_dip,
                          viewport_height_dip](GuiSketchScreenPoint screen)
        -> Result<GuiSketchViewRay> {
      if (!finite(screen))
        return Result<GuiSketchViewRay>::failure(
            Error::validation("gui.viewport_manipulator.mapping",
                              "screen point must be finite"));
      const double horizontal =
          (screen.x - viewport_width_dip * 0.5) * model_units_per_dip;
      const double vertical =
          (viewport_height_dip * 0.5 - screen.y) * model_units_per_dip;
      const Point3 target_plane_point =
          add(camera.target, add(scale(right, horizontal), scale(up, vertical)));
      if (camera.projection == GuiViewportProjection::Perspective) {
        const Vector3 direction = normalized(subtract(target_plane_point, camera.eye));
        if (length(direction) <= kEpsilon)
          return Result<GuiSketchViewRay>::failure(
              Error::validation("gui.viewport_manipulator.mapping",
                                "perspective screen ray is degenerate"));
        return Result<GuiSketchViewRay>::success({camera.eye, direction});
      }
      return Result<GuiSketchViewRay>::success(
          {add(target_plane_point, scale(view, -target_distance)), view});
    };

    return create(std::move(screen_to_ray), std::move(model_to_screen));
  }

  [[nodiscard]] Result<GuiSketchViewRay>
  screen_to_ray(GuiSketchScreenPoint point) const {
    return screen_to_ray_(point);
  }

  [[nodiscard]] Result<GuiSketchScreenPoint> model_to_screen(Point3 point) const {
    return model_to_screen_(point);
  }

private:
  static constexpr double kEpsilon = 1.0e-10;

  GuiViewportManipulatorMapping(ScreenToRay screen_to_ray, ModelToScreen model_to_screen)
      : screen_to_ray_(std::move(screen_to_ray)),
        model_to_screen_(std::move(model_to_screen)) {}

  [[nodiscard]] static bool finite(Point3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
  }

  [[nodiscard]] static bool finite(Vector3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
  }

  [[nodiscard]] static bool finite(GuiSketchScreenPoint value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
  }

  [[nodiscard]] static Vector3 subtract(Point3 left, Point3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
  }

  [[nodiscard]] static Point3 add(Point3 point, Vector3 vector) noexcept {
    return {point.x + vector.x, point.y + vector.y, point.z + vector.z};
  }

  [[nodiscard]] static Vector3 add(Vector3 left, Vector3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
  }

  [[nodiscard]] static Vector3 scale(Vector3 vector, double factor) noexcept {
    return {vector.x * factor, vector.y * factor, vector.z * factor};
  }

  [[nodiscard]] static double dot(Vector3 left, Vector3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
  }

  [[nodiscard]] static Vector3 cross(Vector3 left, Vector3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
  }

  [[nodiscard]] static double length(Vector3 vector) noexcept {
    return std::sqrt(dot(vector, vector));
  }

  [[nodiscard]] static Vector3 normalized(Vector3 vector) noexcept {
    const double magnitude = length(vector);
    return magnitude <= kEpsilon ? Vector3{} : scale(vector, 1.0 / magnitude);
  }

  ScreenToRay screen_to_ray_;
  ModelToScreen model_to_screen_;
};

class GuiViewportManipulatorLayer {
public:
  [[nodiscard]] Result<std::size_t>
  set_mapping(GuiViewportManipulatorMapping mapping) {
    mapping_ = std::move(mapping);
    ++revision_;
    return Result<std::size_t>::success(revision_);
  }

  [[nodiscard]] Result<std::size_t>
  set_handles(std::vector<GuiViewportManipulatorHandle> handles) {
    for (auto& handle : handles) {
      const auto validated = validate_handle(handle);
      if (validated.has_error())
        return Result<std::size_t>::failure(validated.error());
      handle.axis = normalized(handle.axis);
      handle.plane_normal = normalized(handle.plane_normal);
      handle.reference_direction = normalized(handle.reference_direction);
    }
    std::sort(handles.begin(), handles.end(), [](const auto& left, const auto& right) {
      return left.id < right.id;
    });
    for (std::size_t index = 1; index < handles.size(); ++index)
      if (handles[index - 1].id == handles[index].id)
        return Result<std::size_t>::failure(
            Error::validation(handles[index].id, "manipulator handle ids must be unique"));
    handles_ = std::move(handles);
    cancel();
    ++revision_;
    return Result<std::size_t>::success(handles_.size());
  }

  void clear_handles() noexcept {
    handles_.clear();
    cancel();
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
    if (!mapping_)
      return result;
    result.reserve(handles_.size());
    for (const auto& handle : handles_) {
      auto presentation = presentation_for(handle);
      if (presentation)
        result.push_back(std::move(*presentation));
    }
    return result;
  }

  [[nodiscard]] std::vector<GuiViewportManipulatorHit>
  hits_at(GuiSketchScreenPoint pointer) const {
    std::vector<GuiViewportManipulatorHit> hits;
    if (!finite(pointer))
      return hits;
    for (const auto& presentation : presentations()) {
      const auto* handle = find_handle(presentation.handle_id);
      if (!handle)
        continue;
      const double distance = is_ring(handle->kind)
                                  ? std::abs(screen_distance(pointer, presentation.anchor) -
                                             presentation.ring_radius_dip)
                                  : point_segment_distance(pointer, presentation.anchor,
                                                           presentation.endpoint);
      if (distance <= handle->hit_tolerance_dip)
        hits.push_back({handle->id, distance});
    }
    std::sort(hits.begin(), hits.end(), [](const auto& left, const auto& right) {
      if (std::abs(left.screen_distance_dip - right.screen_distance_dip) > kEpsilon)
        return left.screen_distance_dip < right.screen_distance_dip;
      return left.handle_id < right.handle_id;
    });
    return hits;
  }

  [[nodiscard]] bool begin_drag(GuiSketchScreenPoint pointer) {
    if (!mapping_ || active_handle_id_ || !finite(pointer))
      return false;
    const auto hits = hits_at(pointer);
    if (hits.empty())
      return false;
    const auto* handle = find_handle(hits.front().handle_id);
    if (!handle)
      return false;
    auto measurement = pointer_measurement(*handle, pointer);
    if (!measurement)
      return false;
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
    if (!active_handle_id_ || !finite(pointer))
      return false;
    latest_pointer_ = pointer;
    if (numeric_override_) {
      ++revision_;
      return true;
    }
    const auto* handle = find_handle(*active_handle_id_);
    if (!handle)
      return false;
    auto measurement = pointer_measurement(*handle, pointer);
    if (!measurement || !drag_start_measurement_)
      return false;
    const double candidate_value = drag_value(*handle, *measurement);
    candidate_ = make_candidate(*handle, candidate_value,
                                GuiViewportManipulatorInputSource::Drag);
    numeric_text_ = format_candidate(*candidate_);
    ++revision_;
    return true;
  }

  [[nodiscard]] bool set_numeric_input(std::string text) {
    if (!active_handle_id_)
      return false;
    const auto* handle = find_handle(*active_handle_id_);
    if (!handle)
      return false;
    auto parsed = parse_numeric(*handle, text);
    if (!parsed)
      return false;
    numeric_override_ = true;
    candidate_ = make_candidate(*handle, *parsed,
                                GuiViewportManipulatorInputSource::Numeric);
    numeric_text_ = format_candidate(*candidate_);
    ++revision_;
    return true;
  }

  [[nodiscard]] bool clear_numeric_override() {
    if (!active_handle_id_ || !numeric_override_)
      return false;
    numeric_override_ = false;
    return update_drag(latest_pointer_.value_or(drag_start_pointer_.value_or(
        GuiSketchScreenPoint{})));
  }

  [[nodiscard]] std::optional<GuiViewportManipulatorCandidate>
  end_drag(GuiSketchScreenPoint final_pointer) {
    if (!active_handle_id_)
      return std::nullopt;
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
    active_handle_id_.reset();
    drag_start_pointer_.reset();
    latest_pointer_.reset();
    drag_start_measurement_.reset();
    candidate_.reset();
    numeric_text_.clear();
    numeric_override_ = false;
    ++revision_;
  }

  [[nodiscard]] const std::vector<GuiViewportManipulatorHandle>& handles() const noexcept {
    return handles_;
  }

  [[nodiscard]] const std::optional<GuiViewportManipulatorCandidate>&
  candidate() const noexcept {
    return candidate_;
  }

  [[nodiscard]] std::string_view active_handle_id() const noexcept {
    return active_handle_id_.value_or(empty_string());
  }

  [[nodiscard]] std::string_view numeric_text() const noexcept { return numeric_text_; }

  [[nodiscard]] bool numeric_override_active() const noexcept {
    return numeric_override_;
  }

  [[nodiscard]] std::size_t revision() const noexcept { return revision_; }

private:
  static constexpr double kEpsilon = 1.0e-9;
  static constexpr double kPi = 3.141592653589793238462643383279502884;

  [[nodiscard]] static const std::string& empty_string() noexcept {
    static const std::string value;
    return value;
  }

  [[nodiscard]] static Result<std::size_t>
  validate_handle(const GuiViewportManipulatorHandle& handle) {
    if (handle.id.empty())
      return Result<std::size_t>::failure(
          Error::validation("gui.viewport_manipulator", "handle id must not be empty"));
    if (!finite(handle.origin) || !finite(handle.axis) ||
        !finite(handle.plane_normal) || !finite(handle.reference_direction) ||
        !std::isfinite(handle.reference_value) ||
        !std::isfinite(handle.display_size_dip) || handle.display_size_dip <= 0.0 ||
        !std::isfinite(handle.hit_tolerance_dip) || handle.hit_tolerance_dip <= 0.0 ||
        !std::isfinite(handle.count_model_step) || handle.count_model_step <= 0.0 ||
        std::isnan(handle.minimum_value) || std::isnan(handle.maximum_value) ||
        handle.minimum_value > handle.maximum_value)
      return Result<std::size_t>::failure(
          Error::validation(handle.id, "manipulator handle values are invalid"));
    if (length(handle.axis) <= kEpsilon)
      return Result<std::size_t>::failure(
          Error::validation(handle.id, "manipulator axis must be non-zero"));
    if ((is_ring(handle.kind) || handle.kind == GuiViewportManipulatorKind::Radial) &&
        length(handle.plane_normal) <= kEpsilon)
      return Result<std::size_t>::failure(
          Error::validation(handle.id, "radial and angular handles require a plane normal"));
    if ((is_ring(handle.kind) || handle.kind == GuiViewportManipulatorKind::Radial) &&
        length(project_to_plane(handle.reference_direction, handle.plane_normal)) <= kEpsilon)
      return Result<std::size_t>::failure(
          Error::validation(handle.id,
                            "reference direction must lie outside the handle normal"));
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] static std::vector<GuiViewportManipulatorHandle>
  triad(std::string_view prefix, Point3 origin, GuiViewportManipulatorKind kind,
        double display_size_dip) {
    const std::array<std::pair<std::string_view, Vector3>, 3> axes{{
        {"x", {1.0, 0.0, 0.0}},
        {"y", {0.0, 1.0, 0.0}},
        {"z", {0.0, 0.0, 1.0}},
    }};
    std::vector<GuiViewportManipulatorHandle> result;
    result.reserve(3);
    for (const auto& [suffix, axis] : axes) {
      GuiViewportManipulatorHandle handle;
      handle.id = std::string(prefix) + "." + std::string(suffix);
      handle.kind = kind;
      handle.origin = origin;
      handle.axis = axis;
      handle.plane_normal = axis;
      handle.reference_direction =
          axis.z == 0.0 ? Vector3{0.0, 0.0, 1.0} : Vector3{1.0, 0.0, 0.0};
      handle.display_size_dip = display_size_dip;
      result.push_back(std::move(handle));
    }
    return result;
  }

  [[nodiscard]] static GuiViewportManipulatorValueKind
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

  [[nodiscard]] static bool is_ring(GuiViewportManipulatorKind kind) noexcept {
    return kind == GuiViewportManipulatorKind::Angular ||
           kind == GuiViewportManipulatorKind::RotateAxis;
  }

  [[nodiscard]] const GuiViewportManipulatorHandle*
  find_handle(std::string_view id) const noexcept {
    const auto found = std::lower_bound(handles_.begin(), handles_.end(), id,
                                        [](const auto& handle, std::string_view value) {
                                          return handle.id < value;
                                        });
    return found != handles_.end() && found->id == id ? &*found : nullptr;
  }

  [[nodiscard]] std::optional<GuiViewportManipulatorPresentation>
  presentation_for(const GuiViewportManipulatorHandle& handle) const {
    auto anchor = mapping_->model_to_screen(handle.origin);
    if (anchor.has_error())
      return std::nullopt;
    const Vector3 presentation_axis =
        is_ring(handle.kind) || handle.kind == GuiViewportManipulatorKind::Radial
            ? normalized(project_to_plane(handle.reference_direction, handle.plane_normal))
            : handle.axis;
    auto projected = mapping_->model_to_screen(add(handle.origin, presentation_axis));
    GuiSketchScreenPoint direction{};
    if (!projected.has_error())
      direction = {projected.value().x - anchor.value().x,
                   projected.value().y - anchor.value().y};
    if (screen_length(direction) <= kEpsilon)
      direction = fallback_screen_direction(presentation_axis);
    direction = screen_normalized(direction);

    GuiViewportManipulatorPresentation presentation;
    presentation.handle_id = handle.id;
    presentation.kind = handle.kind;
    presentation.anchor = anchor.value();
    presentation.endpoint =
        {presentation.anchor.x + direction.x * handle.display_size_dip,
         presentation.anchor.y + direction.y * handle.display_size_dip};
    presentation.ring_radius_dip = is_ring(handle.kind) ? handle.display_size_dip : 0.0;
    presentation.active = active_handle_id_ && *active_handle_id_ == handle.id;
    if (presentation.active && candidate_)
      presentation.value_text = format_candidate(*candidate_);
    return presentation;
  }

  [[nodiscard]] std::optional<double>
  pointer_measurement(const GuiViewportManipulatorHandle& handle,
                      GuiSketchScreenPoint pointer) const {
    if (!mapping_)
      return std::nullopt;
    auto ray = mapping_->screen_to_ray(pointer);
    if (ray.has_error() || !finite(ray.value().origin) || !finite(ray.value().direction) ||
        length(ray.value().direction) <= kEpsilon)
      return std::nullopt;
    if (is_ring(handle.kind))
      return angular_measurement(handle, ray.value());
    if (handle.kind == GuiViewportManipulatorKind::Radial)
      return radial_measurement(handle, ray.value());
    return linear_measurement(handle, ray.value(), pointer);
  }

  [[nodiscard]] std::optional<double>
  linear_measurement(const GuiViewportManipulatorHandle& handle,
                     const GuiSketchViewRay& ray,
                     GuiSketchScreenPoint pointer) const {
    const Vector3 axis = normalized(handle.axis);
    const Vector3 direction = normalized(ray.direction);
    const Vector3 w0 = subtract(handle.origin, ray.origin);
    const double a = dot(axis, axis);
    const double b = dot(axis, direction);
    const double c = dot(direction, direction);
    const double d = dot(axis, w0);
    const double e = dot(direction, w0);
    const double denominator = a * c - b * b;
    if (std::abs(denominator) > kEpsilon)
      return (b * e - c * d) / denominator;

    auto anchor = mapping_->model_to_screen(handle.origin);
    auto one_unit = mapping_->model_to_screen(add(handle.origin, axis));
    if (anchor.has_error() || one_unit.has_error())
      return std::nullopt;
    GuiSketchScreenPoint projected_axis{one_unit.value().x - anchor.value().x,
                                        one_unit.value().y - anchor.value().y};
    double pixels_per_model = screen_length(projected_axis);
    if (pixels_per_model <= kEpsilon) {
      projected_axis = fallback_screen_direction(axis);
      pixels_per_model = 1.0;
    }
    projected_axis = screen_normalized(projected_axis);
    return ((pointer.x - anchor.value().x) * projected_axis.x +
            (pointer.y - anchor.value().y) * projected_axis.y) /
           pixels_per_model;
  }

  [[nodiscard]] static std::optional<Point3>
  intersect_plane(const GuiSketchViewRay& ray, Point3 origin, Vector3 normal) {
    normal = normalized(normal);
    const double denominator = dot(ray.direction, normal);
    if (std::abs(denominator) <= kEpsilon)
      return std::nullopt;
    const double distance = dot(subtract(origin, ray.origin), normal) / denominator;
    if (!std::isfinite(distance))
      return std::nullopt;
    return add(ray.origin, scale(ray.direction, distance));
  }

  [[nodiscard]] static std::optional<double>
  radial_measurement(const GuiViewportManipulatorHandle& handle,
                     const GuiSketchViewRay& ray) {
    const auto intersection = intersect_plane(ray, handle.origin, handle.plane_normal);
    if (!intersection)
      return std::nullopt;
    return length(project_to_plane(subtract(*intersection, handle.origin),
                                   handle.plane_normal));
  }

  [[nodiscard]] static std::optional<double>
  angular_measurement(const GuiViewportManipulatorHandle& handle,
                      const GuiSketchViewRay& ray) {
    const auto intersection = intersect_plane(ray, handle.origin, handle.plane_normal);
    if (!intersection)
      return std::nullopt;
    const Vector3 normal = normalized(handle.plane_normal);
    const Vector3 reference =
        normalized(project_to_plane(handle.reference_direction, normal));
    const Vector3 radial =
        normalized(project_to_plane(subtract(*intersection, handle.origin), normal));
    if (length(reference) <= kEpsilon || length(radial) <= kEpsilon)
      return std::nullopt;
    const double sine = dot(cross(reference, radial), normal);
    const double cosine = std::clamp(dot(reference, radial), -1.0, 1.0);
    return std::atan2(sine, cosine) * 180.0 / kPi;
  }

  [[nodiscard]] double drag_value(const GuiViewportManipulatorHandle& handle,
                                  double current_measurement) const noexcept {
    double delta = current_measurement - drag_start_measurement_.value_or(current_measurement);
    if (is_ring(handle.kind)) {
      while (delta > 180.0) delta -= 360.0;
      while (delta < -180.0) delta += 360.0;
    }
    if (handle.kind == GuiViewportManipulatorKind::PatternCount)
      delta /= handle.count_model_step;
    return clamp_value(handle, handle.reference_value + delta);
  }

  [[nodiscard]] static GuiViewportManipulatorCandidate
  make_candidate(const GuiViewportManipulatorHandle& handle, double value,
                 GuiViewportManipulatorInputSource source) {
    GuiViewportManipulatorCandidate candidate;
    candidate.handle_id = handle.id;
    candidate.value_kind = value_kind(handle.kind);
    candidate.source = source;
    candidate.scalar_value = clamp_value(handle, value);
    if (handle.kind == GuiViewportManipulatorKind::PatternCount) {
      candidate.count_value = std::max(1, static_cast<int>(std::llround(candidate.scalar_value)));
      candidate.scalar_value = static_cast<double>(candidate.count_value);
    }
    if (handle.kind == GuiViewportManipulatorKind::TranslateAxis ||
        handle.kind == GuiViewportManipulatorKind::RotateAxis)
      candidate.vector_value = scale(normalized(handle.axis), candidate.scalar_value);
    return candidate;
  }

  [[nodiscard]] static double
  clamp_value(const GuiViewportManipulatorHandle& handle, double value) noexcept {
    if (handle.kind == GuiViewportManipulatorKind::Radial ||
        handle.kind == GuiViewportManipulatorKind::PatternSpacing)
      value = std::max(0.0, value);
    return std::clamp(value, handle.minimum_value, handle.maximum_value);
  }

  [[nodiscard]] static std::optional<double>
  parse_numeric(const GuiViewportManipulatorHandle& handle, std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      return std::nullopt;
    const auto last = text.find_last_not_of(" \t\r\n");
    text = text.substr(first, last - first + 1);
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || errno == ERANGE || !std::isfinite(value))
      return std::nullopt;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    const std::string suffix(end);
    const auto kind = value_kind(handle.kind);
    const std::string_view expected = manipulator_unit(kind);
    if (!suffix.empty() && suffix != expected)
      return std::nullopt;
    if (kind == GuiViewportManipulatorValueKind::Count &&
        std::abs(value - std::round(value)) > kEpsilon)
      return std::nullopt;
    return clamp_value(handle, value);
  }

  [[nodiscard]] static std::string
  format_candidate(const GuiViewportManipulatorCandidate& candidate) {
    std::ostringstream stream;
    if (candidate.value_kind == GuiViewportManipulatorValueKind::Count) {
      stream << candidate.count_value;
    } else {
      stream << std::fixed << std::setprecision(3) << candidate.scalar_value;
      std::string number = stream.str();
      while (number.size() > 1 && number.back() == '0') number.pop_back();
      if (!number.empty() && number.back() == '.') number.pop_back();
      stream.str({});
      stream.clear();
      stream << number;
    }
    const auto unit = manipulator_unit(candidate.value_kind);
    if (!unit.empty()) stream << ' ' << unit;
    return stream.str();
  }

  [[nodiscard]] static bool finite(Point3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
  }

  [[nodiscard]] static bool finite(Vector3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
  }

  [[nodiscard]] static bool finite(GuiSketchScreenPoint value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
  }

  [[nodiscard]] static Point3 add(Point3 point, Vector3 vector) noexcept {
    return {point.x + vector.x, point.y + vector.y, point.z + vector.z};
  }

  [[nodiscard]] static Vector3 subtract(Point3 left, Point3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
  }

  [[nodiscard]] static Vector3 add(Vector3 left, Vector3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
  }

  [[nodiscard]] static Vector3 scale(Vector3 vector, double factor) noexcept {
    return {vector.x * factor, vector.y * factor, vector.z * factor};
  }

  [[nodiscard]] static double dot(Vector3 left, Vector3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
  }

  [[nodiscard]] static Vector3 cross(Vector3 left, Vector3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
  }

  [[nodiscard]] static double length(Vector3 vector) noexcept {
    return std::sqrt(dot(vector, vector));
  }

  [[nodiscard]] static Vector3 normalized(Vector3 vector) noexcept {
    const double magnitude = length(vector);
    return magnitude <= kEpsilon ? Vector3{} : scale(vector, 1.0 / magnitude);
  }

  [[nodiscard]] static Vector3 project_to_plane(Vector3 vector, Vector3 normal) noexcept {
    normal = normalized(normal);
    return add(vector, scale(normal, -dot(vector, normal)));
  }

  [[nodiscard]] static double screen_length(GuiSketchScreenPoint vector) noexcept {
    return std::hypot(vector.x, vector.y);
  }

  [[nodiscard]] static GuiSketchScreenPoint
  screen_normalized(GuiSketchScreenPoint vector) noexcept {
    const double magnitude = screen_length(vector);
    return magnitude <= kEpsilon ? GuiSketchScreenPoint{1.0, 0.0}
                                 : GuiSketchScreenPoint{vector.x / magnitude,
                                                        vector.y / magnitude};
  }

  [[nodiscard]] static GuiSketchScreenPoint
  fallback_screen_direction(Vector3 axis) noexcept {
    axis = normalized(axis);
    const double ax = std::abs(axis.x);
    const double ay = std::abs(axis.y);
    const double az = std::abs(axis.z);
    if (ax >= ay && ax >= az) return {axis.x < 0.0 ? -1.0 : 1.0, 0.0};
    if (ay >= ax && ay >= az) return {0.0, axis.y < 0.0 ? 1.0 : -1.0};
    const double sign = axis.z < 0.0 ? -1.0 : 1.0;
    return {0.7071067811865475 * sign, -0.7071067811865475 * sign};
  }

  [[nodiscard]] static double
  screen_distance(GuiSketchScreenPoint left, GuiSketchScreenPoint right) noexcept {
    return std::hypot(left.x - right.x, left.y - right.y);
  }

  [[nodiscard]] static double
  point_segment_distance(GuiSketchScreenPoint point, GuiSketchScreenPoint start,
                         GuiSketchScreenPoint end) noexcept {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double squared = dx * dx + dy * dy;
    if (squared <= kEpsilon)
      return screen_distance(point, start);
    const double t = std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) /
                                    squared,
                                0.0, 1.0);
    return screen_distance(point, {start.x + t * dx, start.y + t * dy});
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
