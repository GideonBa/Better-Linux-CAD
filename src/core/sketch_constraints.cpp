#include "blcad/core/sketch.hpp"

#include <optional>
#include <string>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool is_line_segment_target(const SketchReferenceTarget& target) noexcept {
  return target.kind() == SketchReferenceTargetKind::LineSegment;
}

[[nodiscard]] bool is_point_target(const SketchReferenceTarget& target) noexcept {
  return target.kind() == SketchReferenceTargetKind::LineSegmentStart ||
         target.kind() == SketchReferenceTargetKind::LineSegmentEnd;
}

[[nodiscard]] Result<std::size_t> validate_constraint_id(const SketchConstraintId& id,
                                                         const std::string& object_id) {
  if (id.empty()) {
    return Result<std::size_t>::failure(
        validation_error(object_id, "sketch constraint id must not be empty"));
  }
  return Result<std::size_t>::success(id.value().size());
}

[[nodiscard]] Result<std::size_t> validate_dimension_id(const SketchDimensionId& id,
                                                        const std::string& object_id) {
  if (id.empty()) {
    return Result<std::size_t>::failure(
        validation_error(object_id, "sketch dimension id must not be empty"));
  }
  return Result<std::size_t>::success(id.value().size());
}

[[nodiscard]] Result<std::size_t> validate_line_target(const std::string& object_id,
                                                       const SketchReferenceTarget& target,
                                                       std::string message) {
  if (!is_line_segment_target(target)) {
    return Result<std::size_t>::failure(validation_error(object_id, std::move(message)));
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> validate_point_pair(const std::string& object_id,
                                                      const SketchReferenceTarget& first,
                                                      const SketchReferenceTarget& second) {
  if (!is_point_target(first) || !is_point_target(second)) {
    return Result<std::size_t>::failure(
        validation_error(object_id, "driving dimension requires two line endpoint targets"));
  }
  if (first.entity() == second.entity() && first.kind() == second.kind()) {
    return Result<std::size_t>::failure(
        validation_error(object_id, "driving dimension targets must be distinct"));
  }
  return Result<std::size_t>::success(0);
}

} // namespace

std::string_view to_string(SketchGeometricConstraintKind kind) noexcept {
  switch (kind) {
  case SketchGeometricConstraintKind::Fixed: return "fixed";
  case SketchGeometricConstraintKind::Horizontal: return "horizontal";
  case SketchGeometricConstraintKind::Vertical: return "vertical";
  case SketchGeometricConstraintKind::Parallel: return "parallel";
  case SketchGeometricConstraintKind::Perpendicular: return "perpendicular";
  case SketchGeometricConstraintKind::EqualLength: return "equal_length";
  }
  return "fixed";
}

std::string_view to_string(SketchDrivingDimensionKind kind) noexcept {
  switch (kind) {
  case SketchDrivingDimensionKind::HorizontalDistance: return "horizontal_distance";
  case SketchDrivingDimensionKind::VerticalDistance: return "vertical_distance";
  case SketchDrivingDimensionKind::AlignedDistance: return "aligned_distance";
  case SketchDrivingDimensionKind::PointToPointDistance: return "point_to_point_distance";
  }
  return "horizontal_distance";
}

Result<SketchGeometricConstraint> SketchGeometricConstraint::create_fixed(
    SketchConstraintId id, SketchReferenceTarget target) {
  const auto object_id = id.empty() ? std::string("sketch_geometric_constraint") : id.value();
  auto valid_id = validate_constraint_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchGeometricConstraint>::failure(valid_id.error());
  if (target.kind() != SketchReferenceTargetKind::LineSegment &&
      target.kind() != SketchReferenceTargetKind::LineSegmentStart &&
      target.kind() != SketchReferenceTargetKind::LineSegmentEnd) {
    return Result<SketchGeometricConstraint>::failure(validation_error(
        object_id, "fixed sketch constraint requires an explicit line or line endpoint target"));
  }
  return Result<SketchGeometricConstraint>::success(SketchGeometricConstraint(
      std::move(id), SketchGeometricConstraintKind::Fixed, std::move(target), std::nullopt));
}

Result<SketchGeometricConstraint> SketchGeometricConstraint::create_horizontal(
    SketchConstraintId id, SketchReferenceTarget line) {
  const auto object_id = id.empty() ? std::string("sketch_geometric_constraint") : id.value();
  auto valid_id = validate_constraint_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchGeometricConstraint>::failure(valid_id.error());
  auto valid_target = validate_line_target(
      object_id, line, "horizontal sketch constraint requires a line segment target");
  if (valid_target.has_error()) {
    return Result<SketchGeometricConstraint>::failure(valid_target.error());
  }
  return Result<SketchGeometricConstraint>::success(SketchGeometricConstraint(
      std::move(id), SketchGeometricConstraintKind::Horizontal, std::move(line), std::nullopt));
}

Result<SketchGeometricConstraint> SketchGeometricConstraint::create_vertical(
    SketchConstraintId id, SketchReferenceTarget line) {
  const auto object_id = id.empty() ? std::string("sketch_geometric_constraint") : id.value();
  auto valid_id = validate_constraint_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchGeometricConstraint>::failure(valid_id.error());
  auto valid_target = validate_line_target(
      object_id, line, "vertical sketch constraint requires a line segment target");
  if (valid_target.has_error()) {
    return Result<SketchGeometricConstraint>::failure(valid_target.error());
  }
  return Result<SketchGeometricConstraint>::success(SketchGeometricConstraint(
      std::move(id), SketchGeometricConstraintKind::Vertical, std::move(line), std::nullopt));
}

Result<SketchGeometricConstraint> SketchGeometricConstraint::create_parallel(
    SketchConstraintId id, SketchReferenceTarget first_line, SketchReferenceTarget second_line) {
  const auto object_id = id.empty() ? std::string("sketch_geometric_constraint") : id.value();
  auto valid_id = validate_constraint_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchGeometricConstraint>::failure(valid_id.error());
  auto valid_first = validate_line_target(
      object_id, first_line, "parallel sketch constraint requires line segment targets");
  if (valid_first.has_error()) {
    return Result<SketchGeometricConstraint>::failure(valid_first.error());
  }
  auto valid_second = validate_line_target(
      object_id, second_line, "parallel sketch constraint requires line segment targets");
  if (valid_second.has_error()) {
    return Result<SketchGeometricConstraint>::failure(valid_second.error());
  }
  if (first_line.entity() == second_line.entity()) {
    return Result<SketchGeometricConstraint>::failure(
        validation_error(object_id, "two-line sketch constraint requires two distinct line targets"));
  }
  return Result<SketchGeometricConstraint>::success(SketchGeometricConstraint(
      std::move(id), SketchGeometricConstraintKind::Parallel, std::move(first_line),
      std::move(second_line)));
}

Result<SketchGeometricConstraint> SketchGeometricConstraint::create_perpendicular(
    SketchConstraintId id, SketchReferenceTarget first_line, SketchReferenceTarget second_line) {
  const auto object_id = id.empty() ? std::string("sketch_geometric_constraint") : id.value();
  auto valid_id = validate_constraint_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchGeometricConstraint>::failure(valid_id.error());
  auto valid_first = validate_line_target(
      object_id, first_line, "perpendicular sketch constraint requires line segment targets");
  if (valid_first.has_error()) {
    return Result<SketchGeometricConstraint>::failure(valid_first.error());
  }
  auto valid_second = validate_line_target(
      object_id, second_line, "perpendicular sketch constraint requires line segment targets");
  if (valid_second.has_error()) {
    return Result<SketchGeometricConstraint>::failure(valid_second.error());
  }
  if (first_line.entity() == second_line.entity()) {
    return Result<SketchGeometricConstraint>::failure(
        validation_error(object_id, "two-line sketch constraint requires two distinct line targets"));
  }
  return Result<SketchGeometricConstraint>::success(SketchGeometricConstraint(
      std::move(id), SketchGeometricConstraintKind::Perpendicular, std::move(first_line),
      std::move(second_line)));
}

Result<SketchGeometricConstraint> SketchGeometricConstraint::create_equal_length(
    SketchConstraintId id, SketchReferenceTarget first_line, SketchReferenceTarget second_line) {
  const auto object_id = id.empty() ? std::string("sketch_geometric_constraint") : id.value();
  auto valid_id = validate_constraint_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchGeometricConstraint>::failure(valid_id.error());
  auto valid_first = validate_line_target(
      object_id, first_line, "equal-length sketch constraint requires line segment targets");
  if (valid_first.has_error()) {
    return Result<SketchGeometricConstraint>::failure(valid_first.error());
  }
  auto valid_second = validate_line_target(
      object_id, second_line, "equal-length sketch constraint requires line segment targets");
  if (valid_second.has_error()) {
    return Result<SketchGeometricConstraint>::failure(valid_second.error());
  }
  if (first_line.entity() == second_line.entity()) {
    return Result<SketchGeometricConstraint>::failure(
        validation_error(object_id, "two-line sketch constraint requires two distinct line targets"));
  }
  return Result<SketchGeometricConstraint>::success(SketchGeometricConstraint(
      std::move(id), SketchGeometricConstraintKind::EqualLength, std::move(first_line),
      std::move(second_line)));
}

const SketchConstraintId& SketchGeometricConstraint::id() const noexcept { return id_; }
SketchGeometricConstraintKind SketchGeometricConstraint::kind() const noexcept { return kind_; }
const SketchReferenceTarget& SketchGeometricConstraint::first_target() const noexcept {
  return first_target_;
}
const std::optional<SketchReferenceTarget>& SketchGeometricConstraint::second_target() const noexcept {
  return second_target_;
}

SketchGeometricConstraint::SketchGeometricConstraint(
    SketchConstraintId id, SketchGeometricConstraintKind kind, SketchReferenceTarget first_target,
    std::optional<SketchReferenceTarget> second_target)
    : id_(std::move(id)), kind_(kind), first_target_(std::move(first_target)),
      second_target_(std::move(second_target)) {}

Result<SketchDrivingDimension> SketchDrivingDimension::create_horizontal_distance(
    SketchDimensionId id, SketchReferenceTarget first_point, SketchReferenceTarget second_point,
    ParameterId parameter) {
  const auto object_id = id.empty() ? std::string("sketch_driving_dimension") : id.value();
  auto valid_id = validate_dimension_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchDrivingDimension>::failure(valid_id.error());
  auto valid_targets = validate_point_pair(object_id, first_point, second_point);
  if (valid_targets.has_error()) return Result<SketchDrivingDimension>::failure(valid_targets.error());
  if (parameter.empty()) {
    return Result<SketchDrivingDimension>::failure(
        validation_error(object_id, "driving dimension parameter must not be empty"));
  }
  return Result<SketchDrivingDimension>::success(SketchDrivingDimension(
      std::move(id), SketchDrivingDimensionKind::HorizontalDistance, std::move(first_point),
      std::move(second_point), std::move(parameter)));
}

Result<SketchDrivingDimension> SketchDrivingDimension::create_vertical_distance(
    SketchDimensionId id, SketchReferenceTarget first_point, SketchReferenceTarget second_point,
    ParameterId parameter) {
  const auto object_id = id.empty() ? std::string("sketch_driving_dimension") : id.value();
  auto valid_id = validate_dimension_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchDrivingDimension>::failure(valid_id.error());
  auto valid_targets = validate_point_pair(object_id, first_point, second_point);
  if (valid_targets.has_error()) return Result<SketchDrivingDimension>::failure(valid_targets.error());
  if (parameter.empty()) {
    return Result<SketchDrivingDimension>::failure(
        validation_error(object_id, "driving dimension parameter must not be empty"));
  }
  return Result<SketchDrivingDimension>::success(SketchDrivingDimension(
      std::move(id), SketchDrivingDimensionKind::VerticalDistance, std::move(first_point),
      std::move(second_point), std::move(parameter)));
}

Result<SketchDrivingDimension> SketchDrivingDimension::create_aligned_distance(
    SketchDimensionId id, SketchReferenceTarget first_point, SketchReferenceTarget second_point,
    ParameterId parameter) {
  const auto object_id = id.empty() ? std::string("sketch_driving_dimension") : id.value();
  auto valid_id = validate_dimension_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchDrivingDimension>::failure(valid_id.error());
  auto valid_targets = validate_point_pair(object_id, first_point, second_point);
  if (valid_targets.has_error()) return Result<SketchDrivingDimension>::failure(valid_targets.error());
  if (parameter.empty()) {
    return Result<SketchDrivingDimension>::failure(
        validation_error(object_id, "driving dimension parameter must not be empty"));
  }
  return Result<SketchDrivingDimension>::success(SketchDrivingDimension(
      std::move(id), SketchDrivingDimensionKind::AlignedDistance, std::move(first_point),
      std::move(second_point), std::move(parameter)));
}

Result<SketchDrivingDimension> SketchDrivingDimension::create_point_to_point_distance(
    SketchDimensionId id, SketchReferenceTarget first_point, SketchReferenceTarget second_point,
    ParameterId parameter) {
  const auto object_id = id.empty() ? std::string("sketch_driving_dimension") : id.value();
  auto valid_id = validate_dimension_id(id, object_id);
  if (valid_id.has_error()) return Result<SketchDrivingDimension>::failure(valid_id.error());
  auto valid_targets = validate_point_pair(object_id, first_point, second_point);
  if (valid_targets.has_error()) return Result<SketchDrivingDimension>::failure(valid_targets.error());
  if (parameter.empty()) {
    return Result<SketchDrivingDimension>::failure(
        validation_error(object_id, "driving dimension parameter must not be empty"));
  }
  return Result<SketchDrivingDimension>::success(SketchDrivingDimension(
      std::move(id), SketchDrivingDimensionKind::PointToPointDistance, std::move(first_point),
      std::move(second_point), std::move(parameter)));
}

const SketchDimensionId& SketchDrivingDimension::id() const noexcept { return id_; }
SketchDrivingDimensionKind SketchDrivingDimension::kind() const noexcept { return kind_; }
const SketchReferenceTarget& SketchDrivingDimension::first_target() const noexcept {
  return first_target_;
}
const SketchReferenceTarget& SketchDrivingDimension::second_target() const noexcept {
  return second_target_;
}
const ParameterId& SketchDrivingDimension::parameter() const noexcept { return parameter_; }

SketchDrivingDimension::SketchDrivingDimension(SketchDimensionId id,
                                               SketchDrivingDimensionKind kind,
                                               SketchReferenceTarget first_target,
                                               SketchReferenceTarget second_target,
                                               ParameterId parameter)
    : id_(std::move(id)), kind_(kind), first_target_(std::move(first_target)),
      second_target_(std::move(second_target)), parameter_(std::move(parameter)) {}

const std::vector<SketchGeometricConstraint>& Sketch::geometric_constraints() const noexcept {
  return geometric_constraints_;
}

const std::vector<SketchDrivingDimension>& Sketch::driving_dimensions() const noexcept {
  return driving_dimensions_;
}

const SketchGeometricConstraint* Sketch::find_geometric_constraint(SketchConstraintId id) const noexcept {
  for (const auto& constraint : geometric_constraints_) {
    if (constraint.id() == id) return &constraint;
  }
  return nullptr;
}

const SketchDrivingDimension* Sketch::find_driving_dimension(SketchDimensionId id) const noexcept {
  for (const auto& dimension : driving_dimensions_) {
    if (dimension.id() == id) return &dimension;
  }
  return nullptr;
}

Result<std::size_t> Sketch::add_constraint(SketchGeometricConstraint constraint) {
  if (find_constraint(constraint.id()) != nullptr || find_geometric_constraint(constraint.id()) != nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        constraint.id().value(), "sketch constraint id must be unique within sketch"));
  }
  if (constraint.kind() == SketchGeometricConstraintKind::Fixed &&
      constraint.first_target().kind() != SketchReferenceTargetKind::LineSegment) {
    auto target = validate_point_target(constraint.first_target(), constraint.id().value());
    if (target.has_error()) return Result<std::size_t>::failure(target.error());
  } else {
    auto first = validate_explicit_line_target(constraint.first_target(), constraint.id().value());
    if (first.has_error()) return Result<std::size_t>::failure(first.error());
  }
  if (constraint.second_target().has_value()) {
    auto second = validate_explicit_line_target(constraint.second_target().value(),
                                                constraint.id().value());
    if (second.has_error()) return Result<std::size_t>::failure(second.error());
  }
  geometric_constraints_.push_back(std::move(constraint));
  return Result<std::size_t>::success(geometric_constraints_.size() - 1U);
}

