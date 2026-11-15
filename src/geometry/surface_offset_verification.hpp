#pragma once
#include "blcad/geometry/surface_offset_adapter.hpp"
#include <TopoDS_Face.hxx>
#include <vector>
namespace blcad::geometry::detail {
struct OffsetSkinPair {
  TopoDS_Face source;
  TopoDS_Face first_skin;
  TopoDS_Face last_skin;
  double first_distance;
  double last_distance;
};
// Independent native ray/derivative checks on actual trimmed skin descendants.
[[nodiscard]] SurfaceDistanceReport verify_offset_skins(const std::vector<OffsetSkinPair>&,
                                                        double requested, double budget,
                                                        SurfaceDistanceCriterion, bool approximate);
void validate_offset_source(const TopoDS_Shape&, double first, double last, double tolerance,
                            bool approximate);
} // namespace blcad::geometry::detail
