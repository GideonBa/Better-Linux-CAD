#include "blcad/geometry/body_result_inspector.hpp"

#include <utility>

namespace blcad::geometry {

Result<PartBodyInspection> BodyResultInspector::inspect(const PartDocument& document,
                                                        const ShapeCache& shape_cache) const {
  for (const CachedBodyShape& cached : shape_cache.body_shapes()) {
    if (document.find_body(cached.body_id) == nullptr) {
      return Result<PartBodyInspection>::failure(Error::validation(
          cached.body_id.value(), "cached body result must belong to the inspected part document"));
    }
  }

  PartBodyInspection inspection;
  inspection.bodies.reserve(document.bodies().size());
  for (const Body& body : document.bodies()) {
    BodyResultInspection result{body.id(),         body.name(),  body.kind(),
                                body.visibility(), std::nullopt, std::nullopt};
    if (body.kind() == BodyKind::Solid)
      ++inspection.solid_body_count;
    else
      ++inspection.surface_body_count;

    if (const CachedBodyShape* cached = shape_cache.find_body_result(body.id())) {
      result.source_feature_id = cached->source_feature_id;
      result.shape_summary = shape_inspector_.summarize(cached->shape);
    }
    inspection.bodies.push_back(std::move(result));
  }

  return Result<PartBodyInspection>::success(std::move(inspection));
}

Result<GeometryShape>
BodyResultInspector::resolve_compatible_final_shape(const PartDocument& document,
                                                    const ShapeCache& shape_cache) const {
  if (document.bodies().empty()) {
    const GeometryShape* shape = shape_cache.final_shape();
    if (shape == nullptr) {
      return Result<GeometryShape>::failure(
          Error::geometry(document.id().value(), "historical part has no final shape result"));
    }
    return Result<GeometryShape>::success(*shape);
  }

  if (document.bodies().size() != 1U) {
    return Result<GeometryShape>::failure(Error::geometry(
        document.id().value(), "historical final shape is ambiguous for a multi-body part"));
  }

  const Body& body = document.bodies().front();
  if (body.kind() != BodyKind::Solid) {
    return Result<GeometryShape>::failure(Error::geometry(
        body.id().value(), "historical final shape requires exactly one solid body"));
  }

  const GeometryShape* shape = shape_cache.find_body_shape(body.id());
  if (shape == nullptr) {
    return Result<GeometryShape>::failure(
        Error::geometry(body.id().value(), "solid body has no shape result in the shape cache"));
  }
  return Result<GeometryShape>::success(*shape);
}

} // namespace blcad::geometry
