#pragma once

#include "blcad/core/construction_geometry.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

namespace blcad::geometry {

struct ResolvedConstructionLine {
  ConstructionLineId id;
  Point3 point;
  Vector3 direction;
};

class ConstructionLineResolver {
public:
  [[nodiscard]] Result<ResolvedConstructionLine>
  resolve(const PartDocument& document, ConstructionLineId line_id) const;
};

} // namespace blcad::geometry
