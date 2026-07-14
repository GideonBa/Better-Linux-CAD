#include "blcad/geometry/recompute_executor.hpp"

#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/construction_point_resolver.hpp"
#include "blcad/geometry/dimension_driven_profile_resolver.hpp"
#include "blcad/geometry/reference_generated_profile_resolver.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/sketch_reference_projector.hpp"
#include "blcad/geometry/sketch_region_finder.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr double k_tolerance = 1.0e-9;
constexpr double k_pi = 3.14159265358979323846;

struct LocalCompositeProfileVertices {
  std::vector<Point2> outer;
  std::vector<std::vector<Point2>> inner;
};

struct GlobalCompositeProfileVertices {
  std::vector<Point3> outer;
  std::vector<std::vector<Point3>> inner;
};

struct TransformFrame {
  Point3 origin;
  Vector3 x_axis{1.0, 0.0, 0.0};
  Vector3 y_axis{0.0, 1.0, 0.0};
  Vector3 z_axis{0.0, 0.0, 1.0};
};

[[nodiscard]] Vector3 normalized(Vector3 value) noexcept {
  const double magnitude = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
  return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

[[nodiscard]] Vector3 frame_vector(const TransformFrame& frame, Vector3 value) noexcept {
  return {frame.x_axis.x * value.x + frame.y_axis.x * value.y + frame.z_axis.x * value.z,
          frame.x_axis.y * value.x + frame.y_axis.y * value.y + frame.z_axis.y * value.z,
          frame.x_axis.z * value.x + frame.y_axis.z * value.y + frame.z_axis.z * value.z};
}

[[nodiscard]] Point3 frame_point(const TransformFrame& frame, Point3 value) noexcept {
  const Vector3 offset = frame_vector(frame, {value.x, value.y, value.z});
  return {frame.origin.x + offset.x, frame.origin.y + offset.y, frame.origin.z + offset.z};
}

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Error geometry_error(std::string object_id, std::string message) {
  return Error::geometry(std::move(object_id), std::move(message));
}

struct TransformInput {
  GeometryShape shape;
  std::optional<BodyTransformId> previous_transform;
  GeometryAffineTransform cumulative_transform;
};

[[nodiscard]] Result<TransformInput> resolve_transform_input(const PartDocument& document,
                                                             const ShapeCache& shape_cache,
                                                             const BodyTransform& transform) {
  for (const auto& [producer, dependent] : document.dependency_graph().dependencies()) {
    if (dependent != transform.id().value())
      continue;
    const GeometryShape* shape = shape_cache.find_feature_shape(FeatureId(producer));
    if (shape == nullptr)
      continue;
    const BodyTransform* previous = document.find_body_transform(BodyTransformId(producer));
    if (previous == nullptr)
      return Result<TransformInput>::success(
          TransformInput{*shape, std::nullopt, GeometryAffineTransform::identity()});
    const CachedBodyTransformState* state = shape_cache.find_body_transform_state(previous->id());
    if (state == nullptr)
      return Result<TransformInput>::failure(geometry_error(
          transform.id().value(), "previous body transform state must exist in shape cache"));
    return Result<TransformInput>::success(
        TransformInput{*shape, previous->id(), state->cumulative_transform});
  }
  return Result<TransformInput>::failure(geometry_error(
      transform.id().value(), "body transform input shape must exist in dependency order"));
}

[[nodiscard]] TransformFrame frame_from_affine(const GeometryAffineTransform& transform) noexcept {
  return {transform.transform_point({}), normalized(transform.transform_vector({1.0, 0.0, 0.0})),
          normalized(transform.transform_vector({0.0, 1.0, 0.0})),
          normalized(transform.transform_vector({0.0, 0.0, 1.0}))};
}

[[nodiscard]] Result<TransformFrame>
resolve_transform_frame(const PartDocument& document, const ShapeCache& cache,
                        const BodyTransform& transform,
                        const GeometryAffineTransform& body_transform) {
  if (transform.coordinate_space() == BodyTransformCoordinateSpace::World)
    return Result<TransformFrame>::success(TransformFrame{});
  if (transform.coordinate_space() == BodyTransformCoordinateSpace::BodyLocal)
    return Result<TransformFrame>::success(frame_from_affine(body_transform));
  if (transform.coordinate_space() == BodyTransformCoordinateSpace::SketchLocal) {
    const Sketch* sketch = document.find_sketch(SketchId(*transform.coordinate_reference()));
    auto workplane = WorkplaneResolver{}.resolve_for_sketch(document, *sketch, cache);
    if (workplane.has_error())
      return Result<TransformFrame>::failure(workplane.error());
    return Result<TransformFrame>::success(
        {workplane.value().origin, normalized(workplane.value().x_axis),
         normalized(workplane.value().y_axis), normalized(workplane.value().normal)});
  }

  const std::string& reference = *transform.coordinate_reference();
  if (document.find_datum_plane(DatumPlaneId(reference)) != nullptr ||
      document.find_construction_plane(ConstructionPlaneId(reference)) != nullptr) {
    auto plane = WorkplaneResolver{}.resolve(document, DatumPlaneId(reference));
    if (plane.has_error())
      return Result<TransformFrame>::failure(plane.error());
    TransformFrame frame{plane.value().origin, normalized(plane.value().x_axis),
                         normalized(plane.value().y_axis), normalized(plane.value().normal)};
    if (const auto* cached = cache.find_reference_transform(reference))
      frame = frame_from_affine(
          GeometryAffineTransform::translation({frame.origin.x, frame.origin.y, frame.origin.z})
              .followed_by(cached->cumulative_transform));
    return Result<TransformFrame>::success(frame);
  }

  Point3 point;
  Vector3 direction;
  if (const DatumAxis* axis = document.find_datum_axis(DatumAxisId(reference))) {
    if (axis->kind() == DatumAxisKind::Explicit) {
      point = axis->origin();
      direction = axis->direction();
    } else {
      auto line = ConstructionLineResolver{}.resolve(document, axis->source_construction_line());
      if (line.has_error())
        return Result<TransformFrame>::failure(line.error());
      point = line.value().point;
      direction = line.value().direction;
    }
  } else {
    auto line = ConstructionLineResolver{}.resolve(document, ConstructionLineId(reference));
    if (line.has_error())
      return Result<TransformFrame>::failure(line.error());
    point = line.value().point;
    direction = line.value().direction;
  }
  const Vector3 z = normalized(direction);
  const Vector3 helper = std::abs(z.z) < 0.9 ? Vector3{0.0, 0.0, 1.0} : Vector3{0.0, 1.0, 0.0};
  const Vector3 x = normalized({helper.y * z.z - helper.z * z.y, helper.z * z.x - helper.x * z.z,
                                helper.x * z.y - helper.y * z.x});
  const Vector3 y = {z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z, z.x * x.y - z.y * x.x};
  return Result<TransformFrame>::success({point, x, y, z});
}

[[nodiscard]] Result<std::pair<Point3, Vector3>>
resolve_rotation_axis(const PartDocument& document, const BodyTransform& transform,
                      const TransformFrame& frame, const ShapeCache& cache) {
  const BodyTransformRotationAxis& axis = *transform.rotation_axis();
  if (axis.kind() == BodyTransformRotationAxisKind::Explicit)
    return Result<std::pair<Point3, Vector3>>::success(
        {frame_point(frame, axis.explicit_origin()),
         normalized(frame_vector(frame, axis.explicit_direction()))});
  if (axis.kind() == BodyTransformRotationAxisKind::DatumAxis) {
    const DatumAxis* datum = document.find_datum_axis(axis.datum_axis());
    if (datum->kind() == DatumAxisKind::Explicit)
      return Result<std::pair<Point3, Vector3>>::success({datum->origin(), datum->direction()});
    auto line = ConstructionLineResolver{}.resolve(document, datum->source_construction_line());
    if (line.has_error())
      return Result<std::pair<Point3, Vector3>>::failure(line.error());
    return Result<std::pair<Point3, Vector3>>::success(
        {line.value().point, line.value().direction});
  }
  if (axis.kind() == BodyTransformRotationAxisKind::ConstructionLine) {
    auto line = ConstructionLineResolver{}.resolve(document, axis.construction_line());
    if (line.has_error())
      return Result<std::pair<Point3, Vector3>>::failure(line.error());
    return Result<std::pair<Point3, Vector3>>::success(
        {line.value().point, line.value().direction});
  }
  auto edge = SemanticReferenceEvaluator{}.resolve_edge(document, *axis.semantic_edge(), cache);
  if (edge.has_error())
    return Result<std::pair<Point3, Vector3>>::failure(edge.error());
  return Result<std::pair<Point3, Vector3>>::success(
      {edge.value().start, normalized({edge.value().end.x - edge.value().start.x,
                                       edge.value().end.y - edge.value().start.y,
                                       edge.value().end.z - edge.value().start.z})});
}

[[nodiscard]] Result<GeometryShape> resolve_cached_body_input(const PartDocument& document,
                                                              const ShapeCache& shape_cache,
                                                              const BodyId& body_id,
                                                              std::string_view role) {
  if (const GeometryShape* shape = shape_cache.find_body_shape(body_id))
    return Result<GeometryShape>::success(*shape);

  const std::string body_node = "body:" + body_id.value();
  for (const auto& [producer, dependent] : document.dependency_graph().dependencies()) {
    if (dependent != body_node)
      continue;
    if (const GeometryShape* shape = shape_cache.find_feature_shape(FeatureId(producer)))
      return Result<GeometryShape>::success(*shape);
  }

  return Result<GeometryShape>::failure(geometry_error(
      body_id.value(), std::string(role) + " body shape must exist in the shape cache"));
}

[[nodiscard]] Result<GeometryShape>
resolve_boolean_target_input(const PartDocument& document, const ShapeCache& shape_cache,
                             const BodyBooleanFeature& feature) {
  if (feature.result_mode() == BodyBooleanResultMode::ModifyTarget) {
    for (const auto& [producer, dependent] : document.dependency_graph().dependencies()) {
      if (dependent != feature.id().value() || producer.starts_with("body:"))
        continue;
      if (const GeometryShape* shape = shape_cache.find_feature_shape(FeatureId(producer)))
        return Result<GeometryShape>::success(*shape);
    }
  }
  return resolve_cached_body_input(document, shape_cache, feature.target_body(), "target");
}

[[nodiscard]] Result<std::size_t> store_feature_result(const PartDocument& document,
                                                       const Feature& feature, GeometryShape shape,
                                                       ShapeCache& shape_cache) {
  if (!feature.body_result_context().has_value())
    return shape_cache.set_final_shape(feature.id(), std::move(shape));

  const FeatureBodyResultContext& context = *feature.body_result_context();
  const BodyId& result_body_id = context.effective_produced_body();
  const Body* result_body = document.find_body(result_body_id);
  if (result_body == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(result_body_id.value(), "produced body must exist in part document"));
  }

  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result = shape_cache.store_body_shape(result_body_id, feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;

  if (document.bodies().size() == 1U && result_body->kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(result_body_id);
    return shape_cache.set_final_shape(feature.id(), *cached);
  }

  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] Result<std::size_t> store_revolve_result(const PartDocument& document,
                                                       const RevolveFeature& feature,
                                                       GeometryShape shape,
                                                       ShapeCache& shape_cache) {
  const BodyId& result_body_id = feature.body_result_context().effective_produced_body();
  const Body* result_body = document.find_body(result_body_id);
  if (result_body == nullptr)
    return Result<std::size_t>::failure(
        validation_error(result_body_id.value(), "produced body must exist in part document"));
  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result = shape_cache.store_body_shape(result_body_id, feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;
  if (document.bodies().size() == 1U && result_body->kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(result_body_id);
    return shape_cache.set_final_shape(feature.id(), *cached);
  }
  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] Result<std::size_t> store_sweep_result(const PartDocument& document,
                                                     const SweepFeature& feature,
                                                     GeometryShape shape, ShapeCache& shape_cache) {
  const BodyId& result_body_id = feature.body_result_context().effective_produced_body();
  const Body* result_body = document.find_body(result_body_id);
  if (result_body == nullptr)
    return Result<std::size_t>::failure(
        validation_error(result_body_id.value(), "produced body must exist in part document"));
  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result = shape_cache.store_body_shape(result_body_id, feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;
  if (document.bodies().size() == 1U && result_body->kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(result_body_id);
    return shape_cache.set_final_shape(feature.id(), *cached);
  }
  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] Result<std::size_t> store_loft_result(const PartDocument& document,
                                                    const LoftFeature& feature, GeometryShape shape,
                                                    ShapeCache& shape_cache) {
  const BodyId& result_body_id = feature.body_result_context().effective_produced_body();
  const Body* result_body = document.find_body(result_body_id);
  if (result_body == nullptr)
    return Result<std::size_t>::failure(
        validation_error(result_body_id.value(), "produced body must exist in part document"));
  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result = shape_cache.store_body_shape(result_body_id, feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;
  if (document.bodies().size() == 1U && result_body->kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(result_body_id);
    return shape_cache.set_final_shape(feature.id(), *cached);
  }
  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] Result<std::size_t> store_surface_result(const PartDocument& document,
                                                       const SurfaceFeature& feature,
                                                       GeometryShape shape,
                                                       ShapeCache& shape_cache) {
  const FeatureId& feature_id = surface_feature_id(feature);
  const BodyId& result_body_id = surface_feature_result_body(feature);
  const Body* result_body = document.find_body(result_body_id);
  if (result_body == nullptr || result_body->kind() != BodyKind::Surface)
    return Result<std::size_t>::failure(
        validation_error(result_body_id.value(), "surface result must identify a Surface Body"));
  auto feature_result = shape_cache.store_feature_shape(feature_id, shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result = shape_cache.store_body_shape(result_body_id, feature_id, std::move(shape));
  if (body_result.has_error())
    return body_result;
  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] bool feature_produces_body(const PartDocument& document, std::string_view producer,
                                         const BodyId& body) {
  if (const Feature* feature = document.find_feature(FeatureId(std::string(producer)));
      feature != nullptr && feature->body_result_context().has_value())
    return feature->body_result_context()->effective_produced_body() == body;
  if (const RevolveFeature* feature =
          document.find_revolve_feature(FeatureId(std::string(producer))))
    return feature->body_result_context().effective_produced_body() == body;
  if (const SweepFeature* feature = document.find_sweep_feature(FeatureId(std::string(producer))))
    return feature->body_result_context().effective_produced_body() == body;
  if (const LoftFeature* feature = document.find_loft_feature(FeatureId(std::string(producer))))
    return feature->body_result_context().effective_produced_body() == body;
  if (const SurfaceFeature* feature =
          document.find_surface_feature(FeatureId(std::string(producer))))
    return surface_feature_result_body(*feature) == body;
  if (const LinearPatternFeature* feature =
          document.find_linear_pattern_feature(FeatureId(std::string(producer))))
    return feature->body_result_context().effective_produced_body() == body;
  if (const CircularPatternFeature* feature =
          document.find_circular_pattern_feature(FeatureId(std::string(producer))))
    return feature->body_result_context().effective_produced_body() == body;
  if (const MirrorFeature* feature = document.find_mirror_feature(FeatureId(std::string(producer))))
    return feature->body_result_context().effective_produced_body() == body;
  if (const FilletFeature* feature = document.find_fillet_feature(FeatureId(std::string(producer))))
    return feature->target_body() == body;
  if (const ChamferFeature* feature =
          document.find_chamfer_feature(FeatureId(std::string(producer))))
    return feature->target_body() == body;
  if (const ShellFeature* feature = document.find_shell_feature(FeatureId(std::string(producer))))
    return feature->target_body() == body;
  if (const DraftFeature* feature = document.find_draft_feature(FeatureId(std::string(producer))))
    return feature->target_body() == body;
  if (const BodyBooleanFeature* feature =
          document.find_body_boolean_feature(FeatureId(std::string(producer))))
    return feature->effective_result_body() == body;
  if (const BodyTransform* transform =
          document.find_body_transform(BodyTransformId(std::string(producer))))
    return transform->body() == body;
  return false;
}

[[nodiscard]] Result<const GeometryShape*>
resolve_in_place_body_target(const PartDocument& document, const FeatureId& feature_id,
                             const BodyId& target_body, const ShapeCache& shape_cache) {
  auto order = document.dependency_graph().topological_order();
  if (order.has_error())
    return Result<const GeometryShape*>::failure(order.error());

  const GeometryShape* target = nullptr;
  for (const std::string& producer : order.value()) {
    if (!feature_produces_body(document, producer, target_body))
      continue;
    const bool is_dependency = std::ranges::any_of(
        document.dependency_graph().dependencies(), [&](const auto& dependency) {
          return dependency.first == producer && dependency.second == feature_id.value();
        });
    if (!is_dependency)
      continue;
    if (const GeometryShape* candidate = shape_cache.find_feature_shape(FeatureId(producer));
        candidate != nullptr)
      target = candidate;
  }

  if (target == nullptr)
    target = shape_cache.find_body_shape(target_body);
  if (target == nullptr)
    return Result<const GeometryShape*>::failure(geometry_error(
        target_body.value(), "in-place feature target body shape must exist in shape cache"));
  return Result<const GeometryShape*>::success(target);
}

[[nodiscard]] Result<std::size_t> store_fillet_result(const PartDocument& document,
                                                      const FilletFeature& feature,
                                                      GeometryShape shape,
                                                      ShapeCache& shape_cache) {
  const Body* body = document.find_body(feature.target_body());
  if (body == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature.target_body().value(), "target body must exist"));
  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result =
      shape_cache.store_body_shape(feature.target_body(), feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;
  if (document.bodies().size() == 1U && body->kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(feature.target_body());
    return shape_cache.set_final_shape(feature.id(), *cached);
  }
  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] Result<std::size_t> store_chamfer_result(const PartDocument& document,
                                                       const ChamferFeature& feature,
                                                       GeometryShape shape,
                                                       ShapeCache& shape_cache) {
  const Body* body = document.find_body(feature.target_body());
  if (body == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature.target_body().value(), "target body must exist"));
  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result =
      shape_cache.store_body_shape(feature.target_body(), feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;
  if (document.bodies().size() == 1U && body->kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(feature.target_body());
    return shape_cache.set_final_shape(feature.id(), *cached);
  }
  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] Result<std::size_t> store_shell_result(const PartDocument& document,
                                                     const ShellFeature& feature,
                                                     GeometryShape shape, ShapeCache& shape_cache) {
  const Body* body = document.find_body(feature.target_body());
  if (body == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature.target_body().value(), "target body must exist"));
  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result =
      shape_cache.store_body_shape(feature.target_body(), feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;
  if (document.bodies().size() == 1U && body->kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(feature.target_body());
    return shape_cache.set_final_shape(feature.id(), *cached);
  }
  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] Result<std::size_t> store_draft_result(const PartDocument& document,
                                                     const DraftFeature& feature,
                                                     GeometryShape shape, ShapeCache& shape_cache) {
  const Body* body = document.find_body(feature.target_body());
  if (body == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature.target_body().value(), "target body must exist"));
  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result =
      shape_cache.store_body_shape(feature.target_body(), feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;
  if (document.bodies().size() == 1U && body->kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(feature.target_body());
    return shape_cache.set_final_shape(feature.id(), *cached);
  }
  shape_cache.clear_final_shape();
  return body_result;
}

template <typename PatternFeature>
[[nodiscard]] Result<const GeometryShape*> resolve_pattern_target(const PartDocument& document,
                                                                  const PatternFeature& feature,
                                                                  const ShapeCache& shape_cache) {
  const auto& context = feature.body_result_context();
  if (!context.target_body().has_value())
    return Result<const GeometryShape*>::success(nullptr);
  const BodyId& body = *context.target_body();
  for (const auto& [producer, dependent] : document.dependency_graph().dependencies()) {
    if (dependent != feature.id().value() || producer.starts_with("body:") ||
        !feature_produces_body(document, producer, body))
      continue;
    if (const GeometryShape* shape = shape_cache.find_feature_shape(FeatureId(producer)))
      return Result<const GeometryShape*>::success(shape);
  }
  const GeometryShape* shape = shape_cache.find_body_shape(body);
  if (shape == nullptr)
    return Result<const GeometryShape*>::failure(
        geometry_error(body.value(), "pattern target body shape must exist in shape cache"));
  return Result<const GeometryShape*>::success(shape);
}

template <typename PatternFeature>
[[nodiscard]] Result<std::vector<GeometryShape>>
resolve_pattern_sources(const PatternFeature& feature, const ShapeCache& shape_cache,
                        const GeometryShape* target) {
  std::vector<GeometryShape> sources;
  sources.reserve(feature.sources().size());
  const auto& context = feature.body_result_context();
  for (const auto& source : feature.sources()) {
    const GeometryShape* shape = nullptr;
    if (source.kind() == PatternSourceKind::Feature) {
      const FeatureId& id = std::get<FeatureId>(source.source());
      shape = shape_cache.find_feature_shape(id);
    } else {
      const BodyId& id = std::get<BodyId>(source.source());
      const bool is_in_place_target = context.target_body().has_value() &&
                                      *context.target_body() == id &&
                                      context.effective_produced_body() == id;
      shape = is_in_place_target ? target : shape_cache.find_body_shape(id);
    }
    if (shape == nullptr)
      return Result<std::vector<GeometryShape>>::failure(geometry_error(
          source.source_node_id(), "pattern source shape must exist in shape cache"));
    sources.push_back(*shape);
  }
  return Result<std::vector<GeometryShape>>::success(std::move(sources));
}

[[nodiscard]] Result<std::vector<GeometryShape>>
resolve_mirror_sources(const MirrorFeature& feature, const ShapeCache& shape_cache,
                       const GeometryShape* target) {
  std::vector<GeometryShape> sources;
  sources.reserve(feature.sources().size());
  const auto& context = feature.body_result_context();
  for (const auto& source : feature.sources()) {
    const GeometryShape* shape = nullptr;
    if (source.kind() == MirrorSourceKind::Feature) {
      shape = shape_cache.find_feature_shape(std::get<FeatureId>(source.source()));
    } else {
      const BodyId& id = std::get<BodyId>(source.source());
      const bool is_in_place_target = context.target_body().has_value() &&
                                      *context.target_body() == id &&
                                      context.effective_produced_body() == id;
      shape = is_in_place_target ? target : shape_cache.find_body_shape(id);
    }
    if (shape == nullptr)
      return Result<std::vector<GeometryShape>>::failure(
          geometry_error(source.source_node_id(), "mirror source shape must exist in shape cache"));
    sources.push_back(*shape);
  }
  return Result<std::vector<GeometryShape>>::success(std::move(sources));
}

[[nodiscard]] Result<std::pair<Point3, Vector3>>
resolve_mirror_plane(const PartDocument& document, const PlaneReference& reference,
                     const ShapeCache& shape_cache, const WorkplaneResolver& resolver) {
  Result<ResolvedWorkplane> plane = Result<ResolvedWorkplane>::failure(
      geometry_error(reference.source_node_id(), "unsupported mirror plane source"));
  if (const auto* datum = std::get_if<DatumPlaneId>(&reference.source())) {
    plane = resolver.resolve(document, *datum);
  } else if (const auto* construction = std::get_if<ConstructionPlaneId>(&reference.source())) {
    plane = resolver.resolve(document, DatumPlaneId(construction->value()));
  } else {
    plane = resolver.resolve_generated_face(document,
                                            std::get<SemanticFaceReference>(reference.source()));
  }
  if (plane.has_error())
    return Result<std::pair<Point3, Vector3>>::failure(plane.error());

  Point3 origin = plane.value().origin;
  Vector3 normal = plane.value().normal;
  if (const auto* transform = shape_cache.find_reference_transform(reference.source_node_id())) {
    origin = transform->cumulative_transform.transform_point(origin);
    normal = transform->cumulative_transform.transform_vector(normal);
  }
  return Result<std::pair<Point3, Vector3>>::success({origin, normalized(normal)});
}

template <typename PatternFeature>
[[nodiscard]] Result<GeometryShape> fuse_pattern_shapes(const PatternFeature& feature,
                                                        const std::vector<GeometryShape>& shapes,
                                                        const BodyBooleanAdapter& boolean_adapter) {
  if (shapes.empty())
    return Result<GeometryShape>::failure(
        geometry_error(feature.id().value(), "pattern produced no tool shapes"));
  if (shapes.size() == 1U)
    return Result<GeometryShape>::success(shapes.front());
  std::vector<GeometryShape> tools(shapes.begin() + 1, shapes.end());
  return boolean_adapter.combine(feature.id(), BodyBooleanOperation::Add, shapes.front(), tools);
}

template <typename PatternFeature>
[[nodiscard]] Result<std::size_t>
store_pattern_result(const PartDocument& document, const PatternFeature& feature,
                     GeometryShape shape, ShapeCache& shape_cache) {
  const BodyId& result_body = feature.body_result_context().effective_produced_body();
  if (document.find_body(result_body) == nullptr)
    return Result<std::size_t>::failure(
        validation_error(result_body.value(), "pattern produced body must exist"));
  auto feature_result = shape_cache.store_feature_shape(feature.id(), shape);
  if (feature_result.has_error())
    return feature_result;
  auto body_result = shape_cache.store_body_shape(result_body, feature.id(), std::move(shape));
  if (body_result.has_error())
    return body_result;
  if (document.bodies().size() == 1U && document.bodies().front().kind() == BodyKind::Solid) {
    const GeometryShape* cached = shape_cache.find_body_shape(result_body);
    return shape_cache.set_final_shape(feature.id(), *cached);
  }
  shape_cache.clear_final_shape();
  return body_result;
}

[[nodiscard]] Result<const GeometryShape*> resolve_revolve_target(const PartDocument& document,
                                                                  const RevolveFeature& feature,
                                                                  const ShapeCache& shape_cache) {
  const auto& context = feature.body_result_context();
  if (!context.target_body().has_value())
    return Result<const GeometryShape*>::success(nullptr);
  const BodyId& body = *context.target_body();
  for (const auto& [producer, dependent] : document.dependency_graph().dependencies()) {
    if (dependent != feature.id().value() || producer.starts_with("body:"))
      continue;
    if (feature_produces_body(document, producer, body))
      if (const GeometryShape* shape = shape_cache.find_feature_shape(FeatureId(producer)))
        return Result<const GeometryShape*>::success(shape);
  }
  const GeometryShape* shape = shape_cache.find_body_shape(body);
  if (shape == nullptr)
    return Result<const GeometryShape*>::failure(
        geometry_error(body.value(), "revolve target body shape must exist in shape cache"));
  return Result<const GeometryShape*>::success(shape);
}

[[nodiscard]] Result<std::pair<Point3, Vector3>>
resolve_axis_reference(const PartDocument& document, const AxisReference& reference,
                       const ShapeCache& shape_cache, const WorkplaneResolver& workplane_resolver) {
  Result<std::pair<Point3, Vector3>> resolved = Result<std::pair<Point3, Vector3>>::failure(
      geometry_error(reference.source_node_id(), "unsupported axis source"));
  if (const auto* datum = std::get_if<DatumAxisId>(&reference.source())) {
    const DatumAxis* axis = document.find_datum_axis(*datum);
    if (axis == nullptr)
      return Result<std::pair<Point3, Vector3>>::failure(
          geometry_error(datum->value(), "DatumAxis must exist in part document"));
    if (axis->kind() == DatumAxisKind::Explicit)
      resolved = Result<std::pair<Point3, Vector3>>::success({axis->origin(), axis->direction()});
    else {
      auto line = ConstructionLineResolver{}.resolve(document, axis->source_construction_line());
      if (line.has_error())
        return Result<std::pair<Point3, Vector3>>::failure(line.error());
      resolved =
          Result<std::pair<Point3, Vector3>>::success({line.value().point, line.value().direction});
    }
  } else if (const auto* line_id = std::get_if<ConstructionLineId>(&reference.source())) {
    auto line = ConstructionLineResolver{}.resolve(document, *line_id);
    if (line.has_error())
      return Result<std::pair<Point3, Vector3>>::failure(line.error());
    resolved =
        Result<std::pair<Point3, Vector3>>::success({line.value().point, line.value().direction});
  } else if (const auto* semantic = std::get_if<SemanticAxisReference>(&reference.source())) {
    const Feature* source = document.find_feature(semantic->source_feature());
    if (source == nullptr || source->type() != FeatureType::SubtractiveExtrude)
      return Result<std::pair<Point3, Vector3>>::failure(
          geometry_error(semantic->source_feature().value(),
                         "semantic axis requires a circular subtractive-extrude producer"));
    const Sketch* sketch = document.find_sketch(source->input_sketch());
    if (sketch == nullptr || sketch->circle_profiles().size() != 1U ||
        sketch->profile_count() != 1U)
      return Result<std::pair<Point3, Vector3>>::failure(
          geometry_error(semantic->source_feature().value(),
                         "semantic axis producer requires exactly one circle profile"));
    auto workplane = workplane_resolver.resolve_for_sketch(document, *sketch, shape_cache);
    if (workplane.has_error())
      return Result<std::pair<Point3, Vector3>>::failure(workplane.error());
    const CircleProfile& circle = sketch->circle_profiles().front();
    const Point3 origin = workplane_resolver.evaluate_point(workplane.value(), circle.center());
    Vector3 direction = workplane.value().normal;
    if (source->direction() == ExtrudeDirection::OppositeSketchNormal)
      direction = {-direction.x, -direction.y, -direction.z};
    resolved = Result<std::pair<Point3, Vector3>>::success({origin, direction});
  } else {
    const auto& edge = std::get<SemanticEdgeReference>(reference.source());
    auto line = SemanticReferenceEvaluator{}.resolve_edge(document, edge, shape_cache);
    if (line.has_error())
      return Result<std::pair<Point3, Vector3>>::failure(line.error());
    resolved = Result<std::pair<Point3, Vector3>>::success(
        {line.value().start, normalized({line.value().end.x - line.value().start.x,
                                         line.value().end.y - line.value().start.y,
                                         line.value().end.z - line.value().start.z})});
  }

  if (resolved.has_error())
    return resolved;
  if (const auto* transform = shape_cache.find_reference_transform(reference.source_node_id()))
    return Result<std::pair<Point3, Vector3>>::success(
        {transform->cumulative_transform.transform_point(resolved.value().first),
         normalized(transform->cumulative_transform.transform_vector(resolved.value().second))});
  return resolved;
}

struct ResolvedRevolveAngles {
  double start_deg = 0.0;
  double sweep_deg = 360.0;
};

[[nodiscard]] ResolvedRevolveAngles resolve_revolve_angles(const RevolveAngleExtent& extent) {
  if (extent.mode() == RevolveExtentMode::Full)
    return {};
  if (extent.mode() == RevolveExtentMode::Symmetric)
    return {-*extent.angle_deg() / 2.0, *extent.angle_deg()};
  const double sign = *extent.side() == RevolveSide::Positive ? 1.0 : -1.0;
  return {0.0, sign * *extent.angle_deg()};
}

[[nodiscard]] const Parameter* find_required_parameter(const PartDocument& document,
                                                       const ParameterId& parameter_id) noexcept {
  return document.find_parameter(parameter_id);
}

[[nodiscard]] Vector3 extrude_direction_vector(const ResolvedWorkplane& workplane,
                                               ExtrudeDirection direction) noexcept {
  if (direction == ExtrudeDirection::OppositeSketchNormal) {
    return Vector3{-workplane.normal.x, -workplane.normal.y, -workplane.normal.z};
  }
  return workplane.normal;
}

struct ResolvedExtrudeSpan {
  double start_mm = 0.0;
  double end_mm = 0.0;
};

[[nodiscard]] double point_projection(Point3 from, Point3 to, Vector3 axis) noexcept {
  return (to.x - from.x) * axis.x + (to.y - from.y) * axis.y + (to.z - from.z) * axis.z;
}

[[nodiscard]] Result<const GeometryShape*> resolve_extrude_target(const PartDocument& document,
                                                                  const Feature& feature,
                                                                  const ShapeCache& shape_cache) {
  if (feature.body_result_context().has_value() &&
      feature.body_result_context()->target_body().has_value()) {
    const BodyId& body = *feature.body_result_context()->target_body();
    for (const auto& [producer, dependent] : document.dependency_graph().dependencies()) {
      if (dependent != feature.id().value() || producer.starts_with("body:"))
        continue;
      bool produces_target = false;
      if (const Feature* previous = document.find_feature(FeatureId(producer));
          previous != nullptr && previous->body_result_context().has_value())
        produces_target = previous->body_result_context()->effective_produced_body() == body;
      if (const BodyBooleanFeature* previous =
              document.find_body_boolean_feature(FeatureId(producer));
          previous != nullptr)
        produces_target = previous->effective_result_body() == body;
      if (const BodyTransform* previous = document.find_body_transform(BodyTransformId(producer));
          previous != nullptr)
        produces_target = previous->body() == body;
      if (!produces_target)
        continue;
      if (const GeometryShape* previous = shape_cache.find_feature_shape(FeatureId(producer));
          previous != nullptr)
        return Result<const GeometryShape*>::success(previous);
    }
    const GeometryShape* shape = shape_cache.find_body_shape(body);
    if (shape == nullptr)
      return Result<const GeometryShape*>::failure(
          geometry_error(body.value(), "extrude target body shape must exist in shape cache"));
    return Result<const GeometryShape*>::success(shape);
  }
  if (feature.type() == FeatureType::SubtractiveExtrude) {
    const GeometryShape* shape = shape_cache.find_feature_shape(feature.target_feature());
    if (shape == nullptr)
      return Result<const GeometryShape*>::failure(
          geometry_error(feature.target_feature().value(),
                         "extrude target feature shape must exist in shape cache"));
    return Result<const GeometryShape*>::success(shape);
  }
  return Result<const GeometryShape*>::success(nullptr);
}

[[nodiscard]] Result<ResolvedWorkplane>
resolve_limit_face_workplane(const PartDocument& document, const FaceReference& reference,
                             const WorkplaneResolver& resolver) {
  if (reference.source_kind() != PartFeatureInputSourceKind::SemanticPlanarFace)
    return Result<ResolvedWorkplane>::failure(geometry_error(
        reference.source_node_id(), "extrude limit Geometry currently requires a planar face"));
  return resolver.resolve_generated_face(document,
                                         std::get<SemanticFaceReference>(reference.source()));
}

[[nodiscard]] Result<ResolvedExtrudeSpan>
resolve_extrude_span(const PartDocument& document, const Feature& feature,
                     const ResolvedWorkplane& sketch_workplane, Vector3 direction,
                     const GeometryShape* target, const ClosedProfileAdapter& adapter,
                     const WorkplaneResolver& workplane_resolver) {
  constexpr double kThroughAllMargin = 1.0;
  const auto& extent = feature.extrude_intent().extent();
  const auto distance = [&document, &feature](const std::optional<ParameterId>& id,
                                              std::string_view role) -> Result<double> {
    if (!id.has_value())
      return Result<double>::failure(
          geometry_error(feature.id().value(), std::string(role) + " parameter is missing"));
    const Parameter* parameter = document.find_parameter(*id);
    if (parameter == nullptr || parameter->type() != ParameterType::Length ||
        parameter->value().millimeters() <= k_tolerance)
      return Result<double>::failure(
          geometry_error(id->value(), std::string(role) + " must be a positive Length parameter"));
    return Result<double>::success(parameter->value().millimeters());
  };

  switch (extent.mode()) {
  case ExtrudeExtentMode::Distance: {
    auto value = distance(extent.first_distance_parameter(), "distance extent");
    if (value.has_error())
      return Result<ResolvedExtrudeSpan>::failure(value.error());
    return Result<ResolvedExtrudeSpan>::success({0.0, value.value()});
  }
  case ExtrudeExtentMode::Symmetric: {
    auto value = distance(extent.first_distance_parameter(), "symmetric extent");
    if (value.has_error())
      return Result<ResolvedExtrudeSpan>::failure(value.error());
    return Result<ResolvedExtrudeSpan>::success({-value.value() / 2.0, value.value() / 2.0});
  }
  case ExtrudeExtentMode::TwoSided: {
    auto first = distance(extent.first_distance_parameter(), "first two-sided extent");
    auto second = distance(extent.second_distance_parameter(), "second two-sided extent");
    if (first.has_error())
      return Result<ResolvedExtrudeSpan>::failure(first.error());
    if (second.has_error())
      return Result<ResolvedExtrudeSpan>::failure(second.error());
    return Result<ResolvedExtrudeSpan>::success({-second.value(), first.value()});
  }
  case ExtrudeExtentMode::ThroughAll:
  case ExtrudeExtentMode::ToNext: {
    if (target == nullptr)
      return Result<ResolvedExtrudeSpan>::failure(geometry_error(
          feature.id().value(), "through-all and to-next extents require a target body"));
    if (extent.mode() == ExtrudeExtentMode::ThroughAll) {
      auto projected = adapter.projection_span(*target, sketch_workplane.origin, direction);
      if (projected.has_error())
        return Result<ResolvedExtrudeSpan>::failure(projected.error());
      return Result<ResolvedExtrudeSpan>::success({projected.value().minimum - kThroughAllMargin,
                                                   projected.value().maximum + kThroughAllMargin});
    }
    auto next = adapter.first_intersection_distance(*target, sketch_workplane.origin, direction);
    if (next.has_error())
      return Result<ResolvedExtrudeSpan>::failure(next.error());
    return Result<ResolvedExtrudeSpan>::success({0.0, next.value()});
  }
  case ExtrudeExtentMode::ToFace: {
    auto limit = resolve_limit_face_workplane(document, *extent.first_face(), workplane_resolver);
    if (limit.has_error())
      return Result<ResolvedExtrudeSpan>::failure(limit.error());
    const double alignment =
        std::abs(limit.value().normal.x * direction.x + limit.value().normal.y * direction.y +
                 limit.value().normal.z * direction.z);
    if (alignment < 1.0 - k_tolerance)
      return Result<ResolvedExtrudeSpan>::failure(geometry_error(
          feature.id().value(), "to-face limit plane must be normal to the extrusion direction"));
    const double end = point_projection(sketch_workplane.origin, limit.value().origin, direction);
    if (end <= k_tolerance)
      return Result<ResolvedExtrudeSpan>::failure(
          geometry_error(feature.id().value(), "to-face limit must lie in extrude direction"));
    return Result<ResolvedExtrudeSpan>::success({0.0, end});
  }
  case ExtrudeExtentMode::Between: {
    auto first = resolve_limit_face_workplane(document, *extent.first_face(), workplane_resolver);
    auto second = resolve_limit_face_workplane(document, *extent.second_face(), workplane_resolver);
    if (first.has_error())
      return Result<ResolvedExtrudeSpan>::failure(first.error());
    if (second.has_error())
      return Result<ResolvedExtrudeSpan>::failure(second.error());
    const auto aligned = [direction](const ResolvedWorkplane& plane) {
      return std::abs(plane.normal.x * direction.x + plane.normal.y * direction.y +
                      plane.normal.z * direction.z) >= 1.0 - k_tolerance;
    };
    if (!aligned(first.value()) || !aligned(second.value()))
      return Result<ResolvedExtrudeSpan>::failure(geometry_error(
          feature.id().value(), "between limit planes must be normal to extrusion direction"));
    const double first_distance =
        point_projection(sketch_workplane.origin, first.value().origin, direction);
    const double second_distance =
        point_projection(sketch_workplane.origin, second.value().origin, direction);
    if (std::abs(first_distance - second_distance) <= k_tolerance)
      return Result<ResolvedExtrudeSpan>::failure(
          geometry_error(feature.id().value(), "between limit planes must be distinct"));
    return Result<ResolvedExtrudeSpan>::success(
        {std::min(first_distance, second_distance), std::max(first_distance, second_distance)});
  }
  }
  return Result<ResolvedExtrudeSpan>::failure(
      geometry_error(feature.id().value(), "unsupported extrude extent mode"));
}

[[nodiscard]] Result<std::vector<Point3>>
evaluate_thin_profile_vertices(const PartDocument& document, const Sketch& sketch,
                               const ResolvedWorkplane& workplane, const ExtrudeThinIntent& thin,
                               const WorkplaneResolver& resolver) {
  if (sketch.line_segments().size() != 1U || sketch.profile_count() != 0U)
    return Result<std::vector<Point3>>::failure(geometry_error(
        sketch.id().value(), "thin extrusion currently requires exactly one open line segment"));
  const LineSegment& line = sketch.line_segments().front();
  const double dx = line.end().x - line.start().x;
  const double dy = line.end().y - line.start().y;
  const double line_length = std::sqrt(dx * dx + dy * dy);
  if (line_length <= k_tolerance)
    return Result<std::vector<Point3>>::failure(
        geometry_error(line.id().value(), "thin profile line must not be degenerate"));
  const Parameter* first = document.find_parameter(thin.first_thickness_parameter());
  const Parameter* second = thin.second_thickness_parameter().has_value()
                                ? document.find_parameter(*thin.second_thickness_parameter())
                                : nullptr;
  if (first == nullptr || first->value().millimeters() <= k_tolerance ||
      (thin.mode() == ExtrudeThinMode::TwoSided &&
       (second == nullptr || second->value().millimeters() <= k_tolerance)))
    return Result<std::vector<Point3>>::failure(
        geometry_error(sketch.id().value(), "thin thicknesses must be positive"));
  double left = first->value().millimeters();
  double right = 0.0;
  if (thin.mode() == ExtrudeThinMode::MidPlane) {
    left /= 2.0;
    right = left;
  } else if (thin.mode() == ExtrudeThinMode::TwoSided) {
    right = second->value().millimeters();
  }
  const Point2 normal{-dy / line_length, dx / line_length};
  const auto shifted = [normal](Point2 point, double amount) {
    return Point2{point.x + normal.x * amount, point.y + normal.y * amount};
  };
  return Result<std::vector<Point3>>::success(
      {resolver.evaluate_point(workplane, shifted(line.start(), left)),
       resolver.evaluate_point(workplane, shifted(line.end(), left)),
       resolver.evaluate_point(workplane, shifted(line.end(), -right)),
       resolver.evaluate_point(workplane, shifted(line.start(), -right))});
}

[[nodiscard]] bool has_no_profiles(const Sketch& sketch) noexcept {
  return sketch.rectangle_profiles().empty() && sketch.circle_profiles().empty() &&
         sketch.closed_profiles().empty() && sketch.arc_closed_profiles().empty() &&
         sketch.composite_closed_profiles().empty() && sketch.circular_hole_patterns().empty();
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

[[nodiscard]] bool point_inside_polygon(Point2 point, const std::vector<Point2>& polygon) noexcept {
  bool inside = false;
  for (std::size_t i = 0U, j = polygon.size() - 1U; i < polygon.size(); j = i++) {
    const Point2 a = polygon[i];
    const Point2 b = polygon[j];
    if (on_segment(a, b, point))
      return false;

    const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
                         (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x);
    if (crosses)
      inside = !inside;
  }
  return inside;
}

[[nodiscard]] bool contours_intersect(const std::vector<Point2>& left,
                                      const std::vector<Point2>& right) noexcept {
  for (std::size_t i = 0U; i < left.size(); ++i) {
    const Point2 a1 = left[i];
    const Point2 a2 = left[(i + 1U) % left.size()];
    for (std::size_t j = 0U; j < right.size(); ++j) {
      const Point2 b1 = right[j];
      const Point2 b2 = right[(j + 1U) % right.size()];
      if (segments_intersect(a1, a2, b1, b2))
        return true;
    }
  }
  return false;
}

[[nodiscard]] Result<std::size_t> validate_point_inside_bounds(const ResolvedWorkplane& workplane,
                                                               const std::string& object_id,
                                                               Point2 point) {
  if (!workplane.bounds.enabled)
    return Result<std::size_t>::success(0);

  const double min_x = workplane.bounds.center.x - workplane.bounds.width_mm / 2.0;
  const double max_x = workplane.bounds.center.x + workplane.bounds.width_mm / 2.0;
  const double min_y = workplane.bounds.center.y - workplane.bounds.height_mm / 2.0;
  const double max_y = workplane.bounds.center.y + workplane.bounds.height_mm / 2.0;

  if (point.x < min_x - k_tolerance || point.x > max_x + k_tolerance ||
      point.y < min_y - k_tolerance || point.y > max_y + k_tolerance) {
    return Result<std::size_t>::failure(validation_error(
        object_id, "closed profile vertices must lie inside resolved workplane bounds"));
  }

  return Result<std::size_t>::success(0);
}

[[nodiscard]] std::vector<Point2> rectangle_local_vertices(const RectangleProfile& rectangle,
                                                           const Quantity& width,
                                                           const Quantity& height) {
  const Point2 center = rectangle.center();
  const double half_width = width.millimeters() / 2.0;
  const double half_height = height.millimeters() / 2.0;
  return {Point2{center.x - half_width, center.y - half_height},
          Point2{center.x + half_width, center.y - half_height},
          Point2{center.x + half_width, center.y + half_height},
          Point2{center.x - half_width, center.y + half_height}};
}

[[nodiscard]] Result<Point3> evaluate_bounded_circle_center(const WorkplaneResolver& resolver,
                                                            const ResolvedWorkplane& workplane,
                                                            const std::string& object_id,
                                                            Point2 center,
                                                            const Quantity& diameter) {
  if (workplane.bounds.enabled) {
    const double radius = diameter.millimeters() / 2.0;
    const double min_x = workplane.bounds.center.x - workplane.bounds.width_mm / 2.0;
    const double max_x = workplane.bounds.center.x + workplane.bounds.width_mm / 2.0;
    const double min_y = workplane.bounds.center.y - workplane.bounds.height_mm / 2.0;
    const double max_y = workplane.bounds.center.y + workplane.bounds.height_mm / 2.0;

    if (center.x - radius < min_x - k_tolerance || center.x + radius > max_x + k_tolerance ||
        center.y - radius < min_y - k_tolerance || center.y + radius > max_y + k_tolerance) {
      return Result<Point3>::failure(validation_error(
          object_id, "circle profile must lie fully inside resolved workplane bounds"));
    }
  }

  return Result<Point3>::success(resolver.evaluate_point(workplane, center));
}

[[nodiscard]] Result<Point3> evaluate_bounded_circle_center(const WorkplaneResolver& resolver,
                                                            const ResolvedWorkplane& workplane,
                                                            const CircleProfile& circle,
                                                            const Quantity& diameter) {
  return evaluate_bounded_circle_center(resolver, workplane, circle.id().value(), circle.center(),
                                        diameter);
}

[[nodiscard]] bool
closed_profile_uses_reference_generated_lines(const Sketch& sketch,
                                              const ClosedProfile& profile) noexcept {
  for (const auto& id : profile.line_segments()) {
    if (sketch.find_reference_generated_line(id) != nullptr)
      return true;
  }
  return false;
}

[[nodiscard]] Result<std::vector<Point2>>
evaluate_closed_profile_local_vertices(const PartDocument& document, const Sketch& sketch,
                                       const ClosedProfile& profile) {
  if (closed_profile_uses_reference_generated_lines(sketch, profile)) {
    ReferenceGeneratedProfileResolver resolver;
    return resolver.resolve_closed_profile_vertices(document, sketch, profile,
                                                    sketch.reference_generated_lines());
  }

  if (!sketch.driving_dimensions().empty()) {
    DimensionDrivenProfileResolver resolver;
    return resolver.resolve_closed_profile_vertices(document, sketch, profile);
  }

  return sketch.closed_profile_vertices(profile);
}

[[nodiscard]] Result<std::vector<Point3>>
map_bounded_local_vertices(const WorkplaneResolver& resolver, const ResolvedWorkplane& workplane,
                           const std::string& object_id,
                           const std::vector<Point2>& local_vertices) {
  for (const Point2 vertex : local_vertices) {
    auto valid = validate_point_inside_bounds(workplane, object_id, vertex);
    if (valid.has_error())
      return Result<std::vector<Point3>>::failure(valid.error());
  }

  std::vector<Point3> global_vertices;
  global_vertices.reserve(local_vertices.size());
  for (const Point2 vertex : local_vertices) {
    global_vertices.push_back(resolver.evaluate_point(workplane, vertex));
  }
  return Result<std::vector<Point3>>::success(std::move(global_vertices));
}

[[nodiscard]] Result<std::vector<Point3>> evaluate_bounded_closed_profile_vertices(
    const PartDocument& document, const WorkplaneResolver& resolver,
    const ResolvedWorkplane& workplane, const Sketch& sketch, const ClosedProfile& profile) {
  auto local_vertices = evaluate_closed_profile_local_vertices(document, sketch, profile);
  if (local_vertices.has_error())
    return Result<std::vector<Point3>>::failure(local_vertices.error());

  return map_bounded_local_vertices(resolver, workplane, profile.id().value(),
                                    local_vertices.value());
}

[[nodiscard]] Result<ClosedProfileCurveSegment>
evaluate_arc_curve_segment(const WorkplaneResolver& resolver, const ResolvedWorkplane& workplane,
                           const Sketch& sketch, const std::string& object_id, SketchEntityId id) {
  if (const LineSegment* line = sketch.find_line_segment(id)) {
    auto valid_start = validate_point_inside_bounds(workplane, object_id, line->start());
    if (valid_start.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_start.error());

    auto valid_end = validate_point_inside_bounds(workplane, object_id, line->end());
    if (valid_end.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_end.error());

    return Result<ClosedProfileCurveSegment>::success(ClosedProfileCurveSegment{
        ClosedProfileCurveKind::Line, resolver.evaluate_point(workplane, line->start()),
        resolver.evaluate_point(workplane, line->start()),
        resolver.evaluate_point(workplane, line->end())});
  }

  if (const ArcSegment* arc = sketch.find_arc_segment(id)) {
    auto valid_start = validate_point_inside_bounds(workplane, object_id, arc->start());
    if (valid_start.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_start.error());

    auto valid_mid = validate_point_inside_bounds(workplane, object_id, arc->mid());
    if (valid_mid.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_mid.error());

    auto valid_end = validate_point_inside_bounds(workplane, object_id, arc->end());
    if (valid_end.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_end.error());

    return Result<ClosedProfileCurveSegment>::success(ClosedProfileCurveSegment{
        ClosedProfileCurveKind::CircularArc, resolver.evaluate_point(workplane, arc->start()),
        resolver.evaluate_point(workplane, arc->mid()),
        resolver.evaluate_point(workplane, arc->end())});
  }

  if (const SplineSegment* spline = sketch.find_spline_segment(id)) {
    auto valid_start = validate_point_inside_bounds(workplane, object_id, spline->start());
    if (valid_start.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_start.error());

    auto valid_c1 = validate_point_inside_bounds(workplane, object_id, spline->control1());
    if (valid_c1.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_c1.error());

    auto valid_c2 = validate_point_inside_bounds(workplane, object_id, spline->control2());
    if (valid_c2.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_c2.error());

    auto valid_end = validate_point_inside_bounds(workplane, object_id, spline->end());
    if (valid_end.has_error())
      return Result<ClosedProfileCurveSegment>::failure(valid_end.error());

    return Result<ClosedProfileCurveSegment>::success(
        ClosedProfileCurveSegment{ClosedProfileCurveKind::CubicBezierSpline,
                                  resolver.evaluate_point(workplane, spline->start()),
                                  resolver.evaluate_point(workplane, spline->control1()),
                                  resolver.evaluate_point(workplane, spline->end()),
                                  resolver.evaluate_point(workplane, spline->control2())});
  }

  return Result<ClosedProfileCurveSegment>::failure(
      validation_error(object_id, "arc closed profile curve segment must exist in sketch"));
}

[[nodiscard]] Result<std::vector<ClosedProfileCurveSegment>>
evaluate_bounded_arc_profile_curves(const WorkplaneResolver& resolver,
                                    const ResolvedWorkplane& workplane, const Sketch& sketch,
                                    const ArcClosedProfile& profile) {
  auto vertices = sketch.arc_closed_profile_vertices(profile);
  if (vertices.has_error())
    return Result<std::vector<ClosedProfileCurveSegment>>::failure(vertices.error());

  std::vector<ClosedProfileCurveSegment> curves;
  curves.reserve(profile.curve_segments().size());
  for (const auto& id : profile.curve_segments()) {
    auto curve = evaluate_arc_curve_segment(resolver, workplane, sketch, profile.id().value(), id);
    if (curve.has_error())
      return Result<std::vector<ClosedProfileCurveSegment>>::failure(curve.error());
    curves.push_back(curve.value());
  }

  return Result<std::vector<ClosedProfileCurveSegment>>::success(std::move(curves));
}

[[nodiscard]] Result<std::vector<Point2>>
evaluate_contour_local_vertices(const PartDocument& document, const Sketch& sketch,
                                const ProfileId& id, std::string_view suffix,
                                const std::vector<SketchEntityId>& contour) {
  auto profile = ClosedProfile::create(ProfileId(id.value() + std::string(suffix)), contour);
  if (profile.has_error())
    return Result<std::vector<Point2>>::failure(profile.error());
  return evaluate_closed_profile_local_vertices(document, sketch, profile.value());
}

[[nodiscard]] Result<LocalCompositeProfileVertices>
evaluate_composite_local_vertices(const PartDocument& document, const Sketch& sketch,
                                  const CompositeClosedProfile& profile) {
  LocalCompositeProfileVertices result;

  auto outer = evaluate_contour_local_vertices(document, sketch, profile.id(), ".outer",
                                               profile.outer_contour());
  if (outer.has_error())
    return Result<LocalCompositeProfileVertices>::failure(outer.error());
  result.outer = std::move(outer.value());

  result.inner.reserve(profile.inner_contours().size());
  for (std::size_t index = 0U; index < profile.inner_contours().size(); ++index) {
    auto inner = evaluate_contour_local_vertices(document, sketch, profile.id(),
                                                 ".inner." + std::to_string(index),
                                                 profile.inner_contours()[index]);
    if (inner.has_error())
      return Result<LocalCompositeProfileVertices>::failure(inner.error());
    result.inner.push_back(std::move(inner.value()));
  }

  for (const auto& inner : result.inner) {
    for (const Point2 vertex : inner) {
      if (!point_inside_polygon(vertex, result.outer)) {
        return Result<LocalCompositeProfileVertices>::failure(validation_error(
            profile.id().value(),
            "composite closed profile inner contours must lie strictly inside the outer contour"));
      }
    }
    if (contours_intersect(result.outer, inner)) {
      return Result<LocalCompositeProfileVertices>::failure(validation_error(
          profile.id().value(),
          "composite closed profile inner contours must not intersect the outer contour"));
    }
  }

  for (std::size_t i = 0U; i < result.inner.size(); ++i) {
    for (std::size_t j = i + 1U; j < result.inner.size(); ++j) {
      if (contours_intersect(result.inner[i], result.inner[j]) ||
          point_inside_polygon(result.inner[i].front(), result.inner[j]) ||
          point_inside_polygon(result.inner[j].front(), result.inner[i])) {
        return Result<LocalCompositeProfileVertices>::failure(validation_error(
            profile.id().value(), "composite closed profile inner contours must not overlap"));
      }
    }
  }

  return Result<LocalCompositeProfileVertices>::success(std::move(result));
}

[[nodiscard]] Result<GlobalCompositeProfileVertices> evaluate_bounded_composite_profile_vertices(
    const PartDocument& document, const WorkplaneResolver& resolver,
    const ResolvedWorkplane& workplane, const Sketch& sketch,
    const CompositeClosedProfile& profile) {
  auto local = evaluate_composite_local_vertices(document, sketch, profile);
  if (local.has_error())
    return Result<GlobalCompositeProfileVertices>::failure(local.error());

  auto outer =
      map_bounded_local_vertices(resolver, workplane, profile.id().value(), local.value().outer);
  if (outer.has_error())
    return Result<GlobalCompositeProfileVertices>::failure(outer.error());

  GlobalCompositeProfileVertices global;
  global.outer = std::move(outer.value());
  global.inner.reserve(local.value().inner.size());
  for (const auto& inner : local.value().inner) {
    auto mapped = map_bounded_local_vertices(resolver, workplane, profile.id().value(), inner);
    if (mapped.has_error())
      return Result<GlobalCompositeProfileVertices>::failure(mapped.error());
    global.inner.push_back(std::move(mapped.value()));
  }

  return Result<GlobalCompositeProfileVertices>::success(std::move(global));
}

[[nodiscard]] Result<std::vector<Point3>> evaluate_bounded_detected_region_vertices(
    const PartDocument& document, const WorkplaneResolver& resolver,
    const ResolvedWorkplane& workplane, const Sketch& sketch) {
  SketchRegionFinder finder;
  auto region = finder.find_single_region(document, sketch);
  if (region.has_error())
    return Result<std::vector<Point3>>::failure(region.error());
  return map_bounded_local_vertices(resolver, workplane, region.value().id.value(),
                                    region.value().vertices);
}

[[nodiscard]] SweepPathSegment reversed_sweep_segment(SweepPathSegment segment) {
  std::swap(segment.start, segment.end);
  return segment;
}

[[nodiscard]] Result<double> sweep_coordinate(const PartDocument& document,
                                              const SketchCoordinate3D& coordinate,
                                              const SketchEntityId& id) {
  if (coordinate.source() == SketchCoordinate3DSource::Explicit)
    return Result<double>::success(coordinate.explicit_coordinate()->millimeters());
  const Parameter* parameter = document.find_parameter(*coordinate.parameter());
  if (parameter == nullptr || parameter->type() != ParameterType::Length)
    return Result<double>::failure(
        geometry_error(id.value(), "3D path coordinate requires a Length parameter"));
  return Result<double>::success(parameter->value().millimeters());
}

[[nodiscard]] Result<Point3> sweep_point(const PartDocument& document, const Sketch3D& sketch,
                                         const SketchPointReference3D& reference) {
  if (reference.source() == SketchPointReference3DSource::ConstructionPoint) {
    auto point = ConstructionPointResolver{}.resolve(document, *reference.construction_point());
    return point.has_error() ? Result<Point3>::failure(point.error())
                             : Result<Point3>::success(point.value().position);
  }
  if (reference.source() == SketchPointReference3DSource::LocalPoint) {
    const SketchPoint3D* point = sketch.find_point(*reference.local_point());
    if (point == nullptr)
      return Result<Point3>::failure(
          geometry_error(reference.local_point()->value(), "3D path point must exist"));
    auto x = sweep_coordinate(document, point->x(), point->id());
    auto y = sweep_coordinate(document, point->y(), point->id());
    auto z = sweep_coordinate(document, point->z(), point->id());
    if (x.has_error() || y.has_error() || z.has_error())
      return Result<Point3>::failure(x.has_error()   ? x.error()
                                     : y.has_error() ? y.error()
                                                     : z.error());
    return Result<Point3>::success({x.value(), y.value(), z.value()});
  }
  const Sketch* planar = document.find_sketch(*reference.planar_sketch());
  if (planar == nullptr)
    return Result<Point3>::failure(
        geometry_error(reference.planar_sketch()->value(), "3D path planar sketch must exist"));
  auto workplane = WorkplaneResolver{}.resolve_for_sketch(document, *planar);
  if (workplane.has_error())
    return Result<Point3>::failure(workplane.error());
  const SketchReferenceTarget& target = *reference.planar_point();
  Point2 local{};
  if (target.kind() == SketchReferenceTargetKind::ProjectedPoint) {
    const ProjectedSketchPoint* projected = planar->find_projected_point(target.entity());
    if (projected == nullptr)
      return Result<Point3>::failure(
          geometry_error(target.entity().value(), "3D path projected point must exist"));
    auto point = SketchReferenceProjector{}.resolve_point(document, *planar, *projected);
    if (point.has_error())
      return Result<Point3>::failure(point.error());
    local = point.value().position;
  } else {
    const LineSegment* line = planar->find_line_segment(target.entity());
    if (line == nullptr)
      return Result<Point3>::failure(
          geometry_error(target.entity().value(), "3D path planar line endpoint must exist"));
    local =
        target.kind() == SketchReferenceTargetKind::LineSegmentStart ? line->start() : line->end();
  }
  return Result<Point3>::success(WorkplaneResolver{}.evaluate_point(workplane.value(), local));
}

[[nodiscard]] Result<Point3> sweep_local_point(const PartDocument& document, const Sketch3D& sketch,
                                               SketchEntityId id) {
  auto reference = SketchPointReference3D::create_local_point(std::move(id));
  return sweep_point(document, sketch, reference.value());
}

[[nodiscard]] std::vector<SweepPathSegment> sampled_path(const std::vector<Point3>& controls,
                                                         std::size_t samples = 32U) {
  std::vector<Point3> points;
  points.reserve(samples + 1U);
  for (std::size_t sample = 0U; sample <= samples; ++sample) {
    std::vector<Point3> work = controls;
    const double t = static_cast<double>(sample) / static_cast<double>(samples);
    for (std::size_t level = work.size() - 1U; level > 0U; --level)
      for (std::size_t index = 0U; index < level; ++index)
        work[index] = {(1.0 - t) * work[index].x + t * work[index + 1U].x,
                       (1.0 - t) * work[index].y + t * work[index + 1U].y,
                       (1.0 - t) * work[index].z + t * work[index + 1U].z};
    points.push_back(work.front());
  }
  std::vector<SweepPathSegment> segments;
  segments.reserve(samples);
  for (std::size_t index = 1U; index < points.size(); ++index)
    segments.push_back({SweepPathSegmentKind::Line, points[index - 1U], {}, points[index]});
  return segments;
}

[[nodiscard]] Result<std::vector<SweepPathSegment>>
resolve_basic_sweep_path(const PartDocument& document, const PathCurve& path,
                         const ShapeCache& shape_cache, const WorkplaneResolver& resolver,
                         std::string_view role, PathClosure required_closure = PathClosure::Open,
                         bool allow_surface_boundary_sources = false) {
  if (path.closure() != required_closure)
    return Result<std::vector<SweepPathSegment>>::failure(
        geometry_error(path.id().value(), std::string(role) + (required_closure == PathClosure::Open
                                                                   ? " must be open"
                                                                   : " must be closed")));
  std::vector<SweepPathSegment> resolved;
  resolved.reserve(path.segments().size());
  for (const auto& reference : path.segments()) {
    SweepPathSegment segment;
    if (reference.source_kind() == PathSegmentSourceKind::ConstructionLine) {
      const ConstructionLine* line =
          document.find_construction_line(*reference.construction_line());
      if (line == nullptr || line->kind() != ConstructionLineKind::ThroughTwoPoints ||
          !line->relation().has_value())
        return Result<std::vector<SweepPathSegment>>::failure(
            geometry_error(reference.source_node_id(),
                           "Block 81 requires a construction-line path bounded by two points"));
      const ConstructionPointResolver point_resolver;
      auto start = point_resolver.resolve(document, line->relation()->first_point());
      auto end = point_resolver.resolve(document, line->relation()->second_point());
      if (start.has_error() || end.has_error())
        return Result<std::vector<SweepPathSegment>>::failure(start.has_error() ? start.error()
                                                                                : end.error());
      segment = {SweepPathSegmentKind::Line, start.value().position, {}, end.value().position};
    } else if (reference.source_kind() == PathSegmentSourceKind::PlanarSketchCurve) {
      const Sketch* sketch = document.find_sketch(*reference.planar_sketch());
      if (sketch == nullptr)
        return Result<std::vector<SweepPathSegment>>::failure(
            geometry_error(reference.source_node_id(), "sweep path sketch must exist"));
      auto workplane = resolver.resolve_for_sketch(document, *sketch, shape_cache);
      if (workplane.has_error())
        return Result<std::vector<SweepPathSegment>>::failure(workplane.error());
      if (*reference.planar_curve_kind() == PlanarPathCurveKind::Line) {
        const LineSegment* line = sketch->find_line_segment(*reference.entity());
        if (line == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(), "planar sweep line must exist"));
        segment = {SweepPathSegmentKind::Line,
                   resolver.evaluate_point(workplane.value(), line->start()),
                   {},
                   resolver.evaluate_point(workplane.value(), line->end())};
      } else if (*reference.planar_curve_kind() == PlanarPathCurveKind::Arc) {
        const ArcSegment* arc = sketch->find_arc_segment(*reference.entity());
        if (arc == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(), "planar sweep arc must exist"));
        segment = {SweepPathSegmentKind::CircularArc,
                   resolver.evaluate_point(workplane.value(), arc->start()),
                   resolver.evaluate_point(workplane.value(), arc->mid()),
                   resolver.evaluate_point(workplane.value(), arc->end())};
      } else if (*reference.planar_curve_kind() == PlanarPathCurveKind::Spline) {
        if (!allow_surface_boundary_sources)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(),
                             "Block 81 supports only direct planar line and arc path segments"));
        const SplineSegment* spline = sketch->find_spline_segment(*reference.entity());
        if (spline == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(), "planar surface spline must exist"));
        std::vector<Point3> controls{resolver.evaluate_point(workplane.value(), spline->start()),
                                     resolver.evaluate_point(workplane.value(), spline->control1()),
                                     resolver.evaluate_point(workplane.value(), spline->control2()),
                                     resolver.evaluate_point(workplane.value(), spline->end())};
        auto sampled = sampled_path(controls);
        if (reference.reversed()) {
          std::reverse(sampled.begin(), sampled.end());
          for (auto& item : sampled)
            item = reversed_sweep_segment(item);
        }
        resolved.insert(resolved.end(), sampled.begin(), sampled.end());
        continue;
      } else if (*reference.planar_curve_kind() == PlanarPathCurveKind::ReferenceGeneratedLine) {
        if (!allow_surface_boundary_sources)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(),
                             "Block 81 supports only direct planar line and arc path segments"));
        const ReferenceGeneratedLine* generated =
            sketch->find_reference_generated_line(*reference.entity());
        if (generated == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(geometry_error(
              reference.entity()->value(), "reference-generated surface line must exist"));
        auto line = ReferenceGeneratedProfileResolver{}.resolve_line(document, *sketch, *generated);
        if (line.has_error())
          return Result<std::vector<SweepPathSegment>>::failure(line.error());
        segment = {SweepPathSegmentKind::Line,
                   resolver.evaluate_point(workplane.value(), line.value().start()),
                   {},
                   resolver.evaluate_point(workplane.value(), line.value().end())};
      } else {
        return Result<std::vector<SweepPathSegment>>::failure(geometry_error(
            reference.entity()->value(),
            allow_surface_boundary_sources
                ? "projected infinite lines cannot bound a surface PathCurve"
                : "Block 81 supports only direct planar line and arc path segments"));
      }
    } else if (reference.source_kind() == PathSegmentSourceKind::Sketch3DCurve) {
      const Sketch3D* sketch = document.find_sketch_3d(*reference.sketch_3d());
      if (sketch == nullptr)
        return Result<std::vector<SweepPathSegment>>::failure(
            geometry_error(reference.source_node_id(), "3D path sketch must exist"));
      std::vector<SweepPathSegment> spatial;
      if (*reference.sketch_3d_curve_kind() == Sketch3DPathCurveKind::Line) {
        const SketchLine3D* line = sketch->find_line(*reference.entity());
        if (line == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(), "3D sweep line must exist"));
        auto start = sweep_local_point(document, *sketch, line->start_point());
        auto end = sweep_local_point(document, *sketch, line->end_point());
        if (start.has_error() || end.has_error())
          return Result<std::vector<SweepPathSegment>>::failure(start.has_error() ? start.error()
                                                                                  : end.error());
        spatial.push_back({SweepPathSegmentKind::Line, start.value(), {}, end.value()});
      } else if (*reference.sketch_3d_curve_kind() == Sketch3DPathCurveKind::Polyline) {
        const SketchPolyline3D* polyline = sketch->find_polyline(*reference.entity());
        if (polyline == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(), "3D sweep polyline must exist"));
        for (std::size_t index = 1U; index < polyline->ordered_vertices().size(); ++index) {
          auto start =
              sweep_local_point(document, *sketch, polyline->ordered_vertices()[index - 1U]);
          auto end = sweep_local_point(document, *sketch, polyline->ordered_vertices()[index]);
          if (start.has_error() || end.has_error())
            return Result<std::vector<SweepPathSegment>>::failure(start.has_error() ? start.error()
                                                                                    : end.error());
          spatial.push_back({SweepPathSegmentKind::Line, start.value(), {}, end.value()});
        }
      } else if (*reference.sketch_3d_curve_kind() == Sketch3DPathCurveKind::Arc) {
        const SketchArc3D* arc = sketch->find_arc(*reference.entity());
        if (arc == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(), "3D sweep arc must exist"));
        auto start = sweep_point(document, *sketch, arc->start());
        auto mid = sweep_point(document, *sketch, arc->intermediate());
        auto end = sweep_point(document, *sketch, arc->end());
        if (start.has_error() || mid.has_error() || end.has_error())
          return Result<std::vector<SweepPathSegment>>::failure(start.has_error() ? start.error()
                                                                : mid.has_error() ? mid.error()
                                                                                  : end.error());
        spatial.push_back(
            {SweepPathSegmentKind::CircularArc, start.value(), mid.value(), end.value()});
      } else if (*reference.sketch_3d_curve_kind() == Sketch3DPathCurveKind::Spline) {
        const SketchSpline3D* spline = sketch->find_spline(*reference.entity());
        if (spline == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(), "3D sweep spline must exist"));
        std::vector<Point3> controls;
        for (const auto& point : spline->ordered_points()) {
          auto resolved_point = sweep_point(document, *sketch, point);
          if (resolved_point.has_error())
            return Result<std::vector<SweepPathSegment>>::failure(resolved_point.error());
          controls.push_back(resolved_point.value());
        }
        spatial = sampled_path(controls);
      } else {
        const SketchHelix3D* helix = sketch->find_helix(*reference.entity());
        if (helix == nullptr)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(reference.entity()->value(), "3D sweep helix must exist"));
        const Parameter* radius = document.find_parameter(helix->radius_parameter());
        const Parameter* pitch = document.find_parameter(helix->pitch_parameter());
        const Parameter* turns = document.find_parameter(helix->turns_parameter());
        if (radius == nullptr || radius->type() != ParameterType::Length || pitch == nullptr ||
            pitch->type() != ParameterType::Length || turns == nullptr ||
            turns->type() != ParameterType::Count || turns->value().count_value() == 0U)
          return Result<std::vector<SweepPathSegment>>::failure(
              geometry_error(helix->id().value(), "3D sweep helix parameters are invalid"));
        Point3 origin{};
        Vector3 axis{0.0, 0.0, 1.0};
        if (helix->axis().source() == SketchHelixAxis3DSource::ConstructionLine) {
          auto resolved_axis =
              ConstructionLineResolver{}.resolve(document, *helix->axis().construction_line());
          if (resolved_axis.has_error())
            return Result<std::vector<SweepPathSegment>>::failure(resolved_axis.error());
          origin = resolved_axis.value().point;
          axis = normalized(resolved_axis.value().direction);
        } else {
          const DatumAxis* datum = document.find_datum_axis(*helix->axis().datum_axis());
          if (datum == nullptr)
            return Result<std::vector<SweepPathSegment>>::failure(geometry_error(
                helix->axis().referenced_node_id(), "3D sweep helix DatumAxis must exist"));
          if (datum->kind() == DatumAxisKind::Explicit) {
            origin = datum->origin();
            axis = normalized(datum->direction());
          } else {
            auto resolved_axis =
                ConstructionLineResolver{}.resolve(document, datum->source_construction_line());
            if (resolved_axis.has_error())
              return Result<std::vector<SweepPathSegment>>::failure(resolved_axis.error());
            origin = resolved_axis.value().point;
            axis = normalized(resolved_axis.value().direction);
          }
        }
        const Vector3 helper =
            std::abs(axis.z) < 0.9 ? Vector3{0.0, 0.0, 1.0} : Vector3{0.0, 1.0, 0.0};
        const Vector3 x = normalized({axis.y * helper.z - axis.z * helper.y,
                                      axis.z * helper.x - axis.x * helper.z,
                                      axis.x * helper.y - axis.y * helper.x});
        const Vector3 y = {axis.y * x.z - axis.z * x.y, axis.z * x.x - axis.x * x.z,
                           axis.x * x.y - axis.y * x.x};
        const std::size_t count = turns->value().count_value() * 48U;
        std::vector<Point3> points;
        for (std::size_t index = 0U; index <= count; ++index) {
          const double fraction = static_cast<double>(index) / static_cast<double>(count);
          const double angle =
              2.0 * k_pi * static_cast<double>(turns->value().count_value()) * fraction *
              (helix->handedness() == SketchHelix3DHandedness::RightHanded ? 1.0 : -1.0);
          const double axial = pitch->value().millimeters() *
                               static_cast<double>(turns->value().count_value()) * fraction;
          points.push_back(
              {origin.x +
                   radius->value().millimeters() * (x.x * std::cos(angle) + y.x * std::sin(angle)) +
                   axis.x * axial,
               origin.y +
                   radius->value().millimeters() * (x.y * std::cos(angle) + y.y * std::sin(angle)) +
                   axis.y * axial,
               origin.z +
                   radius->value().millimeters() * (x.z * std::cos(angle) + y.z * std::sin(angle)) +
                   axis.z * axial});
        }
        for (std::size_t index = 1U; index < points.size(); ++index)
          spatial.push_back({SweepPathSegmentKind::Line, points[index - 1U], {}, points[index]});
      }
      if (reference.reversed()) {
        std::reverse(spatial.begin(), spatial.end());
        for (auto& item : spatial)
          item = reversed_sweep_segment(item);
      }
      resolved.insert(resolved.end(), spatial.begin(), spatial.end());
      continue;
    } else {
      if (!allow_surface_boundary_sources)
        return Result<std::vector<SweepPathSegment>>::failure(
            geometry_error(reference.source_node_id(),
                           "semantic generated-edge sweep paths are unsupported in Block 82"));
      auto edge = SemanticReferenceEvaluator{}.resolve_edge(document, *reference.semantic_edge(),
                                                            shape_cache);
      if (edge.has_error())
        return Result<std::vector<SweepPathSegment>>::failure(edge.error());
      segment = {SweepPathSegmentKind::Line, edge.value().start, {}, edge.value().end};
    }
    resolved.push_back(reference.reversed() ? reversed_sweep_segment(segment) : segment);
  }
  return Result<std::vector<SweepPathSegment>>::success(std::move(resolved));
}

