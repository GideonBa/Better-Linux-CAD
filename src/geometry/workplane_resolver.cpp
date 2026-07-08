#include "blcad/geometry/workplane_resolver.hpp"

#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] const Parameter* find_parameter(const PartDocument& document,
                                              const ParameterId& parameter_id) noexcept {
  return document.find_parameter(parameter_id);
}

[[nodiscard]] Result<ResolvedWorkplane> resolve_datum_plane(const DatumPlane& datum_plane) {
  return Result<ResolvedWorkplane>::success(
      ResolvedWorkplane{datum_plane.id(), datum_plane.origin(), datum_plane.x_axis(),
                        datum_plane.y_axis(), datum_plane.normal(), RectangularWorkplaneBounds{}});
}

[[nodiscard]] Result<ResolvedWorkplane> resolve_top_face_workplane(
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

  if (workplane.face_reference().face() != SemanticFace::Top) {
    return Result<ResolvedWorkplane>::failure(
        validation_error(workplane.id().value(), "only top semantic face is supported"));
  }

  const Sketch* source_sketch = document.find_sketch(source_feature->input_sketch());
  if (source_sketch == nullptr) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        source_feature->id().value(), "source feature input sketch must exist in part document"));
  }

  if (source_sketch->rectangle_profiles().size() != 1U ||
      !source_sketch->circle_profiles().empty()) {
    return Result<ResolvedWorkplane>::failure(validation_error(
        source_sketch->id().value(),
        "top-face workplane resolution requires a source sketch with exactly one rectangle profile"));
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

  RectangularWorkplaneBounds bounds;
  bounds.enabled = true;
  bounds.center = Point2{0.0, 0.0};
  bounds.width_mm = width->value().millimeters();
  bounds.height_mm = height->value().millimeters();

  const Point2 rectangle_center = rectangle.center();
  return Result<ResolvedWorkplane>::success(
      ResolvedWorkplane{workplane.id(),
                        Point3{rectangle_center.x, rectangle_center.y,
                               thickness->value().millimeters()},
                        Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
                        Vector3{0.0, 0.0, 1.0}, bounds});
}

} // namespace

Result<ResolvedWorkplane> WorkplaneResolver::resolve(const PartDocument& document,
                                                     DatumPlaneId workplane_id) const {
  if (workplane_id.empty()) {
    return Result<ResolvedWorkplane>::failure(
        validation_error("workplane", "workplane id must not be empty"));
  }

  const DatumPlane* datum_plane = document.find_datum_plane(workplane_id);
  if (datum_plane != nullptr) {
    return resolve_datum_plane(*datum_plane);
  }

  const DerivedWorkplane* derived_workplane = document.find_derived_workplane(workplane_id);
  if (derived_workplane != nullptr) {
    return resolve_top_face_workplane(document, *derived_workplane);
  }

  return Result<ResolvedWorkplane>::failure(
      validation_error(workplane_id.value(), "workplane must exist in part document"));
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
