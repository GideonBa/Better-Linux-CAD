#pragma once

#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::geometry {

// Erzeugt aus einem bestehenden Volumenkoerper einen zentrischen, durchgehenden
// Kreis-Cut. Der Cut entspricht dem MVP-1-`SubtractiveExtrude` mit `through_all`.
// Der Adapter kapselt den OCCT-Boolean-Cut und bleibt damit auf der
// Geometry-Seite der Core/OCCT-Grenze.
class CircularCutAdapter {
public:
  [[nodiscard]] Result<GeometryShape> cut_circular_hole(const GeometryShape& target,
                                                        const Quantity& diameter,
                                                        Point2 center = {}) const;
};

} // namespace blcad::geometry