[[nodiscard]] Result<std::vector<SweepPathSegment>>
resolve_surface_boundary(const PartDocument& document, const BoundaryCurveReference& boundary,
                         const ShapeCache& shape_cache, const WorkplaneResolver& resolver) {
  if (boundary.source_kind() == BoundaryCurveSourceKind::PathCurve) {
    const PathCurveId& path_id = std::get<PathCurveId>(boundary.source());
    const PathCurve* path = document.find_path_curve(path_id);
    if (path == nullptr)
      return Result<std::vector<SweepPathSegment>>::failure(
          geometry_error(path_id.value(), "surface boundary PathCurve must exist"));
    return resolve_basic_sweep_path(document, *path, shape_cache, resolver, "surface boundary",
                                    path->closure(), true);
  }
  const auto& reference = std::get<SemanticEdgeReference>(boundary.source());
  auto edge = SemanticReferenceEvaluator{}.resolve_edge(document, reference, shape_cache);
  if (edge.has_error())
    return Result<std::vector<SweepPathSegment>>::failure(edge.error());
  return Result<std::vector<SweepPathSegment>>::success(
      {{SweepPathSegmentKind::Line, edge.value().start, {}, edge.value().end}});
}

[[nodiscard]] std::vector<ClosedProfileCurveSegment>
line_profile_curves(const std::vector<Point3>& vertices) {
  std::vector<ClosedProfileCurveSegment> curves;
  curves.reserve(vertices.size());
  for (std::size_t index = 0U; index < vertices.size(); ++index)
    curves.push_back({ClosedProfileCurveKind::Line,
                      vertices[index],
                      {},
                      vertices[(index + 1U) % vertices.size()]});
  return curves;
}

