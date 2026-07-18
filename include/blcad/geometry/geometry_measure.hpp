#pragma once

#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"

#include <variant>

namespace blcad::geometry {

using GeometryMeasureTarget =
    std::variant<Point3, AssemblyPointTargetDescriptor, AssemblyLineTargetDescriptor,
                 AssemblyPlanarTargetDescriptor, AssemblyCircularEdgeTargetDescriptor,
                 AssemblyCylindricalSurfaceTargetDescriptor>;

struct GeometryDistanceMeasurement {
  double distance_mm{0.0};
  Point3 witness_a;
  Point3 witness_b;
};

struct GeometryAngleMeasurement {
  double angle_deg{0.0};
};

struct GeometryRadialMeasurement {
  double radius_mm{0.0};
  double diameter_mm{0.0};
};

// Read-only analytic queries over already-derived typed Geometry descriptors.
// Point, linear-edge, and planar-face pairs use exact Euclidean projections;
// circle/cylinder radius remains the authority of its resolved descriptor.
class GeometryMeasureQuery {
public:
  [[nodiscard]] Result<GeometryDistanceMeasurement> distance(const GeometryMeasureTarget& a,
                                                             const GeometryMeasureTarget& b) const;
  [[nodiscard]] Result<GeometryAngleMeasurement> angle(const GeometryMeasureTarget& a,
                                                       const GeometryMeasureTarget& b) const;
  [[nodiscard]] Result<GeometryRadialMeasurement>
  radius_diameter(const GeometryMeasureTarget& target) const;
};

} // namespace blcad::geometry
