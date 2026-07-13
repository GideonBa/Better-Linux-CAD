#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

namespace blcad::geometry {

class ShapeCache;

struct ResolvedSketchPointReference {
  SketchEntityId id;
  Point2 position;
};

struct ResolvedSketchLineReference {
  SketchEntityId id;
  Point2 point;
  Vector2 direction;
};

class SketchReferenceProjector {
public:
  [[nodiscard]] Result<ResolvedSketchPointReference>
  resolve_point(const PartDocument& document, const Sketch& sketch,
                const ProjectedSketchPoint& reference) const;
  [[nodiscard]] Result<ResolvedSketchPointReference>
  resolve_point(const PartDocument& document, const Sketch& sketch,
                const ProjectedSketchPoint& reference, const ShapeCache& shape_cache) const;

  [[nodiscard]] Result<ResolvedSketchLineReference>
  resolve_line(const PartDocument& document, const Sketch& sketch,
               const ProjectedSketchLine& reference) const;
  [[nodiscard]] Result<ResolvedSketchLineReference>
  resolve_line(const PartDocument& document, const Sketch& sketch,
               const ProjectedSketchLine& reference, const ShapeCache& shape_cache) const;
};

} // namespace blcad::geometry