[[nodiscard]] ClosedProfileCurveSegment
reversed_loft_curve(const ClosedProfileCurveSegment& segment) {
  if (segment.kind == ClosedProfileCurveKind::Line)
    return {segment.kind, segment.end, {}, segment.start};
  if (segment.kind == ClosedProfileCurveKind::CircularArc)
    return {segment.kind, segment.end, segment.mid, segment.start};
  return {segment.kind, segment.end, segment.control2, segment.start, segment.mid};
}

[[nodiscard]] Point3 rotate_about_axis(Point3 point, Point3 center, Vector3 axis,
                                       double angle_rad) {
  axis = normalized(axis);
  const Vector3 value{point.x - center.x, point.y - center.y, point.z - center.z};
  const double cosine = std::cos(angle_rad), sine = std::sin(angle_rad);
  const Vector3 crossed{axis.y * value.z - axis.z * value.y, axis.z * value.x - axis.x * value.z,
                        axis.x * value.y - axis.y * value.x};
  const double axial = axis.x * value.x + axis.y * value.y + axis.z * value.z;
  const Vector3 rotated{value.x * cosine + crossed.x * sine + axis.x * axial * (1.0 - cosine),
                        value.y * cosine + crossed.y * sine + axis.y * axial * (1.0 - cosine),
                        value.z * cosine + crossed.z * sine + axis.z * axial * (1.0 - cosine)};
  return {center.x + rotated.x, center.y + rotated.y, center.z + rotated.z};
}

