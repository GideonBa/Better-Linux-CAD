#include "blcad/gui/gui_sketch_create.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kEpsilon = 1.0e-9;
constexpr std::size_t kMinimumPolygonSides = 3U;
constexpr std::size_t kMaximumPolygonSides = 64U;

struct CircleGeometry {
  Point2 center;
  double radius;
};

struct EllipseGeometry {
  Point2 center;
  Point2 major_unit;
  Point2 minor_unit;
  double major_radius;
  double minor_radius;
};

struct SlotGeometry {
  Point2 first_center;
  Point2 second_center;
  Point2 axis_unit;
  Point2 normal_unit;
  double radius;
};

[[nodiscard]] Error create_error(std::string message) {
  return Error::validation("gui.sketch_create", std::move(message));
}

[[nodiscard]] bool finite(Point2 point) noexcept {
  return std::isfinite(point.x) && std::isfinite(point.y);
}

[[nodiscard]] double distance(Point2 first, Point2 second) noexcept {
  return std::hypot(second.x - first.x, second.y - first.y);
}

[[nodiscard]] double cross(Point2 origin, Point2 first, Point2 second) noexcept {
  return (first.x - origin.x) * (second.y - origin.y) -
         (first.y - origin.y) * (second.x - origin.x);
}

[[nodiscard]] double dot(Point2 vector, Point2 direction) noexcept {
  return vector.x * direction.x + vector.y * direction.y;
}

[[nodiscard]] Point2 subtract(Point2 left, Point2 right) noexcept {
  return {left.x - right.x, left.y - right.y};
}

[[nodiscard]] Point2 add(Point2 left, Point2 right) noexcept {
  return {left.x + right.x, left.y + right.y};
}

[[nodiscard]] Point2 scale(Point2 value, double factor) noexcept {
  return {value.x * factor, value.y * factor};
}

[[nodiscard]] double normalize_positive_angle(double angle) noexcept {
  while (angle < 0.0) angle += kTwoPi;
  while (angle >= kTwoPi) angle -= kTwoPi;
  return angle;
}

[[nodiscard]] double normalize_signed_angle(double angle) noexcept {
  while (angle <= -kPi) angle += kTwoPi;
  while (angle > kPi) angle -= kTwoPi;
  return angle;
}

[[nodiscard]] Result<double> parse_double(std::string_view text) {
  double value{};
  const auto* begin = text.data();
  const auto* end = text.data() + text.size();
  const auto [parsed, error] = std::from_chars(begin, end, value);
  if (error != std::errc{} || parsed != end || !std::isfinite(value))
    return Result<double>::failure(create_error("numeric Sketch input must be a finite number"));
  return Result<double>::success(value);
}

[[nodiscard]] std::size_t fixed_pick_count(GuiSketchCreateTool tool) noexcept {
  switch (tool) {
  case GuiSketchCreateTool::Point: return 1U;
  case GuiSketchCreateTool::Line:
  case GuiSketchCreateTool::Polyline:
  case GuiSketchCreateTool::CornerRectangle:
  case GuiSketchCreateTool::CenterRectangle:
  case GuiSketchCreateTool::RegularPolygon:
  case GuiSketchCreateTool::Centerline:
  case GuiSketchCreateTool::CenterRadiusCircle:
  case GuiSketchCreateTool::CenterDiameterCircle:
  case GuiSketchCreateTool::TwoPointCircle: return 2U;
  case GuiSketchCreateTool::ThreePointRectangle:
  case GuiSketchCreateTool::Parallelogram:
  case GuiSketchCreateTool::ThreePointCircle:
  case GuiSketchCreateTool::TangentCircle:
  case GuiSketchCreateTool::CenterStartEndArc:
  case GuiSketchCreateTool::ThreePointArc:
  case GuiSketchCreateTool::TangentArc:
  case GuiSketchCreateTool::Ellipse:
  case GuiSketchCreateTool::CenterSlot:
  case GuiSketchCreateTool::OverallSlot: return 3U;
  case GuiSketchCreateTool::EllipticalArc: return 5U;
  }
  return 2U;
}

[[nodiscard]] bool is_circle_tool(GuiSketchCreateTool tool) noexcept {
  return tool == GuiSketchCreateTool::CenterRadiusCircle ||
         tool == GuiSketchCreateTool::CenterDiameterCircle ||
         tool == GuiSketchCreateTool::TwoPointCircle ||
         tool == GuiSketchCreateTool::ThreePointCircle ||
         tool == GuiSketchCreateTool::TangentCircle;
}

[[nodiscard]] bool is_arc_tool(GuiSketchCreateTool tool) noexcept {
  return tool == GuiSketchCreateTool::CenterStartEndArc ||
         tool == GuiSketchCreateTool::ThreePointArc ||
         tool == GuiSketchCreateTool::TangentArc;
}

[[nodiscard]] bool is_tangent_tool(GuiSketchCreateTool tool) noexcept {
  return tool == GuiSketchCreateTool::TangentCircle ||
         tool == GuiSketchCreateTool::TangentArc;
}

[[nodiscard]] std::string_view constraint_preview_kind(GuiSketchSnapKind snap) noexcept {
  switch (snap) {
  case GuiSketchSnapKind::Endpoint:
  case GuiSketchSnapKind::Origin:
  case GuiSketchSnapKind::Intersection: return "coincident";
  case GuiSketchSnapKind::Midpoint: return "midpoint";
  case GuiSketchSnapKind::Center:
  case GuiSketchSnapKind::Quadrant: return "concentric";
  case GuiSketchSnapKind::Axis: return "point_on_object";
  case GuiSketchSnapKind::HorizontalInference: return "horizontal";
  case GuiSketchSnapKind::VerticalInference: return "vertical";
  case GuiSketchSnapKind::AlignmentX:
  case GuiSketchSnapKind::AlignmentY: return "aligned";
  case GuiSketchSnapKind::Nearest:
  case GuiSketchSnapKind::Grid:
  case GuiSketchSnapKind::None: return {};
  }
  return {};
}

[[nodiscard]] bool sketch_entity_id_taken(const Sketch& sketch, const SketchEntityId& id) {
  return sketch.find_line_segment(id) != nullptr || sketch.find_arc_segment(id) != nullptr ||
         sketch.find_spline_segment(id) != nullptr || sketch.find_projected_point(id) != nullptr ||
         sketch.find_projected_line(id) != nullptr ||
         sketch.find_reference_generated_line(id) != nullptr;
}

