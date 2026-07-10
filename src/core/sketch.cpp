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

  if (((o1 > k_tolerance && o2 < -k_tolerance) || (o1 < -k_tolerance && o2 > k_tolerance)) &&
      ((o3 > k_tolerance && o4 < -k_tolerance) || (o3 < -k_tolerance && o4 > k_tolerance))) {
    return true;
  }

  return (std::abs(o1) <= k_tolerance && on_segment(a1, a2, b1)) ||
         (std::abs(o2) <= k_tolerance && on_segment(a1, a2, b2)) ||
         (std::abs(o3) <= k_tolerance && on_segment(b1, b2, a1)) ||
         (std::abs(o4) <= k_tolerance && on_segment(b1, b2, a2));
}

[[nodiscard]] bool are_adjacent_segments(std::size_t first, std::size_t second,
                                         std::size_t count) noexcept {
  return first + 1U == second || second + 1U == first || (first == 0U && second + 1U == count) ||
         (second == 0U && first + 1U == count);
}

[[nodiscard]] Result<SketchReferenceTarget>
create_target(SketchReferenceTargetKind kind, SketchEntityId entity, std::string_view object_kind) {
  const auto object_id = entity.empty() ? std::string(object_kind) : entity.value();
  if (entity.empty()) {
    return Result<SketchReferenceTarget>::failure(
        Error::validation(object_id, std::string(object_kind) + " entity id must not be empty"));
  }

  return Result<SketchReferenceTarget>::success(SketchReferenceTarget(kind, std::move(entity)));
}

[[nodiscard]] bool is_endpoint_target(SketchReferenceTargetKind kind) noexcept {
  return kind == SketchReferenceTargetKind::LineSegmentStart ||
         kind == SketchReferenceTargetKind::LineSegmentEnd;
}

[[nodiscard]] bool is_line_target(SketchReferenceTargetKind kind) noexcept {
  return kind == SketchReferenceTargetKind::LineSegment;
}

[[nodiscard]] bool is_projected_point_target(SketchReferenceTargetKind kind) noexcept {
  return kind == SketchReferenceTargetKind::ProjectedPoint;
}

[[nodiscard]] bool is_projected_line_target(SketchReferenceTargetKind kind) noexcept {
  return kind == SketchReferenceTargetKind::ProjectedLine;
}

} // namespace

std::string_view to_string(ProjectedSketchPointSource source) noexcept {
  switch (source) {
  case ProjectedSketchPointSource::ConstructionPoint:
    return "construction_point";
  case ProjectedSketchPointSource::SemanticVertex:
    return "semantic_vertex";
  }

  return "unknown";
}

std::string_view to_string(ProjectedSketchLineSource source) noexcept {
  switch (source) {
  case ProjectedSketchLineSource::ConstructionLine:
    return "construction_line";
  case ProjectedSketchLineSource::SemanticEdge:
    return "semantic_edge";
  }

  return "unknown";
}

std::string_view to_string(SketchReferenceTargetKind kind) noexcept {
  switch (kind) {
  case SketchReferenceTargetKind::LineSegment:
    return "line_segment";
  case SketchReferenceTargetKind::LineSegmentStart:
    return "line_segment_start";
  case SketchReferenceTargetKind::LineSegmentEnd:
    return "line_segment_end";
  case SketchReferenceTargetKind::ProjectedPoint:
    return "projected_point";
  case SketchReferenceTargetKind::ProjectedLine:
    return "projected_line";
  }

  return "unknown";
}

std::string_view to_string(SketchConstraintKind kind) noexcept {
  switch (kind) {
  case SketchConstraintKind::CoincidentToProjectedPoint:
    return "coincident_to_projected_point";
  case SketchConstraintKind::ParallelToProjectedLine:
    return "parallel_to_projected_line";
  case SketchConstraintKind::CollinearWithProjectedLine:
    return "collinear_with_projected_line";
  }

  return "unknown";
}

Result<SketchReferenceTarget> SketchReferenceTarget::create_line_segment(SketchEntityId entity) {
  return create_target(SketchReferenceTargetKind::LineSegment, std::move(entity),
                       "line segment reference target");
}

