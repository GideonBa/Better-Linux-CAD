#pragma once
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "spatial_constraint_fixture.hpp"
namespace blcad::test::surface_offset {
using spatial_fixture::checked;
inline void length(PartDocument& p, const std::string& id, double value) {
  (void)checked(p.add_parameter(
      checked(Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
}
inline SurfaceReference plane() {
  return checked(SurfaceReference::create_semantic_face(
      checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))));
}
inline geometry::ShapeCache cache() {
  return checked(geometry::ShapeCache::create(ShapeCacheId("surface.acceptance")));
}
inline std::string saved(const PartDocument& p) {
  return checked(serialize_part_document_to_json(p));
}
inline FeatureBodyResultContext
context(FeatureBodyOperationMode mode = FeatureBodyOperationMode::NewBody,
        BodyId target = BodyId("base.body")) {
  return checked(FeatureBodyResultContext::create(
      mode, mode == FeatureBodyOperationMode::NewBody ? std::nullopt : std::optional(target),
      mode == FeatureBodyOperationMode::NewBody ? std::optional(BodyId("thick.body"))
                                                : std::nullopt));
}
inline PartDocument thickened() {
  auto p = spatial_fixture::part();
  length(p, "tolerance", 1e-6);
  (void)checked(p.set_parameter_value(ParameterId("distance"), checked(Quantity::length_mm(3))));
  (void)checked(p.add_body(checked(Body::create(BodyId("thick.body"), "Thick", BodyKind::Solid))));
  const SurfaceOffsetParameters parameters{ParameterId("distance"), ParameterId("tolerance"),
                                           SurfaceOffsetSide::Symmetric,
                                           SurfaceOffsetApproximation::ExactAnalytic, std::nullopt};
  (void)checked(p.add_surface_feature(checked(SurfaceOffsetFeature::create_thicken(
      FeatureId("thick"), "Thicken", plane(), parameters, context()))));
  return p;
}
} // namespace blcad::test::surface_offset
