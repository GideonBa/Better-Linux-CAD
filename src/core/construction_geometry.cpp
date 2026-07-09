#include "blcad/core/construction_geometry.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace blcad {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] double dot(Vector3 left, Vector3 right) noexcept {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] double length(Vector3 vector) noexcept {
  return std::sqrt(dot(vector, vector));
}

[[nodiscard]] Vector3 cross(Vector3 left, Vector3 right) noexcept {
  return Vector3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                 left.x * right.y - left.y * right.x};
}

[[nodiscard]] bool is_unit(Vector3 vector) noexcept {
  return std::abs(length(vector) - 1.0) <= k_tolerance;
}

[[nodiscard]] bool is_zero(Vector3 vector) noexcept {
  return length(vector) <= k_tolerance;
}

[[nodiscard]] bool is_orthogonal(Vector3 left, Vector3 right) noexcept {
  return std::abs(dot(left, right)) <= k_tolerance;
}

[[nodiscard]] bool same_direction(Vector3 left, Vector3 right) noexcept {
  const Vector3 cross_product = cross(left, right);
  return length(cross_product) <= k_tolerance && dot(left, right) > 0.0;
}

[[nodiscard]] Result<std::size_t> validate_parameter_dependencies(
    const std::string& object_id, const std::vector<ParameterId>& parameter_dependencies) {
  for (const auto& parameter_id : parameter_dependencies) {
    if (parameter_id.empty()) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "construction geometry parameter dependencies must not be empty"));
    }
  }

  for (std::size_t i = 0; i < parameter_dependencies.size(); ++i) {
    for (std::size_t j = i + 1U; j < parameter_dependencies.size(); ++j) {
      if (parameter_dependencies[i] == parameter_dependencies[j]) {
        return Result<std::size_t>::failure(Error::validation(
            object_id, "construction geometry parameter dependencies must be unique"));
      }
    }
  }

  return Result<std::size_t>::success(parameter_dependencies.size());
}

[[nodiscard]] Result<std::size_t> validate_relation_id(const ConstructionRelationId& id,
                                                       const std::string& object_id) {
  if (id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation(object_id, "construction relation id must not be empty"));
  }

  return Result<std::size_t>::success(0);
}

} // namespace

std::string_view to_string(ConstructionRelationType type) noexcept {
  switch (type) {
  case ConstructionRelationType::PlaneOffsetFromPlane:
    return "plane_offset_from_plane";
  case ConstructionRelationType::LineThroughTwoPoints:
    return "line_through_two_points";
  case ConstructionRelationType::PlaneThroughThreePoints:
    return "plane_through_three_points";
  }

  return "plane_offset_from_plane";
}

std::string_view to_string(ConstructionLineKind kind) noexcept {
  switch (kind) {
  case ConstructionLineKind::Explicit:
    return "explicit";
  case ConstructionLineKind::ThroughTwoPoints:
    return "through_two_points";
  }

  return "explicit";
}

std::string_view to_string(ConstructionPlaneKind kind) noexcept {
  switch (kind) {
  case ConstructionPlaneKind::Explicit:
    return "explicit";
  case ConstructionPlaneKind::OffsetFromPlane:
    return "offset_from_plane";
  case ConstructionPlaneKind::ThroughThreePoints:
    return "through_three_points";
  }

  return "explicit";
}

Result<ConstructionRelation> ConstructionRelation::create_plane_offset_from_plane(
    ConstructionRelationId id, DatumPlaneId source_plane, ParameterId offset_parameter) {
  const auto object_id = id.empty() ? std::string("construction_relation") : id.value();

  auto valid_id = validate_relation_id(id, object_id);
  if (valid_id.has_error()) {
    return Result<ConstructionRelation>::failure(valid_id.error());
  }

  if (source_plane.empty()) {
    return Result<ConstructionRelation>::failure(
        Error::validation(object_id, "plane offset source plane must not be empty"));
  }

  if (offset_parameter.empty()) {
    return Result<ConstructionRelation>::failure(
        Error::validation(object_id, "plane offset parameter must not be empty"));
  }

  return Result<ConstructionRelation>::success(ConstructionRelation(
      std::move(id), ConstructionRelationType::PlaneOffsetFromPlane, std::move(source_plane),
      std::move(offset_parameter), ConstructionPointId(), ConstructionPointId(),
      ConstructionPointId()));
}