void apply_loft_section_controls(std::vector<ClosedProfileCurveSegment>& curves,
                                 const ProfileSectionReference& section,
                                 const ResolvedWorkplane& workplane, double rotation_deg) {
  if (section.flip_normal()) {
    std::reverse(curves.begin(), curves.end());
    for (auto& curve : curves)
      curve = reversed_loft_curve(curve);
  }
  if (std::abs(rotation_deg) <= k_tolerance)
    return;
  Point3 center{};
  for (const auto& curve : curves) {
    center.x += curve.start.x;
    center.y += curve.start.y;
    center.z += curve.start.z;
  }
  const double count = static_cast<double>(curves.size());
  center = {center.x / count, center.y / count, center.z / count};
  const double angle = rotation_deg * k_pi / 180.0;
  for (auto& curve : curves) {
    curve.start = rotate_about_axis(curve.start, center, workplane.normal, angle);
    curve.end = rotate_about_axis(curve.end, center, workplane.normal, angle);
    if (curve.kind != ClosedProfileCurveKind::Line)
      curve.mid = rotate_about_axis(curve.mid, center, workplane.normal, angle);
    if (curve.kind == ClosedProfileCurveKind::CubicBezierSpline)
      curve.control2 = rotate_about_axis(curve.control2, center, workplane.normal, angle);
  }
}

template <typename IdRange>
void align_loft_section(std::vector<ClosedProfileCurveSegment>& curves, const IdRange& ids,
                        const std::optional<SketchEntityId>& alignment) {
  if (!alignment.has_value())
    return;
  const auto found = std::find(ids.begin(), ids.end(), *alignment);
  if (found == ids.end())
    return;
  const auto offset = static_cast<std::size_t>(std::distance(ids.begin(), found));
  std::rotate(curves.begin(), curves.begin() + static_cast<std::ptrdiff_t>(offset), curves.end());
}

