#pragma once
#include "blcad/core/edge_treatment_feature.hpp"
#include "face_lineage.hpp"
#include <TopoDS_Shape.hxx>
namespace blcad {
class PartDocument;
}
namespace blcad::geometry {
class ShapeCache;
class StepImportTopologyContext;
namespace detail {
struct NativeFilletResult {
  TopoDS_Shape shape;
  std::vector<FaceLineage> lineage;
};
Result<NativeFilletResult> variable_fillet(const PartDocument&, const FilletFeature&,
                                           const TopoDS_Shape&, const ShapeCache&,
                                           StepImportTopologyContext*,
                                           const std::vector<FaceLineage>&);
} // namespace detail
} // namespace blcad::geometry