[[nodiscard]] bool profile_id_taken(const Sketch& sketch, const ProfileId& id) {
  const auto matches = [&id](const auto& profiles) {
    return std::any_of(profiles.begin(), profiles.end(),
                       [&id](const auto& profile) { return profile.id() == id; });
  };
  return matches(sketch.rectangle_profiles()) || matches(sketch.circle_profiles()) ||
         matches(sketch.closed_profiles()) || matches(sketch.arc_closed_profiles()) ||
         matches(sketch.composite_closed_profiles()) || matches(sketch.circular_hole_patterns());
}

template <typename Taken>
[[nodiscard]] std::string next_free_id(std::string_view prefix, const Taken& taken) {
  for (std::size_t index = 1U;; ++index) {
    std::string candidate = std::string(prefix) + std::to_string(index);
    if (!taken(candidate)) return candidate;
  }
}

[[nodiscard]] Result<CircleGeometry> circle_from_three_points(Point2 first, Point2 second,
                                                              Point2 third) {
  const double determinant =
      2.0 * (first.x * (second.y - third.y) + second.x * (third.y - first.y) +
             third.x * (first.y - second.y));
  if (std::abs(determinant) <= kEpsilon)
    return Result<CircleGeometry>::failure(
        create_error("three-point circle picks must not be collinear"));
  const double first_norm = first.x * first.x + first.y * first.y;
  const double second_norm = second.x * second.x + second.y * second.y;
  const double third_norm = third.x * third.x + third.y * third.y;
  const Point2 center{
      (first_norm * (second.y - third.y) + second_norm * (third.y - first.y) +
       third_norm * (first.y - second.y)) /
          determinant,
      (first_norm * (third.x - second.x) + second_norm * (first.x - third.x) +
       third_norm * (second.x - first.x)) /
          determinant};
  const double radius = distance(center, first);
  if (radius <= kEpsilon || !std::isfinite(radius))
    return Result<CircleGeometry>::failure(create_error("circle radius must be positive"));
  return Result<CircleGeometry>::success({center, radius});
}

[[nodiscard]] Result<CircleGeometry>
circle_geometry(GuiSketchCreateTool tool, const std::vector<GuiSketchCreatePick>& picks) {
  if (tool == GuiSketchCreateTool::CenterRadiusCircle ||
      tool == GuiSketchCreateTool::CenterDiameterCircle) {
    const double radius = distance(picks[0].point, picks[1].point);
    if (radius <= kEpsilon)
      return Result<CircleGeometry>::failure(create_error("circle radius must be positive"));
    return Result<CircleGeometry>::success({picks[0].point, radius});
  }
  if (tool == GuiSketchCreateTool::TwoPointCircle) {
    const Point2 first = picks[0].point;
    const Point2 second = picks[1].point;
    const double diameter = distance(first, second);
    if (diameter <= kEpsilon)
      return Result<CircleGeometry>::failure(
          create_error("two-point circle diameter endpoints must be distinct"));
    return Result<CircleGeometry>::success(
        {{(first.x + second.x) * 0.5, (first.y + second.y) * 0.5}, diameter * 0.5});
  }
  return circle_from_three_points(picks[0].point, picks[1].point, picks[2].point);
}

[[nodiscard]] Result<EllipseGeometry>
ellipse_geometry(Point2 center, Point2 major_reference, Point2 minor_reference) {
  const Point2 major = subtract(major_reference, center);
  const double major_radius = std::hypot(major.x, major.y);
  if (major_radius <= kEpsilon)
    return Result<EllipseGeometry>::failure(
        create_error("ellipse center and major-axis pick must be distinct"));
  const Point2 major_unit{major.x / major_radius, major.y / major_radius};
  const Point2 minor_unit{-major_unit.y, major_unit.x};
  const double minor_radius = std::abs(dot(subtract(minor_reference, center), minor_unit));
  if (minor_radius <= kEpsilon)
    return Result<EllipseGeometry>::failure(
        create_error("ellipse minor-axis pick must leave the major axis"));
  return Result<EllipseGeometry>::success(
      {center, major_unit, minor_unit, major_radius, minor_radius});
}

[[nodiscard]] Point2 ellipse_point(const EllipseGeometry& ellipse, double parameter) noexcept {
  return add(ellipse.center,
             add(scale(ellipse.major_unit, ellipse.major_radius * std::cos(parameter)),
                 scale(ellipse.minor_unit, ellipse.minor_radius * std::sin(parameter))));
}

[[nodiscard]] Point2 ellipse_derivative(const EllipseGeometry& ellipse,
                                        double parameter) noexcept {
  return add(scale(ellipse.major_unit, -ellipse.major_radius * std::sin(parameter)),
             scale(ellipse.minor_unit, ellipse.minor_radius * std::cos(parameter)));
}

[[nodiscard]] double ellipse_parameter(const EllipseGeometry& ellipse, Point2 reference) noexcept {
  const Point2 delta = subtract(reference, ellipse.center);
  return std::atan2(dot(delta, ellipse.minor_unit) / ellipse.minor_radius,
                    dot(delta, ellipse.major_unit) / ellipse.major_radius);
}

[[nodiscard]] Result<SlotGeometry> slot_geometry(GuiSketchCreateTool tool, Point2 first,
                                                Point2 second, Point2 width_reference) {
  const Point2 axis = subtract(second, first);
  const double axis_length = std::hypot(axis.x, axis.y);
  if (axis_length <= kEpsilon)
    return Result<SlotGeometry>::failure(create_error("slot axis picks must be distinct"));
  const Point2 axis_unit{axis.x / axis_length, axis.y / axis_length};
  const Point2 normal_unit{-axis_unit.y, axis_unit.x};
  const double radius = std::abs(dot(subtract(width_reference, first), normal_unit));
  if (radius <= kEpsilon)
    return Result<SlotGeometry>::failure(create_error("slot half-width must be positive"));

  Point2 first_center = first;
  Point2 second_center = second;
  if (tool == GuiSketchCreateTool::OverallSlot) {
    if (axis_length <= 2.0 * radius + kEpsilon)
      return Result<SlotGeometry>::failure(
          create_error("overall slot length must exceed its width"));
    first_center = add(first, scale(axis_unit, radius));
    second_center = add(second, scale(axis_unit, -radius));
  }
  return Result<SlotGeometry>::success(
      {first_center, second_center, axis_unit, normal_unit, radius});
}