[[nodiscard]] Result<std::vector<ClosedProfileCurveSegment>>
resolve_loft_closed_section(const PartDocument& document, const ProfileSectionReference& section,
                            const ShapeCache& cache, const WorkplaneResolver& resolver) {
  const auto& reference = std::get<ProfileRegionReference>(section.source());
  const Sketch* sketch = document.find_sketch(reference.sketch());
  if (sketch == nullptr)
    return Result<std::vector<ClosedProfileCurveSegment>>::failure(
        geometry_error(reference.sketch().value(), "loft section sketch must exist"));
  auto workplane = resolver.resolve_for_sketch(document, *sketch, cache);
  if (workplane.has_error())
    return Result<std::vector<ClosedProfileCurveSegment>>::failure(workplane.error());
  std::vector<ClosedProfileCurveSegment> curves;
  const ProfileId& profile_id = reference.profile();
  if (const RectangleProfile* rectangle = sketch->find_rectangle_profile(profile_id)) {
    const Parameter* width = document.find_parameter(rectangle->width_parameter());
    const Parameter* height = document.find_parameter(rectangle->height_parameter());
    if (width == nullptr || height == nullptr || width->type() != ParameterType::Length ||
        height->type() != ParameterType::Length)
      return Result<std::vector<ClosedProfileCurveSegment>>::failure(
          geometry_error(profile_id.value(), "loft rectangle dimensions must be Length values"));
    auto vertices = map_bounded_local_vertices(
        resolver, workplane.value(), profile_id.value(),
        rectangle_local_vertices(*rectangle, width->value(), height->value()));
    if (vertices.has_error())
      return Result<std::vector<ClosedProfileCurveSegment>>::failure(vertices.error());
    curves = line_profile_curves(vertices.value());
  } else if (const ClosedProfile* closed = sketch->find_closed_profile(profile_id)) {
    auto vertices = evaluate_bounded_closed_profile_vertices(document, resolver, workplane.value(),
                                                             *sketch, *closed);
    if (vertices.has_error())
      return Result<std::vector<ClosedProfileCurveSegment>>::failure(vertices.error());
    curves = line_profile_curves(vertices.value());
    align_loft_section(curves, closed->line_segments(), section.alignment_reference());
  } else if (const ArcClosedProfile* arc = sketch->find_arc_closed_profile(profile_id)) {
    auto resolved = evaluate_bounded_arc_profile_curves(resolver, workplane.value(), *sketch, *arc);
    if (resolved.has_error())
      return Result<std::vector<ClosedProfileCurveSegment>>::failure(resolved.error());
    curves = std::move(resolved.value());
    align_loft_section(curves, arc->curve_segments(), section.alignment_reference());
  } else if (const CircleProfile* circle = sketch->find_circle_profile(profile_id)) {
    const Parameter* diameter = document.find_parameter(circle->diameter_parameter());
    if (diameter == nullptr || diameter->type() != ParameterType::Length)
      return Result<std::vector<ClosedProfileCurveSegment>>::failure(
          geometry_error(profile_id.value(), "loft circle diameter must be a Length"));
    const double radius = diameter->value().millimeters() / 2.0;
    for (std::size_t index = 0U; index < 4U; ++index) {
      const double start = static_cast<double>(index) * k_pi / 2.0;
      const double middle = start + k_pi / 4.0;
      const double end = start + k_pi / 2.0;
      const auto mapped = [&](double angle) {
        return resolver.evaluate_point(workplane.value(),
                                       {circle->center().x + radius * std::cos(angle),
                                        circle->center().y + radius * std::sin(angle)});
      };
      curves.push_back(
          {ClosedProfileCurveKind::CircularArc, mapped(start), mapped(middle), mapped(end)});
    }
  } else {
    return Result<std::vector<ClosedProfileCurveSegment>>::failure(geometry_error(
        profile_id.value(),
        "Block 85 supports rectangle, circle, line, and arc/spline closed loft sections"));
  }
  double rotation_deg = 0.0;
  if (section.rotation_offset().has_value()) {
    const Parameter* rotation = document.find_parameter(*section.rotation_offset());
    if (rotation == nullptr || rotation->type() != ParameterType::Angle)
      return Result<std::vector<ClosedProfileCurveSegment>>::failure(geometry_error(
          section.rotation_offset()->value(), "loft rotation must reference an Angle parameter"));
    rotation_deg = rotation->value().degrees();
  }
  apply_loft_section_controls(curves, section, workplane.value(), rotation_deg);
  return Result<std::vector<ClosedProfileCurveSegment>>::success(std::move(curves));
}

[[nodiscard]] std::vector<SweepPathSegment>
surface_path_from_closed_curves(const std::vector<ClosedProfileCurveSegment>& curves) {
  std::vector<SweepPathSegment> result;
  for (const auto& curve : curves) {
    if (curve.kind == ClosedProfileCurveKind::Line) {
      result.push_back({SweepPathSegmentKind::Line, curve.start, {}, curve.end});
    } else if (curve.kind == ClosedProfileCurveKind::CircularArc) {
      result.push_back({SweepPathSegmentKind::CircularArc, curve.start, curve.mid, curve.end});
    } else {
      auto sampled = sampled_path({curve.start, curve.mid, curve.control2, curve.end});
      result.insert(result.end(), sampled.begin(), sampled.end());
    }
  }
  return result;
}

[[nodiscard]] Result<std::vector<SweepPathSegment>>
resolve_surface_trimming(const PartDocument& document, const TrimmingReference& trimming,
                         const ShapeCache& cache, const WorkplaneResolver& resolver) {
  if (trimming.source_kind() == TrimmingReferenceSourceKind::BoundaryCurve)
    return resolve_surface_boundary(document, std::get<BoundaryCurveReference>(trimming.source()),
                                    cache, resolver);
  auto section = ProfileSectionReference::create_closed_region(
      std::get<ProfileRegionReference>(trimming.source()));
  if (section.has_error())
    return Result<std::vector<SweepPathSegment>>::failure(section.error());
  auto curves = resolve_loft_closed_section(document, section.value(), cache, resolver);
  if (curves.has_error())
    return Result<std::vector<SweepPathSegment>>::failure(curves.error());
  return Result<std::vector<SweepPathSegment>>::success(
      surface_path_from_closed_curves(curves.value()));
}

[[nodiscard]] Result<GeometryShape> resolve_surface_target(const PartDocument& document,
                                                           const SurfaceReference& reference,
                                                           const ShapeCache& cache,
                                                           const SurfaceAdapter& adapter,
                                                           const FeatureId& consumer) {
  if (reference.source_kind() == SurfaceReferenceSourceKind::Body) {
    const BodyId& body = std::get<BodyId>(reference.source());
    const GeometryShape* shape = cache.find_body_shape(body);
    if (shape == nullptr)
      return Result<GeometryShape>::failure(
          geometry_error(body.value(), "surface target Body shape must exist in the shape cache"));
    return Result<GeometryShape>::success(*shape);
  }
  return adapter.extract_semantic_face(consumer, document, cache,
                                       std::get<SemanticFaceReference>(reference.source()));
}

// True when the sketch contains exactly one profile and it is of the kind
// selected by `kind_size`.
template <typename KindSize>
[[nodiscard]] bool has_exactly_one_profile_of_kind(const Sketch& sketch,
                                                   KindSize kind_size) noexcept {
  return kind_size(sketch) == 1U && sketch.profile_count() == 1U;
}

[[nodiscard]] bool has_exactly_one_rectangle_profile(const Sketch& sketch) noexcept {
  return has_exactly_one_profile_of_kind(
      sketch, [](const Sketch& s) { return s.rectangle_profiles().size(); });
}

[[nodiscard]] bool has_exactly_one_closed_profile(const Sketch& sketch) noexcept {
  return has_exactly_one_profile_of_kind(
      sketch, [](const Sketch& s) { return s.closed_profiles().size(); });
}

[[nodiscard]] bool has_exactly_one_arc_closed_profile(const Sketch& sketch) noexcept {
  return has_exactly_one_profile_of_kind(
      sketch, [](const Sketch& s) { return s.arc_closed_profiles().size(); });
}

[[nodiscard]] bool has_exactly_one_composite_closed_profile(const Sketch& sketch) noexcept {
  return has_exactly_one_profile_of_kind(
      sketch, [](const Sketch& s) { return s.composite_closed_profiles().size(); });
}

[[nodiscard]] bool has_exactly_one_circle_profile(const Sketch& sketch) noexcept {
  return has_exactly_one_profile_of_kind(
      sketch, [](const Sketch& s) { return s.circle_profiles().size(); });
}

[[nodiscard]] bool has_exactly_one_circular_hole_pattern(const Sketch& sketch) noexcept {
  return has_exactly_one_profile_of_kind(
      sketch, [](const Sketch& s) { return s.circular_hole_patterns().size(); });
}

} // namespace

