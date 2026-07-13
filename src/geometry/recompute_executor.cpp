#include "blcad/geometry/recompute_executor.hpp"

#include "blcad/geometry/dimension_driven_profile_resolver.hpp"
#include "blcad/geometry/reference_generated_profile_resolver.hpp"
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

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Error geometry_error(std::string object_id, std::string message) {
  return Error::geometry(std::move(object_id), std::move(message));
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

  return Result<GeometryShape>::failure(
      geometry_error(body_id.value(),
                     std::string(role) + " body shape must exist in the shape cache"));
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
  if (feature->body_result_context().has_value() &&
      feature->body_result_context()->operation_mode() != FeatureBodyOperationMode::NewBody) {
    return Result<std::size_t>::failure(geometry_error(
        feature->id().value(),
        "additive extrude body execution currently requires new_body operation mode"));
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

    auto resolved_workplane = workplane_resolver_.resolve_for_sketch(document, *sketch);
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
    auto resolved_workplane = workplane_resolver_.resolve_for_sketch(document, *sketch);
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
  if (feature->body_result_context().has_value() &&
      feature->body_result_context()->operation_mode() != FeatureBodyOperationMode::Cut) {
    return Result<std::size_t>::failure(
        geometry_error(feature->id().value(),
                       "subtractive extrude body execution currently requires cut operation mode"));
  }

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

  auto resolved_workplane = workplane_resolver_.resolve_for_sketch(document, *sketch);
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

Result<std::size_t> GeometryRecomputeExecutor::execute_body_boolean(
    const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const {
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

  auto result = body_boolean_adapter_.combine(feature->id(), feature->operation(), target.value(),
                                              tools);
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
    const BodyBooleanFeature* body_boolean =
        document.find_body_boolean_feature(FeatureId(step.node_id));
    if (feature == nullptr && body_boolean == nullptr)
      continue;

    const FeatureId& executable_id = feature != nullptr ? feature->id() : body_boolean->id();
    auto removed_stale_shape = execution_cache.remove_feature_shape(executable_id);
    if (removed_stale_shape.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(removed_stale_shape.error());
    }

    auto executed = feature != nullptr
                        ? execute_feature(document, *feature, execution_cache)
                        : execute_body_boolean(document, body_boolean->id(), execution_cache);
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
    const BodyBooleanFeature* body_boolean =
        document.find_body_boolean_feature(FeatureId(executable_id));
    if (feature == nullptr && body_boolean == nullptr)
      continue;

    const FeatureId& feature_id = feature != nullptr ? feature->id() : body_boolean->id();
    auto removed_stale_shape = execution_cache.remove_feature_shape(feature_id);
    if (removed_stale_shape.has_error()) {
      return Result<GeometryRecomputeSummary>::failure(removed_stale_shape.error());
    }

    auto executed = feature != nullptr
                        ? execute_feature(document, *feature, execution_cache)
                        : execute_body_boolean(document, body_boolean->id(), execution_cache);
    if (executed.has_error())
      return Result<GeometryRecomputeSummary>::failure(executed.error());

    ++summary.executed_feature_count;
  }

  if (!document.bodies().empty())
    shape_cache = std::move(working_cache);
  return Result<GeometryRecomputeSummary>::success(summary);
}

} // namespace blcad::geometry