[[nodiscard]] std::vector<Point2> sample_circle(const CircleGeometry& circle,
                                                std::size_t segments = 64U) {
  std::vector<Point2> points;
  points.reserve(segments);
  for (std::size_t index = 0U; index < segments; ++index) {
    const double angle = kTwoPi * static_cast<double>(index) / static_cast<double>(segments);
    points.push_back({circle.center.x + circle.radius * std::cos(angle),
                      circle.center.y + circle.radius * std::sin(angle)});
  }
  return points;
}

[[nodiscard]] std::vector<Point2> sample_circular_arc(Point2 center, double radius,
                                                      double start_angle, double sweep,
                                                      std::size_t segments = 32U) {
  std::vector<Point2> points;
  points.reserve(segments + 1U);
  for (std::size_t index = 0U; index <= segments; ++index) {
    const double parameter = static_cast<double>(index) / static_cast<double>(segments);
    const double angle = start_angle + sweep * parameter;
    points.push_back({center.x + radius * std::cos(angle),
                      center.y + radius * std::sin(angle)});
  }
  return points;
}

[[nodiscard]] Result<std::vector<Point2>> sample_three_point_arc(Point2 start, Point2 mid,
                                                                 Point2 end) {
  auto circle = circle_from_three_points(start, mid, end);
  if (circle.has_error()) return Result<std::vector<Point2>>::failure(circle.error());
  const double start_angle =
      std::atan2(start.y - circle.value().center.y, start.x - circle.value().center.x);
  const double mid_angle = normalize_positive_angle(
      std::atan2(mid.y - circle.value().center.y, mid.x - circle.value().center.x) - start_angle);
  double sweep = normalize_positive_angle(
      std::atan2(end.y - circle.value().center.y, end.x - circle.value().center.x) - start_angle);
  if (mid_angle > sweep + kEpsilon) sweep -= kTwoPi;
  if (std::abs(sweep) <= kEpsilon)
    return Result<std::vector<Point2>>::failure(create_error("arc sweep must be non-zero"));
  return Result<std::vector<Point2>>::success(
      sample_circular_arc(circle.value().center, circle.value().radius, start_angle, sweep));
}

[[nodiscard]] Result<std::vector<Point2>> sample_center_arc(Point2 center, Point2 start,
                                                            Point2 end) {
  const double radius = distance(center, start);
  if (radius <= kEpsilon)
    return Result<std::vector<Point2>>::failure(
        create_error("arc center and start pick must be distinct"));
  const double end_radius = distance(center, end);
  if (end_radius <= kEpsilon)
    return Result<std::vector<Point2>>::failure(
        create_error("arc center and end pick must be distinct"));
  const double start_angle = std::atan2(start.y - center.y, start.x - center.x);
  const double end_angle = std::atan2(end.y - center.y, end.x - center.x);
  const double sweep = normalize_signed_angle(end_angle - start_angle);
  if (std::abs(sweep) <= kEpsilon)
    return Result<std::vector<Point2>>::failure(create_error("arc sweep must be non-zero"));
  return Result<std::vector<Point2>>::success(
      sample_circular_arc(center, radius, start_angle, sweep));
}

[[nodiscard]] std::vector<Point2> sample_ellipse(const EllipseGeometry& ellipse,
                                                 double start, double sweep,
                                                 std::size_t segments = 64U) {
  std::vector<Point2> points;
  points.reserve(segments + 1U);
  for (std::size_t index = 0U; index <= segments; ++index)
    points.push_back(ellipse_point(
        ellipse, start + sweep * static_cast<double>(index) / static_cast<double>(segments)));
  return points;
}

[[nodiscard]] std::vector<Point2> sample_slot(const SlotGeometry& slot) {
  const Point2 top_first = add(slot.first_center, scale(slot.normal_unit, slot.radius));
  const Point2 top_second = add(slot.second_center, scale(slot.normal_unit, slot.radius));
  const Point2 bottom_second = add(slot.second_center, scale(slot.normal_unit, -slot.radius));
  const Point2 bottom_first = add(slot.first_center, scale(slot.normal_unit, -slot.radius));
  std::vector<Point2> points;
  points.push_back(top_first);
  points.push_back(top_second);
  const double second_start = std::atan2(slot.normal_unit.y, slot.normal_unit.x);
  auto second_arc = sample_circular_arc(slot.second_center, slot.radius, second_start, -kPi, 16U);
  points.insert(points.end(), std::next(second_arc.begin()), second_arc.end());
  points.push_back(bottom_first);
  const double first_start = std::atan2(-slot.normal_unit.y, -slot.normal_unit.x);
  auto first_arc = sample_circular_arc(slot.first_center, slot.radius, first_start, -kPi, 16U);
  points.insert(points.end(), std::next(first_arc.begin()), first_arc.end());
  return points;
}

} // namespace

std::string_view to_string(GuiSketchCreateTool tool) noexcept {
  switch (tool) {
  case GuiSketchCreateTool::Point: return "point";
  case GuiSketchCreateTool::Line: return "line";
  case GuiSketchCreateTool::Polyline: return "polyline";
  case GuiSketchCreateTool::CornerRectangle: return "corner_rectangle";
  case GuiSketchCreateTool::CenterRectangle: return "center_rectangle";
  case GuiSketchCreateTool::ThreePointRectangle: return "three_point_rectangle";
  case GuiSketchCreateTool::Parallelogram: return "parallelogram";
  case GuiSketchCreateTool::RegularPolygon: return "regular_polygon";
  case GuiSketchCreateTool::Centerline: return "centerline";
  case GuiSketchCreateTool::CenterRadiusCircle: return "center_radius_circle";
  case GuiSketchCreateTool::CenterDiameterCircle: return "center_diameter_circle";
  case GuiSketchCreateTool::TwoPointCircle: return "two_point_circle";
  case GuiSketchCreateTool::ThreePointCircle: return "three_point_circle";
  case GuiSketchCreateTool::TangentCircle: return "tangent_circle";
  case GuiSketchCreateTool::CenterStartEndArc: return "center_start_end_arc";
  case GuiSketchCreateTool::ThreePointArc: return "three_point_arc";
  case GuiSketchCreateTool::TangentArc: return "tangent_arc";
  case GuiSketchCreateTool::Ellipse: return "ellipse";
  case GuiSketchCreateTool::EllipticalArc: return "elliptical_arc";
  case GuiSketchCreateTool::CenterSlot: return "center_slot";
  case GuiSketchCreateTool::OverallSlot: return "overall_slot";
  }
  return "line";
}