Result<SketchReferenceTarget>
SketchReferenceTarget::create_line_segment_start(SketchEntityId entity) {
  return create_target(SketchReferenceTargetKind::LineSegmentStart, std::move(entity),
                       "line segment start reference target");
}

Result<SketchReferenceTarget>
SketchReferenceTarget::create_line_segment_end(SketchEntityId entity) {
  return create_target(SketchReferenceTargetKind::LineSegmentEnd, std::move(entity),
                       "line segment end reference target");
}

Result<SketchReferenceTarget> SketchReferenceTarget::create_projected_point(SketchEntityId entity) {
  return create_target(SketchReferenceTargetKind::ProjectedPoint, std::move(entity),
                       "projected point reference target");
}

Result<SketchReferenceTarget> SketchReferenceTarget::create_projected_line(SketchEntityId entity) {
  return create_target(SketchReferenceTargetKind::ProjectedLine, std::move(entity),
                       "projected line reference target");
}

SketchReferenceTargetKind SketchReferenceTarget::kind() const noexcept {
  return kind_;
}

const SketchEntityId& SketchReferenceTarget::entity() const noexcept {
  return entity_;
}

SketchReferenceTarget::SketchReferenceTarget(SketchReferenceTargetKind kind, SketchEntityId entity)
    : kind_(kind), entity_(std::move(entity)) {}

Result<SketchConstraint>
SketchConstraint::create_coincident_to_projected_point(SketchConstraintId id,
                                                       SketchReferenceTarget constrained_point,
                                                       SketchReferenceTarget projected_point) {
  const auto object_id = id.empty() ? std::string("sketch_constraint") : id.value();
  if (id.empty()) {
    return Result<SketchConstraint>::failure(
        Error::validation(object_id, "sketch constraint id must not be empty"));
  }
  if (!is_endpoint_target(constrained_point.kind())) {
    return Result<SketchConstraint>::failure(Error::validation(
        object_id, "coincident projected-point constraint requires a line endpoint target"));
  }
  if (!is_projected_point_target(projected_point.kind())) {
    return Result<SketchConstraint>::failure(Error::validation(
        object_id, "coincident projected-point constraint requires a projected point reference"));
  }

  return Result<SketchConstraint>::success(
      SketchConstraint(std::move(id), SketchConstraintKind::CoincidentToProjectedPoint,
                       std::move(constrained_point), std::move(projected_point)));
}

Result<SketchConstraint>
SketchConstraint::create_parallel_to_projected_line(SketchConstraintId id,
                                                    SketchReferenceTarget constrained_line,
                                                    SketchReferenceTarget projected_line) {
  const auto object_id = id.empty() ? std::string("sketch_constraint") : id.value();
  if (id.empty()) {
    return Result<SketchConstraint>::failure(
        Error::validation(object_id, "sketch constraint id must not be empty"));
  }
  if (!is_line_target(constrained_line.kind())) {
    return Result<SketchConstraint>::failure(Error::validation(
        object_id, "parallel projected-line constraint requires a line segment target"));
  }
  if (!is_projected_line_target(projected_line.kind())) {
    return Result<SketchConstraint>::failure(Error::validation(
        object_id, "parallel projected-line constraint requires a projected line reference"));
  }

  return Result<SketchConstraint>::success(
      SketchConstraint(std::move(id), SketchConstraintKind::ParallelToProjectedLine,
                       std::move(constrained_line), std::move(projected_line)));
}

Result<SketchConstraint>
SketchConstraint::create_collinear_with_projected_line(SketchConstraintId id,
                                                       SketchReferenceTarget constrained_line,
                                                       SketchReferenceTarget projected_line) {
  const auto object_id = id.empty() ? std::string("sketch_constraint") : id.value();
  if (id.empty()) {
    return Result<SketchConstraint>::failure(
        Error::validation(object_id, "sketch constraint id must not be empty"));
  }
  if (!is_line_target(constrained_line.kind())) {
    return Result<SketchConstraint>::failure(Error::validation(
        object_id, "collinear projected-line constraint requires a line segment target"));
  }
  if (!is_projected_line_target(projected_line.kind())) {
    return Result<SketchConstraint>::failure(Error::validation(
        object_id, "collinear projected-line constraint requires a projected line reference"));
  }

  return Result<SketchConstraint>::success(
      SketchConstraint(std::move(id), SketchConstraintKind::CollinearWithProjectedLine,
                       std::move(constrained_line), std::move(projected_line)));
}

