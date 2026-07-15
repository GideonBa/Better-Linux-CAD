#include "blcad/gui/gui_sketch_interaction.hpp"

#include "blcad/geometry/reference_driven_sketch_helper.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/sketch_reference_projector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace blcad::gui {
namespace {

constexpr double kEpsilon = 1.0e-9;
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr std::size_t kArcSamples = 48;
constexpr std::size_t kSplineSamples = 64;
constexpr std::size_t kCircleSamples = 72;
constexpr double kReferenceLineExtent = 100000.0;

Error interaction_error(std::string message) {
  return Error::validation("gui.sketch_interaction", std::move(message));
}

bool finite(double value) noexcept { return std::isfinite(value); }

bool finite(Point2 point) noexcept { return finite(point.x) && finite(point.y); }

bool finite(Point3 point) noexcept {
  return finite(point.x) && finite(point.y) && finite(point.z);
}

bool finite(Vector3 vector) noexcept {
  return finite(vector.x) && finite(vector.y) && finite(vector.z);
}

double square(double value) noexcept { return value * value; }

double distance(Point2 first, Point2 second) noexcept {
  return std::sqrt(square(first.x - second.x) + square(first.y - second.y));
}

double screen_distance(GuiSketchScreenPoint first, GuiSketchScreenPoint second) noexcept {
  return std::sqrt(square(first.x - second.x) + square(first.y - second.y));
}

double dot(Vector3 first, Vector3 second) noexcept {
  return first.x * second.x + first.y * second.y + first.z * second.z;
}

Vector3 displacement(Point3 from, Point3 to) noexcept {
  return {to.x - from.x, to.y - from.y, to.z - from.z};
}

Point3 translated(Point3 point, Vector3 direction, double scale) noexcept {
  return {point.x + direction.x * scale, point.y + direction.y * scale,
          point.z + direction.z * scale};
}

Point2 interpolate(Point2 first, Point2 second, double t) noexcept {
  return {first.x + (second.x - first.x) * t,
          first.y + (second.y - first.y) * t};
}

GuiSketchScreenPoint interpolate(GuiSketchScreenPoint first, GuiSketchScreenPoint second,
                                 double t) noexcept {
  return {first.x + (second.x - first.x) * t,
          first.y + (second.y - first.y) * t};
}

double cross(Point2 first, Point2 second, Point2 third) noexcept {
  return (second.x - first.x) * (third.y - first.y) -
         (second.y - first.y) * (third.x - first.x);
}

double vector_cross(Vector2 first, Vector2 second) noexcept {
  return first.x * second.y - first.y * second.x;
}

Vector2 vector_between(Point2 first, Point2 second) noexcept {
  return {second.x - first.x, second.y - first.y};
}

Point2 point_plus(Point2 point, Vector2 direction, double scale) noexcept {
  return {point.x + direction.x * scale, point.y + direction.y * scale};
}

double vector_length(Vector2 vector) noexcept {
  return std::sqrt(square(vector.x) + square(vector.y));
}

std::optional<Vector2> normalized(Vector2 vector) noexcept {
  const double length = vector_length(vector);
  if (!finite(length) || length <= kEpsilon)
    return std::nullopt;
  return Vector2{vector.x / length, vector.y / length};
}

std::string entity_semantic(const Sketch& sketch, const SketchEntityId& id) {
  return "sketch/" + sketch.id().value() + "/entity/" + id.value();
}

std::string profile_semantic(const Sketch& sketch, const ProfileId& id) {
  return "sketch/" + sketch.id().value() + "/profile/" + id.value();
}

std::string dimension_semantic(const Sketch& sketch, const SketchDimensionId& id) {
  return "sketch/" + sketch.id().value() + "/dimension/" + id.value();
}

std::string constraint_semantic(const Sketch& sketch, const SketchConstraintId& id) {
  return "sketch/" + sketch.id().value() + "/constraint/" + id.value();
}

struct CircleArcGeometry {
  Point2 center;
  double radius{0.0};
  double start_angle{0.0};
  double sweep_angle{0.0};
};

double normalized_angle(double angle) noexcept {
  double result = std::fmod(angle, kTwoPi);
  if (result < 0.0)
    result += kTwoPi;
  return result;
}

double positive_delta(double from, double to) noexcept {
  return normalized_angle(to - from);
}

std::optional<CircleArcGeometry> circle_arc(Point2 start, Point2 mid, Point2 end) noexcept {
  const double denominator =
      2.0 * (start.x * (mid.y - end.y) + mid.x * (end.y - start.y) +
             end.x * (start.y - mid.y));
  if (!finite(denominator) || std::abs(denominator) <= kEpsilon)
    return std::nullopt;

  const double start_norm = square(start.x) + square(start.y);
  const double mid_norm = square(mid.x) + square(mid.y);
  const double end_norm = square(end.x) + square(end.y);
  Point2 center{
      (start_norm * (mid.y - end.y) + mid_norm * (end.y - start.y) +
       end_norm * (start.y - mid.y)) /
          denominator,
      (start_norm * (end.x - mid.x) + mid_norm * (start.x - end.x) +
       end_norm * (mid.x - start.x)) /
          denominator};
  const double radius = distance(center, start);
  if (!finite(center) || !finite(radius) || radius <= kEpsilon)
    return std::nullopt;

  const double start_angle = std::atan2(start.y - center.y, start.x - center.x);
  const double mid_angle = std::atan2(mid.y - center.y, mid.x - center.x);
  const double end_angle = std::atan2(end.y - center.y, end.x - center.x);
  const double ccw_sweep = positive_delta(start_angle, end_angle);
  const double ccw_to_mid = positive_delta(start_angle, mid_angle);
  const bool counter_clockwise = ccw_to_mid <= ccw_sweep + kEpsilon;
  const double sweep = counter_clockwise ? ccw_sweep : -positive_delta(end_angle, start_angle);
  if (std::abs(sweep) <= kEpsilon)
    return std::nullopt;
  return CircleArcGeometry{center, radius, start_angle, sweep};
}

bool angle_on_arc(const CircleArcGeometry& arc, double angle) noexcept {
  if (arc.sweep_angle > 0.0)
    return positive_delta(arc.start_angle, angle) <= arc.sweep_angle + kEpsilon;
  return positive_delta(angle, arc.start_angle) <= -arc.sweep_angle + kEpsilon;
}

std::vector<Point2> sample_arc(const CircleArcGeometry& arc) {
  std::vector<Point2> points;
  points.reserve(kArcSamples + 1U);
  for (std::size_t index = 0; index <= kArcSamples; ++index) {
    const double t = static_cast<double>(index) / static_cast<double>(kArcSamples);
    const double angle = arc.start_angle + arc.sweep_angle * t;
    points.push_back({arc.center.x + arc.radius * std::cos(angle),
                      arc.center.y + arc.radius * std::sin(angle)});
  }
  return points;
}

std::vector<Point2> sample_spline(const SplineSegment& spline) {
  std::vector<Point2> points;
  points.reserve(kSplineSamples + 1U);
  for (std::size_t index = 0; index <= kSplineSamples; ++index) {
    const double t = static_cast<double>(index) / static_cast<double>(kSplineSamples);
    const double u = 1.0 - t;
    const Point2 p0 = spline.start();
    const Point2 p1 = spline.control1();
    const Point2 p2 = spline.control2();
    const Point2 p3 = spline.end();
    points.push_back({u * u * u * p0.x + 3.0 * u * u * t * p1.x +
                          3.0 * u * t * t * p2.x + t * t * t * p3.x,
                      u * u * u * p0.y + 3.0 * u * u * t * p1.y +
                          3.0 * u * t * t * p2.y + t * t * t * p3.y});
  }
  return points;
}

std::vector<Point2> sample_circle(Point2 center, double radius) {
  std::vector<Point2> points;
  points.reserve(kCircleSamples + 1U);
  for (std::size_t index = 0; index <= kCircleSamples; ++index) {
    const double angle = kTwoPi * static_cast<double>(index) /
                         static_cast<double>(kCircleSamples);
    points.push_back({center.x + radius * std::cos(angle),
                      center.y + radius * std::sin(angle)});
  }
  return points;
}

void add_point(GuiSketchInteractionScene& scene, std::string semantic_id,
               std::string candidate_id, Point2 point, GuiSketchSnapKind kind) {
  if (!finite(point))
    return;
  scene.points.push_back(
      {std::move(semantic_id), std::move(candidate_id), point, kind});
}

void add_curve(GuiSketchInteractionScene& scene, std::string semantic_id,
               std::string candidate_id, GuiSketchCurveKind kind,
               std::vector<Point2> polyline, bool closed = false) {
  if (polyline.size() < 2U ||
      std::any_of(polyline.begin(), polyline.end(), [](Point2 point) { return !finite(point); }))
    return;
  scene.curves.push_back({std::move(semantic_id), std::move(candidate_id), kind,
                          std::move(polyline), closed});
}

std::optional<double> length_parameter(const PartDocument& document, const ParameterId& id) {
  const auto* parameter = document.find_parameter(id);
  if (parameter == nullptr || parameter->type() != ParameterType::Length)
    return std::nullopt;
  const double value = parameter->value().millimeters();
  return finite(value) && value > 0.0 ? std::optional<double>(value) : std::nullopt;
}

std::optional<std::size_t> count_parameter(const PartDocument& document, const ParameterId& id) {
  const auto* parameter = document.find_parameter(id);
  if (parameter == nullptr || parameter->type() != ParameterType::Count)
    return std::nullopt;
  const std::size_t value = parameter->value().count_value();
  return value > 0U ? std::optional<std::size_t>(value) : std::nullopt;
}

struct NearestPlanePoint {
  Point2 point;
  double distance{std::numeric_limits<double>::infinity()};
};

NearestPlanePoint nearest_on_polyline(Point2 query, const std::vector<Point2>& polyline) noexcept {
  NearestPlanePoint result;
  for (std::size_t index = 1; index < polyline.size(); ++index) {
    const Point2 start = polyline[index - 1U];
    const Point2 end = polyline[index];
    const Vector2 segment = vector_between(start, end);
    const double denominator = square(segment.x) + square(segment.y);
    double t = 0.0;
    if (denominator > kEpsilon)
      t = std::clamp(((query.x - start.x) * segment.x + (query.y - start.y) * segment.y) /
                         denominator,
                     0.0, 1.0);
    const Point2 candidate = point_plus(start, segment, t);
    const double candidate_distance = distance(query, candidate);
    if (candidate_distance < result.distance) {
      result = {candidate, candidate_distance};
    }
  }
  return result;
}

struct NearestScreenPoint {
  GuiSketchScreenPoint point;
  Point2 plane_point;
  double distance{std::numeric_limits<double>::infinity()};
};

NearestScreenPoint nearest_on_screen_polyline(
    GuiSketchScreenPoint query, const std::vector<GuiSketchScreenPoint>& screen,
    const std::vector<Point2>& plane) noexcept {
  NearestScreenPoint result;
  if (screen.size() != plane.size())
    return result;
  for (std::size_t index = 1; index < screen.size(); ++index) {
    const auto start = screen[index - 1U];
    const auto end = screen[index];
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double denominator = square(dx) + square(dy);
    double t = 0.0;
    if (denominator > kEpsilon)
      t = std::clamp(((query.x - start.x) * dx + (query.y - start.y) * dy) /
                         denominator,
                     0.0, 1.0);
    const auto candidate = interpolate(start, end, t);
    const double candidate_distance = screen_distance(query, candidate);
    if (candidate_distance < result.distance)
      result = {candidate, interpolate(plane[index - 1U], plane[index], t), candidate_distance};
  }
  return result;
}

std::optional<Point2> segment_intersection(Point2 a, Point2 b, Point2 c, Point2 d) noexcept {
  const Vector2 first = vector_between(a, b);
  const Vector2 second = vector_between(c, d);
  const double denominator = vector_cross(first, second);
  if (std::abs(denominator) <= kEpsilon)
    return std::nullopt;
  const Vector2 delta = vector_between(a, c);
  const double t = vector_cross(delta, second) / denominator;
  const double u = vector_cross(delta, first) / denominator;
  if (t < -kEpsilon || t > 1.0 + kEpsilon || u < -kEpsilon || u > 1.0 + kEpsilon)
    return std::nullopt;
  return point_plus(a, first, std::clamp(t, 0.0, 1.0));
}

void append_intersection(std::vector<Point2>& intersections, Point2 point) {
  if (!finite(point))
    return;
  const bool duplicate = std::any_of(intersections.begin(), intersections.end(),
                                     [point](Point2 existing) {
                                       return distance(existing, point) <= 1.0e-7;
                                     });
  if (!duplicate)
    intersections.push_back(point);
}

void build_intersections(GuiSketchInteractionScene& scene) {
  for (std::size_t first_curve = 0; first_curve < scene.curves.size(); ++first_curve) {
    const auto& first = scene.curves[first_curve].polyline;
    for (std::size_t second_curve = first_curve + 1U; second_curve < scene.curves.size();
         ++second_curve) {
      const auto& second = scene.curves[second_curve].polyline;
      for (std::size_t first_segment = 1; first_segment < first.size(); ++first_segment) {
        for (std::size_t second_segment = 1; second_segment < second.size(); ++second_segment) {
          const auto intersection = segment_intersection(
              first[first_segment - 1U], first[first_segment],
              second[second_segment - 1U], second[second_segment]);
          if (intersection)
            append_intersection(scene.intersections, *intersection);
        }
      }
    }
  }
  std::sort(scene.intersections.begin(), scene.intersections.end(), [](Point2 first, Point2 second) {
    if (std::abs(first.x - second.x) > kEpsilon)
      return first.x < second.x;
    return first.y < second.y;
  });
}

std::optional<Point2> entity_anchor(const Sketch& sketch, const SketchEntityId& id) {
  if (const auto* line = sketch.find_line_segment(id))
    return interpolate(line->start(), line->end(), 0.5);
  if (const auto* arc = sketch.find_arc_segment(id))
    return arc->mid();
  if (const auto* spline = sketch.find_spline_segment(id))
    return sample_spline(*spline)[kSplineSamples / 2U];
  return std::nullopt;
}

std::optional<Point2> target_anchor(
    const Sketch& sketch, const SketchReferenceTarget& target,
    const std::unordered_map<std::string, Point2>& projected_points,
    const std::unordered_map<std::string, Point2>& projected_lines) {
  switch (target.kind()) {
  case SketchReferenceTargetKind::LineSegment:
    return entity_anchor(sketch, target.entity());
  case SketchReferenceTargetKind::LineSegmentStart:
    if (const auto* line = sketch.find_line_segment(target.entity()))
      return line->start();
    break;
  case SketchReferenceTargetKind::LineSegmentEnd:
    if (const auto* line = sketch.find_line_segment(target.entity()))
      return line->end();
    break;
  case SketchReferenceTargetKind::ProjectedPoint: {
    const auto found = projected_points.find(target.entity().value());
    if (found != projected_points.end())
      return found->second;
    break;
  }
  case SketchReferenceTargetKind::ProjectedLine: {
    const auto found = projected_lines.find(target.entity().value());
    if (found != projected_lines.end())
      return found->second;
    break;
  }
  }
  return std::nullopt;
}

int hit_priority(GuiSketchHitKind kind) noexcept {
  switch (kind) {
  case GuiSketchHitKind::Point: return 0;
  case GuiSketchHitKind::Curve: return 1;
  case GuiSketchHitKind::Dimension: return 2;
  case GuiSketchHitKind::Glyph: return 3;
  }
  return 4;
}

int snap_priority(GuiSketchSnapKind kind) noexcept {
  switch (kind) {
  case GuiSketchSnapKind::Endpoint: return 0;
  case GuiSketchSnapKind::Center: return 1;
  case GuiSketchSnapKind::Intersection: return 2;
  case GuiSketchSnapKind::Quadrant: return 3;
  case GuiSketchSnapKind::Midpoint: return 4;
  case GuiSketchSnapKind::Origin: return 5;
  case GuiSketchSnapKind::Axis: return 6;
  case GuiSketchSnapKind::HorizontalInference: return 7;
  case GuiSketchSnapKind::VerticalInference: return 8;
  case GuiSketchSnapKind::AlignmentX: return 9;
  case GuiSketchSnapKind::AlignmentY: return 10;
  case GuiSketchSnapKind::Nearest: return 11;
  case GuiSketchSnapKind::Grid: return 12;
  case GuiSketchSnapKind::None: return 13;
  }
  return 13;
}

std::string inference_text(GuiSketchSnapKind kind, std::string_view candidate_id) {
  switch (kind) {
  case GuiSketchSnapKind::HorizontalInference: return "Horizontal";
  case GuiSketchSnapKind::VerticalInference: return "Vertical";
  case GuiSketchSnapKind::AlignmentX: return "Aligned X with " + std::string(candidate_id);
  case GuiSketchSnapKind::AlignmentY: return "Aligned Y with " + std::string(candidate_id);
  case GuiSketchSnapKind::None: return {};
  default:
    return std::string(to_string(kind)) +
           (candidate_id.empty() ? std::string{} : ": " + std::string(candidate_id));
  }
}

bool segment_intersects_rect(GuiSketchScreenPoint start, GuiSketchScreenPoint end,
                             const GuiSketchScreenRect& rect) noexcept {
  if (rect.contains(start) || rect.contains(end))
    return true;
  const std::array<std::pair<GuiSketchScreenPoint, GuiSketchScreenPoint>, 4> edges{{
      {{rect.left(), rect.top()}, {rect.right(), rect.top()}},
      {{rect.right(), rect.top()}, {rect.right(), rect.bottom()}},
      {{rect.right(), rect.bottom()}, {rect.left(), rect.bottom()}},
      {{rect.left(), rect.bottom()}, {rect.left(), rect.top()}},
  }};
  const Point2 a{start.x, start.y};
  const Point2 b{end.x, end.y};
  for (const auto& [edge_start, edge_end] : edges)
    if (segment_intersection(a, b, {edge_start.x, edge_start.y}, {edge_end.x, edge_end.y}))
      return true;
  return false;
}

} // namespace