Result<ConstructionRelation> ConstructionRelation::create_line_through_two_points(
    ConstructionRelationId id, ConstructionPointId first_point, ConstructionPointId second_point) {
  const auto object_id = id.empty() ? std::string("construction_relation") : id.value();

  auto valid_id = validate_relation_id(id, object_id);
  if (valid_id.has_error()) {
    return Result<ConstructionRelation>::failure(valid_id.error());
  }

  if (first_point.empty() || second_point.empty()) {
    return Result<ConstructionRelation>::failure(
        Error::validation(object_id, "line through two points requires non-empty point references"));
  }

  if (first_point == second_point) {
    return Result<ConstructionRelation>::failure(
        Error::validation(object_id, "line through two points requires two distinct points"));
  }

  return Result<ConstructionRelation>::success(ConstructionRelation(
      std::move(id), ConstructionRelationType::LineThroughTwoPoints, DatumPlaneId(), ParameterId(),
      std::move(first_point), std::move(second_point), ConstructionPointId()));
}

Result<ConstructionRelation> ConstructionRelation::create_plane_through_three_points(
    ConstructionRelationId id, ConstructionPointId first_point, ConstructionPointId second_point,
    ConstructionPointId third_point) {
  const auto object_id = id.empty() ? std::string("construction_relation") : id.value();

  auto valid_id = validate_relation_id(id, object_id);
  if (valid_id.has_error()) {
    return Result<ConstructionRelation>::failure(valid_id.error());
  }

  if (first_point.empty() || second_point.empty() || third_point.empty()) {
    return Result<ConstructionRelation>::failure(
        Error::validation(object_id, "plane through three points requires non-empty point references"));
  }

  if (first_point == second_point || first_point == third_point || second_point == third_point) {
    return Result<ConstructionRelation>::failure(
        Error::validation(object_id, "plane through three points requires three distinct points"));
  }

  return Result<ConstructionRelation>::success(ConstructionRelation(
      std::move(id), ConstructionRelationType::PlaneThroughThreePoints, DatumPlaneId(), ParameterId(),
      std::move(first_point), std::move(second_point), std::move(third_point)));
}

const ConstructionRelationId& ConstructionRelation::id() const noexcept {
  return id_;
}

ConstructionRelationType ConstructionRelation::type() const noexcept {
  return type_;
}

const DatumPlaneId& ConstructionRelation::source_plane() const noexcept {
  return source_plane_;
}

const ParameterId& ConstructionRelation::offset_parameter() const noexcept {
  return offset_parameter_;
}

const ConstructionPointId& ConstructionRelation::first_point() const noexcept {
  return first_point_;
}

const ConstructionPointId& ConstructionRelation::second_point() const noexcept {
  return second_point_;
}

const ConstructionPointId& ConstructionRelation::third_point() const noexcept {
  return third_point_;
}

std::vector<std::string> ConstructionRelation::referenced_node_ids() const {
  switch (type_) {
  case ConstructionRelationType::PlaneOffsetFromPlane:
    return {source_plane_.value(), offset_parameter_.value()};
  case ConstructionRelationType::LineThroughTwoPoints:
    return {first_point_.value(), second_point_.value()};
  case ConstructionRelationType::PlaneThroughThreePoints:
    return {first_point_.value(), second_point_.value(), third_point_.value()};
  }

  return {};
}

std::vector<ParameterId> ConstructionRelation::parameter_dependencies() const {
  if (type_ == ConstructionRelationType::PlaneOffsetFromPlane) {
    return {offset_parameter_};
  }

  return {};
}

ConstructionRelation::ConstructionRelation(ConstructionRelationId id, ConstructionRelationType type,
                                           DatumPlaneId source_plane, ParameterId offset_parameter,
                                           ConstructionPointId first_point,
                                           ConstructionPointId second_point,
                                           ConstructionPointId third_point)
    : id_(std::move(id)), type_(type), source_plane_(std::move(source_plane)),
      offset_parameter_(std::move(offset_parameter)), first_point_(std::move(first_point)),
      second_point_(std::move(second_point)), third_point_(std::move(third_point)) {}

