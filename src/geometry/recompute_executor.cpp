#include "blcad/geometry/recompute_executor.hpp"

#include "blcad/geometry/reference_generated_profile_resolver.hpp"

#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Error geometry_error(std::string object_id, std::string message) {
  return Error::geometry(std::move(object_id), std::move(message));
}

[[nodiscard]] const Parameter* find_required_parameter(const PartDocument& document,
                                                       const ParameterId& parameter_id) noexcept {
  return document.find_parameter(parameter_id);
}

[[nodiscard]] Result<Point3> evaluate_bounded_circle_center(const WorkplaneResolver& resolver,
                                                            const ResolvedWorkplane& workplane,
                                                            const CircleProfile& circle,
                                                            const Quantity& diameter) {
  if (workplane.bounds.enabled) {
    constexpr double k_tolerance = 1.0e-9;
    const double radius = diameter.millimeters() / 2.0;
    const double min_x = workplane.bounds.center.x - workplane.bounds.width_mm / 2.0;
    const double max_x = workplane.bounds.center.x + workplane.bounds.width_mm / 2.0;
    const double min_y = workplane.bounds.center.y - workplane.bounds.height_mm / 2.0;
    const double max_y = workplane.bounds.center.y + workplane.bounds.height_mm / 2.0;
    const Point2 center = circle.center();

    if (center.x - radius < min_x - k_tolerance || center.x + radius > max_x + k_tolerance ||
        center.y - radius < min_y - k_tolerance || center.y + radius > max_y + k_tolerance) {
      return Result<Point3>::failure(validation_error(
          circle.id().value(), "circle profile must lie fully inside resolved workplane bounds"));
    }
  }

  return Result<Point3>::success(resolver.evaluate_point(workplane, circle.center()));
}

[[nodiscard]] bool closed_profile_uses_reference_generated_lines(const Sketch& sketch,
                                                                 const ClosedProfile& profile) noexcept {
  for (const auto& id : profile.line_segments()) {
    if (sketch.find_reference_generated_line(id) != nullptr) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] Result<std::vector<Point2>> evaluate_closed_profile_local_vertices(
    const PartDocument& document, const Sketch& sketch, const ClosedProfile& profile) {
  if (!closed_profile_uses_reference_generated_lines(sketch, profile)) {
    return sketch.closed_profile_vertices(profile);
  }

  ReferenceGeneratedProfileResolver resolver;
  return resolver.resolve_closed_profile_vertices(document, sketch, profile,
                                                  sketch.reference_generated_lines());
}

[[nodiscard]] Result<std::vector<Point3>> evaluate_bounded_closed_profile_vertices(
    const PartDocument& document, const WorkplaneResolver& resolver, const ResolvedWorkplane& workplane,
    const Sketch& sketch, const ClosedProfile& profile) {
  auto local_vertices = evaluate_closed_profile_local_vertices(document, sketch, profile);
  if (local_vertices.has_error()) {
    return Result<std::vector<Point3>>::failure(local_vertices.error());
  }

  if (workplane.bounds.enabled) {
    constexpr double k_tolerance = 1.0e-9;
    const double min_x = workplane.bounds.center.x - workplane.bounds.width_mm / 2.0;
    const double max_x = workplane.bounds.center.x + workplane.bounds.width_mm / 2.0;
    const double min_y = workplane.bounds.center.y - workplane.bounds.height_mm / 2.0;
    const double max_y = workplane.bounds.center.y + workplane.bounds.height_mm / 2.0;

    for (const Point2 vertex : local_vertices.value()) {
      if (vertex.x < min_x - k_tolerance || vertex.x > max_x + k_tolerance ||
          vertex.y < min_y - k_tolerance || vertex.y > max_y + k_tolerance) {
        return Result<std::vector<Point3>>::failure(validation_error(
            profile.id().value(), "closed profile vertices must lie inside resolved workplane bounds"));
      }
    }
  }

  std::vector<Point3> global_vertices;
  global_vertices.reserve(local_vertices.value().size());
  for (const Point2 vertex : local_vertices.value()) {
    global_vertices.push_back(resolver.evaluate_point(workplane, vertex));
  }

  return Result<std::vector<Point3>>::success(std::move(global_vertices));
}

[[nodiscard]] bool has_exactly_one_rectangle_profile(const Sketch& sketch) noexcept {
  return sketch.rectangle_profiles().size() == 1U && sketch.circle_profiles().empty() &&
         sketch.closed_profiles().empty();
}

[[nodiscard]] bool has_exactly_one_closed_profile(const Sketch& sketch) noexcept {
  return sketch.closed_profiles().size() == 1U && sketch.rectangle_profiles().empty() &&
         sketch.circle_profiles().empty();
}

[[nodiscard]] bool has_exactly_one_circle_profile(const Sketch& sketch) noexcept {
  return sketch.circle_profiles().size() == 1U && sketch.rectangle_profiles().empty() &&
         sketch.closed_profiles().empty();
}

} // namespace