Result<GuiSketchCreateController>
GuiSketchCreateController::begin(GuiSketchCreateTool tool, SketchId sketch,
                                 GuiSketchPlaneView plane, GuiSketchCreateOptions options) {
  if (sketch.empty())
    return Result<GuiSketchCreateController>::failure(
        create_error("Sketch creation requires an active Sketch id"));
  if (options.polygon_sides < kMinimumPolygonSides ||
      options.polygon_sides > kMaximumPolygonSides)
    return Result<GuiSketchCreateController>::failure(
        create_error("regular polygon side count must be between 3 and 64"));
  return Result<GuiSketchCreateController>::success(
      GuiSketchCreateController(tool, std::move(sketch), std::move(plane), options));
}

GuiSketchCreateController::GuiSketchCreateController(GuiSketchCreateTool tool, SketchId sketch,
                                                     GuiSketchPlaneView plane,
                                                     GuiSketchCreateOptions options)
    : tool_(tool), sketch_(std::move(sketch)), plane_(std::move(plane)), options_(options) {}

GuiSketchCreateTool GuiSketchCreateController::tool() const noexcept { return tool_; }
const SketchId& GuiSketchCreateController::sketch() const noexcept { return sketch_; }
const GuiSketchCreateOptions& GuiSketchCreateController::options() const noexcept {
  return options_;
}
const std::vector<GuiSketchCreatePick>& GuiSketchCreateController::picks() const noexcept {
  return picks_;
}

std::size_t GuiSketchCreateController::required_picks() const noexcept {
  return fixed_pick_count(tool_);
}

bool GuiSketchCreateController::accepts_more_picks() const noexcept {
  if (tool_ == GuiSketchCreateTool::Polyline) return true;
  return picks_.size() < required_picks();
}

bool GuiSketchCreateController::complete_ready() const noexcept {
  return picks_.size() >= required_picks();
}

Result<std::size_t> GuiSketchCreateController::add_pick(GuiSketchCreatePick pick) {
  if (!finite(pick.point))
    return Result<std::size_t>::failure(create_error("Sketch pick coordinates must be finite"));
  if (!accepts_more_picks())
    return Result<std::size_t>::failure(
        create_error("the active Sketch creation tool already has all required picks"));
  picks_.push_back(pick);
  return Result<std::size_t>::success(picks_.size());
}

Result<std::size_t> GuiSketchCreateController::pop_pick() {
  if (picks_.empty())
    return Result<std::size_t>::failure(
        create_error("the active Sketch creation tool has no pick to remove"));
  picks_.pop_back();
  return Result<std::size_t>::success(picks_.size());
}

Result<std::optional<GuiSketchCreatePick>>
GuiSketchCreateController::apply_numeric(std::string_view text,
                                         std::optional<Point2> hover) const {
  using NumericResult = Result<std::optional<GuiSketchCreatePick>>;
  if (text.empty())
    return NumericResult::failure(create_error("numeric Sketch input must not be empty"));

  const bool plain_integer =
      text.find_first_not_of("0123456789") == std::string_view::npos;
  if (tool_ == GuiSketchCreateTool::RegularPolygon && picks_.empty() && plain_integer)
    return NumericResult::success(std::nullopt);

  const std::size_t separator = text.find(';');
  if (separator != std::string_view::npos) {
    auto first = parse_double(text.substr(0U, separator));
    auto second = parse_double(text.substr(separator + 1U));
    if (first.has_error()) return NumericResult::failure(first.error());
    if (second.has_error()) return NumericResult::failure(second.error());
    if (picks_.empty())
      return NumericResult::success(
          GuiSketchCreatePick{{first.value(), second.value()}, GuiSketchSnapKind::None});
    const Point2 base = picks_.back().point;
    return NumericResult::success(GuiSketchCreatePick{
        {base.x + first.value(), base.y + second.value()}, GuiSketchSnapKind::None});
  }

  if (picks_.empty())
    return NumericResult::failure(
        create_error("polar numeric Sketch input requires a previous pick"));
  const Point2 base = picks_.back().point;
  const std::size_t angle_separator = text.find('<');
  auto length = parse_double(
      angle_separator == std::string_view::npos ? text : text.substr(0U, angle_separator));
  if (length.has_error()) return NumericResult::failure(length.error());
  if (length.value() <= 0.0)
    return NumericResult::failure(create_error("numeric Sketch length must be positive"));

  Point2 direction{1.0, 0.0};
  if (angle_separator != std::string_view::npos) {
    auto angle = parse_double(text.substr(angle_separator + 1U));
    if (angle.has_error()) return NumericResult::failure(angle.error());
    const double radians = angle.value() * kPi / 180.0;
    direction = {std::cos(radians), std::sin(radians)};
  } else {
    if (!hover || distance(*hover, base) <= kEpsilon)
      return NumericResult::failure(create_error(
          "numeric Sketch length without an angle requires a hover direction"));
    const double magnitude = distance(*hover, base);
    direction = {(hover->x - base.x) / magnitude, (hover->y - base.y) / magnitude};
  }
  return NumericResult::success(GuiSketchCreatePick{
      {base.x + direction.x * length.value(), base.y + direction.y * length.value()},
      GuiSketchSnapKind::None});
}

Result<std::size_t> GuiSketchCreateController::set_polygon_sides(std::size_t sides) {
  if (tool_ != GuiSketchCreateTool::RegularPolygon)
    return Result<std::size_t>::failure(
        create_error("polygon side count applies only to the regular polygon tool"));
  if (!picks_.empty())
    return Result<std::size_t>::failure(
        create_error("polygon side count must be chosen before the first pick"));
  if (sides < kMinimumPolygonSides || sides > kMaximumPolygonSides)
    return Result<std::size_t>::failure(
        create_error("regular polygon side count must be between 3 and 64"));
  options_.polygon_sides = sides;
  return Result<std::size_t>::success(sides);
}

