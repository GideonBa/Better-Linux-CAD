#include "blcad/core/body_transform.hpp"

#include <cmath>
#include <utility>

namespace blcad {
namespace {

constexpr double k_tolerance = 1.0e-12;

[[nodiscard]] bool finite(Point3 point) noexcept {
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

[[nodiscard]] bool finite(Vector3 vector) noexcept {
  return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

[[nodiscard]] double squared_length(Vector3 vector) noexcept {
  return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
}

[[nodiscard]] Result<std::size_t>
validate_common(const BodyTransformId& id, const BodyId& body,
                BodyTransformCoordinateSpace coordinate_space,
                const std::optional<std::string>& coordinate_reference) {
  const std::string object_id = id.empty() ? "body_transform" : id.value();
  if (id.empty())
    return Result<std::size_t>::failure(
        Error::validation(object_id, "body transform id must not be empty"));
  if (body.empty())
    return Result<std::size_t>::failure(
        Error::validation(object_id, "body transform body id must not be empty"));
  switch (coordinate_space) {
  case BodyTransformCoordinateSpace::World:
  case BodyTransformCoordinateSpace::BodyLocal:
    if (coordinate_reference.has_value())
      return Result<std::size_t>::failure(Error::validation(
          object_id, "world and body_local coordinate spaces forbid a reference"));
    break;
  case BodyTransformCoordinateSpace::SketchLocal:
  case BodyTransformCoordinateSpace::ConstructionReference:
    if (!coordinate_reference.has_value() || coordinate_reference->empty())
      return Result<std::size_t>::failure(Error::validation(
          object_id, "referenced coordinate spaces require a non-empty reference"));
    break;
  default:
    return Result<std::size_t>::failure(
        Error::validation(object_id, "unsupported body transform coordinate space"));
  }
  return Result<std::size_t>::success(0U);
}

} // namespace

std::string_view to_string(BodyTransformKind kind) noexcept {
  switch (kind) {
  case BodyTransformKind::Translate:
    return "translate";
  case BodyTransformKind::Rotate:
    return "rotate";
  case BodyTransformKind::UniformScale:
    return "uniform_scale";
  }
  return "unknown";
}

std::string_view to_string(BodyTransformCoordinateSpace space) noexcept {
  switch (space) {
  case BodyTransformCoordinateSpace::World:
    return "world";
  case BodyTransformCoordinateSpace::BodyLocal:
    return "body_local";
  case BodyTransformCoordinateSpace::SketchLocal:
    return "sketch_local";
  case BodyTransformCoordinateSpace::ConstructionReference:
    return "construction_reference";
  }
  return "unknown";
}

std::string_view to_string(BodyTransformRotationAxisKind kind) noexcept {
  switch (kind) {
  case BodyTransformRotationAxisKind::Explicit:
    return "explicit";
  case BodyTransformRotationAxisKind::DatumAxis:
    return "datum_axis";
  case BodyTransformRotationAxisKind::ConstructionLine:
    return "construction_line";
  case BodyTransformRotationAxisKind::SemanticEdge:
    return "semantic_edge";
  }
  return "unknown";
}

Result<BodyTransformRotationAxis> BodyTransformRotationAxis::create_explicit(Point3 origin,
                                                                             Vector3 direction) {
  if (!finite(origin) || !finite(direction) || squared_length(direction) <= k_tolerance)
    return Result<BodyTransformRotationAxis>::failure(Error::validation(
        "body_transform.rotation_axis", "explicit rotation axis must be finite and non-zero"));
  return Result<BodyTransformRotationAxis>::success(BodyTransformRotationAxis(
      BodyTransformRotationAxisKind::Explicit, origin, direction, DatumAxisId(),
      ConstructionLineId(), std::nullopt));
}

Result<BodyTransformRotationAxis>
BodyTransformRotationAxis::create_datum_axis(DatumAxisId axis) {
  if (axis.empty())
    return Result<BodyTransformRotationAxis>::failure(Error::validation(
        "body_transform.rotation_axis", "rotation datum axis id must not be empty"));
  return Result<BodyTransformRotationAxis>::success(BodyTransformRotationAxis(
      BodyTransformRotationAxisKind::DatumAxis, {}, {}, std::move(axis), ConstructionLineId(),
      std::nullopt));
}

Result<BodyTransformRotationAxis>
BodyTransformRotationAxis::create_construction_line(ConstructionLineId line) {
  if (line.empty())
    return Result<BodyTransformRotationAxis>::failure(Error::validation(
        "body_transform.rotation_axis", "rotation construction line id must not be empty"));
  return Result<BodyTransformRotationAxis>::success(BodyTransformRotationAxis(
      BodyTransformRotationAxisKind::ConstructionLine, {}, {}, DatumAxisId(), std::move(line),
      std::nullopt));
}

Result<BodyTransformRotationAxis>
BodyTransformRotationAxis::create_semantic_edge(SemanticEdgeReference edge) {
  return Result<BodyTransformRotationAxis>::success(BodyTransformRotationAxis(
      BodyTransformRotationAxisKind::SemanticEdge, {}, {}, DatumAxisId(), ConstructionLineId(),
      std::move(edge)));
}

BodyTransformRotationAxisKind BodyTransformRotationAxis::kind() const noexcept { return kind_; }
Point3 BodyTransformRotationAxis::explicit_origin() const noexcept { return explicit_origin_; }
Vector3 BodyTransformRotationAxis::explicit_direction() const noexcept {
  return explicit_direction_;
}
const DatumAxisId& BodyTransformRotationAxis::datum_axis() const noexcept { return datum_axis_; }
const ConstructionLineId& BodyTransformRotationAxis::construction_line() const noexcept {
  return construction_line_;
}
const std::optional<SemanticEdgeReference>&
BodyTransformRotationAxis::semantic_edge() const noexcept {
  return semantic_edge_;
}

BodyTransformRotationAxis::BodyTransformRotationAxis(
    BodyTransformRotationAxisKind kind, Point3 explicit_origin, Vector3 explicit_direction,
    DatumAxisId datum_axis, ConstructionLineId construction_line,
    std::optional<SemanticEdgeReference> semantic_edge)
    : kind_(kind), explicit_origin_(explicit_origin), explicit_direction_(explicit_direction),
      datum_axis_(std::move(datum_axis)), construction_line_(std::move(construction_line)),
      semantic_edge_(std::move(semantic_edge)) {}

Result<BodyTransform> BodyTransform::create_translate(
    BodyTransformId id, BodyId body, BodyTransformCoordinateSpace coordinate_space,
    std::optional<std::string> coordinate_reference, Vector3 translation_mm,
    bool apply_to_owned_sketches, bool apply_to_owned_construction_geometry) {
  auto valid = validate_common(id, body, coordinate_space, coordinate_reference);
  if (valid.has_error())
    return Result<BodyTransform>::failure(valid.error());
  if (!finite(translation_mm))
    return Result<BodyTransform>::failure(
        Error::validation(id.value(), "body translation must be finite"));
  return Result<BodyTransform>::success(BodyTransform(
      std::move(id), std::move(body), BodyTransformKind::Translate, coordinate_space,
      std::move(coordinate_reference), translation_mm, std::nullopt, 0.0, 1.0, {},
      apply_to_owned_sketches, apply_to_owned_construction_geometry));
}

Result<BodyTransform> BodyTransform::create_rotate(
    BodyTransformId id, BodyId body, BodyTransformCoordinateSpace coordinate_space,
    std::optional<std::string> coordinate_reference, BodyTransformRotationAxis rotation_axis,
    double angle_deg, bool apply_to_owned_sketches,
    bool apply_to_owned_construction_geometry) {
  auto valid = validate_common(id, body, coordinate_space, coordinate_reference);
  if (valid.has_error())
    return Result<BodyTransform>::failure(valid.error());
  if (!std::isfinite(angle_deg))
    return Result<BodyTransform>::failure(
        Error::validation(id.value(), "body rotation angle must be finite"));
  return Result<BodyTransform>::success(BodyTransform(
      std::move(id), std::move(body), BodyTransformKind::Rotate, coordinate_space,
      std::move(coordinate_reference), {}, std::move(rotation_axis), angle_deg, 1.0, {},
      apply_to_owned_sketches, apply_to_owned_construction_geometry));
}

Result<BodyTransform> BodyTransform::create_uniform_scale(
    BodyTransformId id, BodyId body, BodyTransformCoordinateSpace coordinate_space,
    std::optional<std::string> coordinate_reference, double scale_factor, Point3 center,
    bool apply_to_owned_sketches, bool apply_to_owned_construction_geometry) {
  auto valid = validate_common(id, body, coordinate_space, coordinate_reference);
  if (valid.has_error())
    return Result<BodyTransform>::failure(valid.error());
  if (!std::isfinite(scale_factor) || scale_factor <= 0.0 || !finite(center))
    return Result<BodyTransform>::failure(Error::validation(
        id.value(), "uniform scale factor must be positive and center must be finite"));
  return Result<BodyTransform>::success(BodyTransform(
      std::move(id), std::move(body), BodyTransformKind::UniformScale, coordinate_space,
      std::move(coordinate_reference), {}, std::nullopt, 0.0, scale_factor, center,
      apply_to_owned_sketches, apply_to_owned_construction_geometry));
}

const BodyTransformId& BodyTransform::id() const noexcept { return id_; }
const BodyId& BodyTransform::body() const noexcept { return body_; }
BodyTransformKind BodyTransform::kind() const noexcept { return kind_; }
BodyTransformCoordinateSpace BodyTransform::coordinate_space() const noexcept {
  return coordinate_space_;
}
const std::optional<std::string>& BodyTransform::coordinate_reference() const noexcept {
  return coordinate_reference_;
}
Vector3 BodyTransform::translation_mm() const noexcept { return translation_mm_; }
const std::optional<BodyTransformRotationAxis>& BodyTransform::rotation_axis() const noexcept {
  return rotation_axis_;
}
double BodyTransform::angle_deg() const noexcept { return angle_deg_; }
double BodyTransform::scale_factor() const noexcept { return scale_factor_; }
Point3 BodyTransform::scale_center() const noexcept { return scale_center_; }
bool BodyTransform::apply_to_owned_sketches() const noexcept { return apply_to_owned_sketches_; }
bool BodyTransform::apply_to_owned_construction_geometry() const noexcept {
  return apply_to_owned_construction_geometry_;
}

BodyTransform::BodyTransform(BodyTransformId id, BodyId body, BodyTransformKind kind,
                             BodyTransformCoordinateSpace coordinate_space,
                             std::optional<std::string> coordinate_reference,
                             Vector3 translation_mm,
                             std::optional<BodyTransformRotationAxis> rotation_axis,
                             double angle_deg, double scale_factor, Point3 scale_center,
                             bool apply_to_owned_sketches,
                             bool apply_to_owned_construction_geometry)
    : id_(std::move(id)), body_(std::move(body)), kind_(kind),
      coordinate_space_(coordinate_space),
      coordinate_reference_(std::move(coordinate_reference)), translation_mm_(translation_mm),
      rotation_axis_(std::move(rotation_axis)), angle_deg_(angle_deg), scale_factor_(scale_factor),
      scale_center_(scale_center), apply_to_owned_sketches_(apply_to_owned_sketches),
      apply_to_owned_construction_geometry_(apply_to_owned_construction_geometry) {}

} // namespace blcad
