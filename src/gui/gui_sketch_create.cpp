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
constexpr double kEpsilon = 1.0e-9;
constexpr std::size_t kMinimumPolygonSides = 3U;
constexpr std::size_t kMaximumPolygonSides = 64U;

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
  case GuiSketchCreateTool::Centerline: return 2U;
  case GuiSketchCreateTool::ThreePointRectangle:
  case GuiSketchCreateTool::Parallelogram: return 3U;
  }
  return 2U;
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
    if (std::abs(second.x - first.x) <= kEpsilon || std::abs(second.y - first.y) <= kEpsilon)
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
      const double angle = start_angle + 2.0 * kPi * static_cast<double>(index) /
                                             static_cast<double>(options_.polygon_sides);
      ring.push_back({center.x + radius * std::cos(angle),
                      center.y + radius * std::sin(angle)});
    }
    return Result<std::vector<Point2>>::success(std::move(ring));
  }
  case GuiSketchCreateTool::Point:
  case GuiSketchCreateTool::Line:
  case GuiSketchCreateTool::Polyline:
  case GuiSketchCreateTool::Centerline: break;
  }
  return Result<std::vector<Point2>>::failure(
      create_error("the active Sketch creation tool has no corner ring"));
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
  }

  const auto append_constraint = [&result](const GuiSketchCreatePick& pick) {
    const std::string_view kind = constraint_preview_kind(pick.snap);
    if (!kind.empty()) result.constraints.push_back({std::string(kind), pick.point});
  };
  for (const auto& pick : picks_) append_constraint(pick);
  if (hover) append_constraint(*hover);
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

  const auto next_entity_id = [&sketch](std::vector<LineSegment>& created) {
    return SketchEntityId(next_free_id("line.create.", [&](const std::string& candidate) {
      const SketchEntityId id(candidate);
      if (sketch_entity_id_taken(*sketch, id)) return true;
      return std::any_of(created.begin(), created.end(),
                         [&id](const LineSegment& line) { return line.id() == id; });
    }));
  };
  const auto append_chain = [&](const std::vector<Point2>& points,
                                bool closed) -> Result<std::size_t> {
    const std::size_t segment_count = closed ? points.size() : points.size() - 1U;
    for (std::size_t index = 0U; index < segment_count; ++index) {
      auto line = LineSegment::create(next_entity_id(result.lines), points[index],
                                      points[(index + 1U) % points.size()]);
      if (line.has_error()) return Result<std::size_t>::failure(line.error());
      result.lines.push_back(std::move(line.value()));
    }
    return Result<std::size_t>::success(result.lines.size());
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
    auto profile = ClosedProfile::create(
        ProfileId(next_free_id("profile.create.",
                               [&sketch](const std::string& candidate) {
                                 return profile_id_taken(*sketch, ProfileId(candidate));
                               })),
        std::move(ordered));
    if (profile.has_error()) return Result<GuiSketchCreateExpansion>::failure(profile.error());
    result.profile = std::move(profile.value());
    return Result<GuiSketchCreateExpansion>::success(std::move(result));
  }
  }
  return Result<GuiSketchCreateExpansion>::failure(
      create_error("unsupported Sketch creation tool"));
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

  auto lines = std::move(expanded.value().lines);
  auto profile = std::move(expanded.value().profile);
  return workbench.edit_sketch(
      session, sketch_, std::move(expanded.value().label),
      [&lines, &profile](Sketch& candidate) -> Result<std::size_t> {
        std::size_t added_count = 0U;
        for (auto& line : lines) {
          auto added = candidate.add_entity(std::move(line));
          if (added.has_error()) return added;
          added_count = added.value();
        }
        if (profile) {
          auto added = candidate.add_profile(std::move(*profile));
          if (added.has_error()) return added;
          added_count = added.value();
        }
        return Result<std::size_t>::success(added_count);
      });
}

} // namespace blcad::gui