double GuiSketchScreenRect::left() const noexcept { return std::min(start.x, end.x); }
double GuiSketchScreenRect::right() const noexcept { return std::max(start.x, end.x); }
double GuiSketchScreenRect::top() const noexcept { return std::min(start.y, end.y); }
double GuiSketchScreenRect::bottom() const noexcept { return std::max(start.y, end.y); }

bool GuiSketchScreenRect::contains(GuiSketchScreenPoint point) const noexcept {
  return point.x >= left() - kEpsilon && point.x <= right() + kEpsilon &&
         point.y >= top() - kEpsilon && point.y <= bottom() + kEpsilon;
}

GuiSketchPlaneMapping::GuiSketchPlaneMapping(GuiSketchPlaneView plane,
                                             ScreenToRay screen_to_ray,
                                             ModelToScreen model_to_screen)
    : plane_(std::move(plane)), screen_to_ray_(std::move(screen_to_ray)),
      model_to_screen_(std::move(model_to_screen)) {}

Result<GuiSketchPlaneMapping>
GuiSketchPlaneMapping::create(GuiSketchPlaneView plane, ScreenToRay screen_to_ray,
                              ModelToScreen model_to_screen) {
  if (!screen_to_ray || !model_to_screen)
    return Result<GuiSketchPlaneMapping>::failure(
        interaction_error("plane mapping requires screen-to-ray and model-to-screen providers"));
  if (!finite(plane.origin) || !finite(plane.x_axis) || !finite(plane.y_axis) ||
      !finite(plane.normal) || std::abs(dot(plane.normal, plane.normal)) <= kEpsilon)
    return Result<GuiSketchPlaneMapping>::failure(
        interaction_error("plane mapping requires finite non-degenerate workplane axes"));
  return Result<GuiSketchPlaneMapping>::success(
      GuiSketchPlaneMapping(std::move(plane), std::move(screen_to_ray),
                            std::move(model_to_screen)));
}