const SketchConstraintId& SketchConstraint::id() const noexcept {
  return id_;
}

SketchConstraintKind SketchConstraint::kind() const noexcept {
  return kind_;
}

const SketchReferenceTarget& SketchConstraint::constrained_target() const noexcept {
  return constrained_target_;
}

const SketchReferenceTarget& SketchConstraint::reference_target() const noexcept {
  return reference_target_;
}

SketchConstraint::SketchConstraint(SketchConstraintId id, SketchConstraintKind kind,
                                   SketchReferenceTarget constrained_target,
                                   SketchReferenceTarget reference_target)
    : id_(std::move(id)), kind_(kind), constrained_target_(std::move(constrained_target)),
      reference_target_(std::move(reference_target)) {}

Result<ProjectedSketchPoint>
ProjectedSketchPoint::create_from_construction_point(SketchEntityId id, ConstructionPointId point) {
  const auto object_id = id.empty() ? std::string("projected_sketch_point") : id.value();

  if (id.empty()) {
    return Result<ProjectedSketchPoint>::failure(
        Error::validation(object_id, "projected sketch point id must not be empty"));
  }

  if (point.empty()) {
    return Result<ProjectedSketchPoint>::failure(
        Error::validation(object_id, "projected construction point reference must not be empty"));
  }

  return Result<ProjectedSketchPoint>::success(
      ProjectedSketchPoint(std::move(id), ProjectedSketchPointSource::ConstructionPoint,
                           std::move(point), std::nullopt));
}

Result<ProjectedSketchPoint>
ProjectedSketchPoint::create_from_semantic_vertex(SketchEntityId id,
                                                  SemanticVertexReference vertex) {
  const auto object_id = id.empty() ? std::string("projected_sketch_point") : id.value();

  if (id.empty()) {
    return Result<ProjectedSketchPoint>::failure(
        Error::validation(object_id, "projected sketch point id must not be empty"));
  }

  return Result<ProjectedSketchPoint>::success(ProjectedSketchPoint(
      std::move(id), ProjectedSketchPointSource::SemanticVertex, ConstructionPointId{}, vertex));
}

const SketchEntityId& ProjectedSketchPoint::id() const noexcept {
  return id_;
}

ProjectedSketchPointSource ProjectedSketchPoint::source() const noexcept {
  return source_;
}

const ConstructionPointId& ProjectedSketchPoint::construction_point() const noexcept {
  return construction_point_;
}

const std::optional<SemanticVertexReference>&
ProjectedSketchPoint::semantic_vertex() const noexcept {
  return semantic_vertex_;
}

std::string ProjectedSketchPoint::referenced_node_id() const {
  if (source_ == ProjectedSketchPointSource::ConstructionPoint) {
    return construction_point_.value();
  }

  return semantic_vertex_.value().node_id();
}

ProjectedSketchPoint::ProjectedSketchPoint(SketchEntityId id, ProjectedSketchPointSource source,
                                           ConstructionPointId construction_point,
                                           std::optional<SemanticVertexReference> semantic_vertex)
    : id_(std::move(id)), source_(source), construction_point_(std::move(construction_point)),
      semantic_vertex_(std::move(semantic_vertex)) {}

Result<ProjectedSketchLine>
ProjectedSketchLine::create_from_construction_line(SketchEntityId id, ConstructionLineId line) {
  const auto object_id = id.empty() ? std::string("projected_sketch_line") : id.value();

  if (id.empty()) {
    return Result<ProjectedSketchLine>::failure(
        Error::validation(object_id, "projected sketch line id must not be empty"));
  }

  if (line.empty()) {
    return Result<ProjectedSketchLine>::failure(
        Error::validation(object_id, "projected construction line reference must not be empty"));
  }

  return Result<ProjectedSketchLine>::success(ProjectedSketchLine(
      std::move(id), ProjectedSketchLineSource::ConstructionLine, std::move(line), std::nullopt));
}