Result<ConstructionPoint> ConstructionPoint::create_explicit(
    ConstructionPointId id, std::string name, Point3 position,
    std::vector<ParameterId> parameter_dependencies) {
  const auto object_id = id.empty() ? std::string("construction_point") : id.value();

  if (id.empty()) {
    return Result<ConstructionPoint>::failure(
        Error::validation(object_id, "construction point id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionPoint>::failure(
        Error::validation(object_id, "construction point name must not be empty"));
  }

  const auto dependencies = validate_parameter_dependencies(object_id, parameter_dependencies);
  if (dependencies.has_error()) {
    return Result<ConstructionPoint>::failure(dependencies.error());
  }

  return Result<ConstructionPoint>::success(ConstructionPoint(
      std::move(id), std::move(name), position, std::move(parameter_dependencies)));
}

const ConstructionPointId& ConstructionPoint::id() const noexcept {
  return id_;
}

const std::string& ConstructionPoint::name() const noexcept {
  return name_;
}

Point3 ConstructionPoint::position() const noexcept {
  return position_;
}

const std::vector<ParameterId>& ConstructionPoint::parameter_dependencies() const noexcept {
  return parameter_dependencies_;
}

ConstructionPoint::ConstructionPoint(ConstructionPointId id, std::string name, Point3 position,
                                     std::vector<ParameterId> parameter_dependencies)
    : id_(std::move(id)), name_(std::move(name)), position_(position),
      parameter_dependencies_(std::move(parameter_dependencies)) {}

Result<ConstructionLine> ConstructionLine::create_explicit(
    ConstructionLineId id, std::string name, Point3 point, Vector3 direction,
    std::vector<ParameterId> parameter_dependencies) {
  const auto object_id = id.empty() ? std::string("construction_line") : id.value();

  if (id.empty()) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line name must not be empty"));
  }

  if (is_zero(direction)) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line direction must not be zero length"));
  }

  if (!is_unit(direction)) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line direction must be unit length"));
  }

  const auto dependencies = validate_parameter_dependencies(object_id, parameter_dependencies);
  if (dependencies.has_error()) {
    return Result<ConstructionLine>::failure(dependencies.error());
  }

  return Result<ConstructionLine>::success(ConstructionLine(
      std::move(id), std::move(name), ConstructionLineKind::Explicit, point, direction,
      std::move(parameter_dependencies), std::nullopt));
}

Result<ConstructionLine> ConstructionLine::create_through_two_points(ConstructionLineId id,
                                                                     std::string name,
                                                                     ConstructionRelation relation) {
  const auto object_id = id.empty() ? std::string("construction_line") : id.value();

  if (id.empty()) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line name must not be empty"));
  }

  if (relation.type() != ConstructionRelationType::LineThroughTwoPoints) {
    return Result<ConstructionLine>::failure(Error::validation(
        object_id, "construction line through two points requires line-through-two-points relation"));
  }

  return Result<ConstructionLine>::success(ConstructionLine(
      std::move(id), std::move(name), ConstructionLineKind::ThroughTwoPoints, Point3{}, Vector3{}, {},
      std::move(relation)));
}

const ConstructionLineId& ConstructionLine::id() const noexcept {
  return id_;
}

const std::string& ConstructionLine::name() const noexcept {
  return name_;
}

ConstructionLineKind ConstructionLine::kind() const noexcept {
  return kind_;
}

Point3 ConstructionLine::point() const noexcept {
  return point_;
}

Vector3 ConstructionLine::direction() const noexcept {
  return direction_;
}

const std::vector<ParameterId>& ConstructionLine::parameter_dependencies() const noexcept {
  return parameter_dependencies_;
}

const std::optional<ConstructionRelation>& ConstructionLine::relation() const noexcept {
  return relation_;
}

ConstructionLine::ConstructionLine(ConstructionLineId id, std::string name, ConstructionLineKind kind,
                                   Point3 point, Vector3 direction,
                                   std::vector<ParameterId> parameter_dependencies,
                                   std::optional<ConstructionRelation> relation)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), point_(point),
      direction_(direction), parameter_dependencies_(std::move(parameter_dependencies)),
      relation_(std::move(relation)) {}

