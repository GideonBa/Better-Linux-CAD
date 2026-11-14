#pragma once
#include "blcad/core/sketch_3d_solver.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include <functional>
#include <variant>
namespace blcad::geometry {
class StepImportTopologyContext;
struct SpatialCompleteGeometryScope {};
struct SpatialHistoryPrefixScope {
  std::size_t count;
};
// Existing feature resolvers already validate their current working-cache input. This scope
// admits that active evaluation (or its sealed input), without pretending it is a complete result.
struct SpatialFeatureInputScope {};
using SpatialGeometryScope =
    std::variant<SpatialCompleteGeometryScope, SpatialHistoryPrefixScope, SpatialFeatureInputScope>;
using SpatialExternalSketchResolver = std::function<Result<Sketch3D>(Sketch3DId)>;
// Returned callbacks borrow fixed document/cache/import authorities for one
// synchronous solve. Candidate conversion never publishes to PartDocument or
// touches downstream feature history.
class SpatialConstraintGeometry {
public:
  [[nodiscard]] SpatialSolverGeometry queries(const PartDocument&, const ShapeCache&,
                                              StepImportTopologyContext* = nullptr,
                                              SpatialGeometryScope = SpatialCompleteGeometryScope{},
                                              SpatialExternalSketchResolver = {}) const;
};
// Derived solved geometry, not a Part edit: parameter changes replay from authored seeds. The
// returned owner retains source bindings/constraints; rejected solutions are never substituted.
[[nodiscard]] Result<Sketch3D>
resolve_spatial_sketch_geometry(const PartDocument&, Sketch3DId, const ShapeCache&,
                                StepImportTopologyContext* = nullptr,
                                SpatialGeometryScope = SpatialCompleteGeometryScope{});
} // namespace blcad::geometry
