#include "blcad/core/sketch.hpp"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace blcad {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool same_point(Point2 left, Point2 right) noexcept {
  return std::abs(left.x - right.x) <= k_tolerance && std::abs(left.y - right.y) <= k_tolerance;
}

[[nodiscard]] double orientation(Point2 a, Point2 b, Point2 c) noexcept {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

[[nodiscard]] bool has_duplicate_id(const std::vector<SketchEntityId>& ids) noexcept {
  for (std::size_t i = 0; i < ids.size(); ++i) {
    for (std::size_t j = i + 1U; j < ids.size(); ++j) {
      if (ids[i] == ids[j]) return true;
    }
  }
  return false;
}

struct CurveEndpointPair {
  Point2 start;
  Point2 end;
};

} // namespace

std::string_view to_string(SketchTrimExtendOperationKind kind) noexcept {
  switch (kind) {
  case SketchTrimExtendOperationKind::Trim: return "trim";
  case SketchTrimExtendOperationKind::Extend: return "extend";
  }
  return "trim";
}

Result<ArcSegment> ArcSegment::create_three_point(SketchEntityId id, Point2 start, Point2 mid,
                                                  Point2 end) {
  const auto object_id = id.empty() ? std::string("arc_segment") : id.value();
  if (id.empty()) {
    return Result<ArcSegment>::failure(validation_error(object_id, "arc segment id must not be empty"));
  }
  if (same_point(start, mid) || same_point(start, end) || same_point(mid, end)) {
    return Result<ArcSegment>::failure(
        validation_error(object_id, "arc segment points must be distinct"));
  }
  if (std::abs(orientation(start, mid, end)) <= k_tolerance) {
    return Result<ArcSegment>::failure(
        validation_error(object_id, "arc segment three-point definition must not be collinear"));
  }
  return Result<ArcSegment>::success(ArcSegment(std::move(id), start, mid, end));
}

const SketchEntityId& ArcSegment::id() const noexcept { return id_; }
Point2 ArcSegment::start() const noexcept { return start_; }
Point2 ArcSegment::mid() const noexcept { return mid_; }
Point2 ArcSegment::end() const noexcept { return end_; }

ArcSegment::ArcSegment(SketchEntityId id, Point2 start, Point2 mid, Point2 end)
    : id_(std::move(id)), start_(start), mid_(mid), end_(end) {}

Result<SketchTrimExtendOperation> SketchTrimExtendOperation::create_trim(
    SketchTrimOperationId id, SketchEntityId target_entity, Point2 replacement_endpoint) {
  const auto object_id = id.empty() ? std::string("sketch_trim_operation") : id.value();
  if (id.empty()) {
    return Result<SketchTrimExtendOperation>::failure(
        validation_error(object_id, "sketch trim operation id must not be empty"));
  }
  if (target_entity.empty()) {
    return Result<SketchTrimExtendOperation>::failure(
        validation_error(object_id, "sketch trim operation target entity must not be empty"));
  }
  return Result<SketchTrimExtendOperation>::success(SketchTrimExtendOperation(
      std::move(id), SketchTrimExtendOperationKind::Trim, std::move(target_entity), replacement_endpoint));
}

Result<SketchTrimExtendOperation> SketchTrimExtendOperation::create_extend(
    SketchTrimOperationId id, SketchEntityId target_entity, Point2 replacement_endpoint) {
  const auto object_id = id.empty() ? std::string("sketch_trim_operation") : id.value();
  if (id.empty()) {
    return Result<SketchTrimExtendOperation>::failure(
        validation_error(object_id, "sketch trim operation id must not be empty"));
  }
  if (target_entity.empty()) {
    return Result<SketchTrimExtendOperation>::failure(
        validation_error(object_id, "sketch trim operation target entity must not be empty"));
  }
  return Result<SketchTrimExtendOperation>::success(SketchTrimExtendOperation(
      std::move(id), SketchTrimExtendOperationKind::Extend, std::move(target_entity), replacement_endpoint));
}

const SketchTrimOperationId& SketchTrimExtendOperation::id() const noexcept { return id_; }
SketchTrimExtendOperationKind SketchTrimExtendOperation::kind() const noexcept { return kind_; }
const SketchEntityId& SketchTrimExtendOperation::target_entity() const noexcept { return target_entity_; }
Point2 SketchTrimExtendOperation::replacement_endpoint() const noexcept { return replacement_endpoint_; }

SketchTrimExtendOperation::SketchTrimExtendOperation(SketchTrimOperationId id,
                                                     SketchTrimExtendOperationKind kind,
                                                     SketchEntityId target_entity,
                                                     Point2 replacement_endpoint)
    : id_(std::move(id)), kind_(kind), target_entity_(std::move(target_entity)),
      replacement_endpoint_(replacement_endpoint) {}

Result<ArcClosedProfile> ArcClosedProfile::create(ProfileId id,
                                                  std::vector<SketchEntityId> curve_segments) {
  const auto object_id = id.empty() ? std::string("arc_closed_profile") : id.value();
  if (id.empty()) {
    return Result<ArcClosedProfile>::failure(
        validation_error(object_id, "arc closed profile id must not be empty"));
  }
  if (curve_segments.size() < 3U) {
    return Result<ArcClosedProfile>::failure(
        validation_error(object_id, "arc closed profile requires at least three curve segments"));
  }
  for (const auto& segment : curve_segments) {
    if (segment.empty()) {
      return Result<ArcClosedProfile>::failure(
          validation_error(object_id, "arc closed profile curve segment ids must not be empty"));
    }
  }
  if (has_duplicate_id(curve_segments)) {
    return Result<ArcClosedProfile>::failure(
        validation_error(object_id, "arc closed profile curve segment ids must be unique"));
  }
  return Result<ArcClosedProfile>::success(ArcClosedProfile(std::move(id), std::move(curve_segments)));
}