Result<GuiSketchPlaneMapping>
GuiSketchPlaneMapping::create_orthographic(GuiSketchPlaneView plane,
                                           double viewport_width_dip,
                                           double viewport_height_dip,
                                           Point2 plane_center,
                                           double model_units_per_dip) {
  if (!finite(viewport_width_dip) || !finite(viewport_height_dip) ||
      !finite(model_units_per_dip) || viewport_width_dip <= 0.0 ||
      viewport_height_dip <= 0.0 || model_units_per_dip <= 0.0 || !finite(plane_center))
    return Result<GuiSketchPlaneMapping>::failure(
        interaction_error("orthographic mapping requires positive finite viewport and scale values"));

  const GuiSketchPlaneView provider_plane = plane;
  ScreenToRay screen_to_ray = [provider_plane, viewport_width_dip, viewport_height_dip,
                               plane_center, model_units_per_dip](GuiSketchScreenPoint point) {
    if (!finite(point.x) || !finite(point.y))
      return Result<GuiSketchViewRay>::failure(interaction_error("screen point must be finite"));
    const Point2 plane_point{
        plane_center.x + (point.x - viewport_width_dip * 0.5) * model_units_per_dip,
        plane_center.y - (point.y - viewport_height_dip * 0.5) * model_units_per_dip};
    const Point3 model_point = provider_plane.plane_to_model(plane_point);
    return Result<GuiSketchViewRay>::success(
        {translated(model_point, provider_plane.normal, -1000.0), provider_plane.normal});
  };
  ModelToScreen model_to_screen = [provider_plane, viewport_width_dip, viewport_height_dip,
                                   plane_center, model_units_per_dip](Point3 point) {
    if (!finite(point))
      return Result<GuiSketchScreenPoint>::failure(interaction_error("model point must be finite"));
    const Point2 plane_point = provider_plane.model_to_plane(point);
    return Result<GuiSketchScreenPoint>::success(
        {viewport_width_dip * 0.5 + (plane_point.x - plane_center.x) / model_units_per_dip,
         viewport_height_dip * 0.5 - (plane_point.y - plane_center.y) / model_units_per_dip});
  };
  return create(std::move(plane), std::move(screen_to_ray), std::move(model_to_screen));
}