Result<std::size_t> GeometryRecomputeExecutor::execute_path_extrude(const PartDocument& document,
                                                                    const Feature& feature,
                                                                    ShapeCache& shape_cache) const {
  if (!feature.path_curve().has_value())
    return Result<std::size_t>::failure(
        geometry_error(feature.id().value(), "path extrude requires a PathCurveId"));
  ShapeCache working_cache = shape_cache;
  const PathCurve* path = document.find_path_curve(*feature.path_curve());
  if (path == nullptr)
    return Result<std::size_t>::failure(
        geometry_error(feature.path_curve()->value(), "path extrude PathCurve must exist"));
  auto trajectory = resolve_basic_sweep_path(document, *path, working_cache, workplane_resolver_,
                                             "path extrude trajectory");
  if (trajectory.has_error())
    return Result<std::size_t>::failure(trajectory.error());

  const Sketch* sketch = document.find_sketch(feature.input_sketch());
  if (sketch == nullptr)
    return Result<std::size_t>::failure(
        geometry_error(feature.input_sketch().value(), "path extrude profile sketch must exist"));
  auto workplane = workplane_resolver_.resolve_for_sketch(document, *sketch, working_cache);
  if (workplane.has_error())
    return Result<std::size_t>::failure(workplane.error());

  std::vector<ClosedProfileCurveSegment> curves;
  if (has_exactly_one_rectangle_profile(*sketch)) {
    const RectangleProfile& rectangle = sketch->rectangle_profiles().front();
    const Parameter* width = document.find_parameter(rectangle.width_parameter());
    const Parameter* height = document.find_parameter(rectangle.height_parameter());
    if (width == nullptr || height == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(rectangle.id().value(), "path extrude dimensions must exist"));
    auto vertices = map_bounded_local_vertices(
        workplane_resolver_, workplane.value(), rectangle.id().value(),
        rectangle_local_vertices(rectangle, width->value(), height->value()));
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());
    curves = line_profile_curves(vertices.value());
  } else if (has_exactly_one_circle_profile(*sketch)) {
    const CircleProfile& circle = sketch->circle_profiles().front();
    const Parameter* diameter = document.find_parameter(circle.diameter_parameter());
    if (diameter == nullptr || diameter->type() != ParameterType::Length)
      return Result<std::size_t>::failure(
          geometry_error(circle.id().value(), "path extrude circle diameter must be a Length"));
    const double radius = diameter->value().millimeters() / 2.0;
    for (std::size_t index = 0U; index < 4U; ++index) {
      const double start = static_cast<double>(index) * k_pi / 2.0;
      const double middle = start + k_pi / 4.0;
      const double end = start + k_pi / 2.0;
      const auto mapped = [&](double angle) {
        return workplane_resolver_.evaluate_point(workplane.value(),
                                                  {circle.center().x + radius * std::cos(angle),
                                                   circle.center().y + radius * std::sin(angle)});
      };
      curves.push_back(
          {ClosedProfileCurveKind::CircularArc, mapped(start), mapped(middle), mapped(end)});
    }
  } else if (has_exactly_one_closed_profile(*sketch)) {
    auto vertices =
        evaluate_bounded_closed_profile_vertices(document, workplane_resolver_, workplane.value(),
                                                 *sketch, sketch->closed_profiles().front());
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());
    curves = line_profile_curves(vertices.value());
  } else if (has_exactly_one_arc_closed_profile(*sketch)) {
    auto resolved = evaluate_bounded_arc_profile_curves(
        workplane_resolver_, workplane.value(), *sketch, sketch->arc_closed_profiles().front());
    if (resolved.has_error())
      return Result<std::size_t>::failure(resolved.error());
    curves = std::move(resolved.value());
  } else if (has_no_profiles(*sketch)) {
    auto vertices = evaluate_bounded_detected_region_vertices(document, workplane_resolver_,
                                                              workplane.value(), *sketch);
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());
    curves = line_profile_curves(vertices.value());
  } else {
    return Result<std::size_t>::failure(geometry_error(
        sketch->id().value(),
        "path extrude supports one rectangle, circle, line/arc closed, or detected profile"));
  }

  auto tool = sweep_adapter_.sweep_closed_profile(
      feature.id(), curves, trajectory.value(), path->orientation_rule(), path->fixed_up_vector());
  if (tool.has_error())
    return Result<std::size_t>::failure(tool.error());
  auto target = resolve_extrude_target(document, feature, working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  const FeatureBodyOperationMode mode =
      feature.body_result_context().has_value()
          ? feature.body_result_context()->operation_mode()
          : (feature.type() == FeatureType::AdditiveExtrude ? FeatureBodyOperationMode::NewBody
                                                            : FeatureBodyOperationMode::Cut);
  GeometryShape result = tool.value();
  if (mode != FeatureBodyOperationMode::NewBody) {
    if (target.value() == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(feature.id().value(), "path extrude operation requires a target shape"));
    BodyBooleanOperation operation = BodyBooleanOperation::Add;
    if (mode == FeatureBodyOperationMode::Cut)
      operation = BodyBooleanOperation::Subtract;
    else if (mode == FeatureBodyOperationMode::Intersect)
      operation = BodyBooleanOperation::Intersect;
    auto combined = body_boolean_adapter_.combine(feature.id(), operation, *target.value(),
                                                  std::vector<GeometryShape>{tool.value()});
    if (combined.has_error())
      return Result<std::size_t>::failure(combined.error());
    result = std::move(combined.value());
  }
  auto stored = store_feature_result(document, feature, std::move(result), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_richer_extrude(
    const PartDocument& document, const Feature& feature, ShapeCache& shape_cache) const {
  if (feature.direction() == ExtrudeDirection::Path)
    return execute_path_extrude(document, feature, shape_cache);
  const Sketch* sketch = document.find_sketch(feature.input_sketch());
  if (sketch == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature.id().value(), "feature input sketch must exist in part document"));
  auto workplane = workplane_resolver_.resolve_for_sketch(document, *sketch, shape_cache);
  if (workplane.has_error())
    return Result<std::size_t>::failure(workplane.error());
  const Vector3 direction = extrude_direction_vector(workplane.value(), feature.direction());
  auto target = resolve_extrude_target(document, feature, shape_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());

  Result<std::vector<Point3>> vertices = Result<std::vector<Point3>>::failure(
      geometry_error(sketch->id().value(), "unsupported richer extrude profile"));
  if (feature.extrude_intent().thin().has_value()) {
    vertices =
        evaluate_thin_profile_vertices(document, *sketch, workplane.value(),
                                       *feature.extrude_intent().thin(), workplane_resolver_);
  } else if (has_exactly_one_rectangle_profile(*sketch)) {
    const RectangleProfile& rectangle = sketch->rectangle_profiles().front();
    const Parameter* width = document.find_parameter(rectangle.width_parameter());
    const Parameter* height = document.find_parameter(rectangle.height_parameter());
    if (width == nullptr || height == nullptr)
      return Result<std::size_t>::failure(geometry_error(
          rectangle.id().value(), "rectangle dimensions must exist for richer extrusion"));
    vertices = map_bounded_local_vertices(
        workplane_resolver_, workplane.value(), rectangle.id().value(),
        rectangle_local_vertices(rectangle, width->value(), height->value()));
  } else if (has_exactly_one_closed_profile(*sketch)) {
    vertices =
        evaluate_bounded_closed_profile_vertices(document, workplane_resolver_, workplane.value(),
                                                 *sketch, sketch->closed_profiles().front());
  } else if (has_no_profiles(*sketch)) {
    vertices = evaluate_bounded_detected_region_vertices(document, workplane_resolver_,
                                                         workplane.value(), *sketch);
  } else {
    return Result<std::size_t>::failure(geometry_error(
        sketch->id().value(),
        "richer extrusion currently supports rectangle, line-based closed region, detected "
        "region, or one-line thin profiles"));
  }
  if (vertices.has_error())
    return Result<std::size_t>::failure(vertices.error());

  Point3 profile_center;
  for (const Point3 vertex : vertices.value()) {
    profile_center.x += vertex.x;
    profile_center.y += vertex.y;
    profile_center.z += vertex.z;
  }
  const double vertex_count = static_cast<double>(vertices.value().size());
  profile_center.x /= vertex_count;
  profile_center.y /= vertex_count;
  profile_center.z /= vertex_count;
  ResolvedWorkplane span_workplane = workplane.value();
  span_workplane.origin = profile_center;
  auto span = resolve_extrude_span(document, feature, span_workplane, direction, target.value(),
                                   closed_profile_adapter_, workplane_resolver_);
  if (span.has_error())
    return Result<std::size_t>::failure(span.error());

  auto tool = closed_profile_adapter_.make_extruded_profile_span(
      vertices.value(), direction, span.value().start_mm, span.value().end_mm,
      feature.extrude_intent().taper_angle_deg());
  if (tool.has_error())
    return Result<std::size_t>::failure(tool.error());

  const FeatureBodyOperationMode mode =
      feature.body_result_context().has_value()
          ? feature.body_result_context()->operation_mode()
          : (feature.type() == FeatureType::AdditiveExtrude ? FeatureBodyOperationMode::NewBody
                                                            : FeatureBodyOperationMode::Cut);
  GeometryShape result = tool.value();
  if (mode != FeatureBodyOperationMode::NewBody) {
    if (target.value() == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(feature.id().value(), "modifying extrusion requires a target shape"));
    BodyBooleanOperation operation = BodyBooleanOperation::Add;
    if (mode == FeatureBodyOperationMode::Cut)
      operation = BodyBooleanOperation::Subtract;
    else if (mode == FeatureBodyOperationMode::Intersect)
      operation = BodyBooleanOperation::Intersect;
    auto combined = body_boolean_adapter_.combine(feature.id(), operation, *target.value(),
                                                  std::vector<GeometryShape>{tool.value()});
    if (combined.has_error())
      return Result<std::size_t>::failure(combined.error());
    result = std::move(combined.value());
  }
  return store_feature_result(document, feature, std::move(result), shape_cache);
}

Result<std::size_t> GeometryRecomputeExecutor::execute_revolve(const PartDocument& document,
                                                               FeatureId feature_id,
                                                               ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(
        validation_error("revolve", "feature id must not be empty"));
  const RevolveFeature* feature = document.find_revolve_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "revolve feature must exist in part document"));

  ShapeCache working_cache = shape_cache;
  const Sketch* sketch = document.find_sketch(feature->profile().sketch());
  if (sketch == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature->profile().sketch().value(), "revolve profile sketch must exist"));
  auto workplane = workplane_resolver_.resolve_for_sketch(document, *sketch, working_cache);
  if (workplane.has_error())
    return Result<std::size_t>::failure(workplane.error());
  auto axis = resolve_axis_reference(document, feature->axis(), working_cache, workplane_resolver_);
  if (axis.has_error())
    return Result<std::size_t>::failure(axis.error());
  const ResolvedRevolveAngles angles = resolve_revolve_angles(feature->extent());

  Result<GeometryShape> tool = Result<GeometryShape>::failure(
      geometry_error(feature->id().value(), "unsupported revolve profile region"));
  const ProfileId& profile_id = feature->profile().profile();
  if (const RectangleProfile* rectangle = sketch->find_rectangle_profile(profile_id)) {
    const Parameter* width = document.find_parameter(rectangle->width_parameter());
    const Parameter* height = document.find_parameter(rectangle->height_parameter());
    if (width == nullptr || height == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(profile_id.value(), "revolve rectangle dimensions must exist"));
    auto vertices = map_bounded_local_vertices(
        workplane_resolver_, workplane.value(), profile_id.value(),
        rectangle_local_vertices(*rectangle, width->value(), height->value()));
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());
    tool =
        revolve_adapter_.revolve_profile(feature->id(), vertices.value(), axis.value().first,
                                         axis.value().second, angles.start_deg, angles.sweep_deg);
  } else if (const ClosedProfile* closed = sketch->find_closed_profile(profile_id)) {
    auto vertices = evaluate_bounded_closed_profile_vertices(document, workplane_resolver_,
                                                             workplane.value(), *sketch, *closed);
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());
    tool =
        revolve_adapter_.revolve_profile(feature->id(), vertices.value(), axis.value().first,
                                         axis.value().second, angles.start_deg, angles.sweep_deg);
  } else if (const ArcClosedProfile* arc = sketch->find_arc_closed_profile(profile_id)) {
    auto curves =
        evaluate_bounded_arc_profile_curves(workplane_resolver_, workplane.value(), *sketch, *arc);
    if (curves.has_error())
      return Result<std::size_t>::failure(curves.error());
    tool = revolve_adapter_.revolve_profile_curves(feature->id(), curves.value(),
                                                   axis.value().first, axis.value().second,
                                                   angles.start_deg, angles.sweep_deg);
  } else if (const CompositeClosedProfile* composite =
                 sketch->find_composite_closed_profile(profile_id)) {
    auto vertices = evaluate_bounded_composite_profile_vertices(
        document, workplane_resolver_, workplane.value(), *sketch, *composite);
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());
    tool = revolve_adapter_.revolve_profile_with_holes(
        feature->id(), vertices.value().outer, vertices.value().inner, axis.value().first,
        axis.value().second, angles.start_deg, angles.sweep_deg);
  } else if (const CircleProfile* circle = sketch->find_circle_profile(profile_id)) {
    const Parameter* diameter = document.find_parameter(circle->diameter_parameter());
    if (diameter == nullptr || diameter->type() != ParameterType::Length)
      return Result<std::size_t>::failure(
          geometry_error(profile_id.value(), "revolve circle diameter must be a Length parameter"));
    auto bounded_center = evaluate_bounded_circle_center(workplane_resolver_, workplane.value(),
                                                         *circle, diameter->value());
    if (bounded_center.has_error())
      return Result<std::size_t>::failure(bounded_center.error());
    const double radius = diameter->value().millimeters() / 2.0;
    const Point2 center = circle->center();
    std::vector<ClosedProfileCurveSegment> curves;
    curves.reserve(4U);
    for (std::size_t index = 0U; index < 4U; ++index) {
      const double start = static_cast<double>(index) * k_pi / 2.0;
      const double middle = start + k_pi / 4.0;
      const double end = start + k_pi / 2.0;
      const auto mapped = [&](double angle) {
        return workplane_resolver_.evaluate_point(
            workplane.value(),
            {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)});
      };
      curves.push_back(
          {ClosedProfileCurveKind::CircularArc, mapped(start), mapped(middle), mapped(end)});
    }
    tool = revolve_adapter_.revolve_profile_curves(feature->id(), curves, axis.value().first,
                                                   axis.value().second, angles.start_deg,
                                                   angles.sweep_deg);
  }
  if (tool.has_error())
    return Result<std::size_t>::failure(tool.error());

  auto target = resolve_revolve_target(document, *feature, working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  GeometryShape result = tool.value();
  const FeatureBodyOperationMode mode = feature->body_result_context().operation_mode();
  if (mode != FeatureBodyOperationMode::NewBody) {
    if (target.value() == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(feature->id().value(), "modifying revolve requires a target shape"));
    BodyBooleanOperation operation = BodyBooleanOperation::Add;
    if (mode == FeatureBodyOperationMode::Cut)
      operation = BodyBooleanOperation::Subtract;
    else if (mode == FeatureBodyOperationMode::Intersect)
      operation = BodyBooleanOperation::Intersect;
    auto combined = body_boolean_adapter_.combine(feature->id(), operation, *target.value(),
                                                  std::vector<GeometryShape>{tool.value()});
    if (combined.has_error())
      return Result<std::size_t>::failure(combined.error());
    result = std::move(combined.value());
  }
  auto stored = store_revolve_result(document, *feature, std::move(result), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_sweep(const PartDocument& document,
                                                             FeatureId feature_id,
                                                             ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(validation_error("sweep", "feature id must not be empty"));
  const SweepFeature* feature = document.find_sweep_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "sweep feature must exist in part document"));

  ShapeCache working_cache = shape_cache;
  const PathCurve* trajectory = document.find_path_curve(feature->path());
  if (trajectory == nullptr)
    return Result<std::size_t>::failure(
        geometry_error(feature->path().value(), "sweep trajectory must exist"));
  auto path = resolve_basic_sweep_path(document, *trajectory, working_cache, workplane_resolver_,
                                       "sweep trajectory");
  if (path.has_error())
    return Result<std::size_t>::failure(path.error());

  double twist_angle_deg = 0.0;
  if (feature->twist_parameter().has_value()) {
    const Parameter* twist = document.find_parameter(*feature->twist_parameter());
    if (twist == nullptr || twist->type() != ParameterType::Angle)
      return Result<std::size_t>::failure(geometry_error(
          feature->twist_parameter()->value(), "sweep twist must reference an Angle parameter"));
    twist_angle_deg = twist->value().degrees();
  }
  std::vector<SweepPathSegment> guide_segments;
  const std::vector<SweepPathSegment>* guide = nullptr;
  if (feature->guide_path().has_value()) {
    const PathCurve* guide_path = document.find_path_curve(*feature->guide_path());
    if (guide_path == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(feature->guide_path()->value(), "sweep guide must exist"));
    auto resolved_guide = resolve_basic_sweep_path(document, *guide_path, working_cache,
                                                   workplane_resolver_, "sweep guide");
    if (resolved_guide.has_error())
      return Result<std::size_t>::failure(resolved_guide.error());
    guide_segments = std::move(resolved_guide.value());
    guide = &guide_segments;
  }

  const PathOrientationRule orientation =
      feature->orientation_override().value_or(trajectory->orientation_rule());
  const std::optional<Vector3> fixed_up = feature->orientation_override().has_value()
                                              ? feature->fixed_up_vector_override()
                                              : trajectory->fixed_up_vector();
  Result<GeometryShape> tool = Result<GeometryShape>::failure(
      geometry_error(feature_id.value(), "unsupported sweep profile"));

  if (feature->profile().kind() == SweepProfileKind::OpenPath) {
    const PathCurveId& profile_id = std::get<PathCurveId>(feature->profile().source());
    const PathCurve* profile = document.find_path_curve(profile_id);
    if (profile == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(profile_id.value(), "surface sweep profile PathCurve must exist"));
    auto profile_segments = resolve_basic_sweep_path(document, *profile, working_cache,
                                                     workplane_resolver_, "surface profile");
    if (profile_segments.has_error())
      return Result<std::size_t>::failure(profile_segments.error());
    tool = sweep_adapter_.sweep_open_profile(feature->id(), profile_segments.value(), path.value(),
                                             orientation, fixed_up, twist_angle_deg, guide);
  } else {
    const auto& profile_reference = std::get<ProfileRegionReference>(feature->profile().source());
    const Sketch* sketch = document.find_sketch(profile_reference.sketch());
    if (sketch == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(profile_reference.sketch().value(), "sweep profile sketch must exist"));
    auto workplane = workplane_resolver_.resolve_for_sketch(document, *sketch, working_cache);
    if (workplane.has_error())
      return Result<std::size_t>::failure(workplane.error());
    const ProfileId& profile_id = profile_reference.profile();
    std::vector<ClosedProfileCurveSegment> curves;
    if (const RectangleProfile* rectangle = sketch->find_rectangle_profile(profile_id)) {
      const Parameter* width = document.find_parameter(rectangle->width_parameter());
      const Parameter* height = document.find_parameter(rectangle->height_parameter());
      if (width == nullptr || height == nullptr)
        return Result<std::size_t>::failure(
            geometry_error(profile_id.value(), "sweep rectangle dimensions must exist"));
      auto vertices = map_bounded_local_vertices(
          workplane_resolver_, workplane.value(), profile_id.value(),
          rectangle_local_vertices(*rectangle, width->value(), height->value()));
      if (vertices.has_error())
        return Result<std::size_t>::failure(vertices.error());
      curves = line_profile_curves(vertices.value());
    } else if (const ClosedProfile* closed = sketch->find_closed_profile(profile_id)) {
      auto vertices = evaluate_bounded_closed_profile_vertices(document, workplane_resolver_,
                                                               workplane.value(), *sketch, *closed);
      if (vertices.has_error())
        return Result<std::size_t>::failure(vertices.error());
      curves = line_profile_curves(vertices.value());
    } else if (const ArcClosedProfile* arc = sketch->find_arc_closed_profile(profile_id)) {
      auto resolved = evaluate_bounded_arc_profile_curves(workplane_resolver_, workplane.value(),
                                                          *sketch, *arc);
      if (resolved.has_error())
        return Result<std::size_t>::failure(resolved.error());
      curves = std::move(resolved.value());
    } else if (const CircleProfile* circle = sketch->find_circle_profile(profile_id)) {
      const Parameter* diameter = document.find_parameter(circle->diameter_parameter());
      if (diameter == nullptr || diameter->type() != ParameterType::Length)
        return Result<std::size_t>::failure(
            geometry_error(profile_id.value(), "sweep circle diameter must be a Length parameter"));
      const double radius = diameter->value().millimeters() / 2.0;
      for (std::size_t index = 0U; index < 4U; ++index) {
        const double start = static_cast<double>(index) * k_pi / 2.0;
        const double middle = start + k_pi / 4.0;
        const double end = start + k_pi / 2.0;
        const auto mapped = [&](double angle) {
          return workplane_resolver_.evaluate_point(
              workplane.value(), {circle->center().x + radius * std::cos(angle),
                                  circle->center().y + radius * std::sin(angle)});
        };
        curves.push_back(
            {ClosedProfileCurveKind::CircularArc, mapped(start), mapped(middle), mapped(end)});
      }
    } else {
      return Result<std::size_t>::failure(geometry_error(
          profile_id.value(),
          "Block 81 supports rectangle, line/arc closed, and circle sweep profiles"));
    }
    tool = sweep_adapter_.sweep_closed_profile(feature->id(), curves, path.value(), orientation,
                                               fixed_up, twist_angle_deg, guide);
  }
  if (tool.has_error())
    return Result<std::size_t>::failure(tool.error());

  GeometryShape result = tool.value();
  const auto& context = feature->body_result_context();
  if (context.operation_mode() != FeatureBodyOperationMode::NewBody) {
    if (!context.target_body().has_value())
      return Result<std::size_t>::failure(
          geometry_error(feature_id.value(), "modifying sweep requires a target Body"));
    auto target = resolve_in_place_body_target(document, feature->id(), *context.target_body(),
                                               working_cache);
    if (target.has_error())
      return Result<std::size_t>::failure(target.error());
    BodyBooleanOperation operation = BodyBooleanOperation::Add;
    if (context.operation_mode() == FeatureBodyOperationMode::Cut)
      operation = BodyBooleanOperation::Subtract;
    else if (context.operation_mode() == FeatureBodyOperationMode::Intersect)
      operation = BodyBooleanOperation::Intersect;
    auto combined = body_boolean_adapter_.combine(feature->id(), operation, *target.value(),
                                                  std::vector<GeometryShape>{tool.value()});
    if (combined.has_error())
      return Result<std::size_t>::failure(combined.error());
    result = std::move(combined.value());
  }
  auto stored = store_sweep_result(document, *feature, std::move(result), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_loft(const PartDocument& document,
                                                            FeatureId feature_id,
                                                            ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(validation_error("loft", "feature id must not be empty"));
  const LoftFeature* feature = document.find_loft_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "loft feature must exist in part document"));
  if (feature->sections().size() < 2U)
    return Result<std::size_t>::failure(
        geometry_error(feature_id.value(), "loft requires at least two ordered sections"));
  if (feature->continuity() == LoftContinuity::G2)
    return Result<std::size_t>::failure(
        geometry_error(feature_id.value(),
                       "G2 loft continuity is unsupported without a verified curvature guarantee"));

  ShapeCache working_cache = shape_cache;
  std::vector<SweepPathSegment> center_path_segments;
  const std::vector<SweepPathSegment>* center_path = nullptr;
  if (feature->path_curve().has_value()) {
    const PathCurve* path = document.find_path_curve(*feature->path_curve());
    if (path == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(feature->path_curve()->value(), "loft center path must exist"));
    auto resolved = resolve_basic_sweep_path(document, *path, working_cache, workplane_resolver_,
                                             "loft center path");
    if (resolved.has_error())
      return Result<std::size_t>::failure(resolved.error());
    center_path_segments = std::move(resolved.value());
    center_path = &center_path_segments;
  }
  std::vector<std::vector<SweepPathSegment>> guide_segments;
  guide_segments.reserve(feature->guide_curves().size());
  for (const PathCurveId& guide_id : feature->guide_curves()) {
    const PathCurve* guide = document.find_path_curve(guide_id);
    if (guide == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(guide_id.value(), "loft guide curve must exist"));
    auto resolved = resolve_basic_sweep_path(document, *guide, working_cache, workplane_resolver_,
                                             "loft guide curve");
    if (resolved.has_error())
      return Result<std::size_t>::failure(resolved.error());
    guide_segments.push_back(std::move(resolved.value()));
  }
  Result<GeometryShape> tool = Result<GeometryShape>::failure(
      geometry_error(feature_id.value(), "unsupported two-section loft"));
  const bool open = feature->sections().front().kind() == LoftSectionKind::OpenPath;
  if (open) {
    if (feature->kind() != LoftFeatureKind::LoftSurface)
      return Result<std::size_t>::failure(
          geometry_error(feature_id.value(), "open loft sections require LoftSurface"));
    std::vector<std::vector<SweepPathSegment>> sections;
    sections.reserve(feature->sections().size());
    for (const auto& section : feature->sections()) {
      if (section.rotation_offset().has_value()) {
        const Parameter* rotation = document.find_parameter(*section.rotation_offset());
        if (rotation == nullptr || rotation->type() != ParameterType::Angle ||
            std::abs(rotation->value().degrees()) > k_tolerance)
          return Result<std::size_t>::failure(geometry_error(
              feature_id.value(), "non-zero rotation of open loft sections is unsupported"));
      }
      const PathCurveId& path_id = std::get<PathCurveId>(section.source());
      const PathCurve* path = document.find_path_curve(path_id);
      if (path == nullptr)
        return Result<std::size_t>::failure(
            geometry_error(path_id.value(), "open loft section PathCurve must exist"));
      auto resolved = resolve_basic_sweep_path(document, *path, working_cache, workplane_resolver_,
                                               "open loft section");
      if (resolved.has_error())
        return Result<std::size_t>::failure(resolved.error());
      if (section.flip_normal()) {
        std::reverse(resolved.value().begin(), resolved.value().end());
        for (auto& segment : resolved.value())
          segment = reversed_sweep_segment(segment);
      }
      sections.push_back(std::move(resolved.value()));
    }
    tool = loft_adapter_.loft_open_sections(feature->id(), sections, center_path, guide_segments,
                                            feature->continuity());
  } else {
    std::vector<std::vector<ClosedProfileCurveSegment>> sections;
    sections.reserve(feature->sections().size());
    for (const auto& section : feature->sections()) {
      auto resolved =
          resolve_loft_closed_section(document, section, working_cache, workplane_resolver_);
      if (resolved.has_error())
        return Result<std::size_t>::failure(resolved.error());
      sections.push_back(std::move(resolved.value()));
    }
    tool = loft_adapter_.loft_closed_sections(feature->id(), sections,
                                              feature->kind() != LoftFeatureKind::LoftSurface,
                                              center_path, guide_segments, feature->continuity());
  }
  if (tool.has_error())
    return Result<std::size_t>::failure(tool.error());

  GeometryShape result = tool.value();
  const auto& context = feature->body_result_context();
  if (context.operation_mode() != FeatureBodyOperationMode::NewBody) {
    if (!context.target_body().has_value())
      return Result<std::size_t>::failure(
          geometry_error(feature_id.value(), "modifying loft requires a target Body"));
    auto target = resolve_in_place_body_target(document, feature->id(), *context.target_body(),
                                               working_cache);
    if (target.has_error())
      return Result<std::size_t>::failure(target.error());
    BodyBooleanOperation operation = BodyBooleanOperation::Add;
    if (context.operation_mode() == FeatureBodyOperationMode::Cut)
      operation = BodyBooleanOperation::Subtract;
    else if (context.operation_mode() == FeatureBodyOperationMode::Intersect)
      operation = BodyBooleanOperation::Intersect;
    auto combined = body_boolean_adapter_.combine(feature->id(), operation, *target.value(),
                                                  std::vector<GeometryShape>{tool.value()});
    if (combined.has_error())
      return Result<std::size_t>::failure(combined.error());
    result = std::move(combined.value());
  }
  auto stored = store_loft_result(document, *feature, std::move(result), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_surface_feature(
    const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(
        validation_error("surface_feature", "feature id must not be empty"));
  const SurfaceFeature* feature = document.find_surface_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "surface feature must exist in part document"));
  const SurfaceFeatureKind kind = surface_feature_kind(*feature);
  if (kind == SurfaceFeatureKind::SurfaceStitch || kind == SurfaceFeatureKind::ClosedShellToSolid)
    return Result<std::size_t>::failure(
        geometry_error(feature_id.value(), "surface feature Geometry is deferred beyond Block 90"));

  ShapeCache working_cache = shape_cache;
  Result<GeometryShape> result = Result<GeometryShape>::failure(
      geometry_error(feature_id.value(), "unsupported surface feature"));
  if (kind == SurfaceFeatureKind::BoundarySurface || kind == SurfaceFeatureKind::FillSurface) {
    std::vector<BoundaryCurveReference> authored_boundaries;
    if (const auto* boundary = std::get_if<BoundarySurfaceFeature>(feature))
      authored_boundaries = boundary->boundaries();
    else
      authored_boundaries = std::get<FillSurfaceFeature>(*feature).boundaries();

    std::vector<std::vector<SweepPathSegment>> boundaries;
    boundaries.reserve(authored_boundaries.size());
    for (const auto& authored : authored_boundaries) {
      auto resolved =
          resolve_surface_boundary(document, authored, working_cache, workplane_resolver_);
      if (resolved.has_error())
        return Result<std::size_t>::failure(resolved.error());
      boundaries.push_back(std::move(resolved.value()));
    }
    result = kind == SurfaceFeatureKind::BoundarySurface
                 ? surface_adapter_.make_boundary_surface(feature_id, boundaries)
                 : surface_adapter_.make_fill_surface(feature_id, boundaries);
  } else if (kind == SurfaceFeatureKind::TrimSurface) {
    const auto& trim = std::get<TrimSurfaceFeature>(*feature);
    auto target = resolve_surface_target(document, trim.target(), working_cache, surface_adapter_,
                                         feature_id);
    if (target.has_error())
      return Result<std::size_t>::failure(target.error());
    auto trimming =
        resolve_surface_trimming(document, trim.trimming(), working_cache, workplane_resolver_);
    if (trimming.has_error())
      return Result<std::size_t>::failure(trimming.error());
    result = surface_adapter_.trim_surface(feature_id, target.value(), trimming.value());
  } else {
    const auto& extend = std::get<ExtendSurfaceFeature>(*feature);
    auto target = resolve_surface_target(document, extend.target(), working_cache, surface_adapter_,
                                         feature_id);
    if (target.has_error())
      return Result<std::size_t>::failure(target.error());
    auto boundary =
        resolve_surface_boundary(document, extend.boundary(), working_cache, workplane_resolver_);
    if (boundary.has_error())
      return Result<std::size_t>::failure(boundary.error());
    const Parameter* distance = document.find_parameter(extend.distance_parameter());
    if (distance == nullptr || distance->type() != ParameterType::Length ||
        !distance->value().is_positive_length())
      return Result<std::size_t>::failure(
          geometry_error(extend.distance_parameter().value(),
                         "surface extension distance must resolve to a positive Length parameter"));
    result = surface_adapter_.extend_surface(feature_id, target.value(), boundary.value(),
                                             distance->value().millimeters());
  }
  if (result.has_error())
    return Result<std::size_t>::failure(result.error());
  auto stored = store_surface_result(document, *feature, std::move(result.value()), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_linear_pattern(
    const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(
        validation_error("linear_pattern", "feature id must not be empty"));
  const LinearPatternFeature* feature = document.find_linear_pattern_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "linear pattern feature must exist in part document"));
  const Parameter* count = document.find_parameter(feature->count_parameter());
  const Parameter* extent = document.find_parameter(feature->extent_parameter());
  if (count == nullptr || count->type() != ParameterType::Count ||
      count->value().count_value() < 2U)
    return Result<std::size_t>::failure(
        geometry_error(feature->id().value(), "linear pattern count must resolve to at least 2"));
  if (extent == nullptr || extent->type() != ParameterType::Length)
    return Result<std::size_t>::failure(geometry_error(
        feature->id().value(), "linear pattern extent must resolve to a Length parameter"));

  ShapeCache working_cache = shape_cache;
  auto target = resolve_pattern_target(document, *feature, working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  auto sources = resolve_pattern_sources(*feature, working_cache, target.value());
  if (sources.has_error())
    return Result<std::size_t>::failure(sources.error());
  auto direction =
      resolve_axis_reference(document, feature->direction(), working_cache, workplane_resolver_);
  if (direction.has_error())
    return Result<std::size_t>::failure(direction.error());
  Vector3 signed_direction = normalized(direction.value().second);
  if (feature->direction_sign() == PatternDirectionSign::Negative)
    signed_direction = {-signed_direction.x, -signed_direction.y, -signed_direction.z};
  const std::size_t instance_count = count->value().count_value();
  double spacing_mm = extent->value().millimeters();
  if (feature->extent_mode() == LinearPatternExtentMode::TotalExtent)
    spacing_mm /= static_cast<double>(instance_count - 1U);
  auto instances = linear_pattern_adapter_.generate_instances(
      feature->id(), sources.value(), signed_direction, instance_count, spacing_mm);
  if (instances.has_error())
    return Result<std::size_t>::failure(instances.error());

  std::vector<GeometryShape> tools;
  const auto& context = feature->body_result_context();
  if (context.operation_mode() == FeatureBodyOperationMode::NewBody) {
    tools = std::move(instances.value());
  } else {
    if (target.value() == nullptr)
      return Result<std::size_t>::failure(geometry_error(
          feature->id().value(), "modifying linear pattern requires a target shape"));
    const std::size_t source_count = feature->sources().size();
    tools.reserve(instances.value().size());
    for (std::size_t index = 0U; index < instances.value().size(); ++index) {
      const std::size_t instance_index = index / source_count;
      const auto& source = feature->sources().at(index % source_count);
      bool original_in_target = false;
      if (instance_index == 0U && context.target_body().has_value()) {
        if (source.kind() == PatternSourceKind::Body)
          original_in_target = std::get<BodyId>(source.source()) == *context.target_body();
        else
          original_in_target = feature_produces_body(
              document, std::get<FeatureId>(source.source()).value(), *context.target_body());
      }
      if (!original_in_target)
        tools.push_back(instances.value().at(index));
    }
  }

  auto tool_union = fuse_pattern_shapes(*feature, tools, body_boolean_adapter_);
  if (tool_union.has_error())
    return Result<std::size_t>::failure(tool_union.error());
  GeometryShape result = tool_union.value();
  if (context.operation_mode() != FeatureBodyOperationMode::NewBody) {
    BodyBooleanOperation operation = BodyBooleanOperation::Add;
    if (context.operation_mode() == FeatureBodyOperationMode::Cut)
      operation = BodyBooleanOperation::Subtract;
    else if (context.operation_mode() == FeatureBodyOperationMode::Intersect)
      operation = BodyBooleanOperation::Intersect;
    auto combined = body_boolean_adapter_.combine(feature->id(), operation, *target.value(),
                                                  {tool_union.value()});
    if (combined.has_error())
      return Result<std::size_t>::failure(combined.error());
    result = std::move(combined.value());
  }
  auto stored = store_pattern_result(document, *feature, std::move(result), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_circular_pattern(
    const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(
        validation_error("circular_pattern", "feature id must not be empty"));
  const CircularPatternFeature* feature = document.find_circular_pattern_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(validation_error(
        feature_id.value(), "circular pattern feature must exist in part document"));
  const Parameter* count = document.find_parameter(feature->count_parameter());
  if (count == nullptr || count->type() != ParameterType::Count ||
      count->value().count_value() < 2U)
    return Result<std::size_t>::failure(
        geometry_error(feature->id().value(), "circular pattern count must resolve to at least 2"));
  if (!feature->equal_spacing())
    return Result<std::size_t>::failure(
        geometry_error(feature->id().value(), "circular pattern requires equal spacing"));

  ShapeCache working_cache = shape_cache;
  auto target = resolve_pattern_target(document, *feature, working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  auto sources = resolve_pattern_sources(*feature, working_cache, target.value());
  if (sources.has_error())
    return Result<std::size_t>::failure(sources.error());
  auto axis = resolve_axis_reference(document, feature->axis(), working_cache, workplane_resolver_);
  if (axis.has_error())
    return Result<std::size_t>::failure(axis.error());
  auto instances = circular_pattern_adapter_.generate_instances(
      feature->id(), sources.value(), axis.value().first, axis.value().second,
      count->value().count_value(), feature->total_angle_deg());
  if (instances.has_error())
    return Result<std::size_t>::failure(instances.error());

  std::vector<GeometryShape> tools;
  const auto& context = feature->body_result_context();
  if (context.operation_mode() == FeatureBodyOperationMode::NewBody) {
    tools = std::move(instances.value());
  } else {
    if (target.value() == nullptr)
      return Result<std::size_t>::failure(geometry_error(
          feature->id().value(), "modifying circular pattern requires a target shape"));
    const std::size_t source_count = feature->sources().size();
    tools.reserve(instances.value().size());
    for (std::size_t index = 0U; index < instances.value().size(); ++index) {
      const std::size_t instance_index = index / source_count;
      const auto& source = feature->sources().at(index % source_count);
      bool original_in_target = false;
      if (instance_index == 0U && context.target_body().has_value()) {
        if (source.kind() == PatternSourceKind::Body)
          original_in_target = std::get<BodyId>(source.source()) == *context.target_body();
        else
          original_in_target = feature_produces_body(
              document, std::get<FeatureId>(source.source()).value(), *context.target_body());
      }
      if (!original_in_target)
        tools.push_back(instances.value().at(index));
    }
  }

  auto tool_union = fuse_pattern_shapes(*feature, tools, body_boolean_adapter_);
  if (tool_union.has_error())
    return Result<std::size_t>::failure(tool_union.error());
  GeometryShape result = tool_union.value();
  if (context.operation_mode() != FeatureBodyOperationMode::NewBody) {
    BodyBooleanOperation operation = BodyBooleanOperation::Add;
    if (context.operation_mode() == FeatureBodyOperationMode::Cut)
      operation = BodyBooleanOperation::Subtract;
    else if (context.operation_mode() == FeatureBodyOperationMode::Intersect)
      operation = BodyBooleanOperation::Intersect;
    auto combined = body_boolean_adapter_.combine(feature->id(), operation, *target.value(),
                                                  {tool_union.value()});
    if (combined.has_error())
      return Result<std::size_t>::failure(combined.error());
    result = std::move(combined.value());
  }
  auto stored = store_pattern_result(document, *feature, std::move(result), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_mirror(const PartDocument& document,
                                                              FeatureId feature_id,
                                                              ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(validation_error("mirror", "feature id must not be empty"));
  const MirrorFeature* feature = document.find_mirror_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "mirror feature must exist in part document"));

  ShapeCache working_cache = shape_cache;
  auto target = resolve_pattern_target(document, *feature, working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  auto sources = resolve_mirror_sources(*feature, working_cache, target.value());
  if (sources.has_error())
    return Result<std::size_t>::failure(sources.error());
  auto plane =
      resolve_mirror_plane(document, feature->mirror_plane(), working_cache, workplane_resolver_);
  if (plane.has_error())
    return Result<std::size_t>::failure(plane.error());
  auto reflected = mirror_adapter_.reflect_sources(feature->id(), sources.value(),
                                                   plane.value().first, plane.value().second);
  if (reflected.has_error())
    return Result<std::size_t>::failure(reflected.error());
  auto tool_union = fuse_pattern_shapes(*feature, reflected.value(), body_boolean_adapter_);
  if (tool_union.has_error())
    return Result<std::size_t>::failure(tool_union.error());

  GeometryShape result = tool_union.value();
  const auto& context = feature->body_result_context();
  if (context.operation_mode() != FeatureBodyOperationMode::NewBody) {
    if (target.value() == nullptr)
      return Result<std::size_t>::failure(
          geometry_error(feature->id().value(), "modifying mirror requires a target shape"));
    BodyBooleanOperation operation = BodyBooleanOperation::Add;
    if (context.operation_mode() == FeatureBodyOperationMode::Cut)
      operation = BodyBooleanOperation::Subtract;
    else if (context.operation_mode() == FeatureBodyOperationMode::Intersect)
      operation = BodyBooleanOperation::Intersect;
    auto combined = body_boolean_adapter_.combine(feature->id(), operation, *target.value(),
                                                  std::vector<GeometryShape>{tool_union.value()});
    if (combined.has_error())
      return Result<std::size_t>::failure(combined.error());
    result = std::move(combined.value());
  }

  auto stored = store_pattern_result(document, *feature, std::move(result), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

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
  if (feature->direction() == ExtrudeDirection::Path ||
      !feature->extrude_intent().is_historical_additive_default() ||
      (feature->body_result_context().has_value() &&
       feature->body_result_context()->operation_mode() != FeatureBodyOperationMode::NewBody))
    return execute_richer_extrude(document, *feature, shape_cache);

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

    auto resolved_workplane =
        workplane_resolver_.resolve_for_sketch(document, *sketch, shape_cache);
    if (resolved_workplane.has_error()) {
      return Result<std::size_t>::failure(resolved_workplane.error());
    }

    auto vertices = map_bounded_local_vertices(
        workplane_resolver_, resolved_workplane.value(), rectangle.id().value(),
        rectangle_local_vertices(rectangle, width->value(), height->value()));
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());

    auto shape = closed_profile_adapter_.make_extruded_profile(
        vertices.value(),
        extrude_direction_vector(resolved_workplane.value(), feature->direction()),
        thickness->value());
    if (shape.has_error())
      return Result<std::size_t>::failure(shape.error());

    return store_feature_result(document, *feature, std::move(shape.value()), shape_cache);
  }

  if (has_exactly_one_closed_profile(*sketch) || has_no_profiles(*sketch) ||
      has_exactly_one_arc_closed_profile(*sketch) ||
      has_exactly_one_composite_closed_profile(*sketch)) {
    auto resolved_workplane =
        workplane_resolver_.resolve_for_sketch(document, *sketch, shape_cache);
    if (resolved_workplane.has_error()) {
      return Result<std::size_t>::failure(resolved_workplane.error());
    }
    const Vector3 direction =
        extrude_direction_vector(resolved_workplane.value(), feature->direction());

    if (has_exactly_one_arc_closed_profile(*sketch)) {
      auto curves =
          evaluate_bounded_arc_profile_curves(workplane_resolver_, resolved_workplane.value(),
                                              *sketch, sketch->arc_closed_profiles().front());
      if (curves.has_error())
        return Result<std::size_t>::failure(curves.error());

      auto shape = closed_profile_adapter_.make_extruded_profile_from_curves(
          curves.value(), direction, thickness->value());
      if (shape.has_error())
        return Result<std::size_t>::failure(shape.error());

      return store_feature_result(document, *feature, std::move(shape.value()), shape_cache);
    }

    if (has_exactly_one_composite_closed_profile(*sketch)) {
      auto vertices = evaluate_bounded_composite_profile_vertices(
          document, workplane_resolver_, resolved_workplane.value(), *sketch,
          sketch->composite_closed_profiles().front());
      if (vertices.has_error())
        return Result<std::size_t>::failure(vertices.error());

      auto shape = closed_profile_adapter_.make_extruded_profile_with_holes(
          vertices.value().outer, vertices.value().inner, direction, thickness->value());
      if (shape.has_error())
        return Result<std::size_t>::failure(shape.error());

      return store_feature_result(document, *feature, std::move(shape.value()), shape_cache);
    }

    Result<std::vector<Point3>> vertices =
        has_exactly_one_closed_profile(*sketch)
            ? evaluate_bounded_closed_profile_vertices(document, workplane_resolver_,
                                                       resolved_workplane.value(), *sketch,
                                                       sketch->closed_profiles().front())
            : evaluate_bounded_detected_region_vertices(document, workplane_resolver_,
                                                        resolved_workplane.value(), *sketch);
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());

    auto shape = closed_profile_adapter_.make_extruded_profile(vertices.value(), direction,
                                                               thickness->value());
    if (shape.has_error())
      return Result<std::size_t>::failure(shape.error());

    return store_feature_result(document, *feature, std::move(shape.value()), shape_cache);
  }

  return Result<std::size_t>::failure(validation_error(
      sketch->id().value(),
      "additive extrude requires exactly one rectangle, closed profile, arc closed profile, "
      "composite closed profile, or detected simple region"));
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
  if (feature->direction() == ExtrudeDirection::Path ||
      !feature->extrude_intent().is_historical_subtractive_default() ||
      (feature->body_result_context().has_value() &&
       feature->body_result_context()->operation_mode() != FeatureBodyOperationMode::Cut))
    return execute_richer_extrude(document, *feature, shape_cache);

  const Sketch* sketch = document.find_sketch(feature->input_sketch());
  if (sketch == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        feature->id().value(), "feature input sketch must exist in part document"));
  }

  if (!has_exactly_one_circle_profile(*sketch) && !has_exactly_one_closed_profile(*sketch) &&
      !has_exactly_one_arc_closed_profile(*sketch) &&
      !has_exactly_one_composite_closed_profile(*sketch) &&
      !has_exactly_one_circular_hole_pattern(*sketch) && !has_no_profiles(*sketch)) {
    return Result<std::size_t>::failure(validation_error(
        sketch->id().value(),
        "subtractive extrude requires exactly one circle, closed profile, arc closed profile, "
        "composite closed profile, circular hole pattern, or detected simple region"));
  }

  const GeometryShape* target = nullptr;
  std::string target_object_id = feature->target_feature().value();
  if (feature->body_result_context().has_value()) {
    const BodyId& target_body = *feature->body_result_context()->target_body();
    if (document.find_body(target_body) == nullptr) {
      return Result<std::size_t>::failure(
          validation_error(target_body.value(), "target body must exist in part document"));
    }
    target = shape_cache.find_body_shape(target_body);
    target_object_id = target_body.value();
  } else {
    target = shape_cache.find_feature_shape(feature->target_feature());
  }
  if (target == nullptr) {
    return Result<std::size_t>::failure(geometry_error(
        target_object_id, feature->body_result_context().has_value()
                              ? "target body shape must exist in the shape cache"
                              : "target feature shape must exist in the shape cache"));
  }

  auto resolved_workplane = workplane_resolver_.resolve_for_sketch(document, *sketch, shape_cache);
  if (resolved_workplane.has_error()) {
    return Result<std::size_t>::failure(resolved_workplane.error());
  }
  const Vector3 direction =
      extrude_direction_vector(resolved_workplane.value(), feature->direction());

  if (has_exactly_one_circle_profile(*sketch)) {
    const CircleProfile& circle = sketch->circle_profiles().front();
    const Parameter* diameter = find_required_parameter(document, circle.diameter_parameter());
    if (diameter == nullptr) {
      return Result<std::size_t>::failure(
          validation_error(circle.diameter_parameter().value(),
                           "circle diameter parameter must exist in part document"));
    }

    auto global_center = evaluate_bounded_circle_center(
        workplane_resolver_, resolved_workplane.value(), circle, diameter->value());
    if (global_center.has_error())
      return Result<std::size_t>::failure(global_center.error());

    auto shape = circular_cut_adapter_.cut_circular_hole_along_axis(
        *target, diameter->value(), global_center.value(), direction);
    if (shape.has_error())
      return Result<std::size_t>::failure(shape.error());

    return store_feature_result(document, *feature, std::move(shape.value()), shape_cache);
  }

  if (has_exactly_one_circular_hole_pattern(*sketch)) {
    const CircularHolePattern& pattern = sketch->circular_hole_patterns().front();
    const Parameter* radius = find_required_parameter(document, pattern.radius_parameter());
    if (radius == nullptr || radius->type() != ParameterType::Length) {
      return Result<std::size_t>::failure(
          validation_error(pattern.radius_parameter().value(),
                           "pattern radius parameter must be a length in part document"));
    }
    const Parameter* count = find_required_parameter(document, pattern.count_parameter());
    if (count == nullptr || count->type() != ParameterType::Count) {
      return Result<std::size_t>::failure(
          validation_error(pattern.count_parameter().value(),
                           "pattern count parameter must be a count in part document"));
    }
    const Parameter* diameter =
        find_required_parameter(document, pattern.hole_diameter_parameter());
    if (diameter == nullptr || diameter->type() != ParameterType::Length) {
      return Result<std::size_t>::failure(
          validation_error(pattern.hole_diameter_parameter().value(),
                           "pattern hole diameter parameter must be a length in part document"));
    }

    const std::size_t hole_count = count->value().count_value();
    const double pattern_radius = radius->value().millimeters();
    const double angle_offset_rad = pattern.angle_offset_deg() * k_pi / 180.0;

    GeometryShape current = *target;
    for (std::size_t index = 0U; index < hole_count; ++index) {
      const double angle = angle_offset_rad + 2.0 * k_pi * static_cast<double>(index) /
                                                  static_cast<double>(hole_count);
      const Point2 local_center{pattern.center().x + pattern_radius * std::cos(angle),
                                pattern.center().y + pattern_radius * std::sin(angle)};

      auto global_center = evaluate_bounded_circle_center(
          workplane_resolver_, resolved_workplane.value(),
          pattern.id().value() + ".hole." + std::to_string(index), local_center, diameter->value());
      if (global_center.has_error())
        return Result<std::size_t>::failure(global_center.error());

      auto shape = circular_cut_adapter_.cut_circular_hole_along_axis(
          current, diameter->value(), global_center.value(), direction);
      if (shape.has_error())
        return Result<std::size_t>::failure(shape.error());
      current = std::move(shape.value());
    }

    return store_feature_result(document, *feature, std::move(current), shape_cache);
  }

  if (has_exactly_one_arc_closed_profile(*sketch)) {
    auto curves =
        evaluate_bounded_arc_profile_curves(workplane_resolver_, resolved_workplane.value(),
                                            *sketch, sketch->arc_closed_profiles().front());
    if (curves.has_error())
      return Result<std::size_t>::failure(curves.error());

    auto shape =
        closed_profile_adapter_.cut_profile_curves_through_all(*target, curves.value(), direction);
    if (shape.has_error())
      return Result<std::size_t>::failure(shape.error());

    return store_feature_result(document, *feature, std::move(shape.value()), shape_cache);
  }

  if (has_exactly_one_composite_closed_profile(*sketch)) {
    auto vertices = evaluate_bounded_composite_profile_vertices(
        document, workplane_resolver_, resolved_workplane.value(), *sketch,
        sketch->composite_closed_profiles().front());
    if (vertices.has_error())
      return Result<std::size_t>::failure(vertices.error());

    auto shape = closed_profile_adapter_.cut_profile_with_holes_through_all(
        *target, vertices.value().outer, vertices.value().inner, direction);
    if (shape.has_error())
      return Result<std::size_t>::failure(shape.error());

    return store_feature_result(document, *feature, std::move(shape.value()), shape_cache);
  }

  Result<std::vector<Point3>> vertices =
      has_exactly_one_closed_profile(*sketch)
          ? evaluate_bounded_closed_profile_vertices(document, workplane_resolver_,
                                                     resolved_workplane.value(), *sketch,
                                                     sketch->closed_profiles().front())
          : evaluate_bounded_detected_region_vertices(document, workplane_resolver_,
                                                      resolved_workplane.value(), *sketch);
  if (vertices.has_error())
    return Result<std::size_t>::failure(vertices.error());

  auto shape =
      closed_profile_adapter_.cut_profile_through_all(*target, vertices.value(), direction);
  if (shape.has_error())
    return Result<std::size_t>::failure(shape.error());

  return store_feature_result(document, *feature, std::move(shape.value()), shape_cache);
}

Result<std::size_t> GeometryRecomputeExecutor::execute_fillet(const PartDocument& document,
                                                              FeatureId feature_id,
                                                              ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(validation_error("fillet", "feature id must not be empty"));
  const FilletFeature* feature = document.find_fillet_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "fillet feature must exist in part document"));

  const Parameter* radius = document.find_parameter(feature->radius_parameter());
  if (radius == nullptr || radius->type() != ParameterType::Length ||
      radius->value().millimeters() <= k_tolerance)
    return Result<std::size_t>::failure(validation_error(
        feature->radius_parameter().value(), "fillet radius must be a positive length"));

  ShapeCache working_cache = shape_cache;
  auto target =
      resolve_in_place_body_target(document, feature->id(), feature->target_body(), working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  auto result = fillet_adapter_.apply(document, *feature, *target.value(), working_cache,
                                      radius->value().millimeters());
  if (result.has_error())
    return Result<std::size_t>::failure(result.error());
  auto stored = store_fillet_result(document, *feature, std::move(result.value()), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_chamfer(const PartDocument& document,
                                                               FeatureId feature_id,
                                                               ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(
        validation_error("chamfer", "feature id must not be empty"));
  const ChamferFeature* feature = document.find_chamfer_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "chamfer feature must exist in part document"));

  const Parameter* first = document.find_parameter(feature->first_parameter());
  if (first == nullptr || first->type() != ParameterType::Length ||
      first->value().millimeters() <= k_tolerance)
    return Result<std::size_t>::failure(validation_error(
        feature->first_parameter().value(), "first chamfer parameter must be a positive length"));

  std::optional<double> second_value;
  if (feature->second_parameter().has_value()) {
    const Parameter* second = document.find_parameter(*feature->second_parameter());
    const ParameterType expected = feature->mode() == ChamferMode::DistanceAngle
                                       ? ParameterType::Angle
                                       : ParameterType::Length;
    if (second == nullptr || second->type() != expected)
      return Result<std::size_t>::failure(validation_error(
          feature->second_parameter()->value(), "second chamfer parameter has the wrong type"));
    second_value = expected == ParameterType::Angle ? second->value().degrees()
                                                    : second->value().millimeters();
    if (*second_value <= k_tolerance || (expected == ParameterType::Angle && *second_value >= 90.0))
      return Result<std::size_t>::failure(validation_error(
          feature->second_parameter()->value(), "second chamfer parameter is outside its range"));
  }

  ShapeCache working_cache = shape_cache;
  auto target =
      resolve_in_place_body_target(document, feature->id(), feature->target_body(), working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  auto result = chamfer_adapter_.apply(document, *feature, *target.value(), working_cache,
                                       first->value().millimeters(), second_value);
  if (result.has_error())
    return Result<std::size_t>::failure(result.error());
  auto stored = store_chamfer_result(document, *feature, std::move(result.value()), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_shell(const PartDocument& document,
                                                             FeatureId feature_id,
                                                             ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(validation_error("shell", "feature id must not be empty"));
  const ShellFeature* feature = document.find_shell_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "shell feature must exist in part document"));
  const Parameter* thickness = document.find_parameter(feature->thickness_parameter());
  if (thickness == nullptr || thickness->type() != ParameterType::Length ||
      !std::isfinite(thickness->value().millimeters()) ||
      thickness->value().millimeters() <= k_tolerance)
    return Result<std::size_t>::failure(
        validation_error(feature->thickness_parameter().value(),
                         "shell thickness must be a positive finite length"));

  ShapeCache working_cache = shape_cache;
  auto target =
      resolve_in_place_body_target(document, feature->id(), feature->target_body(), working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  auto result = shell_adapter_.apply(document, *feature, *target.value(), working_cache,
                                     thickness->value().millimeters());
  if (result.has_error())
    return Result<std::size_t>::failure(result.error());
  auto stored = store_shell_result(document, *feature, std::move(result.value()), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_draft(const PartDocument& document,
                                                             FeatureId feature_id,
                                                             ShapeCache& shape_cache) const {
  if (feature_id.empty())
    return Result<std::size_t>::failure(validation_error("draft", "feature id must not be empty"));
  const DraftFeature* feature = document.find_draft_feature(feature_id);
  if (feature == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "draft feature must exist in part document"));
  const Parameter* angle = document.find_parameter(feature->angle_parameter());
  if (angle == nullptr || angle->type() != ParameterType::Angle ||
      !std::isfinite(angle->value().degrees()) ||
      std::abs(angle->value().degrees()) <= k_tolerance ||
      std::abs(angle->value().degrees()) >= 90.0)
    return Result<std::size_t>::failure(
        validation_error(feature->angle_parameter().value(),
                         "draft angle must be finite, non-zero and |angle| < 90 degrees"));

  ShapeCache working_cache = shape_cache;
  auto target =
      resolve_in_place_body_target(document, feature->id(), feature->target_body(), working_cache);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());
  auto direction = resolve_axis_reference(document, feature->pull_direction(), working_cache,
                                          workplane_resolver_);
  if (direction.has_error())
    return Result<std::size_t>::failure(direction.error());
  auto plane =
      resolve_mirror_plane(document, feature->neutral_plane(), working_cache, workplane_resolver_);
  if (plane.has_error())
    return Result<std::size_t>::failure(plane.error());
  auto result = draft_adapter_.apply(document, *feature, *target.value(), working_cache,
                                     direction.value().second, plane.value().first,
                                     plane.value().second, angle->value().degrees());
  if (result.has_error())
    return Result<std::size_t>::failure(result.error());
  auto stored = store_draft_result(document, *feature, std::move(result.value()), working_cache);
  if (stored.has_error())
    return stored;
  shape_cache = std::move(working_cache);
  return stored;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_body_boolean(const PartDocument& document,
                                                                    FeatureId feature_id,
                                                                    ShapeCache& shape_cache) const {
  if (feature_id.empty()) {
    return Result<std::size_t>::failure(
        validation_error("body_boolean", "feature id must not be empty"));
  }
  const BodyBooleanFeature* feature = document.find_body_boolean_feature(feature_id);
  if (feature == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), "body boolean feature must exist in part document"));
  }

  ShapeCache working_cache = shape_cache;
  auto target = resolve_boolean_target_input(document, working_cache, *feature);
  if (target.has_error())
    return Result<std::size_t>::failure(target.error());

  std::vector<GeometryShape> tools;
  tools.reserve(feature->tool_bodies().size());
  for (const BodyId& tool_body : feature->tool_bodies()) {
    auto tool = resolve_cached_body_input(document, working_cache, tool_body, "tool");
    if (tool.has_error())
      return Result<std::size_t>::failure(tool.error());
    tools.push_back(std::move(tool.value()));
  }

  auto result =
      body_boolean_adapter_.combine(feature->id(), feature->operation(), target.value(), tools);
  if (result.has_error())
    return Result<std::size_t>::failure(result.error());

  if (!feature->keep_tool_bodies()) {
    for (const BodyId& tool_body : feature->tool_bodies()) {
      auto removed = working_cache.remove_body_shape(tool_body);
      if (removed.has_error())
        return Result<std::size_t>::failure(removed.error());
    }
  }
  auto stored_feature = working_cache.store_feature_shape(feature->id(), result.value());
  if (stored_feature.has_error())
    return stored_feature;
  auto stored_body = working_cache.store_body_shape(feature->effective_result_body(), feature->id(),
                                                    std::move(result.value()));
  if (stored_body.has_error())
    return stored_body;

  working_cache.clear_final_shape();
  shape_cache = std::move(working_cache);
  return stored_body;
}

