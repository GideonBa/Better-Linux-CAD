#include "blcad/core/sketch.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace blcad {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] bool same_point(Point2 left, Point2 right) noexcept {
  return std::abs(left.x - right.x) <= k_tolerance && std::abs(left.y - right.y) <= k_tolerance;
}

[[nodiscard]] double orientation(Point2 a, Point2 b, Point2 c) noexcept {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

[[nodiscard]] bool on_segment(Point2 a, Point2 b, Point2 p) noexcept {
  return std::min(a.x, b.x) - k_tolerance <= p.x && p.x <= std::max(a.x, b.x) + k_tolerance &&
         std::min(a.y, b.y) - k_tolerance <= p.y && p.y <= std::max(a.y, b.y) + k_tolerance &&
         std::abs(orientation(a, b, p)) <= k_tolerance;
}

[[nodiscard]] bool segments_intersect(Point2 a1, Point2 a2, Point2 b1, Point2 b2) noexcept {
  const double o1 = orientation(a1, a2, b1);
  const double o2 = orientation(a1, a2, b2);
  const double o3 = orientation(b1, b2, a1);
  const double o4 = orientation(b1, b2, a2);

  if (((o1 > k_tolerance && o2 < -k_tolerance) ||
       (o1 < -k_tolerance && o2 > k_tolerance)) &&
      ((o3 > k_tolerance && o4 < -k_tolerance) ||
       (o3 < -k_tolerance && o4 > k_tolerance))) {
    return true;
  }

  return (std::abs(o1) <= k_tolerance && on_segment(a1, a2, b1)) ||
         (std::abs(o2) <= k_tolerance && on_segment(a1, a2, b2)) ||
         (std::abs(o3) <= k_tolerance && on_segment(b1, b2, a1)) ||
         (std::abs(o4) <= k_tolerance && on_segment(b1, b2, a2));
}

[[nodiscard]] bool are_adjacent_segments(std::size_t first, std::size_t second,
                                         std::size_t count) noexcept {
  return first + 1U == second || second + 1U == first ||
         (first == 0U && second + 1U == count) || (second == 0U && first + 1U == count);
}

} // namespace

Result<LineSegment> LineSegment::create(SketchEntityId id, Point2 start, Point2 end) {
  const auto object_id = id.empty() ? std::string("line_segment") : id.value();

  if (id.empty()) {
    return Result<LineSegment>::failure(
        Error::validation(object_id, "line segment id must not be empty"));
  }

  if (same_point(start, end)) {
    return Result<LineSegment>::failure(
        Error::validation(object_id, "line segment endpoints must not be identical"));
  }

  return Result<LineSegment>::success(LineSegment(std::move(id), start, end));
}

const SketchEntityId& LineSegment::id() const noexcept {
  return id_;
}

Point2 LineSegment::start() const noexcept {
  return start_;
}

Point2 LineSegment::end() const noexcept {
  return end_;
}

LineSegment::LineSegment(SketchEntityId id, Point2 start, Point2 end)
    : id_(std::move(id)), start_(start), end_(end) {}

Result<RectangleProfile> RectangleProfile::create(ProfileId id, ParameterId width_parameter,
                                                  ParameterId height_parameter, Point2 center) {
  const auto object_id = id.empty() ? std::string("rectangle_profile") : id.value();

  if (id.empty()) {
    return Result<RectangleProfile>::failure(
        Error::validation(object_id, "rectangle profile id must not be empty"));
  }

  if (width_parameter.empty()) {
    return Result<RectangleProfile>::failure(
        Error::validation(object_id, "rectangle width parameter id must not be empty"));
  }

  if (height_parameter.empty()) {
    return Result<RectangleProfile>::failure(
        Error::validation(object_id, "rectangle height parameter id must not be empty"));
  }

  return Result<RectangleProfile>::success(RectangleProfile(
      std::move(id), std::move(width_parameter), std::move(height_parameter), center));
}

const ProfileId& RectangleProfile::id() const noexcept {
  return id_;
}

Point2 RectangleProfile::center() const noexcept {
  return center_;
}

const ParameterId& RectangleProfile::width_parameter() const noexcept {
  return width_parameter_;
}

const ParameterId& RectangleProfile::height_parameter() const noexcept {
  return height_parameter_;
}

RectangleProfile::RectangleProfile(ProfileId id, ParameterId width_parameter,
                                   ParameterId height_parameter, Point2 center)
    : id_(std::move(id)), width_parameter_(std::move(width_parameter)),
      height_parameter_(std::move(height_parameter)), center_(center) {}