const GuiSketchPlaneView& GuiSketchPlaneMapping::plane() const noexcept { return plane_; }

Result<GuiSketchViewRay>
GuiSketchPlaneMapping::screen_to_ray(GuiSketchScreenPoint point) const {
  return screen_to_ray_(point);
}

Result<Point2> GuiSketchPlaneMapping::screen_to_plane(GuiSketchScreenPoint point) const {
  auto ray = screen_to_ray(point);
  if (ray.has_error())
    return Result<Point2>::failure(ray.error());
  if (!finite(ray.value().origin) || !finite(ray.value().direction))
    return Result<Point2>::failure(interaction_error("view ray must be finite"));
  const double denominator = dot(ray.value().direction, plane_.normal);
  if (std::abs(denominator) <= kEpsilon)
    return Result<Point2>::failure(interaction_error("view ray is parallel to the active Sketch plane"));
  const double t = dot(displacement(ray.value().origin, plane_.origin), plane_.normal) / denominator;
  const Point3 model = translated(ray.value().origin, ray.value().direction, t);
  return Result<Point2>::success(plane_.model_to_plane(model));
}

Result<Point3> GuiSketchPlaneMapping::screen_to_model(GuiSketchScreenPoint point) const {
  auto plane_point = screen_to_plane(point);
  if (plane_point.has_error())
    return Result<Point3>::failure(plane_point.error());
  return Result<Point3>::success(plane_.plane_to_model(plane_point.value()));
}

Result<GuiSketchScreenPoint> GuiSketchPlaneMapping::plane_to_screen(Point2 point) const {
  if (!finite(point))
    return Result<GuiSketchScreenPoint>::failure(interaction_error("plane point must be finite"));
  return model_to_screen(plane_.plane_to_model(point));
}

Result<GuiSketchScreenPoint> GuiSketchPlaneMapping::model_to_screen(Point3 point) const {
  return model_to_screen_(point);
}

std::string_view to_string(GuiSketchHitKind kind) noexcept {
  switch (kind) {
  case GuiSketchHitKind::Point: return "point";
  case GuiSketchHitKind::Curve: return "curve";
  case GuiSketchHitKind::Dimension: return "dimension";
  case GuiSketchHitKind::Glyph: return "glyph";
  }
  return "curve";
}

std::string_view to_string(GuiSketchSnapKind kind) noexcept {
  switch (kind) {
  case GuiSketchSnapKind::None: return "none";
  case GuiSketchSnapKind::Origin: return "origin";
  case GuiSketchSnapKind::Axis: return "axis";
  case GuiSketchSnapKind::Endpoint: return "endpoint";
  case GuiSketchSnapKind::Midpoint: return "midpoint";
  case GuiSketchSnapKind::Center: return "center";
  case GuiSketchSnapKind::Quadrant: return "quadrant";
  case GuiSketchSnapKind::Intersection: return "intersection";
  case GuiSketchSnapKind::Nearest: return "nearest";
  case GuiSketchSnapKind::Grid: return "grid";
  case GuiSketchSnapKind::HorizontalInference: return "horizontal inference";
  case GuiSketchSnapKind::VerticalInference: return "vertical inference";
  case GuiSketchSnapKind::AlignmentX: return "x alignment";
  case GuiSketchSnapKind::AlignmentY: return "y alignment";
  }
  return "none";
}

std::string_view to_string(GuiSketchBoxSelectionMode mode) noexcept {
  switch (mode) {
  case GuiSketchBoxSelectionMode::Window: return "window";
  case GuiSketchBoxSelectionMode::Crossing: return "crossing";
  }
  return "window";
}

Result<GuiSketchInteractionScene>
GuiSketchInteractionSceneBuilder::build(const PartDocument& document, const Sketch& sketch) const {
  return build_impl(document, sketch, nullptr);
}

Result<GuiSketchInteractionScene>
GuiSketchInteractionSceneBuilder::build(const PartDocument& document, const Sketch& sketch,
                                        const geometry::ShapeCache& shape_cache) const {
  return build_impl(document, sketch, &shape_cache);
}

