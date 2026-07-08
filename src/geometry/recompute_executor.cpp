#include "blcad/geometry/recompute_executor.hpp"

#include <string>
#include <utility>

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

  if (sketch->rectangle_profiles().size() != 1U || !sketch->circle_profiles().empty()) {
    return Result<std::size_t>::failure(validation_error(
        sketch->id().value(), "additive extrude requires exactly one rectangle profile"));
  }

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

  const Parameter* thickness = find_required_parameter(document, feature->length_parameter());
  if (thickness == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(feature->length_parameter().value(),
                         "additive extrude length parameter must exist in part document"));
  }

  auto shape = rectangle_extrusion_adapter_.make_extruded_rectangle(width->value(), height->value(),
                                                                    thickness->value());
  if (shape.has_error()) {
    return Result<std::size_t>::failure(shape.error());
  }

  return shape_cache.set_final_shape(feature->id(), std::move(shape.value()));
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

  if (sketch->circle_profiles().size() != 1U || !sketch->rectangle_profiles().empty()) {
    return Result<std::size_t>::failure(validation_error(
        sketch->id().value(), "subtractive extrude requires exactly one circle profile"));
  }

  const CircleProfile& circle = sketch->circle_profiles().front();

  const Parameter* diameter = find_required_parameter(document, circle.diameter_parameter());
  if (diameter == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(circle.diameter_parameter().value(),
                         "circle diameter parameter must exist in part document"));
  }

  const GeometryShape* target = shape_cache.find_feature_shape(feature->target_feature());
  if (target == nullptr) {
    return Result<std::size_t>::failure(geometry_error(
        feature->target_feature().value(), "target feature shape must exist in the shape cache"));
  }

  auto resolved_workplane = workplane_resolver_.resolve(document, sketch->workplane());
  if (resolved_workplane.has_error()) {
    return Result<std::size_t>::failure(resolved_workplane.error());
  }

  const auto global_center = evaluate_bounded_circle_center(
      workplane_resolver_, resolved_workplane.value(), circle, diameter->value());
  if (global_center.has_error()) {
    return Result<std::size_t>::failure(global_center.error());
  }

  auto shape = circular_cut_adapter_.cut_circular_hole(
      *target, diameter->value(), Point2{global_center.value().x, global_center.value().y});
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
  const auto order = document.dependency_graph().topological_order();
  if (order.has_error()) {
    return Result<GeometryRecomputeSummary>::failure(order.error());
  }

  GeometryRecomputeSummary summary;

  for (const std::string& node_id : order.value()) {
    const Feature* feature = document.find_feature(FeatureId(node_id));
    if (feature == nullptr) {
      continue;
    }

    auto executed = execute_feature(document, *feature, shape_cache);
    if (executed.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(executed.error());
    }

    ++summary.executed_feature_count;
  }

  return Result<GeometryRecomputeSummary>::success(summary);
}

} // namespace blcad::geometry