const ProfileId& ArcClosedProfile::id() const noexcept { return id_; }
const std::vector<SketchEntityId>& ArcClosedProfile::curve_segments() const noexcept {
  return curve_segments_;
}

ArcClosedProfile::ArcClosedProfile(ProfileId id, std::vector<SketchEntityId> curve_segments)
    : id_(std::move(id)), curve_segments_(std::move(curve_segments)) {}

Result<std::size_t> Sketch::add_entity(ArcSegment arc_segment) {
  if (find_line_segment(arc_segment.id()) != nullptr || find_arc_segment(arc_segment.id()) != nullptr ||
      find_projected_point(arc_segment.id()) != nullptr || find_projected_line(arc_segment.id()) != nullptr ||
      find_reference_generated_line(arc_segment.id()) != nullptr) {
    return Result<std::size_t>::failure(
        validation_error(arc_segment.id().value(), "sketch entity id must be unique within sketch"));
  }
  arc_segments_.push_back(std::move(arc_segment));
  return Result<std::size_t>::success(arc_segments_.size() - 1U);
}

Result<std::size_t> Sketch::add_trim_extend_operation(SketchTrimExtendOperation operation) {
  if (find_trim_extend_operation(operation.id()) != nullptr) {
    return Result<std::size_t>::failure(validation_error(
        operation.id().value(), "sketch trim/extend operation id must be unique within sketch"));
  }
  if (find_line_segment(operation.target_entity()) == nullptr &&
      find_arc_segment(operation.target_entity()) == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        operation.id().value(), "sketch trim/extend target must be an explicit line or arc"));
  }
  trim_extend_operations_.push_back(std::move(operation));
  return Result<std::size_t>::success(trim_extend_operations_.size() - 1U);
}

Result<std::size_t> Sketch::add_profile(ArcClosedProfile profile) {
  if (find_rectangle_profile(profile.id()) != nullptr || find_circle_profile(profile.id()) != nullptr ||
      find_closed_profile(profile.id()) != nullptr || find_arc_closed_profile(profile.id()) != nullptr ||
      find_composite_closed_profile(profile.id()) != nullptr) {
    return Result<std::size_t>::failure(
        validation_error(profile.id().value(), "profile id must be unique within sketch"));
  }
  auto valid = validate_arc_closed_profile(profile);
  if (valid.has_error()) return Result<std::size_t>::failure(valid.error());
  arc_closed_profiles_.push_back(std::move(profile));
  return Result<std::size_t>::success(arc_closed_profiles_.size() - 1U);
}

const std::vector<ArcSegment>& Sketch::arc_segments() const noexcept { return arc_segments_; }
const std::vector<SketchTrimExtendOperation>& Sketch::trim_extend_operations() const noexcept {
  return trim_extend_operations_;
}
const std::vector<ArcClosedProfile>& Sketch::arc_closed_profiles() const noexcept {
  return arc_closed_profiles_;
}

const ArcSegment* Sketch::find_arc_segment(SketchEntityId id) const noexcept {
  for (const auto& arc : arc_segments_) {
    if (arc.id() == id) return &arc;
  }
  return nullptr;
}

const SketchTrimExtendOperation* Sketch::find_trim_extend_operation(
    SketchTrimOperationId id) const noexcept {
  for (const auto& operation : trim_extend_operations_) {
    if (operation.id() == id) return &operation;
  }
  return nullptr;
}

const ArcClosedProfile* Sketch::find_arc_closed_profile(ProfileId id) const noexcept {
  for (const auto& profile : arc_closed_profiles_) {
    if (profile.id() == id) return &profile;
  }
  return nullptr;
}

Result<std::vector<Point2>> Sketch::arc_closed_profile_vertices(
    const ArcClosedProfile& profile) const {
  return validate_arc_closed_profile(profile);
}

Result<std::vector<Point2>> Sketch::validate_arc_closed_profile(
    const ArcClosedProfile& profile) const {
  std::vector<CurveEndpointPair> curves;
  curves.reserve(profile.curve_segments().size());

  for (const auto& curve_id : profile.curve_segments()) {
    if (const LineSegment* line = find_line_segment(curve_id)) {
      curves.push_back(CurveEndpointPair{line->start(), line->end()});
      continue;
    }
    if (const ArcSegment* arc = find_arc_segment(curve_id)) {
      curves.push_back(CurveEndpointPair{arc->start(), arc->end()});
      continue;
    }
    return Result<std::vector<Point2>>::failure(validation_error(
        profile.id().value(), "arc closed profile curve segment must exist in sketch"));
  }

  for (std::size_t index = 0; index < curves.size(); ++index) {
    const auto& current = curves[index];
    const auto& next = curves[(index + 1U) % curves.size()];
    if (!same_point(current.end, next.start)) {
      return Result<std::vector<Point2>>::failure(validation_error(
          profile.id().value(), "arc closed profile curve segments must be ordered and connected"));
    }
  }

  std::vector<Point2> vertices;
  vertices.reserve(curves.size());
  for (const auto& curve : curves) {
    vertices.push_back(curve.start);
  }
  return Result<std::vector<Point2>>::success(std::move(vertices));
}

} // namespace blcad
