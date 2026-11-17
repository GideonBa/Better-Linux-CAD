#pragma once
#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include <optional>
#include <string>
#include <vector>
namespace blcad::geometry {
class StepImportTopologyContext;
struct FilletStationGuide {
  std::string group, station;
  ParameterId radius;
  Point3 anchor, basis_origin;
  Vector3 radius_axis, basis_direction;
  double chain_length_mm = 0;
};
// Read-only geometry of the same expanded, oriented source contours used by execution.
class FilletGuideQuery {
public:
  [[nodiscard]] static Result<std::vector<FilletStationGuide>>
  stations(const PartDocument&, const FilletFeature&, const ShapeCache&,
           StepImportTopologyContext* = nullptr, std::optional<std::size_t> history_prefix = {});
  [[nodiscard]] static Result<double>
  full_round_half_width(const PartDocument&, const FilletFeature&, const ShapeCache&,
                        StepImportTopologyContext* = nullptr,
                        std::optional<std::size_t> history_prefix = {});
};
} // namespace blcad::geometry
