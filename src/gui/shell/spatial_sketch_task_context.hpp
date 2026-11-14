#pragma once
#include "blcad/core/part_document.hpp"
#include "blcad/geometry/spatial_constraint_geometry.hpp"
#include <memory>

namespace blcad::gui {
class GuiDocumentSession;
struct SpatialSketchTaskResources {
  std::vector<Parameter> parameters;
  std::vector<DatumAxis> axes;
  std::size_t generation{};
  [[nodiscard]] Result<std::size_t> add_to(PartDocument&) const;
};
// Immutable source authority for a single local task revision. Numeric candidate evaluation
// never rebuilds the feature graph or borrows a downstream result as its own support.
class SpatialSketchTaskContext {
public:
  [[nodiscard]] static Result<std::shared_ptr<SpatialSketchTaskContext>>
  create(const GuiDocumentSession&, Sketch3DId, const SpatialSketchTaskResources&);
  [[nodiscard]] SpatialSolveResult solve(const Sketch3D&, const SpatialSolverOptions& = {}) const;
  [[nodiscard]] Result<SpatialCurveJet> jet(const Sketch3D&, const SpatialConstraintTarget&,
                                            double) const;
  [[nodiscard]] const PartDocument& part() const noexcept {
    return part_;
  }

private:
  SpatialSketchTaskContext(PartDocument, geometry::ShapeCache,
                           std::unique_ptr<geometry::StepImportTopologyContext>,
                           std::optional<std::size_t>);
  [[nodiscard]] geometry::SpatialGeometryScope scope() const;
  [[nodiscard]] SpatialSolverGeometry queries() const;
  PartDocument part_;
  geometry::ShapeCache cache_;
  std::unique_ptr<geometry::StepImportTopologyContext> imports_;
  std::optional<std::size_t> prefix_;
};
} // namespace blcad::gui
