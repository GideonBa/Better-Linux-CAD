#include "blcad/geometry/workplane_resolver.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] const Parameter* find_parameter(const PartDocument& document,
                                              const ParameterId& parameter_id) noexcept {
  return document.find_parameter(parameter_id);
}

[[nodiscard]] RectangularWorkplaneBounds make_bounds(double width_mm, double height_mm) noexcept {
  RectangularWorkplaneBounds bounds;
  bounds.enabled = true;
  bounds.center = Point2{0.0, 0.0};
  bounds.width_mm = width_mm;
  bounds.height_mm = height_mm;
  return bounds;
}

[[nodiscard]] Vector3 vector_between(Point3 from, Point3 to) noexcept {
  return Vector3{to.x - from.x, to.y - from.y, to.z - from.z};
}

[[nodiscard]] double dot(Vector3 left, Vector3 right) noexcept {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] double length(Vector3 vector) noexcept {
  return std::sqrt(dot(vector, vector));
}

[[nodiscard]] Vector3 normalize(Vector3 vector) noexcept {
  const double vector_length = length(vector);
  return Vector3{vector.x / vector_length, vector.y / vector_length, vector.z / vector_length};
}

[[nodiscard]] Vector3 cross(Vector3 left, Vector3 right) noexcept {
  return Vector3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                 left.x * right.y - left.y * right.x};
}

[[nodiscard]] Point3 translated(Point3 point, Vector3 direction, double distance) noexcept {
  return Point3{point.x + direction.x * distance, point.y + direction.y * distance,
                point.z + direction.z * distance};
}

[[nodiscard]] Result<ResolvedWorkplane> resolve_datum_plane(const DatumPlane& datum_plane) {
  return Result<ResolvedWorkplane>::success(
      ResolvedWorkplane{datum_plane.id(), datum_plane.origin(), datum_plane.x_axis(),
                        datum_plane.y_axis(), datum_plane.normal(), RectangularWorkplaneBounds{}});
}

[[nodiscard]] Result<ResolvedWorkplane> resolve_explicit_construction_plane(
    const ConstructionPlane& plane) {
  return Result<ResolvedWorkplane>::success(
      ResolvedWorkplane{plane.workplane_id(), plane.origin(), plane.x_axis(), plane.y_axis(),
                        plane.normal(), RectangularWorkplaneBounds{}});
}