Result<GuiSketchInteractionScene>
GuiSketchInteractionSceneBuilder::build_impl(const PartDocument& document, const Sketch& sketch,
                                             const geometry::ShapeCache* shape_cache) const {
  if (document.find_sketch(sketch.id()) == nullptr)
    return Result<GuiSketchInteractionScene>::failure(
        interaction_error("interaction scene Sketch must belong to the supplied Part document"));

  GuiSketchInteractionScene scene;
  scene.sketch = sketch.id();
  std::unordered_map<std::string, Point2> projected_points;
  std::unordered_map<std::string, Point2> projected_lines;

  for (const auto& line : sketch.line_segments()) {
    const std::string semantic = entity_semantic(sketch, line.id());
    add_curve(scene, semantic, line.id().value() + ":curve", GuiSketchCurveKind::Line,
              {line.start(), line.end()});
    add_point(scene, semantic, line.id().value() + ":start", line.start(), GuiSketchSnapKind::Endpoint);
    add_point(scene, semantic, line.id().value() + ":end", line.end(), GuiSketchSnapKind::Endpoint);
    add_point(scene, semantic, line.id().value() + ":midpoint",
              interpolate(line.start(), line.end(), 0.5), GuiSketchSnapKind::Midpoint);
  }

  for (const auto& arc : sketch.arc_segments()) {
    const auto geometry = circle_arc(arc.start(), arc.mid(), arc.end());
    if (!geometry)
      return Result<GuiSketchInteractionScene>::failure(
          interaction_error("active Sketch contains a degenerate three-point arc"));
    const std::string semantic = entity_semantic(sketch, arc.id());
    add_curve(scene, semantic, arc.id().value() + ":curve", GuiSketchCurveKind::Arc,
              sample_arc(*geometry));
    add_point(scene, semantic, arc.id().value() + ":start", arc.start(), GuiSketchSnapKind::Endpoint);
    add_point(scene, semantic, arc.id().value() + ":end", arc.end(), GuiSketchSnapKind::Endpoint);
    add_point(scene, semantic, arc.id().value() + ":midpoint", arc.mid(), GuiSketchSnapKind::Midpoint);
    add_point(scene, semantic, arc.id().value() + ":center", geometry->center, GuiSketchSnapKind::Center);
    const std::array<double, 4> quadrants{0.0, kTwoPi * 0.25, kTwoPi * 0.5, kTwoPi * 0.75};
    for (std::size_t index = 0; index < quadrants.size(); ++index) {
      if (!angle_on_arc(*geometry, quadrants[index]))
        continue;
      add_point(scene, semantic, arc.id().value() + ":quadrant:" + std::to_string(index),
                {geometry->center.x + geometry->radius * std::cos(quadrants[index]),
                 geometry->center.y + geometry->radius * std::sin(quadrants[index])},
                GuiSketchSnapKind::Quadrant);
    }
  }

  for (const auto& spline : sketch.spline_segments()) {
    const std::string semantic = entity_semantic(sketch, spline.id());
    add_curve(scene, semantic, spline.id().value() + ":curve", GuiSketchCurveKind::Spline,
              sample_spline(spline));
    add_point(scene, semantic, spline.id().value() + ":start", spline.start(), GuiSketchSnapKind::Endpoint);
    add_point(scene, semantic, spline.id().value() + ":end", spline.end(), GuiSketchSnapKind::Endpoint);
  }

  geometry::SketchReferenceProjector projector;
  for (const auto& reference : sketch.projected_points()) {
    const auto resolved = shape_cache != nullptr
                              ? projector.resolve_point(document, sketch, reference, *shape_cache)
                              : projector.resolve_point(document, sketch, reference);
    if (resolved.has_error()) {
      ++scene.unresolved_reference_count;
      continue;
    }
    projected_points.emplace(reference.id().value(), resolved.value().position);
    add_point(scene, entity_semantic(sketch, reference.id()), reference.id().value() + ":projected",
              resolved.value().position, GuiSketchSnapKind::Endpoint);
  }

  for (const auto& reference : sketch.projected_lines()) {
    const auto resolved = shape_cache != nullptr
                              ? projector.resolve_line(document, sketch, reference, *shape_cache)
                              : projector.resolve_line(document, sketch, reference);
    if (resolved.has_error()) {
      ++scene.unresolved_reference_count;
      continue;
    }
    const auto direction = normalized(resolved.value().direction);
    if (!direction) {
      ++scene.unresolved_reference_count;
      continue;
    }
    projected_lines.emplace(reference.id().value(), resolved.value().point);
    add_curve(scene, entity_semantic(sketch, reference.id()), reference.id().value() + ":projected-line",
              GuiSketchCurveKind::ReferenceLine,
              {point_plus(resolved.value().point, *direction, -kReferenceLineExtent),
               point_plus(resolved.value().point, *direction, kReferenceLineExtent)});
  }

  geometry::ReferenceDrivenSketchHelper reference_helper;
  for (const auto& reference_line : sketch.reference_generated_lines()) {
    const auto* start_constraint = sketch.find_constraint(reference_line.start_constraint());
    const auto* end_constraint = sketch.find_constraint(reference_line.end_constraint());
    if (start_constraint == nullptr || end_constraint == nullptr) {
      ++scene.unresolved_reference_count;
      continue;
    }
    auto line = reference_helper.create_profile_helper_line_from_projected_point_constraints(
        document, sketch, reference_line.id(), *start_constraint, *end_constraint);
    if (line.has_error()) {
      ++scene.unresolved_reference_count;
      continue;
    }
    const std::string semantic = entity_semantic(sketch, reference_line.id());
    add_curve(scene, semantic, reference_line.id().value() + ":reference", GuiSketchCurveKind::Line,
              {line.value().start(), line.value().end()});
    add_point(scene, semantic, reference_line.id().value() + ":start", line.value().start(),
              GuiSketchSnapKind::Endpoint);
    add_point(scene, semantic, reference_line.id().value() + ":end", line.value().end(),
              GuiSketchSnapKind::Endpoint);
    add_point(scene, semantic, reference_line.id().value() + ":midpoint",
              interpolate(line.value().start(), line.value().end(), 0.5),
              GuiSketchSnapKind::Midpoint);
  }

  for (const auto& profile : sketch.rectangle_profiles()) {
    const auto width = length_parameter(document, profile.width_parameter());
    const auto height = length_parameter(document, profile.height_parameter());
    if (!width || !height)
      continue;
    const Point2 center = profile.center();
    const double half_width = *width * 0.5;
    const double half_height = *height * 0.5;
    const std::array<Point2, 4> corners{{
        {center.x - half_width, center.y - half_height},
        {center.x + half_width, center.y - half_height},
        {center.x + half_width, center.y + half_height},
        {center.x - half_width, center.y + half_height},
    }};
    const std::string semantic = profile_semantic(sketch, profile.id());
    for (std::size_t index = 0; index < corners.size(); ++index) {
      add_curve(scene, semantic, profile.id().value() + ":edge:" + std::to_string(index),
                GuiSketchCurveKind::Line,
                {corners[index], corners[(index + 1U) % corners.size()]});
      add_point(scene, semantic, profile.id().value() + ":corner:" + std::to_string(index),
                corners[index], GuiSketchSnapKind::Endpoint);
      add_point(scene, semantic, profile.id().value() + ":midpoint:" + std::to_string(index),
                interpolate(corners[index], corners[(index + 1U) % corners.size()], 0.5),
                GuiSketchSnapKind::Midpoint);
    }
    add_point(scene, semantic, profile.id().value() + ":center", center, GuiSketchSnapKind::Center);
  }

  for (const auto& profile : sketch.circle_profiles()) {
    const auto diameter = length_parameter(document, profile.diameter_parameter());
    if (!diameter)
      continue;
    const double radius = *diameter * 0.5;
    const Point2 center = profile.center();
    const std::string semantic = profile_semantic(sketch, profile.id());
    add_curve(scene, semantic, profile.id().value() + ":circle", GuiSketchCurveKind::Circle,
              sample_circle(center, radius), true);
    add_point(scene, semantic, profile.id().value() + ":center", center, GuiSketchSnapKind::Center);
    const std::array<Point2, 4> quadrants{{
        {center.x + radius, center.y}, {center.x, center.y + radius},
        {center.x - radius, center.y}, {center.x, center.y - radius}}};
    for (std::size_t index = 0; index < quadrants.size(); ++index)
      add_point(scene, semantic, profile.id().value() + ":quadrant:" + std::to_string(index),
                quadrants[index], GuiSketchSnapKind::Quadrant);
  }

  for (const auto& pattern : sketch.circular_hole_patterns()) {
    const auto radius = length_parameter(document, pattern.radius_parameter());
    const auto count = count_parameter(document, pattern.count_parameter());
    const auto diameter = length_parameter(document, pattern.hole_diameter_parameter());
    if (!radius || !count || !diameter)
      continue;
    const double hole_radius = *diameter * 0.5;
    const double offset = pattern.angle_offset_deg() * kTwoPi / 360.0;
    const std::string semantic = profile_semantic(sketch, pattern.id());
    for (std::size_t index = 0; index < *count; ++index) {
      const double angle = offset + kTwoPi * static_cast<double>(index) /
                                       static_cast<double>(*count);
      const Point2 center{pattern.center().x + *radius * std::cos(angle),
                          pattern.center().y + *radius * std::sin(angle)};
      add_curve(scene, semantic, pattern.id().value() + ":hole:" + std::to_string(index),
                GuiSketchCurveKind::Circle, sample_circle(center, hole_radius), true);
      add_point(scene, semantic, pattern.id().value() + ":center:" + std::to_string(index),
                center, GuiSketchSnapKind::Center);
      const std::array<Point2, 4> quadrants{{
          {center.x + hole_radius, center.y}, {center.x, center.y + hole_radius},
          {center.x - hole_radius, center.y}, {center.x, center.y - hole_radius}}};
      for (std::size_t quadrant = 0; quadrant < quadrants.size(); ++quadrant)
        add_point(scene, semantic,
                  pattern.id().value() + ":quadrant:" + std::to_string(index) + ":" +
                      std::to_string(quadrant),
                  quadrants[quadrant], GuiSketchSnapKind::Quadrant);
    }
  }

  for (const auto& dimension : sketch.driving_dimensions()) {
    const auto first = target_anchor(sketch, dimension.first_target(), projected_points,
                                     projected_lines);
    const auto second = target_anchor(sketch, dimension.second_target(), projected_points,
                                      projected_lines);
    if (first && second)
      scene.annotations.push_back(
          {dimension_semantic(sketch, dimension.id()), dimension.id().value(),
           interpolate(*first, *second, 0.5), GuiSketchHitKind::Dimension});
  }

  for (const auto& constraint : sketch.constraints()) {
    const auto anchor = target_anchor(sketch, constraint.constrained_target(), projected_points,
                                      projected_lines);
    if (anchor)
      scene.annotations.push_back(
          {constraint_semantic(sketch, constraint.id()), constraint.id().value(), *anchor,
           GuiSketchHitKind::Glyph});
  }
  for (const auto& constraint : sketch.geometric_constraints()) {
    const auto anchor = target_anchor(sketch, constraint.first_target(), projected_points,
                                      projected_lines);
    if (anchor)
      scene.annotations.push_back(
          {constraint_semantic(sketch, constraint.id()), constraint.id().value(), *anchor,
           GuiSketchHitKind::Glyph});
  }
  for (const auto& continuity : sketch.tangent_continuities()) {
    const auto anchor = entity_anchor(sketch, continuity.first_entity());
    if (anchor)
      scene.annotations.push_back(
          {constraint_semantic(sketch, continuity.id()), continuity.id().value(), *anchor,
           GuiSketchHitKind::Glyph});
  }

  build_intersections(scene);
  return Result<GuiSketchInteractionScene>::success(std::move(scene));
}

