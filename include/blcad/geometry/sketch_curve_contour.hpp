#pragma once
#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch_contour.hpp"

namespace blcad::geometry {
// Part-context witnesses supplement the unchanged authored entity/role IDs.
[[nodiscard]] Result<std::vector<SketchContourCurve>>
resolve_sketch_curve_contour(const PartDocument& document, const Sketch& sketch,
                             const std::vector<SketchEntityId>& ids);
} // namespace blcad::geometry