Result<std::vector<Point2>>
GuiSketchCreateController::corner_ring(std::optional<Point2> hover) const {
  std::vector<Point2> anchors;
  anchors.reserve(picks_.size() + 1U);
  for (const auto& pick : picks_) anchors.push_back(pick.point);
  if (anchors.size() < required_picks() && hover) anchors.push_back(*hover);
  if (anchors.size() < required_picks())
    return Result<std::vector<Point2>>::failure(
        create_error("the Sketch creation tool does not have enough picks yet"));

  switch (tool_) {
  case GuiSketchCreateTool::CornerRectangle: {
    const Point2 first = anchors[0];
    const Point2 second = anchors[1];
    if (std::abs(second.x - first.x) <= kEpsilon ||
        std::abs(second.y - first.y) <= kEpsilon)
      return Result<std::vector<Point2>>::failure(
          create_error("rectangle corners must span a non-degenerate width and height"));
    return Result<std::vector<Point2>>::success(
        {first, {second.x, first.y}, second, {first.x, second.y}});
  }
  case GuiSketchCreateTool::CenterRectangle: {
    const Point2 center = anchors[0];
    const Point2 corner = anchors[1];
    const double half_width = std::abs(corner.x - center.x);
    const double half_height = std::abs(corner.y - center.y);
    if (half_width <= kEpsilon || half_height <= kEpsilon)
      return Result<std::vector<Point2>>::failure(
          create_error("rectangle corners must span a non-degenerate width and height"));
    return Result<std::vector<Point2>>::success({{center.x - half_width, center.y - half_height},
                                                 {center.x + half_width, center.y - half_height},
                                                 {center.x + half_width, center.y + half_height},
                                                 {center.x - half_width, center.y + half_height}});
  }
  case GuiSketchCreateTool::ThreePointRectangle: {
    const Point2 base_start = anchors[0];
    const Point2 base_end = anchors[1];
    const Point2 height_reference = anchors[2];
    const double base_length = distance(base_start, base_end);
    if (base_length <= kEpsilon)
      return Result<std::vector<Point2>>::failure(
          create_error("rectangle base corners must be distinct"));
    const Point2 unit{(base_end.x - base_start.x) / base_length,
                      (base_end.y - base_start.y) / base_length};
    const Point2 normal{-unit.y, unit.x};
    const double height = (height_reference.x - base_start.x) * normal.x +
                          (height_reference.y - base_start.y) * normal.y;
    if (std::abs(height) <= kEpsilon)
      return Result<std::vector<Point2>>::failure(
          create_error("rectangle height reference must leave the base line"));
    return Result<std::vector<Point2>>::success(
        {base_start,
         base_end,
         {base_end.x + normal.x * height, base_end.y + normal.y * height},
         {base_start.x + normal.x * height, base_start.y + normal.y * height}});
  }
  case GuiSketchCreateTool::Parallelogram: {
    const Point2 first = anchors[0];
    const Point2 second = anchors[1];
    const Point2 third = anchors[2];
    if (std::abs(cross(first, second, third)) <= kEpsilon)
      return Result<std::vector<Point2>>::failure(
          create_error("parallelogram picks must not be collinear"));
    return Result<std::vector<Point2>>::success(
        {first, second, third, {first.x + third.x - second.x, first.y + third.y - second.y}});
  }
  case GuiSketchCreateTool::RegularPolygon: {
    const Point2 center = anchors[0];
    const Point2 vertex = anchors[1];
    const double radius = distance(center, vertex);
    if (radius <= kEpsilon)
      return Result<std::vector<Point2>>::failure(
          create_error("regular polygon radius must be positive"));
    const double start_angle = std::atan2(vertex.y - center.y, vertex.x - center.x);
    std::vector<Point2> ring;
    ring.reserve(options_.polygon_sides);
    for (std::size_t index = 0U; index < options_.polygon_sides; ++index) {
      const double angle = start_angle + kTwoPi * static_cast<double>(index) /
                                             static_cast<double>(options_.polygon_sides);
      ring.push_back({center.x + radius * std::cos(angle),
                      center.y + radius * std::sin(angle)});
    }
    return Result<std::vector<Point2>>::success(std::move(ring));
  }
  default: break;
  }
  return Result<std::vector<Point2>>::failure(
      create_error("the active Sketch creation tool has no corner ring"));
}

Result<std::vector<Point2>>
GuiSketchCreateController::conic_preview(std::optional<Point2> hover, bool& closed) const {
  std::vector<GuiSketchCreatePick> anchors = picks_;
  if (anchors.size() < required_picks() && hover)
    anchors.push_back({*hover, GuiSketchSnapKind::None});
  if (anchors.size() < required_picks())
    return Result<std::vector<Point2>>::failure(
        create_error("the Sketch creation tool does not have enough picks yet"));

  if (is_circle_tool(tool_)) {
    auto circle = circle_geometry(tool_, anchors);
    if (circle.has_error()) return Result<std::vector<Point2>>::failure(circle.error());
    closed = true;
    return Result<std::vector<Point2>>::success(sample_circle(circle.value()));
  }
  if (tool_ == GuiSketchCreateTool::CenterStartEndArc) {
    closed = false;
    return sample_center_arc(anchors[0].point, anchors[1].point, anchors[2].point);
  }
  if (tool_ == GuiSketchCreateTool::ThreePointArc ||
      tool_ == GuiSketchCreateTool::TangentArc) {
    closed = false;
    return sample_three_point_arc(anchors[0].point, anchors[1].point, anchors[2].point);
  }
  if (tool_ == GuiSketchCreateTool::Ellipse ||
      tool_ == GuiSketchCreateTool::EllipticalArc) {
    auto ellipse = ellipse_geometry(anchors[0].point, anchors[1].point, anchors[2].point);
    if (ellipse.has_error()) return Result<std::vector<Point2>>::failure(ellipse.error());
    if (tool_ == GuiSketchCreateTool::Ellipse) {
      closed = true;
      auto points = sample_ellipse(ellipse.value(), 0.0, kTwoPi);
      points.pop_back();
      return Result<std::vector<Point2>>::success(std::move(points));
    }
    const double start = ellipse_parameter(ellipse.value(), anchors[3].point);
    const double sweep = normalize_signed_angle(
        ellipse_parameter(ellipse.value(), anchors[4].point) - start);
    if (std::abs(sweep) <= kEpsilon)
      return Result<std::vector<Point2>>::failure(
          create_error("elliptical arc sweep must be non-zero"));
    closed = false;
    return Result<std::vector<Point2>>::success(
        sample_ellipse(ellipse.value(), start, sweep, 32U));
  }
  if (tool_ == GuiSketchCreateTool::CenterSlot ||
      tool_ == GuiSketchCreateTool::OverallSlot) {
    auto slot = slot_geometry(tool_, anchors[0].point, anchors[1].point, anchors[2].point);
    if (slot.has_error()) return Result<std::vector<Point2>>::failure(slot.error());
    closed = true;
    return Result<std::vector<Point2>>::success(sample_slot(slot.value()));
  }
  return Result<std::vector<Point2>>::failure(create_error("tool is not a conic or slot tool"));
}