GuiSketchInteractionController::GuiSketchInteractionController(
    GuiSketchPlaneMapping mapping, GuiSketchInteractionScene scene,
    GuiSketchInteractionConfig config)
    : mapping_(std::move(mapping)), scene_(std::move(scene)), config_(std::move(config)) {}

Result<GuiSketchInteractionController>
GuiSketchInteractionController::create(GuiSketchPlaneMapping mapping,
                                       GuiSketchInteractionScene scene,
                                       GuiSketchInteractionConfig config) {
  if (!finite(config.point_hit_tolerance_dip) || config.point_hit_tolerance_dip <= 0.0 ||
      !finite(config.curve_hit_tolerance_dip) || config.curve_hit_tolerance_dip <= 0.0 ||
      !finite(config.annotation_hit_tolerance_dip) || config.annotation_hit_tolerance_dip <= 0.0 ||
      !finite(config.snap_tolerance_dip) || config.snap_tolerance_dip <= 0.0 ||
      !finite(config.stacked_hit_reset_tolerance_dip) ||
      config.stacked_hit_reset_tolerance_dip <= 0.0 || !finite(config.grid.spacing) ||
      config.grid.spacing <= 0.0 || config.grid.major_every == 0U)
    return Result<GuiSketchInteractionController>::failure(
        interaction_error("interaction tolerances and grid spacing must be positive and finite"));
  return Result<GuiSketchInteractionController>::success(
      GuiSketchInteractionController(std::move(mapping), std::move(scene), std::move(config)));
}

const GuiSketchPlaneMapping& GuiSketchInteractionController::mapping() const noexcept {
  return mapping_;
}

const GuiSketchInteractionScene& GuiSketchInteractionController::scene() const noexcept {
  return scene_;
}

const GuiSketchInteractionConfig& GuiSketchInteractionController::config() const noexcept {
  return config_;
}

void GuiSketchInteractionController::set_config(GuiSketchInteractionConfig config) {
  if (finite(config.point_hit_tolerance_dip) && config.point_hit_tolerance_dip > 0.0 &&
      finite(config.curve_hit_tolerance_dip) && config.curve_hit_tolerance_dip > 0.0 &&
      finite(config.annotation_hit_tolerance_dip) && config.annotation_hit_tolerance_dip > 0.0 &&
      finite(config.snap_tolerance_dip) && config.snap_tolerance_dip > 0.0 &&
      finite(config.stacked_hit_reset_tolerance_dip) &&
      config.stacked_hit_reset_tolerance_dip > 0.0 && finite(config.grid.spacing) &&
      config.grid.spacing > 0.0 && config.grid.major_every > 0U)
    config_ = std::move(config);
}

Result<std::vector<GuiSketchScreenPoint>>
GuiSketchInteractionController::screen_polyline(const GuiSketchCurvePrimitive& curve) const {
  std::vector<GuiSketchScreenPoint> result;
  result.reserve(curve.polyline.size());
  for (const auto point : curve.polyline) {
    auto screen = mapping_.plane_to_screen(point);
    if (screen.has_error())
      return Result<std::vector<GuiSketchScreenPoint>>::failure(screen.error());
    result.push_back(screen.value());
  }
  return Result<std::vector<GuiSketchScreenPoint>>::success(std::move(result));
}

