#pragma once
#include "blcad/core/surface_feature.hpp"
#include "blcad/geometry/shape_cache.hpp"
namespace blcad::geometry {
class StepImportTopologyContext;
struct SurfaceNormalFrame {
  Point3 origin;
  Vector3 normal;
};
class SurfaceSelectionGeometry {
public:
  [[nodiscard]] static Result<SurfaceNormalFrame> normal_frame(const GeometryShape&);
  // owned_override is the real effective predecessor for an in-place operation.
  [[nodiscard]] static Result<GeometryShape> resolve(const PartDocument&, const ShapeCache&,
                                                     const SurfaceReference&, FeatureId consumer,
                                                     double tolerance_mm,
                                                     StepImportTopologyContext* = nullptr,
                                                     const GeometryShape* owned_override = nullptr);
  [[nodiscard]] static GeometryShape tag_single_patch(const GeometryShape&, FeatureId,
                                                      std::string authored_role);
};
} // namespace blcad::geometry