GuiSketchCreatePreview
GuiSketchCreateController::preview(std::optional<GuiSketchCreatePick> hover) const {
  GuiSketchCreatePreview result;
  const std::optional<Point2> hover_point =
      hover ? std::optional<Point2>(hover->point) : std::nullopt;

  switch (tool_) {
  case GuiSketchCreateTool::Point:
    if (!picks_.empty())
      result.rubber_band.push_back(picks_.front().point);
    else if (hover_point)
      result.rubber_band.push_back(*hover_point);
    break;
  case GuiSketchCreateTool::Line:
  case GuiSketchCreateTool::Polyline:
  case GuiSketchCreateTool::Centerline:
    for (const auto& pick : picks_) result.rubber_band.push_back(pick.point);
    if (hover_point && accepts_more_picks()) result.rubber_band.push_back(*hover_point);
    break;
  case GuiSketchCreateTool::CornerRectangle:
  case GuiSketchCreateTool::CenterRectangle:
  case GuiSketchCreateTool::ThreePointRectangle:
  case GuiSketchCreateTool::Parallelogram:
  case GuiSketchCreateTool::RegularPolygon: {
    auto ring = corner_ring(hover_point);
    if (ring.has_error()) {
      for (const auto& pick : picks_) result.rubber_band.push_back(pick.point);
      if (hover_point && accepts_more_picks()) result.rubber_band.push_back(*hover_point);
    } else {
      result.rubber_band = std::move(ring.value());
      result.closed = true;
    }
    break;
  }
  default: {
    bool closed = false;
    auto curve = conic_preview(hover_point, closed);
    if (curve.has_error()) {
      for (const auto& pick : picks_) result.rubber_band.push_back(pick.point);
      if (hover_point && accepts_more_picks()) result.rubber_band.push_back(*hover_point);
    } else {
      result.rubber_band = std::move(curve.value());
      result.closed = closed;
    }
    break;
  }
  }

  const auto append_constraint = [&result](const GuiSketchCreatePick& pick) {
    const std::string_view kind = constraint_preview_kind(pick.snap);
    if (!kind.empty()) result.constraints.push_back({std::string(kind), pick.point});
  };
  for (const auto& pick : picks_) append_constraint(pick);
  if (hover) append_constraint(*hover);
  if (is_tangent_tool(tool_)) {
    for (const auto& pick : picks_) result.constraints.push_back({"tangent", pick.point});
    if (hover) result.constraints.push_back({"tangent", hover->point});
  }
  return result;
}