Result<ProjectedSketchLine>
ProjectedSketchLine::create_from_semantic_edge(SketchEntityId id, SemanticEdgeReference edge) {
  const auto object_id = id.empty() ? std::string("projected_sketch_line") : id.value();

  if (id.empty()) {
    return Result<ProjectedSketchLine>::failure(
        Error::validation(object_id, "projected sketch line id must not be empty"));
  }

  return Result<ProjectedSketchLine>::success(ProjectedSketchLine(
      std::move(id), ProjectedSketchLineSource::SemanticEdge, ConstructionLineId{}, edge));
}

const SketchEntityId& ProjectedSketchLine::id() const noexcept {
  return id_;
}

ProjectedSketchLineSource ProjectedSketchLine::source() const noexcept {
  return source_;
}

const ConstructionLineId& ProjectedSketchLine::construction_line() const noexcept {
  return construction_line_;
}

const std::optional<SemanticEdgeReference>& ProjectedSketchLine::semantic_edge() const noexcept {
  return semantic_edge_;
}

std::string ProjectedSketchLine::referenced_node_id() const {
  if (source_ == ProjectedSketchLineSource::ConstructionLine) {
    return construction_line_.value();
  }

  return semantic_edge_.value().node_id();
}

ProjectedSketchLine::ProjectedSketchLine(SketchEntityId id, ProjectedSketchLineSource source,
                                         ConstructionLineId construction_line,
                                         std::optional<SemanticEdgeReference> semantic_edge)
    : id_(std::move(id)), source_(source), construction_line_(std::move(construction_line)),
      semantic_edge_(std::move(semantic_edge)) {}

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
        return Result<ClosedProfile>::failure(Error::validation(
            object_id, "closed profile line segment ids must be unique within profile"));
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
    return Result<std::size_t>::failure(Error::validation(
        line_segment.id().value(), "sketch entity id must be unique within sketch"));
  }

  line_segments_.push_back(std::move(line_segment));
  return Result<std::size_t>::success(line_segments_.size() - 1U);
}

Result<std::size_t> Sketch::add_reference(ProjectedSketchPoint point_reference) {
  if (has_entity_id(point_reference.id())) {
    return Result<std::size_t>::failure(Error::validation(
        point_reference.id().value(), "sketch reference id must be unique within sketch"));
  }

  projected_points_.push_back(std::move(point_reference));
  return Result<std::size_t>::success(projected_points_.size() - 1U);
}

Result<std::size_t> Sketch::add_reference(ProjectedSketchLine line_reference) {
  if (has_entity_id(line_reference.id())) {
    return Result<std::size_t>::failure(Error::validation(
        line_reference.id().value(), "sketch reference id must be unique within sketch"));
  }

  projected_lines_.push_back(std::move(line_reference));
  return Result<std::size_t>::success(projected_lines_.size() - 1U);
}

