#pragma once
#include "blcad/core/sketch_3d_solver.hpp"
#include "blcad/geometry/shape_cache.hpp"
namespace blcad::geometry {
class StepImportTopologyContext;
// Returned callbacks borrow fixed document/cache/import authorities for one
// synchronous solve. Candidate conversion never publishes to PartDocument or
// touches downstream feature history.
class SpatialConstraintGeometry {
public:
  [[nodiscard]] SpatialSolverGeometry queries(const PartDocument&, const ShapeCache&,
                                              StepImportTopologyContext* = nullptr) const;
};
} // namespace blcad::geometry
