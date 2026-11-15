#pragma once
#include "blcad/core/part_document.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
namespace blcad::geometry::detail {
struct NativeLoftNetwork {
  bool closed;
  std::vector<std::vector<TopoDS_Edge>> sections;
  std::vector<TopoDS_Edge> rails;
  std::vector<TopoDS_Wire> contours;
};
[[nodiscard]] Result<NativeLoftNetwork>
resolve_constrained_loft_network(const PartDocument&, const LoftFeature&, const ShapeCache&,
                                 StepImportTopologyContext*, double);
} // namespace blcad::geometry::detail