Result<std::size_t> Sketch::add_constraint(SketchConstraint constraint) {
  if (has_constraint_id(constraint.id())) {
    return Result<std::size_t>::failure(Error::validation(
        constraint.id().value(), "sketch constraint id must be unique within sketch"));
  }

  auto constrained_target =
      validate_constraint_target(constraint.constrained_target(), constraint.id().value());
  if (constrained_target.has_error()) {
    return Result<std::size_t>::failure(constrained_target.error());
  }

  auto reference_target =
      validate_constraint_target(constraint.reference_target(), constraint.id().value());
  if (reference_target.has_error()) {
    return Result<std::size_t>::failure(reference_target.error());
  }

  constraints_.push_back(std::move(constraint));
  return Result<std::size_t>::success(constraints_.size() - 1U);
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

Result<CircularHolePattern> CircularHolePattern::create(ProfileId id, ParameterId radius_parameter,
                                                        ParameterId count_parameter,
                                                        ParameterId hole_diameter_parameter,
                                                        Point2 center, double angle_offset_deg) {
  const auto object_id = id.empty() ? std::string("circular_hole_pattern") : id.value();
  if (id.empty()) {
    return Result<CircularHolePattern>::failure(
        Error::validation(object_id, "circular hole pattern id must not be empty"));
  }
  if (radius_parameter.empty()) {
    return Result<CircularHolePattern>::failure(
        Error::validation(object_id, "circular hole pattern radius parameter must not be empty"));
  }
  if (count_parameter.empty()) {
    return Result<CircularHolePattern>::failure(
        Error::validation(object_id, "circular hole pattern count parameter must not be empty"));
  }
  if (hole_diameter_parameter.empty()) {
    return Result<CircularHolePattern>::failure(Error::validation(
        object_id, "circular hole pattern hole diameter parameter must not be empty"));
  }
  if (!std::isfinite(angle_offset_deg)) {
    return Result<CircularHolePattern>::failure(
        Error::validation(object_id, "circular hole pattern angle offset must be finite"));
  }
  return Result<CircularHolePattern>::success(
      CircularHolePattern(std::move(id), std::move(radius_parameter), std::move(count_parameter),
                          std::move(hole_diameter_parameter), center, angle_offset_deg));
}

CircularHolePattern::CircularHolePattern(ProfileId id, ParameterId radius_parameter,
                                         ParameterId count_parameter,
                                         ParameterId hole_diameter_parameter, Point2 center,
                                         double angle_offset_deg)
    : id_(std::move(id)), radius_parameter_(std::move(radius_parameter)),
      count_parameter_(std::move(count_parameter)),
      hole_diameter_parameter_(std::move(hole_diameter_parameter)), center_(center),
      angle_offset_deg_(angle_offset_deg) {}

const ProfileId& CircularHolePattern::id() const noexcept {
  return id_;
}
Point2 CircularHolePattern::center() const noexcept {
  return center_;
}
double CircularHolePattern::angle_offset_deg() const noexcept {
  return angle_offset_deg_;
}
const ParameterId& CircularHolePattern::radius_parameter() const noexcept {
  return radius_parameter_;
}
const ParameterId& CircularHolePattern::count_parameter() const noexcept {
  return count_parameter_;
}
const ParameterId& CircularHolePattern::hole_diameter_parameter() const noexcept {
  return hole_diameter_parameter_;
}

Result<std::size_t> Sketch::add_profile(CircularHolePattern pattern) {
  if (has_profile_id(pattern.id())) {
    return Result<std::size_t>::failure(
        Error::validation(pattern.id().value(), "profile id must be unique within sketch"));
  }

  circular_hole_patterns_.push_back(std::move(pattern));
  return Result<std::size_t>::success(circular_hole_patterns_.size() - 1U);
}

const std::vector<CircularHolePattern>& Sketch::circular_hole_patterns() const noexcept {
  return circular_hole_patterns_;
}

const CircularHolePattern* Sketch::find_circular_hole_pattern(ProfileId id) const noexcept {
  for (const auto& pattern : circular_hole_patterns_) {
    if (pattern.id() == id)
      return &pattern;
  }
  return nullptr;
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
const std::vector<ProjectedSketchPoint>& Sketch::projected_points() const noexcept {
  return projected_points_;
}
const std::vector<ProjectedSketchLine>& Sketch::projected_lines() const noexcept {
  return projected_lines_;
}
const std::vector<SketchConstraint>& Sketch::constraints() const noexcept {
  return constraints_;
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
  return rectangle_profiles_.size() + circle_profiles_.size() + closed_profiles_.size() +
         arc_closed_profiles_.size() + composite_closed_profiles_.size() +
         circular_hole_patterns_.size();
}

const LineSegment* Sketch::find_line_segment(SketchEntityId id) const noexcept {
  for (const auto& line_segment : line_segments_) {
    if (line_segment.id() == id)
      return &line_segment;
  }
  return nullptr;
}

const ProjectedSketchPoint* Sketch::find_projected_point(SketchEntityId id) const noexcept {
  for (const auto& point : projected_points_) {
    if (point.id() == id)
      return &point;
  }
  return nullptr;
}

const ProjectedSketchLine* Sketch::find_projected_line(SketchEntityId id) const noexcept {
  for (const auto& line : projected_lines_) {
    if (line.id() == id)
      return &line;
  }
  return nullptr;
}

const SketchConstraint* Sketch::find_constraint(SketchConstraintId id) const noexcept {
  for (const auto& constraint : constraints_) {
    if (constraint.id() == id)
      return &constraint;
  }
  return nullptr;
}

const RectangleProfile* Sketch::find_rectangle_profile(ProfileId id) const noexcept {
  for (const auto& profile : rectangle_profiles_) {
    if (profile.id() == id)
      return &profile;
  }
  return nullptr;
}

const CircleProfile* Sketch::find_circle_profile(ProfileId id) const noexcept {
  for (const auto& profile : circle_profiles_) {
    if (profile.id() == id)
      return &profile;
  }
  return nullptr;
}

const ClosedProfile* Sketch::find_closed_profile(ProfileId id) const noexcept {
  for (const auto& profile : closed_profiles_) {
    if (profile.id() == id)
      return &profile;
  }
  return nullptr;
}

Result<std::vector<Point2>> Sketch::closed_profile_vertices(const ClosedProfile& profile) const {
  return validate_closed_profile(profile);
}

Sketch::Sketch(SketchId id, std::string name, DatumPlaneId workplane)
    : id_(std::move(id)), name_(std::move(name)), workplane_(std::move(workplane)) {}

bool Sketch::has_entity_id(const SketchEntityId& id) const noexcept {
  return find_line_segment(id) != nullptr || find_projected_point(id) != nullptr ||
         find_projected_line(id) != nullptr || find_reference_generated_line(id) != nullptr;
}

bool Sketch::has_constraint_id(const SketchConstraintId& id) const noexcept {
  return find_constraint(id) != nullptr;
}

bool Sketch::has_profile_id(const ProfileId& id) const noexcept {
  return find_rectangle_profile(id) != nullptr || find_circle_profile(id) != nullptr ||
         find_closed_profile(id) != nullptr || find_arc_closed_profile(id) != nullptr ||
         find_composite_closed_profile(id) != nullptr || find_circular_hole_pattern(id) != nullptr;
}

Result<std::size_t> Sketch::validate_constraint_target(const SketchReferenceTarget& target,
                                                       const std::string& object_id) const {
  switch (target.kind()) {
  case SketchReferenceTargetKind::LineSegment:
  case SketchReferenceTargetKind::LineSegmentStart:
  case SketchReferenceTargetKind::LineSegmentEnd:
    if (find_line_segment(target.entity()) == nullptr &&
        find_reference_generated_line(target.entity()) == nullptr) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "sketch constraint line segment target must exist"));
    }
    break;
  case SketchReferenceTargetKind::ProjectedPoint:
    if (find_projected_point(target.entity()) == nullptr) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "sketch constraint projected point target must exist"));
    }
    break;
  case SketchReferenceTargetKind::ProjectedLine:
    if (find_projected_line(target.entity()) == nullptr) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "sketch constraint projected line target must exist"));
    }
    break;
  }

  return Result<std::size_t>::success(0);
}