Result<std::size_t> GeometryRecomputeExecutor::execute_additive_extrude(
    const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const {
  if (feature_id.empty()) {
    return Result<std::size_t>::failure(
        validation_error("feature", "feature id must not be empty"));
  }

  const Feature* feature = document.find_feature(feature_id);
  if (feature == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "feature must exist in part document"));
  }

  if (feature->type() != FeatureType::AdditiveExtrude) {
    return Result<std::size_t>::failure(
        geometry_error(feature->id().value(), "only additive extrude features are supported"));
  }

  const Sketch* sketch = document.find_sketch(feature->input_sketch());
  if (sketch == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        feature->id().value(), "feature input sketch must exist in part document"));
  }

  const Parameter* thickness = find_required_parameter(document, feature->length_parameter());
  if (thickness == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(feature->length_parameter().value(),
                         "additive extrude length parameter must exist in part document"));
  }

  if (has_exactly_one_rectangle_profile(*sketch)) {
    const RectangleProfile& rectangle = sketch->rectangle_profiles().front();

    const Parameter* width = find_required_parameter(document, rectangle.width_parameter());
    if (width == nullptr) {
      return Result<std::size_t>::failure(
          validation_error(rectangle.width_parameter().value(),
                           "rectangle width parameter must exist in part document"));
    }

    const Parameter* height = find_required_parameter(document, rectangle.height_parameter());
    if (height == nullptr) {
      return Result<std::size_t>::failure(
          validation_error(rectangle.height_parameter().value(),
                           "rectangle height parameter must exist in part document"));
    }

    auto shape = rectangle_extrusion_adapter_.make_extruded_rectangle(width->value(), height->value(),
                                                                      thickness->value());
    if (shape.has_error()) {
      return Result<std::size_t>::failure(shape.error());
    }

    return shape_cache.set_final_shape(feature->id(), std::move(shape.value()));
  }

  if (has_exactly_one_closed_profile(*sketch)) {
    auto resolved_workplane = workplane_resolver_.resolve_for_sketch(document, *sketch);
    if (resolved_workplane.has_error()) {
      return Result<std::size_t>::failure(resolved_workplane.error());
    }

    const ClosedProfile& profile = sketch->closed_profiles().front();
    auto vertices = evaluate_bounded_closed_profile_vertices(document, workplane_resolver_,
                                                            resolved_workplane.value(), *sketch, profile);
    if (vertices.has_error()) {
      return Result<std::size_t>::failure(vertices.error());
    }

    auto shape = closed_profile_adapter_.make_extruded_profile(
        vertices.value(), resolved_workplane.value().normal, thickness->value());
    if (shape.has_error()) {
      return Result<std::size_t>::failure(shape.error());
    }

    return shape_cache.set_final_shape(feature->id(), std::move(shape.value()));
  }

  return Result<std::size_t>::failure(validation_error(
      sketch->id().value(), "additive extrude requires exactly one rectangle or closed profile"));
}

