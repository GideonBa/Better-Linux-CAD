#pragma once

#include "blcad/core/construction_geometry.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

namespace blcad::geometry {

struct ResolvedConstructionPoint {
  ConstructionPointId id;
  Point3 position;
};

class ConstructionPointResolver {
public:
  [[nodiscard]] Result<ResolvedConstructionPoint> resolve(const PartDocument& document,
                                                          ConstructionPointId point_id) const;
};

} // namespace blcad::geometry