Result<std::vector<Point2>> Sketch::validate_closed_profile(const ClosedProfile& profile) const {
  std::vector<const LineSegment*> segments;
  segments.reserve(profile.line_segments().size());
  bool contains_reference_generated_line = false;

  for (const auto& line_segment_id : profile.line_segments()) {
    const LineSegment* line_segment = find_line_segment(line_segment_id);
    if (line_segment != nullptr) {
      if (contains_reference_generated_line) {
        return Result<std::vector<Point2>>::failure(Error::validation(
            profile.id().value(),
            "closed profile must not mix explicit and reference-generated lines"));
      }
      segments.push_back(line_segment);
      continue;
    }

    if (find_reference_generated_line(line_segment_id) != nullptr) {
      if (!segments.empty()) {
        return Result<std::vector<Point2>>::failure(Error::validation(
            profile.id().value(),
            "closed profile must not mix explicit and reference-generated lines"));
      }
      contains_reference_generated_line = true;
      continue;
    }

    return Result<std::vector<Point2>>::failure(Error::validation(
        profile.id().value(), "closed profile line segment must exist in sketch"));
  }

  if (contains_reference_generated_line) {
    return Result<std::vector<Point2>>::success({});
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
      if (are_adjacent_segments(i, j, segments.size()))
        continue;
      if (segments_intersect(segments[i]->start(), segments[i]->end(), segments[j]->start(),
                             segments[j]->end())) {
        return Result<std::vector<Point2>>::failure(Error::validation(
            profile.id().value(), "closed profile line segments must not self-intersect"));
      }
    }
  }

  return Result<std::vector<Point2>>::success(std::move(vertices));
}

} // namespace blcad