Result<std::vector<GuiSketchHit>>
GuiSketchInteractionController::hits_at(GuiSketchScreenPoint screen_point) const {
  auto raw = mapping_.screen_to_plane(screen_point);
  if (raw.has_error())
    return Result<std::vector<GuiSketchHit>>::failure(raw.error());

  std::vector<GuiSketchHit> hits;
  for (const auto& point : scene_.points) {
    auto screen = mapping_.plane_to_screen(point.point);
    if (screen.has_error())
      return Result<std::vector<GuiSketchHit>>::failure(screen.error());
    const double screen_delta = screen_distance(screen_point, screen.value());
    if (screen_delta <= config_.point_hit_tolerance_dip)
      hits.push_back({GuiSketchHitKind::Point, point.semantic_id, point.candidate_id, point.point,
                      distance(raw.value(), point.point), screen_delta});
  }

  for (const auto& curve : scene_.curves) {
    auto screen = screen_polyline(curve);
    if (screen.has_error())
      return Result<std::vector<GuiSketchHit>>::failure(screen.error());
    const auto nearest = nearest_on_screen_polyline(screen_point, screen.value(), curve.polyline);
    if (nearest.distance <= config_.curve_hit_tolerance_dip)
      hits.push_back({GuiSketchHitKind::Curve, curve.semantic_id, curve.candidate_id,
                      nearest.plane_point, distance(raw.value(), nearest.plane_point),
                      nearest.distance});
  }

  for (const auto& annotation : scene_.annotations) {
    auto screen = mapping_.plane_to_screen(annotation.point);
    if (screen.has_error())
      return Result<std::vector<GuiSketchHit>>::failure(screen.error());
    const double screen_delta = screen_distance(screen_point, screen.value());
    if (screen_delta <= config_.annotation_hit_tolerance_dip)
      hits.push_back({annotation.hit_kind, annotation.semantic_id, annotation.candidate_id,
                      annotation.point, distance(raw.value(), annotation.point), screen_delta});
  }

  std::sort(hits.begin(), hits.end(), [](const GuiSketchHit& first, const GuiSketchHit& second) {
    const int first_priority = hit_priority(first.kind);
    const int second_priority = hit_priority(second.kind);
    if (first_priority != second_priority)
      return first_priority < second_priority;
    if (std::abs(first.screen_distance_dip - second.screen_distance_dip) > kEpsilon)
      return first.screen_distance_dip < second.screen_distance_dip;
    if (std::abs(first.model_distance - second.model_distance) > kEpsilon)
      return first.model_distance < second.model_distance;
    return first.candidate_id < second.candidate_id;
  });
  return Result<std::vector<GuiSketchHit>>::success(std::move(hits));
}

Result<std::optional<GuiSketchHit>>
GuiSketchInteractionController::cycle_hit(GuiSketchScreenPoint screen_point) {
  auto hits = hits_at(screen_point);
  if (hits.has_error())
    return Result<std::optional<GuiSketchHit>>::failure(hits.error());
  if (hits.value().empty()) {
    reset_hit_cycle();
    return Result<std::optional<GuiSketchHit>>::success(std::nullopt);
  }

  std::vector<std::string> signature;
  signature.reserve(hits.value().size());
  for (const auto& hit : hits.value())
    signature.push_back(std::string(to_string(hit.kind)) + ":" + hit.candidate_id);

  const bool same_position = last_cycle_point_.has_value() &&
                             screen_distance(*last_cycle_point_, screen_point) <=
                                 config_.stacked_hit_reset_tolerance_dip;
  if (same_position && signature == last_cycle_signature_)
    cycle_index_ = (cycle_index_ + 1U) % hits.value().size();
  else
    cycle_index_ = 0U;
  last_cycle_point_ = screen_point;
  last_cycle_signature_ = std::move(signature);
  return Result<std::optional<GuiSketchHit>>::success(hits.value()[cycle_index_]);
}

void GuiSketchInteractionController::reset_hit_cycle() noexcept {
  last_cycle_point_.reset();
  last_cycle_signature_.clear();
  cycle_index_ = 0U;
}

Result<GuiSketchSnapResult>
GuiSketchInteractionController::snap(GuiSketchScreenPoint screen_point,
                                     std::optional<Point2> inference_anchor) const {
  auto raw = mapping_.screen_to_plane(screen_point);
  if (raw.has_error())
    return Result<GuiSketchSnapResult>::failure(raw.error());

  struct Candidate {
    Point2 point;
    GuiSketchSnapKind kind{GuiSketchSnapKind::None};
    std::string id;
    double model_distance{0.0};
    double screen_distance{0.0};
  };
  std::vector<Candidate> candidates;
  const auto append_candidate = [&](Point2 point, GuiSketchSnapKind kind, std::string id) {
    auto screen = mapping_.plane_to_screen(point);
    if (screen.has_error())
      return;
    const double screen_delta = screen_distance(screen_point, screen.value());
    if (screen_delta > config_.snap_tolerance_dip)
      return;
    candidates.push_back(
        {point, kind, std::move(id), distance(raw.value(), point), screen_delta});
  };

  append_candidate({0.0, 0.0}, GuiSketchSnapKind::Origin, "origin");
  append_candidate({raw.value().x, 0.0}, GuiSketchSnapKind::Axis, "axis/x");
  append_candidate({0.0, raw.value().y}, GuiSketchSnapKind::Axis, "axis/y");
  for (const auto& point : scene_.points)
    append_candidate(point.point, point.snap_kind, point.candidate_id);
  for (std::size_t index = 0; index < scene_.intersections.size(); ++index)
    append_candidate(scene_.intersections[index], GuiSketchSnapKind::Intersection,
                     "intersection/" + std::to_string(index));
  for (const auto& curve : scene_.curves) {
    const auto nearest = nearest_on_polyline(raw.value(), curve.polyline);
    if (finite(nearest.distance))
      append_candidate(nearest.point, GuiSketchSnapKind::Nearest,
                       curve.candidate_id + ":nearest");
  }

  if (config_.grid.snap_enabled) {
    append_candidate({std::round(raw.value().x / config_.grid.spacing) * config_.grid.spacing,
                      std::round(raw.value().y / config_.grid.spacing) * config_.grid.spacing},
                     GuiSketchSnapKind::Grid, "grid");
  }

  if (inference_anchor) {
    append_candidate({raw.value().x, inference_anchor->y},
                     GuiSketchSnapKind::HorizontalInference, "anchor/horizontal");
    append_candidate({inference_anchor->x, raw.value().y},
                     GuiSketchSnapKind::VerticalInference, "anchor/vertical");
  }
  for (const auto& point : scene_.points) {
    append_candidate({point.point.x, raw.value().y}, GuiSketchSnapKind::AlignmentX,
                     point.candidate_id);
    append_candidate({raw.value().x, point.point.y}, GuiSketchSnapKind::AlignmentY,
                     point.candidate_id);
  }

  if (candidates.empty())
    return Result<GuiSketchSnapResult>::success(
        {raw.value(), raw.value(), GuiSketchSnapKind::None, {}, {}, 0.0, 0.0});

  std::sort(candidates.begin(), candidates.end(), [](const Candidate& first, const Candidate& second) {
    if (std::abs(first.model_distance - second.model_distance) > kEpsilon)
      return first.model_distance < second.model_distance;
    if (std::abs(first.screen_distance - second.screen_distance) > kEpsilon)
      return first.screen_distance < second.screen_distance;
    const int first_priority = snap_priority(first.kind);
    const int second_priority = snap_priority(second.kind);
    if (first_priority != second_priority)
      return first_priority < second_priority;
    return first.id < second.id;
  });
  const auto& selected = candidates.front();
  return Result<GuiSketchSnapResult>::success(
      {raw.value(), selected.point, selected.kind, selected.id,
       inference_text(selected.kind, selected.id), selected.model_distance,
       selected.screen_distance});
}