Result<std::size_t> GeometryRecomputeExecutor::execute_body_transform(
    const PartDocument& document, BodyTransformId transform_id, ShapeCache& shape_cache) const {
  if (transform_id.empty())
    return Result<std::size_t>::failure(
        validation_error("body_transform", "transform id must not be empty"));
  const BodyTransform* transform = document.find_body_transform(transform_id);
  if (transform == nullptr)
    return Result<std::size_t>::failure(
        validation_error(transform_id.value(), "body transform must exist in part document"));

  ShapeCache working_cache = shape_cache;
  auto input = resolve_transform_input(document, working_cache, *transform);
  if (input.has_error())
    return Result<std::size_t>::failure(input.error());
  auto frame = resolve_transform_frame(document, working_cache, *transform,
                                       input.value().cumulative_transform);
  if (frame.has_error())
    return Result<std::size_t>::failure(frame.error());

  GeometryAffineTransform operation;
  Result<GeometryShape> transformed = Result<GeometryShape>::failure(
      geometry_error(transform->id().value(), "unsupported body transform kind"));
  if (transform->kind() == BodyTransformKind::Translate) {
    const Vector3 offset = frame_vector(frame.value(), transform->translation_mm());
    operation = GeometryAffineTransform::translation(offset);
    transformed = body_transform_adapter_.translate(input.value().shape, offset);
  } else if (transform->kind() == BodyTransformKind::Rotate) {
    auto axis = resolve_rotation_axis(document, *transform, frame.value(), working_cache);
    if (axis.has_error())
      return Result<std::size_t>::failure(axis.error());
    operation = GeometryAffineTransform::rotation(axis.value().first, axis.value().second,
                                                  transform->angle_deg());
    transformed = body_transform_adapter_.rotate(input.value().shape, axis.value().first,
                                                 axis.value().second, transform->angle_deg());
  } else {
    const Point3 center = frame_point(frame.value(), transform->scale_center());
    operation = GeometryAffineTransform::uniform_scale(center, transform->scale_factor());
    transformed = body_transform_adapter_.uniform_scale(input.value().shape, center,
                                                        transform->scale_factor());
  }
  if (transformed.has_error())
    return Result<std::size_t>::failure(transformed.error());

  const GeometryAffineTransform cumulative =
      input.value().cumulative_transform.followed_by(operation);
  const FeatureId cache_id(transform->id().value());
  auto feature_result = working_cache.store_feature_shape(cache_id, transformed.value());
  if (feature_result.has_error())
    return feature_result;
  auto body_result =
      working_cache.store_body_shape(transform->body(), cache_id, std::move(transformed.value()));
  if (body_result.has_error())
    return body_result;
  auto state_result =
      working_cache.store_body_transform_state(transform->id(), transform->body(), cumulative);
  if (state_result.has_error())
    return state_result;

  for (const SketchOwnership& ownership : document.sketch_ownerships()) {
    if (ownership.owning_body() != transform->body())
      continue;
    const auto advanced_reference = [&](std::string_view reference_id, bool applies) {
      GeometryAffineTransform prior = GeometryAffineTransform::identity();
      if (input.value().previous_transform.has_value()) {
        if (const auto* cached = working_cache.find_reference_transform(
                reference_id, *input.value().previous_transform))
          prior = cached->cumulative_transform;
      }
      return applies ? prior.followed_by(operation) : prior;
    };

    auto stored = working_cache.store_reference_transform(
        ownership.sketch().value(), transform->body(), transform->id(),
        advanced_reference(ownership.sketch().value(), transform->apply_to_owned_sketches()));
    if (stored.has_error())
      return stored;

    const Sketch* sketch = document.find_sketch(ownership.sketch());
    stored = working_cache.store_reference_transform(
        sketch->workplane().value(), transform->body(), transform->id(),
        advanced_reference(sketch->workplane().value(),
                           transform->apply_to_owned_construction_geometry()));
    if (stored.has_error())
      return stored;
    for (const ProjectedSketchPoint& projected : sketch->projected_points()) {
      if (projected.source() != ProjectedSketchPointSource::ConstructionPoint)
        continue;
      stored = working_cache.store_reference_transform(
          projected.construction_point().value(), transform->body(), transform->id(),
          advanced_reference(projected.construction_point().value(),
                             transform->apply_to_owned_construction_geometry()));
      if (stored.has_error())
        return stored;
    }
    for (const ProjectedSketchLine& projected : sketch->projected_lines()) {
      if (projected.source() != ProjectedSketchLineSource::ConstructionLine)
        continue;
      stored = working_cache.store_reference_transform(
          projected.construction_line().value(), transform->body(), transform->id(),
          advanced_reference(projected.construction_line().value(),
                             transform->apply_to_owned_construction_geometry()));
      if (stored.has_error())
        return stored;
    }
  }

  if (document.bodies().size() == 1U && document.bodies().front().kind() == BodyKind::Solid) {
    const GeometryShape* body_shape = working_cache.find_body_shape(transform->body());
    auto final_result = working_cache.set_final_shape(cache_id, *body_shape);
    if (final_result.has_error())
      return final_result;
  } else {
    working_cache.clear_final_shape();
  }
  shape_cache = std::move(working_cache);
  return body_result;
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
  ShapeCache working_cache = shape_cache;
  ShapeCache& execution_cache = document.bodies().empty() ? shape_cache : working_cache;
  GeometryRecomputeSummary summary;
  for (const RecomputeStep& step : plan.steps()) {
    const Feature* feature = document.find_feature(FeatureId(step.node_id));
    const RevolveFeature* revolve = document.find_revolve_feature(FeatureId(step.node_id));
    const SweepFeature* sweep = document.find_sweep_feature(FeatureId(step.node_id));
    const LoftFeature* loft = document.find_loft_feature(FeatureId(step.node_id));
    const SurfaceFeature* surface = document.find_surface_feature(FeatureId(step.node_id));
    const LinearPatternFeature* linear_pattern =
        document.find_linear_pattern_feature(FeatureId(step.node_id));
    const CircularPatternFeature* circular_pattern =
        document.find_circular_pattern_feature(FeatureId(step.node_id));
    const MirrorFeature* mirror = document.find_mirror_feature(FeatureId(step.node_id));
    const FilletFeature* fillet = document.find_fillet_feature(FeatureId(step.node_id));
    const ChamferFeature* chamfer = document.find_chamfer_feature(FeatureId(step.node_id));
    const ShellFeature* shell = document.find_shell_feature(FeatureId(step.node_id));
    const DraftFeature* draft = document.find_draft_feature(FeatureId(step.node_id));
    const BodyBooleanFeature* body_boolean =
        document.find_body_boolean_feature(FeatureId(step.node_id));
    const BodyTransform* body_transform =
        document.find_body_transform(BodyTransformId(step.node_id));
    if (feature == nullptr && revolve == nullptr && sweep == nullptr && loft == nullptr &&
        surface == nullptr && linear_pattern == nullptr && circular_pattern == nullptr &&
        mirror == nullptr && fillet == nullptr && chamfer == nullptr && shell == nullptr &&
        draft == nullptr && body_boolean == nullptr && body_transform == nullptr)
      continue;

    const FeatureId executable_id = feature != nullptr            ? feature->id()
                                    : revolve != nullptr          ? revolve->id()
                                    : sweep != nullptr            ? sweep->id()
                                    : loft != nullptr             ? loft->id()
                                    : surface != nullptr          ? surface_feature_id(*surface)
                                    : linear_pattern != nullptr   ? linear_pattern->id()
                                    : circular_pattern != nullptr ? circular_pattern->id()
                                    : mirror != nullptr           ? mirror->id()
                                    : fillet != nullptr           ? fillet->id()
                                    : chamfer != nullptr          ? chamfer->id()
                                    : shell != nullptr            ? shell->id()
                                    : draft != nullptr            ? draft->id()
                                    : body_boolean != nullptr
                                        ? body_boolean->id()
                                        : FeatureId(body_transform->id().value());
    auto removed_stale_shape = execution_cache.remove_feature_shape(executable_id);
    if (removed_stale_shape.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(removed_stale_shape.error());
    }

    auto executed =
        feature != nullptr   ? execute_feature(document, *feature, execution_cache)
        : revolve != nullptr ? execute_revolve(document, revolve->id(), execution_cache)
        : sweep != nullptr   ? execute_sweep(document, sweep->id(), execution_cache)
        : loft != nullptr    ? execute_loft(document, loft->id(), execution_cache)
        : surface != nullptr
            ? execute_surface_feature(document, surface_feature_id(*surface), execution_cache)
        : linear_pattern != nullptr
            ? execute_linear_pattern(document, linear_pattern->id(), execution_cache)
        : circular_pattern != nullptr
            ? execute_circular_pattern(document, circular_pattern->id(), execution_cache)
        : mirror != nullptr  ? execute_mirror(document, mirror->id(), execution_cache)
        : fillet != nullptr  ? execute_fillet(document, fillet->id(), execution_cache)
        : chamfer != nullptr ? execute_chamfer(document, chamfer->id(), execution_cache)
        : shell != nullptr   ? execute_shell(document, shell->id(), execution_cache)
        : draft != nullptr   ? execute_draft(document, draft->id(), execution_cache)
        : body_boolean != nullptr
            ? execute_body_boolean(document, body_boolean->id(), execution_cache)
            : execute_body_transform(document, body_transform->id(), execution_cache);
    if (executed.has_error())
      return Result<GeometryRecomputeSummary>::failure(executed.error());

    ++summary.executed_feature_count;
  }

  if (!document.bodies().empty())
    shape_cache = std::move(working_cache);
  return Result<GeometryRecomputeSummary>::success(summary);
}