Result<std::size_t> Sketch::add_dimension(SketchDrivingDimension dimension) {
  if (has_dimension_id(dimension.id())) {
    return Result<std::size_t>::failure(Error::validation(
        dimension.id().value(), "sketch dimension id must be unique within sketch"));
  }
  auto first = validate_point_target(dimension.first_target(), dimension.id().value());
  if (first.has_error()) return Result<std::size_t>::failure(first.error());
  auto second = validate_point_target(dimension.second_target(), dimension.id().value());
  if (second.has_error()) return Result<std::size_t>::failure(second.error());
  driving_dimensions_.push_back(std::move(dimension));
  return Result<std::size_t>::success(driving_dimensions_.size() - 1U);
}

bool Sketch::has_dimension_id(const SketchDimensionId& id) const noexcept {
  return find_driving_dimension(id) != nullptr;
}

Result<std::size_t> Sketch::validate_point_target(const SketchReferenceTarget& target,
                                                  const std::string& object_id) const {
  if (target.kind() != SketchReferenceTargetKind::LineSegmentStart &&
      target.kind() != SketchReferenceTargetKind::LineSegmentEnd) {
    return Result<std::size_t>::failure(
        validation_error(object_id, "sketch dimension target must be an explicit line endpoint"));
  }
  if (find_line_segment(target.entity()) == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(object_id, "sketch dimension endpoint target must exist"));
  }
  return Result<std::size_t>::success(0);
}

Result<std::size_t> Sketch::validate_explicit_line_target(const SketchReferenceTarget& target,
                                                          const std::string& object_id) const {
  if (target.kind() != SketchReferenceTargetKind::LineSegment) {
    return Result<std::size_t>::failure(
        validation_error(object_id, "sketch geometric constraint target must be an explicit line"));
  }
  if (find_line_segment(target.entity()) == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(object_id, "sketch geometric constraint line target must exist"));
  }
  return Result<std::size_t>::success(0);
}

} // namespace blcad
