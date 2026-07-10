#include "blcad/core/sketch.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace blcad {
namespace {

constexpr double k_tolerance = 1.0e-9;
constexpr double k_pi = 3.141592653589793238462643383279502884;
constexpr double k_two_pi = 2.0 * k_pi;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool same_point(Point2 left, Point2 right) noexcept {
  return std::abs(left.x - right.x) <= k_tolerance && std::abs(left.y - right.y) <= k_tolerance;
}

[[nodiscard]] double orientation(Point2 a, Point2 b, Point2 c) noexcept {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

[[nodiscard]] double distance(Point2 a, Point2 b) noexcept {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] bool has_duplicate_id(const std::vector<SketchEntityId>& ids) noexcept {
  for (std::size_t i = 0; i < ids.size(); ++i) {
    for (std::size_t j = i + 1U; j < ids.size(); ++j) {
      if (ids[i] == ids[j]) return true;
    }
  }
  return false;
}

struct ArcGeometry { Point2 start; Point2 mid; Point2 end; Point2 center; double radius; bool counter_clockwise; };
struct CurveGeometry { SketchEntityId id; Point2 start; Point2 end; bool is_arc; ArcGeometry arc; };

[[nodiscard]] double normalize_angle(double angle) noexcept { while (angle < 0.0) angle += k_two_pi; while (angle >= k_two_pi) angle -= k_two_pi; return angle; }
[[nodiscard]] double angle_of(Point2 center, Point2 point) noexcept { return normalize_angle(std::atan2(point.y - center.y, point.x - center.x)); }
[[nodiscard]] bool angle_between_ccw(double start, double end, double value) noexcept { const double ccw_end = end < start ? end + k_two_pi : end; double candidate = value < start ? value + k_two_pi : value; return candidate >= start - k_tolerance && candidate <= ccw_end + k_tolerance; }
[[nodiscard]] bool arc_contains_point(const ArcGeometry& arc, Point2 point) noexcept { if (std::abs(distance(point, arc.center) - arc.radius) > 1.0e-7) return false; const double s = angle_of(arc.center, arc.start); const double e = angle_of(arc.center, arc.end); const double p = angle_of(arc.center, point); return arc.counter_clockwise ? angle_between_ccw(s, e, p) : angle_between_ccw(e, s, p); }

[[nodiscard]] Result<ArcGeometry> make_arc_geometry(const ArcSegment& arc) {
  const Point2 start = arc.start(); const Point2 mid = arc.mid(); const Point2 end = arc.end();
  const double d = 2.0 * (start.x * (mid.y - end.y) + mid.x * (end.y - start.y) + end.x * (start.y - mid.y));
  if (std::abs(d) <= k_tolerance) return Result<ArcGeometry>::failure(validation_error(arc.id().value(), "arc segment three-point definition must not be collinear"));
  const double sn = start.x * start.x + start.y * start.y; const double mn = mid.x * mid.x + mid.y * mid.y; const double en = end.x * end.x + end.y * end.y;
  const Point2 center{(sn * (mid.y - end.y) + mn * (end.y - start.y) + en * (start.y - mid.y)) / d, (sn * (end.x - mid.x) + mn * (start.x - end.x) + en * (mid.x - start.x)) / d};
  return Result<ArcGeometry>::success(ArcGeometry{start, mid, end, center, distance(center, start), orientation(start, mid, end) > 0.0});
}

[[nodiscard]] bool parameter_on_segment(double parameter) noexcept { return parameter >= -k_tolerance && parameter <= 1.0 + k_tolerance; }
[[nodiscard]] bool point_on_segment(Point2 start, Point2 end, Point2 point) noexcept { if (std::abs(orientation(start, end, point)) > k_tolerance) return false; return point.x >= std::min(start.x, end.x) - k_tolerance && point.x <= std::max(start.x, end.x) + k_tolerance && point.y >= std::min(start.y, end.y) - k_tolerance && point.y <= std::max(start.y, end.y) + k_tolerance; }

[[nodiscard]] bool line_line_intersects(Point2 a1, Point2 a2, Point2 b1, Point2 b2) noexcept {
  const double o1 = orientation(a1, a2, b1); const double o2 = orientation(a1, a2, b2); const double o3 = orientation(b1, b2, a1); const double o4 = orientation(b1, b2, a2);
  if (((o1 > k_tolerance && o2 < -k_tolerance) || (o1 < -k_tolerance && o2 > k_tolerance)) && ((o3 > k_tolerance && o4 < -k_tolerance) || (o3 < -k_tolerance && o4 > k_tolerance))) return true;
  return point_on_segment(a1, a2, b1) || point_on_segment(a1, a2, b2) || point_on_segment(b1, b2, a1) || point_on_segment(b1, b2, a2);
}

[[nodiscard]] std::vector<Point2> line_arc_intersections(Point2 line_start, Point2 line_end, const ArcGeometry& arc) {
  std::vector<Point2> result; const double dx = line_end.x - line_start.x; const double dy = line_end.y - line_start.y; const double fx = line_start.x - arc.center.x; const double fy = line_start.y - arc.center.y; const double a = dx * dx + dy * dy; const double b = 2.0 * (fx * dx + fy * dy); const double c = fx * fx + fy * fy - arc.radius * arc.radius; const double discriminant = b * b - 4.0 * a * c; if (discriminant < -k_tolerance) return result; const double root = std::sqrt(std::max(0.0, discriminant)); const double parameters[2] = {(-b - root) / (2.0 * a), (-b + root) / (2.0 * a)};
  for (const double parameter : parameters) { if (!parameter_on_segment(parameter)) continue; const Point2 point{line_start.x + parameter * dx, line_start.y + parameter * dy}; if (!arc_contains_point(arc, point)) continue; bool duplicate = false; for (const auto& existing : result) duplicate = duplicate || same_point(existing, point); if (!duplicate) result.push_back(point); }
  return result;
}

[[nodiscard]] std::vector<Point2> arc_arc_intersections(const ArcGeometry& first, const ArcGeometry& second) {
  std::vector<Point2> result; const double d = distance(first.center, second.center); if (d <= k_tolerance) return result; if (d > first.radius + second.radius + k_tolerance) return result; if (d < std::abs(first.radius - second.radius) - k_tolerance) return result; const double a = (first.radius * first.radius - second.radius * second.radius + d * d) / (2.0 * d); const double h2 = first.radius * first.radius - a * a; if (h2 < -k_tolerance) return result; const double h = std::sqrt(std::max(0.0, h2)); const double x2 = first.center.x + a * (second.center.x - first.center.x) / d; const double y2 = first.center.y + a * (second.center.y - first.center.y) / d; const double rx = -(second.center.y - first.center.y) * (h / d); const double ry = (second.center.x - first.center.x) * (h / d); const Point2 candidates[2] = {Point2{x2 + rx, y2 + ry}, Point2{x2 - rx, y2 - ry}};
  for (const Point2 point : candidates) { if (!arc_contains_point(first, point) || !arc_contains_point(second, point)) continue; bool duplicate = false; for (const auto& existing : result) duplicate = duplicate || same_point(existing, point); if (!duplicate) result.push_back(point); }
  return result;
}

[[nodiscard]] bool is_adjacent(std::size_t first, std::size_t second, std::size_t count) noexcept { return first + 1U == second || second + 1U == first || (first == 0U && second + 1U == count) || (second == 0U && first + 1U == count); }
[[nodiscard]] bool is_allowed_shared_endpoint(const CurveGeometry& first, const CurveGeometry& second, Point2 point, bool adjacent) noexcept { if (!adjacent) return false; return (same_point(first.end, point) && same_point(second.start, point)) || (same_point(first.start, point) && same_point(second.end, point)); }

[[nodiscard]] bool curves_intersect_invalidly(const CurveGeometry& first, const CurveGeometry& second, bool adjacent) {
  if (!first.is_arc && !second.is_arc) return !adjacent && line_line_intersects(first.start, first.end, second.start, second.end);
  std::vector<Point2> intersections;
  if (!first.is_arc && second.is_arc) intersections = line_arc_intersections(first.start, first.end, second.arc);
  else if (first.is_arc && !second.is_arc) intersections = line_arc_intersections(second.start, second.end, first.arc);
  else intersections = arc_arc_intersections(first.arc, second.arc);
  for (const Point2 point : intersections) if (!is_allowed_shared_endpoint(first, second, point, adjacent)) return true;
  return false;
}

} // namespace

std::string_view to_string(SketchTrimExtendOperationKind kind) noexcept { switch (kind) { case SketchTrimExtendOperationKind::Trim: return "trim"; case SketchTrimExtendOperationKind::Extend: return "extend"; } return "trim"; }

Result<ArcSegment> ArcSegment::create_three_point(SketchEntityId id, Point2 start, Point2 mid, Point2 end) {
  const auto object_id = id.empty() ? std::string("arc_segment") : id.value();
  if (id.empty()) return Result<ArcSegment>::failure(validation_error(object_id, "arc segment id must not be empty"));
  if (same_point(start, mid) || same_point(start, end) || same_point(mid, end)) return Result<ArcSegment>::failure(validation_error(object_id, "arc segment points must be distinct"));
  if (std::abs(orientation(start, mid, end)) <= k_tolerance) return Result<ArcSegment>::failure(validation_error(object_id, "arc segment three-point definition must not be collinear"));
  return Result<ArcSegment>::success(ArcSegment(std::move(id), start, mid, end));
}

const SketchEntityId& ArcSegment::id() const noexcept { return id_; }
Point2 ArcSegment::start() const noexcept { return start_; }
Point2 ArcSegment::mid() const noexcept { return mid_; }
Point2 ArcSegment::end() const noexcept { return end_; }
ArcSegment::ArcSegment(SketchEntityId id, Point2 start, Point2 mid, Point2 end) : id_(std::move(id)), start_(start), mid_(mid), end_(end) {}

Result<SketchTrimExtendOperation> SketchTrimExtendOperation::create_trim(SketchTrimOperationId id, SketchEntityId target_entity, Point2 replacement_endpoint) { const auto object_id = id.empty() ? std::string("sketch_trim_operation") : id.value(); if (id.empty()) return Result<SketchTrimExtendOperation>::failure(validation_error(object_id, "sketch trim operation id must not be empty")); if (target_entity.empty()) return Result<SketchTrimExtendOperation>::failure(validation_error(object_id, "sketch trim operation target entity must not be empty")); return Result<SketchTrimExtendOperation>::success(SketchTrimExtendOperation(std::move(id), SketchTrimExtendOperationKind::Trim, std::move(target_entity), replacement_endpoint)); }
Result<SketchTrimExtendOperation> SketchTrimExtendOperation::create_extend(SketchTrimOperationId id, SketchEntityId target_entity, Point2 replacement_endpoint) { const auto object_id = id.empty() ? std::string("sketch_trim_operation") : id.value(); if (id.empty()) return Result<SketchTrimExtendOperation>::failure(validation_error(object_id, "sketch trim operation id must not be empty")); if (target_entity.empty()) return Result<SketchTrimExtendOperation>::failure(validation_error(object_id, "sketch trim operation target entity must not be empty")); return Result<SketchTrimExtendOperation>::success(SketchTrimExtendOperation(std::move(id), SketchTrimExtendOperationKind::Extend, std::move(target_entity), replacement_endpoint)); }
const SketchTrimOperationId& SketchTrimExtendOperation::id() const noexcept { return id_; }
SketchTrimExtendOperationKind SketchTrimExtendOperation::kind() const noexcept { return kind_; }
const SketchEntityId& SketchTrimExtendOperation::target_entity() const noexcept { return target_entity_; }
Point2 SketchTrimExtendOperation::replacement_endpoint() const noexcept { return replacement_endpoint_; }
SketchTrimExtendOperation::SketchTrimExtendOperation(SketchTrimOperationId id, SketchTrimExtendOperationKind kind, SketchEntityId target_entity, Point2 replacement_endpoint) : id_(std::move(id)), kind_(kind), target_entity_(std::move(target_entity)), replacement_endpoint_(replacement_endpoint) {}

Result<ArcClosedProfile> ArcClosedProfile::create(ProfileId id, std::vector<SketchEntityId> curve_segments) { const auto object_id = id.empty() ? std::string("arc_closed_profile") : id.value(); if (id.empty()) return Result<ArcClosedProfile>::failure(validation_error(object_id, "arc closed profile id must not be empty")); if (curve_segments.size() < 3U) return Result<ArcClosedProfile>::failure(validation_error(object_id, "arc closed profile requires at least three curve segments")); for (const auto& segment : curve_segments) if (segment.empty()) return Result<ArcClosedProfile>::failure(validation_error(object_id, "arc closed profile curve segment ids must not be empty")); if (has_duplicate_id(curve_segments)) return Result<ArcClosedProfile>::failure(validation_error(object_id, "arc closed profile curve segment ids must be unique")); return Result<ArcClosedProfile>::success(ArcClosedProfile(std::move(id), std::move(curve_segments))); }
const ProfileId& ArcClosedProfile::id() const noexcept { return id_; }
const std::vector<SketchEntityId>& ArcClosedProfile::curve_segments() const noexcept { return curve_segments_; }
ArcClosedProfile::ArcClosedProfile(ProfileId id, std::vector<SketchEntityId> curve_segments) : id_(std::move(id)), curve_segments_(std::move(curve_segments)) {}

Result<std::size_t> Sketch::add_entity(ArcSegment arc_segment) { if (find_line_segment(arc_segment.id()) != nullptr || find_arc_segment(arc_segment.id()) != nullptr || find_projected_point(arc_segment.id()) != nullptr || find_projected_line(arc_segment.id()) != nullptr || find_reference_generated_line(arc_segment.id()) != nullptr) return Result<std::size_t>::failure(validation_error(arc_segment.id().value(), "sketch entity id must be unique within sketch")); arc_segments_.push_back(std::move(arc_segment)); return Result<std::size_t>::success(arc_segments_.size() - 1U); }
Result<std::size_t> Sketch::add_trim_extend_operation(SketchTrimExtendOperation operation) { if (find_trim_extend_operation(operation.id()) != nullptr) return Result<std::size_t>::failure(validation_error(operation.id().value(), "sketch trim/extend operation id must be unique within sketch")); if (find_line_segment(operation.target_entity()) == nullptr && find_arc_segment(operation.target_entity()) == nullptr) return Result<std::size_t>::failure(validation_error(operation.id().value(), "sketch trim/extend target must be an explicit line or arc")); trim_extend_operations_.push_back(std::move(operation)); return Result<std::size_t>::success(trim_extend_operations_.size() - 1U); }
Result<std::size_t> Sketch::add_profile(ArcClosedProfile profile) { if (find_rectangle_profile(profile.id()) != nullptr || find_circle_profile(profile.id()) != nullptr || find_closed_profile(profile.id()) != nullptr || find_arc_closed_profile(profile.id()) != nullptr || find_composite_closed_profile(profile.id()) != nullptr) return Result<std::size_t>::failure(validation_error(profile.id().value(), "profile id must be unique within sketch")); auto valid = validate_arc_closed_profile(profile); if (valid.has_error()) return Result<std::size_t>::failure(valid.error()); arc_closed_profiles_.push_back(std::move(profile)); return Result<std::size_t>::success(arc_closed_profiles_.size() - 1U); }

const std::vector<ArcSegment>& Sketch::arc_segments() const noexcept { return arc_segments_; }
const std::vector<SketchTrimExtendOperation>& Sketch::trim_extend_operations() const noexcept { return trim_extend_operations_; }
const std::vector<ArcClosedProfile>& Sketch::arc_closed_profiles() const noexcept { return arc_closed_profiles_; }
const ArcSegment* Sketch::find_arc_segment(SketchEntityId id) const noexcept { for (const auto& arc : arc_segments_) if (arc.id() == id) return &arc; return nullptr; }
const SketchTrimExtendOperation* Sketch::find_trim_extend_operation(SketchTrimOperationId id) const noexcept { for (const auto& operation : trim_extend_operations_) if (operation.id() == id) return &operation; return nullptr; }
const ArcClosedProfile* Sketch::find_arc_closed_profile(ProfileId id) const noexcept { for (const auto& profile : arc_closed_profiles_) if (profile.id() == id) return &profile; return nullptr; }
Result<std::vector<Point2>> Sketch::arc_closed_profile_vertices(const ArcClosedProfile& profile) const { return validate_arc_closed_profile(profile); }

Result<std::vector<Point2>> Sketch::validate_arc_closed_profile(const ArcClosedProfile& profile) const {
  std::vector<CurveGeometry> curves; curves.reserve(profile.curve_segments().size());
  for (const auto& curve_id : profile.curve_segments()) { if (const LineSegment* line = find_line_segment(curve_id)) { curves.push_back(CurveGeometry{curve_id, line->start(), line->end(), false, {}}); continue; } if (const ArcSegment* arc = find_arc_segment(curve_id)) { auto arc_geometry = make_arc_geometry(*arc); if (arc_geometry.has_error()) return Result<std::vector<Point2>>::failure(arc_geometry.error()); curves.push_back(CurveGeometry{curve_id, arc->start(), arc->end(), true, arc_geometry.value()}); continue; } return Result<std::vector<Point2>>::failure(validation_error(profile.id().value(), "arc closed profile curve segment must exist in sketch")); }
  for (std::size_t index = 0; index < curves.size(); ++index) { const auto& current = curves[index]; const auto& next = curves[(index + 1U) % curves.size()]; if (!same_point(current.end, next.start)) return Result<std::vector<Point2>>::failure(validation_error(profile.id().value(), "arc closed profile curve segments must be ordered and connected")); }
  for (std::size_t i = 0; i < curves.size(); ++i) for (std::size_t j = i + 1U; j < curves.size(); ++j) if (curves_intersect_invalidly(curves[i], curves[j], is_adjacent(i, j, curves.size()))) return Result<std::vector<Point2>>::failure(validation_error(profile.id().value(), "arc closed profile curve segments must not self-intersect"));
  std::vector<Point2> vertices; vertices.reserve(curves.size()); for (const auto& curve : curves) vertices.push_back(curve.start); return Result<std::vector<Point2>>::success(std::move(vertices));
}

} // namespace blcad