Result<GuiSketchCreateExpansion>
GuiSketchCreateController::expansion(const PartDocument& document) const {
  if (!complete_ready())
    return Result<GuiSketchCreateExpansion>::failure(
        create_error("the Sketch creation tool does not have enough picks yet"));
  const Sketch* sketch = document.find_sketch(sketch_);
  if (sketch == nullptr)
    return Result<GuiSketchCreateExpansion>::failure(
        create_error("Sketch creation requires an existing planar Sketch"));

  GuiSketchCreateExpansion result;
  result.label = "Create sketch " + std::string(to_string(tool_));

  const auto created_entity_taken = [&result](const SketchEntityId& id) {
    const auto line = std::any_of(result.lines.begin(), result.lines.end(),
                                  [&id](const LineSegment& entity) { return entity.id() == id; });
    const auto arc = std::any_of(result.arcs.begin(), result.arcs.end(),
                                 [&id](const ArcSegment& entity) { return entity.id() == id; });
    const auto spline = std::any_of(result.splines.begin(), result.splines.end(),
                                    [&id](const SplineSegment& entity) { return entity.id() == id; });
    return line || arc || spline;
  };
  const auto next_entity_id = [&sketch, &created_entity_taken](std::string_view prefix) {
    return SketchEntityId(next_free_id(prefix, [&](const std::string& candidate) {
      const SketchEntityId id(candidate);
      return sketch_entity_id_taken(*sketch, id) || created_entity_taken(id);
    }));
  };
  const auto next_profile_id = [&sketch]() {
    return ProfileId(next_free_id("profile.create.", [&](const std::string& candidate) {
      return profile_id_taken(*sketch, ProfileId(candidate));
    }));
  };
  const auto append_chain = [&](const std::vector<Point2>& points,
                                bool closed) -> Result<std::size_t> {
    const std::size_t segment_count = closed ? points.size() : points.size() - 1U;
    for (std::size_t index = 0U; index < segment_count; ++index) {
      auto line = LineSegment::create(next_entity_id("line.create."), points[index],
                                      points[(index + 1U) % points.size()]);
      if (line.has_error()) return Result<std::size_t>::failure(line.error());
      result.lines.push_back(std::move(line.value()));
    }
    return Result<std::size_t>::success(result.lines.size());
  };
  const auto append_ellipse = [&](const EllipseGeometry& ellipse, double start,
                                  double sweep) -> Result<std::size_t> {
    const std::size_t segment_count = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(std::abs(sweep) / (kPi * 0.5))));
    const double step = sweep / static_cast<double>(segment_count);
    Point2 first_start{};
    for (std::size_t index = 0U; index < segment_count; ++index) {
      const double first_parameter = start + step * static_cast<double>(index);
      const double second_parameter = start + step * static_cast<double>(index + 1U);
      Point2 first = ellipse_point(ellipse, first_parameter);
      Point2 second = ellipse_point(ellipse, second_parameter);
      if (index == 0U) first_start = first;
      if (index + 1U == segment_count && std::abs(std::abs(sweep) - kTwoPi) <= kEpsilon)
        second = first_start;
      const double alpha = (4.0 / 3.0) * std::tan(step * 0.25);
      const Point2 control1 = add(first, scale(ellipse_derivative(ellipse, first_parameter), alpha));
      const Point2 control2 =
          add(second, scale(ellipse_derivative(ellipse, second_parameter), -alpha));
      auto spline = SplineSegment::create_cubic_bezier(
          next_entity_id("spline.create."), first, control1, control2, second);
      if (spline.has_error()) return Result<std::size_t>::failure(spline.error());
      result.splines.push_back(std::move(spline.value()));
    }
    return Result<std::size_t>::success(result.splines.size());
  };

  switch (tool_) {
  case GuiSketchCreateTool::Point: {
    const Point3 position = plane_.plane_to_model(picks_.front().point);
    auto point = ConstructionPoint::create_explicit(
        ConstructionPointId(next_free_id(
            "construction.point.create.",
            [&document](const std::string& candidate) {
              return document.find_construction_point(ConstructionPointId(candidate)) != nullptr;
            })),
        "Sketch point", position);
    if (point.has_error()) return Result<GuiSketchCreateExpansion>::failure(point.error());
    result.construction_point = std::move(point.value());
    result.label = "Create construction point";
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }
  case GuiSketchCreateTool::Centerline: {
    const Point3 start = plane_.plane_to_model(picks_[0].point);
    const Point3 end = plane_.plane_to_model(picks_[1].point);
    const Vector3 delta{end.x - start.x, end.y - start.y, end.z - start.z};
    const double magnitude = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (magnitude <= kEpsilon)
      return Result<GuiSketchCreateExpansion>::failure(
          create_error("centerline picks must be distinct"));
    const Vector3 direction{delta.x / magnitude, delta.y / magnitude, delta.z / magnitude};
    auto line = ConstructionLine::create_explicit(
        ConstructionLineId(next_free_id(
            "construction.line.create.",
            [&document](const std::string& candidate) {
              return document.find_construction_line(ConstructionLineId(candidate)) != nullptr;
            })),
        "Sketch centerline", start, direction);
    if (line.has_error()) return Result<GuiSketchCreateExpansion>::failure(line.error());
    result.construction_line = std::move(line.value());
    result.label = "Create construction centerline";
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }
  case GuiSketchCreateTool::Line:
  case GuiSketchCreateTool::Polyline: {
    std::vector<Point2> points;
    points.reserve(picks_.size());
    for (const auto& pick : picks_) points.push_back(pick.point);
    for (std::size_t index = 1U; index < points.size(); ++index)
      if (distance(points[index - 1U], points[index]) <= kEpsilon)
        return Result<GuiSketchCreateExpansion>::failure(
            create_error("consecutive Sketch picks must be distinct"));
    auto appended = append_chain(points, false);
    if (appended.has_error())
      return Result<GuiSketchCreateExpansion>::failure(appended.error());
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }
  case GuiSketchCreateTool::CornerRectangle:
  case GuiSketchCreateTool::CenterRectangle:
  case GuiSketchCreateTool::ThreePointRectangle:
  case GuiSketchCreateTool::Parallelogram:
  case GuiSketchCreateTool::RegularPolygon: {
    auto ring = corner_ring(std::nullopt);
    if (ring.has_error()) return Result<GuiSketchCreateExpansion>::failure(ring.error());
    auto appended = append_chain(ring.value(), true);
    if (appended.has_error())
      return Result<GuiSketchCreateExpansion>::failure(appended.error());
    std::vector<SketchEntityId> ordered;
    ordered.reserve(result.lines.size());
    for (const auto& line : result.lines) ordered.push_back(line.id());
    auto profile = ClosedProfile::create(next_profile_id(), std::move(ordered));
    if (profile.has_error()) return Result<GuiSketchCreateExpansion>::failure(profile.error());
    result.profile = std::move(profile.value());
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }
  default: break;
  }

  if (is_circle_tool(tool_)) {
    auto circle = circle_geometry(tool_, picks_);
    if (circle.has_error()) return Result<GuiSketchCreateExpansion>::failure(circle.error());
    const ParameterId diameter_id(next_free_id(
        "parameter.sketch.circle_diameter.", [&document](const std::string& candidate) {
          return document.find_parameter(ParameterId(candidate)) != nullptr;
        }));
    auto diameter = Quantity::length_mm(circle.value().radius * 2.0, diameter_id.value());
    if (diameter.has_error()) return Result<GuiSketchCreateExpansion>::failure(diameter.error());
    auto parameter = Parameter::create_length(diameter_id, diameter_id.value(), diameter.value());
    if (parameter.has_error()) return Result<GuiSketchCreateExpansion>::failure(parameter.error());
    auto profile = CircleProfile::create(next_profile_id(), diameter_id, circle.value().center);
    if (profile.has_error()) return Result<GuiSketchCreateExpansion>::failure(profile.error());
    result.parameter = std::move(parameter.value());
    result.circle_profile = std::move(profile.value());
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }

  if (is_arc_tool(tool_)) {
    Point2 start{};
    Point2 mid{};
    Point2 end{};
    if (tool_ == GuiSketchCreateTool::CenterStartEndArc) {
      auto samples = sample_center_arc(picks_[0].point, picks_[1].point, picks_[2].point);
      if (samples.has_error()) return Result<GuiSketchCreateExpansion>::failure(samples.error());
      start = samples.value().front();
      mid = samples.value()[samples.value().size() / 2U];
      end = samples.value().back();
    } else {
      start = picks_[0].point;
      mid = picks_[1].point;
      end = picks_[2].point;
    }
    auto arc = ArcSegment::create_three_point(next_entity_id("arc.create."), start, mid, end);
    if (arc.has_error()) return Result<GuiSketchCreateExpansion>::failure(arc.error());
    result.arcs.push_back(std::move(arc.value()));
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }

  if (tool_ == GuiSketchCreateTool::Ellipse ||
      tool_ == GuiSketchCreateTool::EllipticalArc) {
    auto ellipse = ellipse_geometry(picks_[0].point, picks_[1].point, picks_[2].point);
    if (ellipse.has_error()) return Result<GuiSketchCreateExpansion>::failure(ellipse.error());
    double start = 0.0;
    double sweep = kTwoPi;
    if (tool_ == GuiSketchCreateTool::EllipticalArc) {
      start = ellipse_parameter(ellipse.value(), picks_[3].point);
      sweep = normalize_signed_angle(ellipse_parameter(ellipse.value(), picks_[4].point) - start);
      if (std::abs(sweep) <= kEpsilon)
        return Result<GuiSketchCreateExpansion>::failure(
            create_error("elliptical arc sweep must be non-zero"));
    }
    auto appended = append_ellipse(ellipse.value(), start, sweep);
    if (appended.has_error())
      return Result<GuiSketchCreateExpansion>::failure(appended.error());
    if (tool_ == GuiSketchCreateTool::Ellipse) {
      std::vector<SketchEntityId> ordered;
      ordered.reserve(result.splines.size());
      for (const auto& spline : result.splines) ordered.push_back(spline.id());
      auto profile = ArcClosedProfile::create(next_profile_id(), std::move(ordered));
      if (profile.has_error()) return Result<GuiSketchCreateExpansion>::failure(profile.error());
      result.curve_profile = std::move(profile.value());
    }
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }

  if (tool_ == GuiSketchCreateTool::CenterSlot ||
      tool_ == GuiSketchCreateTool::OverallSlot) {
    auto slot = slot_geometry(tool_, picks_[0].point, picks_[1].point, picks_[2].point);
    if (slot.has_error()) return Result<GuiSketchCreateExpansion>::failure(slot.error());
    const Point2 top_first = add(slot.value().first_center,
                                 scale(slot.value().normal_unit, slot.value().radius));
    const Point2 top_second = add(slot.value().second_center,
                                  scale(slot.value().normal_unit, slot.value().radius));
    const Point2 bottom_second = add(slot.value().second_center,
                                     scale(slot.value().normal_unit, -slot.value().radius));
    const Point2 bottom_first = add(slot.value().first_center,
                                    scale(slot.value().normal_unit, -slot.value().radius));
    auto top = LineSegment::create(next_entity_id("line.create."), top_first, top_second);
    if (top.has_error()) return Result<GuiSketchCreateExpansion>::failure(top.error());
    result.lines.push_back(std::move(top.value()));
    auto second_arc = ArcSegment::create_three_point(
        next_entity_id("arc.create."), top_second,
        add(slot.value().second_center, scale(slot.value().axis_unit, slot.value().radius)),
        bottom_second);
    if (second_arc.has_error())
      return Result<GuiSketchCreateExpansion>::failure(second_arc.error());
    result.arcs.push_back(std::move(second_arc.value()));
    auto bottom = LineSegment::create(next_entity_id("line.create."), bottom_second, bottom_first);
    if (bottom.has_error()) return Result<GuiSketchCreateExpansion>::failure(bottom.error());
    result.lines.push_back(std::move(bottom.value()));
    auto first_arc = ArcSegment::create_three_point(
        next_entity_id("arc.create."), bottom_first,
        add(slot.value().first_center, scale(slot.value().axis_unit, -slot.value().radius)),
        top_first);
    if (first_arc.has_error())
      return Result<GuiSketchCreateExpansion>::failure(first_arc.error());
    result.arcs.push_back(std::move(first_arc.value()));
    std::vector<SketchEntityId> ordered{result.lines[0].id(), result.arcs[0].id(),
                                        result.lines[1].id(), result.arcs[1].id()};
    auto profile = ArcClosedProfile::create(next_profile_id(), std::move(ordered));
    if (profile.has_error()) return Result<GuiSketchCreateExpansion>::failure(profile.error());
    result.curve_profile = std::move(profile.value());
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }

  return Result<GuiSketchCreateExpansion>::failure(create_error("unsupported Sketch creation tool"));
}