Result<CircleProfile> CircleProfile::create(ProfileId id, ParameterId diameter_parameter,
                                            Point2 center) {
  const auto object_id = id.empty() ? std::string("circle_profile") : id.value();

  if (id.empty()) {
    return Result<CircleProfile>::failure(
        Error::validation(object_id, "circle profile id must not be empty"));
  }

  if (diameter_parameter.empty()) {
    return Result<CircleProfile>::failure(
        Error::validation(object_id, "circle diameter parameter id must not be empty"));
  }

  return Result<CircleProfile>::success(
      CircleProfile(std::move(id), std::move(diameter_parameter), center));
}

const ProfileId& CircleProfile::id() const noexcept {
  return id_;
}

Point2 CircleProfile::center() const noexcept {
  return center_;
}

const ParameterId& CircleProfile::diameter_parameter() const noexcept {
  return diameter_parameter_;
}

CircleProfile::CircleProfile(ProfileId id, ParameterId diameter_parameter, Point2 center)
    : id_(std::move(id)), diameter_parameter_(std::move(diameter_parameter)), center_(center) {}

Result<ClosedProfile> ClosedProfile::create(ProfileId id,
                                            std::vector<SketchEntityId> line_segments) {
  const auto object_id = id.empty() ? std::string("closed_profile") : id.value();

  if (id.empty()) {
    return Result<ClosedProfile>::failure(
        Error::validation(object_id, "closed profile id must not be empty"));
  }

  if (line_segments.size() < 3U) {
    return Result<ClosedProfile>::failure(
        Error::validation(object_id, "closed profile requires at least three line segments"));
  }

  for (const auto& line_segment : line_segments) {
    if (line_segment.empty()) {
      return Result<ClosedProfile>::failure(
          Error::validation(object_id, "closed profile line segment ids must not be empty"));
    }
  }

  for (std::size_t i = 0; i < line_segments.size(); ++i) {
    for (std::size_t j = i + 1U; j < line_segments.size(); ++j) {
      if (line_segments[i] == line_segments[j]) {
        return Result<ClosedProfile>::failure(
            Error::validation(object_id,
                              "closed profile line segment ids must be unique within profile"));
      }
    }
  }

  return Result<ClosedProfile>::success(ClosedProfile(std::move(id), std::move(line_segments)));
}

const ProfileId& ClosedProfile::id() const noexcept {
  return id_;
}

const std::vector<SketchEntityId>& ClosedProfile::line_segments() const noexcept {
  return line_segments_;
}

ClosedProfile::ClosedProfile(ProfileId id, std::vector<SketchEntityId> line_segments)
    : id_(std::move(id)), line_segments_(std::move(line_segments)) {}

Result<Sketch> Sketch::create(SketchId id, std::string name, DatumPlaneId workplane) {
  const auto object_id = id.empty() ? std::string("sketch") : id.value();

  if (id.empty()) {
    return Result<Sketch>::failure(Error::validation(object_id, "sketch id must not be empty"));
  }

  if (name.empty()) {
    return Result<Sketch>::failure(Error::validation(object_id, "sketch name must not be empty"));
  }

  if (workplane.empty()) {
    return Result<Sketch>::failure(
        Error::validation(object_id, "sketch workplane id must not be empty"));
  }

  return Result<Sketch>::success(Sketch(std::move(id), std::move(name), std::move(workplane)));
}

Result<std::size_t> Sketch::add_entity(LineSegment line_segment) {
  if (has_entity_id(line_segment.id())) {
    return Result<std::size_t>::failure(
        Error::validation(line_segment.id().value(), "sketch entity id must be unique within sketch"));
  }

  line_segments_.push_back(std::move(line_segment));
  return Result<std::size_t>::success(line_segments_.size() - 1U);
}

Result<std::size_t> Sketch::add_profile(RectangleProfile profile) {
  if (has_profile_id(profile.id())) {
    return Result<std::size_t>::failure(
        Error::validation(profile.id().value(), "profile id must be unique within sketch"));
  }

  rectangle_profiles_.push_back(std::move(profile));
  return Result<std::size_t>::success(profile_count() - 1U);
}

Result<std::size_t> Sketch::add_profile(CircleProfile profile) {
  if (has_profile_id(profile.id())) {
    return Result<std::size_t>::failure(
        Error::validation(profile.id().value(), "profile id must be unique within sketch"));
  }

  circle_profiles_.push_back(std::move(profile));
  return Result<std::size_t>::success(profile_count() - 1U);
}

