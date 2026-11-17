#pragma once
#include "blcad/geometry/workplane_resolver.hpp"
#include "emboss_outline.hpp"
#include <Geom_Surface.hxx>
#include <TopoDS_Face.hxx>
#include <gp_GTrsf2d.hxx>
namespace blcad::geometry::detail {
struct EmbossChart {
  Handle(Geom_Surface) base, depth;
  gp_GTrsf2d transform;
  bool periodic;
  double reference_radius;
};
EmbossChart emboss_chart(const EmbossIntent&, const std::vector<TopoDS_Face>&,
                         const ResolvedWorkplane&, double, double);
void map_emboss_outlines(std::vector<EmbossOutlineLoop>&, EmbossChart&);
std::vector<EmbossOutlineLoop> emboss_profile_outlines(const PartDocument&,
                                                       const ProfileRegionReference&,
                                                       const ResolvedWorkplane&);
} // namespace blcad::geometry::detail