[[nodiscard]] Result<ResolvedWorkplane> resolve_additive_extrude_face_workplane(
    const PartDocument& document, const DerivedWorkplane& workplane) {
  const FeatureId& source_feature_id = workplane.face_reference().source_feature();
  const Feature* source_feature = document.find_feature(source_feature_id);
  if (source_feature == nullptr) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        workplane.id().value(), "derived workplane source feature must exist in part document"));
  }
  if (source_feature->type() != FeatureType::AdditiveExtrude) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        workplane.id().value(), "derived workplane source feature must be an additive extrude"));
  }
  const Sketch* source_sketch = document.find_sketch(source_feature->input_sketch());
  if (source_sketch == nullptr) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        source_feature->id().value(), "source feature input sketch must exist in part document"));
  }
  if (source_sketch->rectangle_profiles().size() != 1U ||
      !source_sketch->circle_profiles().empty() || !source_sketch->closed_profiles().empty()) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        source_sketch->id().value(),
        "derived workplane resolution requires a source sketch with exactly one rectangle profile"));
  }

  const RectangleProfile& rectangle = source_sketch->rectangle_profiles().front();
  const Parameter* width = find_parameter(document, rectangle.width_parameter());
  if (width == nullptr) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        rectangle.width_parameter().value(), "rectangle width parameter must exist in part document"));
  }
  const Parameter* height = find_parameter(document, rectangle.height_parameter());
  if (height == nullptr) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        rectangle.height_parameter().value(), "rectangle height parameter must exist in part document"));
  }
  const Parameter* thickness = find_parameter(document, source_feature->length_parameter());
  if (thickness == nullptr) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        source_feature->length_parameter().value(),
        "additive extrude length parameter must exist in part document"));
  }

  const Point2 rectangle_center = rectangle.center();
  const double width_mm = width->value().millimeters();
  const double height_mm = height->value().millimeters();
  const double thickness_mm = thickness->value().millimeters();

  switch (workplane.face_reference().face()) {
  case SemanticFace::Top:
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{
        workplane.id(), Point3{rectangle_center.x, rectangle_center.y, thickness_mm},
        Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0},
        make_bounds(width_mm, height_mm)});
  case SemanticFace::Bottom:
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{
        workplane.id(), Point3{rectangle_center.x, rectangle_center.y, 0.0},
        Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, -1.0},
        make_bounds(width_mm, height_mm)});
  case SemanticFace::Right:
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{
        workplane.id(), Point3{rectangle_center.x + width_mm / 2.0, rectangle_center.y, thickness_mm / 2.0},
        Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0}, Vector3{1.0, 0.0, 0.0},
        make_bounds(height_mm, thickness_mm)});
  case SemanticFace::Left:
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{
        workplane.id(), Point3{rectangle_center.x - width_mm / 2.0, rectangle_center.y, thickness_mm / 2.0},
        Vector3{0.0, -1.0, 0.0}, Vector3{0.0, 0.0, 1.0}, Vector3{-1.0, 0.0, 0.0},
        make_bounds(height_mm, thickness_mm)});
  case SemanticFace::Front:
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{
        workplane.id(), Point3{rectangle_center.x, rectangle_center.y + height_mm / 2.0, thickness_mm / 2.0},
        Vector3{-1.0, 0.0, 0.0}, Vector3{0.0, 0.0, 1.0}, Vector3{0.0, 1.0, 0.0},
        make_bounds(width_mm, thickness_mm)});
  case SemanticFace::Back:
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{
        workplane.id(), Point3{rectangle_center.x, rectangle_center.y - height_mm / 2.0, thickness_mm / 2.0},
        Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 0.0, 1.0}, Vector3{0.0, -1.0, 0.0},
        make_bounds(width_mm, thickness_mm)});
  }
  return Result<ResolvedWorkplane>::failure(
      validation_error(workplane.id().value(), "unsupported semantic face"));
}

[[nodiscard]] Result<ResolvedWorkplane> resolve_relation_driven_construction_plane(
    const PartDocument& document, const WorkplaneResolver& resolver, const ConstructionPlane& plane) {
  if (!plane.relation().has_value()) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        plane.id().value(), "relation-driven construction plane must carry a relation"));
  }
  const ConstructionRelation& relation = plane.relation().value();
  if (plane.kind() == ConstructionPlaneKind::OffsetFromPlane) {
    auto source = resolver.resolve(document, relation.source_plane());
    if (source.has_error()) return Result<ResolvedWorkplane>::failure(source.error());
    const Parameter* offset = find_parameter(document, relation.offset_parameter());
    if (offset == nullptr) return Result<ResolvedWorkplane>::failure(validation_error(relation.offset_parameter().value(), "plane offset parameter must exist in part document"));
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{plane.workplane_id(), translated(source.value().origin, source.value().normal, offset->value().millimeters()), source.value().x_axis, source.value().y_axis, source.value().normal, RectangularWorkplaneBounds{}});
  }
  if (plane.kind() == ConstructionPlaneKind::ThroughThreePoints) {
    const ConstructionPoint* first = document.find_construction_point(relation.first_point());
    const ConstructionPoint* second = document.find_construction_point(relation.second_point());
    const ConstructionPoint* third = document.find_construction_point(relation.third_point());
    if (first == nullptr || second == nullptr || third == nullptr) return Result<ResolvedWorkplane>::failure(validation_error(plane.id().value(), "plane through three points references must exist in part document"));
    const Vector3 first_to_second = vector_between(first->position(), second->position());
    const Vector3 first_to_third = vector_between(first->position(), third->position());
    const Vector3 normal_seed = cross(first_to_second, first_to_third);
    if (length(first_to_second) <= k_tolerance || length(normal_seed) <= k_tolerance) return Result<ResolvedWorkplane>::failure(validation_error(plane.id().value(), "plane through three points requires non-collinear points"));
    const Vector3 x_axis = normalize(first_to_second);
    const Vector3 normal = normalize(normal_seed);
    const Vector3 y_axis = normalize(cross(normal, x_axis));
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{plane.workplane_id(), first->position(), x_axis, y_axis, normal, RectangularWorkplaneBounds{}});
  }
  if (plane.kind() == ConstructionPlaneKind::ParallelToPlaneThroughPoint) {
    auto source = resolver.resolve(document, relation.source_plane());
    if (source.has_error()) return Result<ResolvedWorkplane>::failure(source.error());
    const ConstructionPoint* point = document.find_construction_point(relation.first_point());
    if (point == nullptr) return Result<ResolvedWorkplane>::failure(validation_error(plane.id().value(), "plane parallel to plane through point reference point must exist"));
    return Result<ResolvedWorkplane>::success(ResolvedWorkplane{plane.workplane_id(), point->position(), source.value().x_axis, source.value().y_axis, source.value().normal, RectangularWorkplaneBounds{}});
  }
  return Result<ResolvedWorkplane>::failure(validation_error(plane.id().value(), "unsupported construction plane relation"));
}

