#pragma once
#include "blcad/core/part_document.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
namespace blcad::geometry {
class ShapeCache;
class StepImportTopologyContext;
class SketchDrivenAdapter {
public:
  Result<GeometryShape> apply(const PartDocument&, const SketchDrivenFeature&, const GeometryShape&,
                              const ShapeCache&, StepImportTopologyContext* = nullptr) const;

private:
  Result<GeometryShape> apply_emboss(const PartDocument&, const SketchDrivenFeature&,
                                     const GeometryShape&, const ShapeCache&,
                                     StepImportTopologyContext*) const;
};
} // namespace blcad::geometry
