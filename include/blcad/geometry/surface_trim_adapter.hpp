#pragma once
#include "blcad/geometry/surface_adapter.hpp"
namespace blcad::geometry {
class SurfaceTrimAdapter {
public:
  [[nodiscard]] Result<SurfaceExtensionFrame> extension_frame(FeatureId, const GeometryShape&,
                                                              const std::vector<SweepPathSegment>&,
                                                              double tolerance_mm) const;
  [[nodiscard]] Result<GeometryShape> trim(FeatureId, const GeometryShape&,
                                           const std::vector<SweepPathSegment>&, SurfaceTrimSide,
                                           SurfaceTrimProjection, std::optional<Vector3> direction,
                                           double tolerance_mm,
                                           const std::string& contour_identity) const;
  [[nodiscard]] Result<GeometryShape>
  extend(FeatureId, const GeometryShape&, const std::vector<SweepPathSegment>&, double distance_mm,
         SurfaceExtendMode, double tolerance_mm, std::optional<double> maximum_deviation_mm,
         const std::string& boundary_identity) const;
};
} // namespace blcad::geometry