Result<GeometryRecomputeSummary>
GeometryRecomputeExecutor::execute_document(const PartDocument& document,
                                            ShapeCache& shape_cache) const {
  ShapeCache working_cache = shape_cache;
  ShapeCache& execution_cache = document.bodies().empty() ? shape_cache : working_cache;
  GeometryRecomputeSummary summary;
  std::vector<std::string> executable_ids;
  if (document.bodies().empty()) {
    executable_ids.reserve(document.features().size());
    for (const Feature& feature : document.features())
      executable_ids.push_back(feature.id().value());
  } else {
    auto order = document.dependency_graph().topological_order();
    if (order.has_error())
      return Result<GeometryRecomputeSummary>::failure(order.error());
    executable_ids = std::move(order.value());
  }

  for (const std::string& executable_id : executable_ids) {
    const Feature* feature = document.find_feature(FeatureId(executable_id));
    const RevolveFeature* revolve = document.find_revolve_feature(FeatureId(executable_id));
    const SweepFeature* sweep = document.find_sweep_feature(FeatureId(executable_id));
    const LoftFeature* loft = document.find_loft_feature(FeatureId(executable_id));
    const SurfaceFeature* surface = document.find_surface_feature(FeatureId(executable_id));
    const LinearPatternFeature* linear_pattern =
        document.find_linear_pattern_feature(FeatureId(executable_id));
    const CircularPatternFeature* circular_pattern =
        document.find_circular_pattern_feature(FeatureId(executable_id));
    const MirrorFeature* mirror = document.find_mirror_feature(FeatureId(executable_id));
    const FilletFeature* fillet = document.find_fillet_feature(FeatureId(executable_id));
    const ChamferFeature* chamfer = document.find_chamfer_feature(FeatureId(executable_id));
    const ShellFeature* shell = document.find_shell_feature(FeatureId(executable_id));
    const DraftFeature* draft = document.find_draft_feature(FeatureId(executable_id));
    const BodyBooleanFeature* body_boolean =
        document.find_body_boolean_feature(FeatureId(executable_id));
    const BodyTransform* body_transform =
        document.find_body_transform(BodyTransformId(executable_id));
    if (feature == nullptr && revolve == nullptr && sweep == nullptr && loft == nullptr &&
        surface == nullptr && linear_pattern == nullptr && circular_pattern == nullptr &&
        mirror == nullptr && fillet == nullptr && chamfer == nullptr && shell == nullptr &&
        draft == nullptr && body_boolean == nullptr && body_transform == nullptr)
      continue;

    const FeatureId feature_id = feature != nullptr            ? feature->id()
                                 : revolve != nullptr          ? revolve->id()
                                 : sweep != nullptr            ? sweep->id()
                                 : loft != nullptr             ? loft->id()
                                 : surface != nullptr          ? surface_feature_id(*surface)
                                 : linear_pattern != nullptr   ? linear_pattern->id()
                                 : circular_pattern != nullptr ? circular_pattern->id()
                                 : mirror != nullptr           ? mirror->id()
                                 : fillet != nullptr           ? fillet->id()
                                 : chamfer != nullptr          ? chamfer->id()
                                 : shell != nullptr            ? shell->id()
                                 : draft != nullptr            ? draft->id()
                                 : body_boolean != nullptr
                                     ? body_boolean->id()
                                     : FeatureId(body_transform->id().value());
    auto removed_stale_shape = execution_cache.remove_feature_shape(feature_id);
    if (removed_stale_shape.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(removed_stale_shape.error());
    }

    auto executed =
        feature != nullptr   ? execute_feature(document, *feature, execution_cache)
        : revolve != nullptr ? execute_revolve(document, revolve->id(), execution_cache)
        : sweep != nullptr   ? execute_sweep(document, sweep->id(), execution_cache)
        : loft != nullptr    ? execute_loft(document, loft->id(), execution_cache)
        : surface != nullptr
            ? execute_surface_feature(document, surface_feature_id(*surface), execution_cache)
        : linear_pattern != nullptr
            ? execute_linear_pattern(document, linear_pattern->id(), execution_cache)
        : circular_pattern != nullptr
            ? execute_circular_pattern(document, circular_pattern->id(), execution_cache)
        : mirror != nullptr  ? execute_mirror(document, mirror->id(), execution_cache)
        : fillet != nullptr  ? execute_fillet(document, fillet->id(), execution_cache)
        : chamfer != nullptr ? execute_chamfer(document, chamfer->id(), execution_cache)
        : shell != nullptr   ? execute_shell(document, shell->id(), execution_cache)
        : draft != nullptr   ? execute_draft(document, draft->id(), execution_cache)
        : body_boolean != nullptr
            ? execute_body_boolean(document, body_boolean->id(), execution_cache)
            : execute_body_transform(document, body_transform->id(), execution_cache);
    if (executed.has_error())
      return Result<GeometryRecomputeSummary>::failure(executed.error());

    ++summary.executed_feature_count;
  }

  if (!document.bodies().empty())
    shape_cache = std::move(working_cache);
  return Result<GeometryRecomputeSummary>::success(summary);
}

} // namespace blcad::geometry
