#pragma once
#include "variable_fillet_adapter.hpp"
namespace blcad::geometry::detail {
Result<NativeFilletResult> face_fillet(const PartDocument&, const FilletFeature&,
                                       const TopoDS_Shape&, const ShapeCache&,
                                       StepImportTopologyContext*, const std::vector<FaceLineage>&);
}