Result<std::size_t> Sketch::add_profile(ClosedProfile profile) {
  if (has_profile_id(profile.id())) {
    return Result<std::size_t>::failure(
        Error::validation(profile.id().value(), "profile id must be unique within sketch"));
  }

  const auto validated = validate_closed_profile(profile);
  if (validated.has_error()) {
    return Result<std::size_t>::failure(validated.error());
  }

  closed_profiles_.push_back(std::move(profile));
  return Result<std::size_t>::success(profile_count() - 1U);
}

const SketchId& Sketch::id() const noexcept {
  return id_;
}

const std::string& Sketch::name() const noexcept {
  return name_;
}

const DatumPlaneId& Sketch::workplane() const noexcept {
  return workplane_;
}

const std::vector<LineSegment>& Sketch::line_segments() const noexcept {
  return line_segments_;
}

const std::vector<RectangleProfile>& Sketch::rectangle_profiles() const noexcept {
  return rectangle_profiles_;
}

const std::vector<CircleProfile>& Sketch::circle_profiles() const noexcept {
  return circle_profiles_;
}

const std::vector<ClosedProfile>& Sketch::closed_profiles() const noexcept {
  return closed_profiles_;
}

std::size_t Sketch::profile_count() const noexcept {
  return rectangle_profiles_.size() + circle_profiles_.size() + closed_profiles_.size();
}

const LineSegment* Sketch::find_line_segment(SketchEntityId id) const noexcept {
  for (const auto& line_segment : line_segments_) {
    if (line_segment.id() == id) {
      return &line_segment;
    }
  }

  return nullptr;
}

const RectangleProfile* Sketch::find_rectangle_profile(ProfileId id) const noexcept {
  for (const auto& profile : rectangle_profiles_) {
    if (profile.id() == id) {
      return &profile;
    }
  }

  return nullptr;
}

const CircleProfile* Sketch::find_circle_profile(ProfileId id) const noexcept {
  for (const auto& profile : circle_profiles_) {
    if (profile.id() == id) {
      return &profile;
    }
  }

  return nullptr;
}

const ClosedProfile* Sketch::find_closed_profile(ProfileId id) const noexcept {
  for (const auto& profile : closed_profiles_) {
    if (profile.id() == id) {
      return &profile;
    }
  }

  return nullptr;
}

Result<std::vector<Point2>> Sketch::closed_profile_vertices(const ClosedProfile& profile) const {
  return validate_closed_profile(profile);
}

Sketch::Sketch(SketchId id, std::string name, DatumPlaneId workplane)
    : id_(std::move(id)), name_(std::move(name)), workplane_(std::move(workplane)) {}

bool Sketch::has_entity_id(const SketchEntityId& id) const noexcept {
  return find_line_segment(id) != nullptr;
}

bool Sketch::has_profile_id(const ProfileId& id) const noexcept {
  return find_rectangle_profile(id) != nullptr || find_circle_profile(id) != nullptr ||
         find_closed_profile(id) != nullptr;
}

Result<std::vector<Point2>> Sketch::validate_closed_profile(const ClosedProfile& profile) const {
  std::vector<const LineSegment*> segments;
  segments.reserve(profile.line_segments().size());

  for (const auto& line_segment_id : profile.line_segments()) {
    const LineSegment* line_segment = find_line_segment(line_segment_id);
    if (line_segment == nullptr) {
      return Result<std::vector<Point2>>::failure(Error::validation(
          profile.id().value(), "closed profile line segment must exist in sketch"));
    }

    segments.push_back(line_segment);
  }

  for (std::size_t i = 0; i < segments.size(); ++i) {
    const LineSegment& current = *segments[i];
    const LineSegment& next = *segments[(i + 1U) % segments.size()];
    if (!same_point(current.end(), next.start())) {
      return Result<std::vector<Point2>>::failure(Error::validation(
          profile.id().value(), "closed profile line segments must be ordered and connected"));
    }
  }

  std::vector<Point2> vertices;
  vertices.reserve(segments.size());
  for (const LineSegment* segment : segments) {
    vertices.push_back(segment->start());
  }

  for (std::size_t i = 0; i < segments.size(); ++i) {
    for (std::size_t j = i + 1U; j < segments.size(); ++j) {
      if (are_adjacent_segments(i, j, segments.size())) {
        continue;
      }

      if (segments_intersect(segments[i]->start(), segments[i]->end(), segments[j]->start(),
                             segments[j]->end())) {
        return Result<std::vector<Point2>>::failure(
            Error::validation(profile.id().value(),
                              "closed profile line segments must not self-intersect"));
      }
    }
  }

  return Result<std::vector<Point2>>::success(std::move(vertices));
}

} // namespace blcad