[[nodiscard]] Result<ResolvedWorkplane> resolve_construction_plane(const PartDocument& document,
                                                                   const WorkplaneResolver& resolver,
                                                                   const ConstructionPlane& plane) {
  if (plane.kind() == ConstructionPlaneKind::Explicit) return resolve_explicit_construction_plane(plane);
  return resolve_relation_driven_construction_plane(document, resolver, plane);
}

} // namespace

Result<ResolvedWorkplane> WorkplaneResolver::resolve(const PartDocument& document,
                                                     DatumPlaneId workplane_id) const {
  if (workplane_id.empty()) return Result<ResolvedWorkplane>::failure(validation_error("workplane", "workplane id must not be empty"));
  const DatumPlane* datum_plane = document.find_datum_plane(workplane_id);
  if (datum_plane != nullptr) return resolve_datum_plane(*datum_plane);
  const ConstructionPlane* construction_plane = document.find_construction_plane(ConstructionPlaneId(workplane_id.value()));
  if (construction_plane != nullptr) return resolve_construction_plane(document, *this, *construction_plane);
  const DerivedWorkplane* derived_workplane = document.find_derived_workplane(workplane_id);
  if (derived_workplane != nullptr) return resolve_additive_extrude_face_workplane(document, *derived_workplane);
  return Result<ResolvedWorkplane>::failure(validation_error(workplane_id.value(), "workplane must exist in part document"));
}

Result<ResolvedWorkplane> WorkplaneResolver::resolve_for_sketch(const PartDocument& document,
                                                                const Sketch& sketch) const {
  auto workplane = resolve(document, sketch.workplane());
  if (workplane.has_error()) return workplane;
  const SketchOriginOverrideRecord* origin_override = document.find_sketch_origin_override(sketch.id());
  if (origin_override == nullptr) return workplane;
  const Point2 override_origin = origin_override->local_origin();
  workplane.value().origin = evaluate_point(workplane.value(), override_origin);
  if (workplane.value().bounds.enabled) {
    workplane.value().bounds.center.x -= override_origin.x;
    workplane.value().bounds.center.y -= override_origin.y;
  }
  return workplane;
}

Point3 WorkplaneResolver::evaluate_point(const ResolvedWorkplane& workplane,
                                         Point2 local_point) const noexcept {
  return Point3{workplane.origin.x + local_point.x * workplane.x_axis.x +
                    local_point.y * workplane.y_axis.x,
                workplane.origin.y + local_point.x * workplane.x_axis.y +
                    local_point.y * workplane.y_axis.y,
                workplane.origin.z + local_point.x * workplane.x_axis.z +
                    local_point.y * workplane.y_axis.z};
}

} // namespace blcad::geometry
