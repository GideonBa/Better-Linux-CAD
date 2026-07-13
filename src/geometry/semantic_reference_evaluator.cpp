#include "blcad/geometry/semantic_reference_evaluator.hpp"

#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Point3 translated(Point3 point, Vector3 direction, double distance) noexcept {
  return Point3{point.x + direction.x * distance, point.y + direction.y * distance,
                point.z + direction.z * distance};
}

struct RectangularExtrudeCorners {
  Point3 bottom_front_right;
  Point3 bottom_front_left;
  Point3 bottom_back_right;
  Point3 bottom_back_left;
  Point3 top_front_right;
  Point3 top_front_left;
  Point3 top_back_right;
  Point3 top_back_left;
};

[[nodiscard]] Result<RectangularExtrudeCorners>
resolve_rectangular_additive_extrude_corners(const PartDocument& document, FeatureId feature_id) {
  const Feature* feature = document.find_feature(feature_id);
  if (feature == nullptr) {
    return Result<RectangularExtrudeCorners>::failure(validation_error(
        feature_id.value(), "semantic reference source feature must exist in part document"));
  }

  if (feature->type() != FeatureType::AdditiveExtrude) {
    return Result<RectangularExtrudeCorners>::failure(validation_error(
        feature_id.value(), "semantic reference source feature must be an additive extrude"));
  }

  const Sketch* sketch = document.find_sketch(feature->input_sketch());
  if (sketch == nullptr) {
    return Result<RectangularExtrudeCorners>::failure(validation_error(
        feature->id().value(), "semantic reference source feature input sketch must exist"));
  }

  if (sketch->rectangle_profiles().size() != 1U || !sketch->circle_profiles().empty() ||
      !sketch->closed_profiles().empty()) {
    return Result<RectangularExtrudeCorners>::failure(
        validation_error(sketch->id().value(), "semantic edge and vertex evaluation requires a "
                                               "source sketch with exactly one rectangle profile"));
  }

  const RectangleProfile& rectangle = sketch->rectangle_profiles().front();
  const Parameter* width = document.find_parameter(rectangle.width_parameter());
  const Parameter* height = document.find_parameter(rectangle.height_parameter());
  const Parameter* depth = document.find_parameter(feature->length_parameter());
  if (width == nullptr || height == nullptr || depth == nullptr) {
    return Result<RectangularExtrudeCorners>::failure(validation_error(
        feature->id().value(), "semantic reference source feature parameters must exist"));
  }

  const WorkplaneResolver resolver;
  auto workplane = resolver.resolve(document, sketch->workplane());
  if (workplane.has_error()) {
    return Result<RectangularExtrudeCorners>::failure(workplane.error());
  }

  const Point2 center = rectangle.center();
  const double half_width = width->value().millimeters() / 2.0;
  const double half_height = height->value().millimeters() / 2.0;
  const double depth_mm = depth->value().millimeters();

  const Point3 bottom_front_right = resolver.evaluate_point(
      workplane.value(), Point2{center.x + half_width, center.y + half_height});
  const Point3 bottom_front_left = resolver.evaluate_point(
      workplane.value(), Point2{center.x - half_width, center.y + half_height});
  const Point3 bottom_back_right = resolver.evaluate_point(
      workplane.value(), Point2{center.x + half_width, center.y - half_height});
  const Point3 bottom_back_left = resolver.evaluate_point(
      workplane.value(), Point2{center.x - half_width, center.y - half_height});

  return Result<RectangularExtrudeCorners>::success(RectangularExtrudeCorners{
      bottom_front_right, bottom_front_left, bottom_back_right, bottom_back_left,
      translated(bottom_front_right, workplane.value().normal, depth_mm),
      translated(bottom_front_left, workplane.value().normal, depth_mm),
      translated(bottom_back_right, workplane.value().normal, depth_mm),
      translated(bottom_back_left, workplane.value().normal, depth_mm)});
}

[[nodiscard]] Point3 select_vertex(const RectangularExtrudeCorners& corners,
                                   SemanticVertex vertex) noexcept {
  switch (vertex) {
  case SemanticVertex::TopFrontRight:
    return corners.top_front_right;
  case SemanticVertex::TopFrontLeft:
    return corners.top_front_left;
  case SemanticVertex::TopBackRight:
    return corners.top_back_right;
  case SemanticVertex::TopBackLeft:
    return corners.top_back_left;
  case SemanticVertex::BottomFrontRight:
    return corners.bottom_front_right;
  case SemanticVertex::BottomFrontLeft:
    return corners.bottom_front_left;
  case SemanticVertex::BottomBackRight:
    return corners.bottom_back_right;
  case SemanticVertex::BottomBackLeft:
    return corners.bottom_back_left;
  }

  return corners.top_front_right;
}

