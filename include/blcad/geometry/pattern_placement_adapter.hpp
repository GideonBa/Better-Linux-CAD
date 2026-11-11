#pragma once

#include "blcad/geometry/hole_instance_geometry.hpp"
#include <gp_Trsf.hxx>

namespace blcad::geometry {
struct PatternPlacementTransform {
  PatternOccurrenceIndex index;
  gp_Trsf transform;
};
class PatternPlacementAdapter {
public:
  [[nodiscard]] static Result<GeometryShape> place(const GeometryShape&, const gp_Trsf&);
  [[nodiscard]] static Result<std::size_t>
  validate_holes(const PartDocument&, const std::vector<PatternSourceReference>&,
                 std::size_t occurrences);
  [[nodiscard]] static Result<GeometryShape>
  repeat_hole(const PartDocument&, FeatureId pattern, const HoleFeature&,
              const GeometryShape& before, const GeometryShape& seed, const GeometryShape& target,
              const std::vector<PatternPlacementTransform>&);
};
} // namespace blcad::geometry