Result<std::size_t> GuiSketchCreateController::commit(GuiDocumentSession& session,
                                                      const GuiSketchWorkbench& workbench) const {
  if (session.part_document() == nullptr)
    return Result<std::size_t>::failure(
        create_error("Sketch creation requires an active Part document"));
  auto expanded = expansion(*session.part_document());
  if (expanded.has_error()) return Result<std::size_t>::failure(expanded.error());

  if (expanded.value().construction_point)
    return workbench.create_construction_point(session,
                                               std::move(*expanded.value().construction_point));
  if (expanded.value().construction_line)
    return workbench.create_construction_line(session,
                                              std::move(*expanded.value().construction_line));

  const SketchId sketch_id = sketch_;
  GuiSketchCreateExpansion value = std::move(expanded.value());
  const std::string label = value.label;
  return session.commit_part_transaction(
      label, [sketch_id, value = std::move(value)](PartDocument& document) mutable
                 -> Result<std::size_t> {
        if (value.parameter) {
          auto added = document.add_parameter(std::move(*value.parameter));
          if (added.has_error()) return added;
        }
        const Sketch* existing = document.find_sketch(sketch_id);
        if (existing == nullptr)
          return Result<std::size_t>::failure(
              Error::validation(sketch_id.value(), "Sketch creation target no longer exists"));
        Sketch candidate = *existing;
        std::size_t added_count = 0U;
        for (auto& line : value.lines) {
          auto added = candidate.add_entity(std::move(line));
          if (added.has_error()) return added;
          added_count = added.value();
        }
        for (auto& arc : value.arcs) {
          auto added = candidate.add_entity(std::move(arc));
          if (added.has_error()) return added;
          added_count = added.value();
        }
        for (auto& spline : value.splines) {
          auto added = candidate.add_entity(std::move(spline));
          if (added.has_error()) return added;
          added_count = added.value();
        }
        if (value.profile) {
          auto added = candidate.add_profile(std::move(*value.profile));
          if (added.has_error()) return added;
          added_count = added.value();
        }
        if (value.curve_profile) {
          auto added = candidate.add_profile(std::move(*value.curve_profile));
          if (added.has_error()) return added;
          added_count = added.value();
        }
        if (value.circle_profile) {
          auto added = candidate.add_profile(std::move(*value.circle_profile));
          if (added.has_error()) return added;
          added_count = added.value();
        }
        auto updated = document.update_sketch(std::move(candidate));
        if (updated.has_error()) return updated;
        return Result<std::size_t>::success(added_count);
      });
}

} // namespace blcad::gui