[[nodiscard]] ResolvedSemanticEdge make_edge(SemanticEdgeReference reference, Point3 start,
                                             Point3 end) {
  return ResolvedSemanticEdge{std::move(reference), start, end};
}

[[nodiscard]] ResolvedSemanticEdge select_edge(const RectangularExtrudeCorners& corners,
                                               SemanticEdgeReference reference) {
  switch (reference.edge()) {
  case SemanticEdge::TopFront:
    return make_edge(std::move(reference), corners.top_front_left, corners.top_front_right);
  case SemanticEdge::TopBack:
    return make_edge(std::move(reference), corners.top_back_left, corners.top_back_right);
  case SemanticEdge::TopRight:
    return make_edge(std::move(reference), corners.top_front_right, corners.top_back_right);
  case SemanticEdge::TopLeft:
    return make_edge(std::move(reference), corners.top_front_left, corners.top_back_left);
  case SemanticEdge::BottomFront:
    return make_edge(std::move(reference), corners.bottom_front_left, corners.bottom_front_right);
  case SemanticEdge::BottomBack:
    return make_edge(std::move(reference), corners.bottom_back_left, corners.bottom_back_right);
  case SemanticEdge::BottomRight:
    return make_edge(std::move(reference), corners.bottom_front_right, corners.bottom_back_right);
  case SemanticEdge::BottomLeft:
    return make_edge(std::move(reference), corners.bottom_front_left, corners.bottom_back_left);
  case SemanticEdge::FrontRight:
    return make_edge(std::move(reference), corners.bottom_front_right, corners.top_front_right);
  case SemanticEdge::FrontLeft:
    return make_edge(std::move(reference), corners.bottom_front_left, corners.top_front_left);
  case SemanticEdge::BackRight:
    return make_edge(std::move(reference), corners.bottom_back_right, corners.top_back_right);
  case SemanticEdge::BackLeft:
    return make_edge(std::move(reference), corners.bottom_back_left, corners.top_back_left);
  }

  return make_edge(std::move(reference), corners.top_front_left, corners.top_front_right);
}

} // namespace

namespace {
[[nodiscard]] const GeometryAffineTransform*
source_transform(const PartDocument& document, const ShapeCache& cache,
                 const FeatureId& feature_id) noexcept {
  const Feature* feature = document.find_feature(feature_id);
  if (feature == nullptr || !feature->body_result_context().has_value())
    return nullptr;
  const BodyId body = feature->body_result_context()->effective_produced_body();
  const CachedBodyTransformState* state = cache.find_latest_body_transform_state(body);
  return state == nullptr ? nullptr : &state->cumulative_transform;
}
} // namespace

Result<ResolvedSemanticEdge>
SemanticReferenceEvaluator::resolve_edge(const PartDocument& document,
                                         SemanticEdgeReference reference) const {
  auto corners = resolve_rectangular_additive_extrude_corners(document, reference.source_feature());
  if (corners.has_error()) {
    return Result<ResolvedSemanticEdge>::failure(corners.error());
  }

  return Result<ResolvedSemanticEdge>::success(select_edge(corners.value(), std::move(reference)));
}

Result<ResolvedSemanticEdge>
SemanticReferenceEvaluator::resolve_edge(const PartDocument& document,
                                         SemanticEdgeReference reference,
                                         const ShapeCache& shape_cache) const {
  const FeatureId source = reference.source_feature();
  auto resolved = resolve_edge(document, std::move(reference));
  if (resolved.has_error())
    return resolved;
  if (const GeometryAffineTransform* transform = source_transform(document, shape_cache, source)) {
    resolved.value().start = transform->transform_point(resolved.value().start);
    resolved.value().end = transform->transform_point(resolved.value().end);
  }
  return resolved;
}

Result<ResolvedSemanticVertex>
SemanticReferenceEvaluator::resolve_vertex(const PartDocument& document,
                                           SemanticVertexReference reference) const {
  auto corners = resolve_rectangular_additive_extrude_corners(document, reference.source_feature());
  if (corners.has_error()) {
    return Result<ResolvedSemanticVertex>::failure(corners.error());
  }

  return Result<ResolvedSemanticVertex>::success(
      ResolvedSemanticVertex{reference, select_vertex(corners.value(), reference.vertex())});
}

Result<ResolvedSemanticVertex>
SemanticReferenceEvaluator::resolve_vertex(const PartDocument& document,
                                           SemanticVertexReference reference,
                                           const ShapeCache& shape_cache) const {
  const FeatureId source = reference.source_feature();
  auto resolved = resolve_vertex(document, std::move(reference));
  if (resolved.has_error())
    return resolved;
  if (const GeometryAffineTransform* transform = source_transform(document, shape_cache, source))
    resolved.value().position = transform->transform_point(resolved.value().position);
  return resolved;
}

} // namespace blcad::geometry