Result<ConstructionPlane> ConstructionPlane::create_explicit(
    ConstructionPlaneId id, std::string name, Point3 origin, Vector3 x_axis, Vector3 y_axis,
    Vector3 normal, std::vector<ParameterId> parameter_dependencies) {
  const auto object_id = id.empty() ? std::string("construction_plane") : id.value();

  if (id.empty()) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane name must not be empty"));
  }

  if (is_zero(x_axis) || is_zero(y_axis) || is_zero(normal)) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane axes and normal must not be zero length"));
  }

  if (!is_unit(x_axis) || !is_unit(y_axis) || !is_unit(normal)) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane axes and normal must be unit length"));
  }

  if (!is_orthogonal(x_axis, y_axis) || !is_orthogonal(x_axis, normal) ||
      !is_orthogonal(y_axis, normal)) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane axes and normal must be orthogonal"));
  }

  if (!same_direction(cross(x_axis, y_axis), normal)) {
    return Result<ConstructionPlane>::failure(Error::validation(
        object_id, "construction plane normal must match x_axis cross y_axis"));
  }

  const auto dependencies = validate_parameter_dependencies(object_id, parameter_dependencies);
  if (dependencies.has_error()) {
    return Result<ConstructionPlane>::failure(dependencies.error());
  }

  return Result<ConstructionPlane>::success(ConstructionPlane(
      std::move(id), std::move(name), ConstructionPlaneKind::Explicit, origin, x_axis, y_axis,
      normal, std::move(parameter_dependencies), std::nullopt));
}

Result<ConstructionPlane> ConstructionPlane::create_offset_from_plane(ConstructionPlaneId id,
                                                                      std::string name,
                                                                      ConstructionRelation relation) {
  const auto object_id = id.empty() ? std::string("construction_plane") : id.value();

  if (id.empty()) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane name must not be empty"));
  }

  if (relation.type() != ConstructionRelationType::PlaneOffsetFromPlane) {
    return Result<ConstructionPlane>::failure(Error::validation(
        object_id, "construction plane offset requires plane-offset-from-plane relation"));
  }

  auto parameter_dependencies = relation.parameter_dependencies();
  return Result<ConstructionPlane>::success(ConstructionPlane(
      std::move(id), std::move(name), ConstructionPlaneKind::OffsetFromPlane, Point3{}, Vector3{},
      Vector3{}, Vector3{}, std::move(parameter_dependencies), std::move(relation)));
}

Result<ConstructionPlane> ConstructionPlane::create_through_three_points(
    ConstructionPlaneId id, std::string name, ConstructionRelation relation) {
  const auto object_id = id.empty() ? std::string("construction_plane") : id.value();

  if (id.empty()) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane name must not be empty"));
  }

  if (relation.type() != ConstructionRelationType::PlaneThroughThreePoints) {
    return Result<ConstructionPlane>::failure(Error::validation(
        object_id, "construction plane through three points requires plane-through-three-points relation"));
  }

  return Result<ConstructionPlane>::success(ConstructionPlane(
      std::move(id), std::move(name), ConstructionPlaneKind::ThroughThreePoints, Point3{}, Vector3{},
      Vector3{}, Vector3{}, {}, std::move(relation)));
}

const ConstructionPlaneId& ConstructionPlane::id() const noexcept {
  return id_;
}

const std::string& ConstructionPlane::name() const noexcept {
  return name_;
}

ConstructionPlaneKind ConstructionPlane::kind() const noexcept {
  return kind_;
}

Point3 ConstructionPlane::origin() const noexcept {
  return origin_;
}

Vector3 ConstructionPlane::x_axis() const noexcept {
  return x_axis_;
}

Vector3 ConstructionPlane::y_axis() const noexcept {
  return y_axis_;
}

Vector3 ConstructionPlane::normal() const noexcept {
  return normal_;
}

DatumPlaneId ConstructionPlane::workplane_id() const {
  return DatumPlaneId(id_.value());
}

const std::vector<ParameterId>& ConstructionPlane::parameter_dependencies() const noexcept {
  return parameter_dependencies_;
}

const std::optional<ConstructionRelation>& ConstructionPlane::relation() const noexcept {
  return relation_;
}

ConstructionPlane::ConstructionPlane(ConstructionPlaneId id, std::string name,
                                     ConstructionPlaneKind kind, Point3 origin, Vector3 x_axis,
                                     Vector3 y_axis, Vector3 normal,
                                     std::vector<ParameterId> parameter_dependencies,
                                     std::optional<ConstructionRelation> relation)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), origin_(origin), x_axis_(x_axis),
      y_axis_(y_axis), normal_(normal), parameter_dependencies_(std::move(parameter_dependencies)),
      relation_(std::move(relation)) {}

} // namespace blcad
