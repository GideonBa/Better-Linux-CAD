#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

namespace blcad::geometry {

struct RectangularWorkplaneBounds {
  bool enabled = false;
  Point2 center;
  double width_mm = 0.0;
  double height_mm = 0.0;
};

struct ResolvedWorkplane {
  DatumPlaneId id;
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;
  RectangularWorkplaneBounds bounds;
};

class WorkplaneResolver {
public:
  [[nodiscard]] Result<ResolvedWorkplane> resolve(const PartDocument& document,
                                                   DatumPlaneId workplane_id) const;

  [[nodiscard]] Result<ResolvedWorkplane>
  resolve_generated_face(const PartDocument& document,
                         const SemanticFaceReference& face_reference) const;

  [[nodiscard]] Result<ResolvedWorkplane> resolve_for_sketch(const PartDocument& document,
                                                              const Sketch& sketch) const;

  [[nodiscard]] Point3 evaluate_point(const ResolvedWorkplane& workplane,
                                       Point2 local_point) const noexcept;
};

} // namespace blcad::geometry
