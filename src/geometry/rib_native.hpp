#pragma once
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "face_lineage.hpp"
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <string>
namespace blcad::geometry::detail {
struct RibNative {
  TopoDS_Shape shape;
  std::vector<FaceLineage> lineage;
};
struct RibSpine {
  TopoDS_Wire wire;
  std::vector<std::pair<TopoDS_Edge, std::string>> edges;
  bool closed;
};
std::vector<RibSpine> rib_spines(const PartDocument&, const RibWebIntent&,
                                 const ResolvedWorkplane&);
RibNative rib_tool(const RibSpine&, const RibWebIntent&, const FeatureId&, const ResolvedWorkplane&,
                   double, double, double, double, gp_Vec);
RibNative join_rib(const RibNative&, const RibNative&, const FeatureId&,
                   bool reject_crossing = false);
void scope_rib_fragments(RibNative&, const std::vector<FaceLineage>&, const FeatureId&);
double rib_volume(const TopoDS_Shape&);
void validate_rib_solid(const TopoDS_Shape&);
} // namespace blcad::geometry::detail
