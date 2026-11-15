#pragma once
#include "blcad/core/surface_feature.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include <optional>
namespace blcad::geometry {
enum class SurfaceDistanceCriterion { OffsetDistance, WallThickness, SkinSeparation };
struct SurfaceDistanceReport {
  SurfaceDistanceCriterion criterion = SurfaceDistanceCriterion::OffsetDistance;
  double requested_mm = 0;
  double minimum_mm = 0;
  double maximum_mm = 0;
  double maximum_error_mm = 0;
  double permitted_error_mm = 0;
  std::size_t stations = 0;
  bool approximate = false;
  bool before_body_operation = true;
};
struct SurfaceConstructionResult {
  GeometryShape shape;
  SurfaceDistanceReport verification;
};
class SurfaceOffsetAdapter {
public:
  [[nodiscard]] static std::optional<SurfaceDistanceReport> verification(const GeometryShape&);
  [[nodiscard]] static GeometryShape with_verification(const GeometryShape&, SurfaceDistanceReport);
  [[nodiscard]] Result<SurfaceConstructionResult>
  construct(FeatureId, const GeometryShape& source, SurfaceFeatureKind, double distance_mm,
            SurfaceOffsetSide, double tolerance_mm, SurfaceOffsetApproximation,
            std::optional<double> maximum_deviation_mm = std::nullopt) const;
};
} // namespace blcad::geometry