Result<std::vector<GuiSelection>>
GuiSketchInteractionController::box_select(GuiSketchScreenRect rectangle,
                                           GuiSketchBoxSelectionMode mode) const {
  std::unordered_set<std::string> selected_ids;
  for (const auto& point : scene_.points) {
    auto screen = mapping_.plane_to_screen(point.point);
    if (screen.has_error())
      return Result<std::vector<GuiSelection>>::failure(screen.error());
    if (rectangle.contains(screen.value()))
      selected_ids.insert(point.semantic_id);
  }

  for (const auto& curve : scene_.curves) {
    auto screen = screen_polyline(curve);
    if (screen.has_error())
      return Result<std::vector<GuiSelection>>::failure(screen.error());
    bool selected = false;
    if (mode == GuiSketchBoxSelectionMode::Window) {
      selected = std::all_of(screen.value().begin(), screen.value().end(),
                             [&rectangle](GuiSketchScreenPoint point) {
                               return rectangle.contains(point);
                             });
    } else {
      selected = std::any_of(screen.value().begin(), screen.value().end(),
                             [&rectangle](GuiSketchScreenPoint point) {
                               return rectangle.contains(point);
                             });
      for (std::size_t index = 1; index < screen.value().size() && !selected; ++index)
        selected = segment_intersects_rect(screen.value()[index - 1U], screen.value()[index],
                                           rectangle);
    }
    if (selected)
      selected_ids.insert(curve.semantic_id);
  }

  for (const auto& annotation : scene_.annotations) {
    auto screen = mapping_.plane_to_screen(annotation.point);
    if (screen.has_error())
      return Result<std::vector<GuiSelection>>::failure(screen.error());
    if (rectangle.contains(screen.value()))
      selected_ids.insert(annotation.semantic_id);
  }

  std::vector<std::string> ordered_ids(selected_ids.begin(), selected_ids.end());
  std::sort(ordered_ids.begin(), ordered_ids.end());
  std::vector<GuiSelection> selections;
  selections.reserve(ordered_ids.size());
  for (auto& id : ordered_ids)
    selections.push_back({GuiSelectionKind::SketchEntity, std::move(id)});
  return Result<std::vector<GuiSelection>>::success(std::move(selections));
}

Result<std::vector<GuiSketchScreenSegment>>
GuiSketchInteractionController::grid_lines(double viewport_width_dip,
                                           double viewport_height_dip,
                                           std::size_t maximum_lines) const {
  if (!config_.grid.visible)
    return Result<std::vector<GuiSketchScreenSegment>>::success({});
  if (!finite(viewport_width_dip) || !finite(viewport_height_dip) ||
      viewport_width_dip <= 0.0 || viewport_height_dip <= 0.0 || maximum_lines < 2U)
    return Result<std::vector<GuiSketchScreenSegment>>::failure(
        interaction_error("grid generation requires a positive viewport and at least two lines"));

  const std::array<GuiSketchScreenPoint, 4> corners{{
      {0.0, 0.0}, {viewport_width_dip, 0.0},
      {viewport_width_dip, viewport_height_dip}, {0.0, viewport_height_dip}}};
  std::array<Point2, 4> plane_corners{};
  for (std::size_t index = 0; index < corners.size(); ++index) {
    auto plane = mapping_.screen_to_plane(corners[index]);
    if (plane.has_error())
      return Result<std::vector<GuiSketchScreenSegment>>::failure(plane.error());
    plane_corners[index] = plane.value();
  }
  const auto [min_x, max_x] = std::minmax_element(
      plane_corners.begin(), plane_corners.end(),
      [](Point2 first, Point2 second) { return first.x < second.x; });
  const auto [min_y, max_y] = std::minmax_element(
      plane_corners.begin(), plane_corners.end(),
      [](Point2 first, Point2 second) { return first.y < second.y; });

  const long long first_x = static_cast<long long>(std::floor(min_x->x / config_.grid.spacing));
  const long long last_x = static_cast<long long>(std::ceil(max_x->x / config_.grid.spacing));
  const long long first_y = static_cast<long long>(std::floor(min_y->y / config_.grid.spacing));
  const long long last_y = static_cast<long long>(std::ceil(max_y->y / config_.grid.spacing));
  const std::size_t x_count = static_cast<std::size_t>(std::max(0LL, last_x - first_x + 1LL));
  const std::size_t y_count = static_cast<std::size_t>(std::max(0LL, last_y - first_y + 1LL));
  const std::size_t total = x_count + y_count;
  const std::size_t stride = total <= maximum_lines
                                 ? 1U
                                 : static_cast<std::size_t>(
                                       std::ceil(static_cast<double>(total) /
                                                 static_cast<double>(maximum_lines)));

  std::vector<GuiSketchScreenSegment> lines;
  lines.reserve(std::min(total, maximum_lines + 2U));
  const auto append_line = [&](Point2 start, Point2 end, long long index) {
    auto screen_start = mapping_.plane_to_screen(start);
    auto screen_end = mapping_.plane_to_screen(end);
    if (screen_start.has_error() || screen_end.has_error())
      return;
    const long long major_every = static_cast<long long>(config_.grid.major_every);
    const bool major = index % major_every == 0LL;
    lines.push_back({screen_start.value(), screen_end.value(), major});
  };
  for (long long index = first_x; index <= last_x; index += static_cast<long long>(stride))
    append_line({static_cast<double>(index) * config_.grid.spacing, min_y->y},
                {static_cast<double>(index) * config_.grid.spacing, max_y->y}, index);
  for (long long index = first_y; index <= last_y; index += static_cast<long long>(stride))
    append_line({min_x->x, static_cast<double>(index) * config_.grid.spacing},
                {max_x->x, static_cast<double>(index) * config_.grid.spacing}, index);
  return Result<std::vector<GuiSketchScreenSegment>>::success(std::move(lines));
}

} // namespace blcad::gui