Result<std::size_t> GeometryRecomputeExecutor::execute_subtractive_extrude(
    const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const {
  if (feature_id.empty()) {
    return Result<std::size_t>::failure(
        validation_error("feature", "feature id must not be empty"));
  }

  const Feature* feature = document.find_feature(feature_id);
  if (feature == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "feature must exist in part document"));
  }

  if (feature->type() != FeatureType::SubtractiveExtrude) {
    return Result<std::size_t>::failure(
        geometry_error(feature->id().value(), "only subtractive extrude features are supported"));
  }

  const Sketch* sketch = document.find_sketch(feature->input_sketch());
  if (sketch == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        feature->id().value(), "feature input sketch must exist in part document"));
  }

  if (!has_exactly_one_circle_profile(*sketch) && !has_exactly_one_closed_profile(*sketch)) {
    return Result<std::size_t>::failure(validation_error(
        sketch->id().value(), "subtractive extrude requires exactly one circle or closed profile"));
  }

  const GeometryShape* target = shape_cache.find_feature_shape(feature->target_feature());
  if (target == nullptr) {
    return Result<std::size_t>::failure(geometry_error(
        feature->target_feature().value(), "target feature shape must exist in the shape cache"));
  }

  auto resolved_workplane = workplane_resolver_.resolve_for_sketch(document, *sketch);
  if (resolved_workplane.has_error()) {
    return Result<std::size_t>::failure(resolved_workplane.error());
  }

  if (has_exactly_one_circle_profile(*sketch)) {
    const CircleProfile& circle = sketch->circle_profiles().front();

    const Parameter* diameter = find_required_parameter(document, circle.diameter_parameter());
    if (diameter == nullptr) {
      return Result<std::size_t>::failure(
          validation_error(circle.diameter_parameter().value(),
                           "circle diameter parameter must exist in part document"));
    }

    const auto global_center = evaluate_bounded_circle_center(
        workplane_resolver_, resolved_workplane.value(), circle, diameter->value());
    if (global_center.has_error()) {
      return Result<std::size_t>::failure(global_center.error());
    }

    auto shape = circular_cut_adapter_.cut_circular_hole_along_axis(
        *target, diameter->value(), global_center.value(), resolved_workplane.value().normal);
    if (shape.has_error()) {
      return Result<std::size_t>::failure(shape.error());
    }

    return shape_cache.set_final_shape(feature->id(), std::move(shape.value()));
  }

  const ClosedProfile& profile = sketch->closed_profiles().front();
  auto vertices = evaluate_bounded_closed_profile_vertices(document, workplane_resolver_,
                                                          resolved_workplane.value(), *sketch, profile);
  if (vertices.has_error()) {
    return Result<std::size_t>::failure(vertices.error());
  }

  auto shape = closed_profile_adapter_.cut_profile_through_all(
      *target, vertices.value(), resolved_workplane.value().normal);
  if (shape.has_error()) {
    return Result<std::size_t>::failure(shape.error());
  }

  return shape_cache.set_final_shape(feature->id(), std::move(shape.value()));
}

Result<std::size_t> GeometryRecomputeExecutor::execute_feature(const PartDocument& document,
                                                               const Feature& feature,
                                                               ShapeCache& shape_cache) const {
  switch (feature.type()) {
  case FeatureType::AdditiveExtrude:
    return execute_additive_extrude(document, feature.id(), shape_cache);
  case FeatureType::SubtractiveExtrude:
    return execute_subtractive_extrude(document, feature.id(), shape_cache);
  }

  return Result<std::size_t>::failure(
      geometry_error(feature.id().value(), "feature type is not supported by geometry recompute"));
}

Result<GeometryRecomputeSummary>
GeometryRecomputeExecutor::execute_plan(const PartDocument& document, const RecomputePlan& plan,
                                        ShapeCache& shape_cache) const {
  GeometryRecomputeSummary summary;

  for (const RecomputeStep& step : plan.steps()) {
    const Feature* feature = document.find_feature(FeatureId(step.node_id));
    if (feature == nullptr) {
      continue;
    }

    auto removed_stale_shape = shape_cache.remove_feature_shape(feature->id());
    if (removed_stale_shape.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(removed_stale_shape.error());
    }

    auto executed = execute_feature(document, *feature, shape_cache);
    if (executed.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(executed.error());
    }

    ++summary.executed_feature_count;
  }

  return Result<GeometryRecomputeSummary>::success(summary);
}

Result<GeometryRecomputeSummary>
GeometryRecomputeExecutor::execute_document(const PartDocument& document,
                                            ShapeCache& shape_cache) const {
  GeometryRecomputeSummary summary;

  for (const Feature& feature : document.features()) {
    auto removed_stale_shape = shape_cache.remove_feature_shape(feature.id());
    if (removed_stale_shape.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(removed_stale_shape.error());
    }

    auto executed = execute_feature(document, feature, shape_cache);
    if (executed.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(executed.error());
    }

    ++summary.executed_feature_count;
  }

  return Result<GeometryRecomputeSummary>::success(summary);
}

} // namespace blcad::geometry
