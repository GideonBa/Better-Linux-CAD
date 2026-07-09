#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/sketch_reference_projector.hpp"

namespace blcad::geometry {

struct ResolvedCoincidentProjectedPointConstraint {
  SketchConstraintId id;
  SketchEntityId constrained_entity;
  Point2 point;
};

struct ResolvedProjectedLineConstraint {
  SketchConstraintId id;
  SketchEntityId constrained_line;
  Point2 point;
  Vector2 direction;
};

class ReferenceDrivenSketchHelper {
public:
  [[nodiscard]] Result<ResolvedCoincidentProjectedPointConstraint>
  resolve_coincident_point_constraint(const PartDocument& document, const Sketch& sketch,
                                      const SketchConstraint& constraint) const;

  [[nodiscard]] Result<ResolvedProjectedLineConstraint>
  resolve_projected_line_constraint(const PartDocument& document, const Sketch& sketch,
                                    const SketchConstraint& constraint) const;

  [[nodiscard]] Result<LineSegment> create_profile_helper_line_from_projected_point_constraints(
      const PartDocument& document, const Sketch& sketch, SketchEntityId line_id,
      const SketchConstraint& start_constraint, const SketchConstraint& end_constraint) const;
};

} // namespace blcad::geometry
