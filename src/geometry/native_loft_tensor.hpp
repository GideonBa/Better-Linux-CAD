#pragma once
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <array>
#include <vector>
namespace blcad::geometry::detail {
// The four ordered bounds are root section, right rail, reversed tip section, reversed left rail.
[[nodiscard]] Result<TopoDS_Face> native_loft_tensor(FeatureId id,
                                                     const std::array<TopoDS_Edge, 4>& bounds,
                                                     const std::vector<TopoDS_Edge>& sections,
                                                     double tolerance);
} // namespace blcad::geometry::detail
