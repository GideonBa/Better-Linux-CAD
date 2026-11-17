#pragma once
#include <Law_Function.hxx>
#include <vector>
namespace blcad::geometry::detail {
struct RadiusKnot {
  double length_mm, radius_mm;
};
// Native curvilinear coordinate x maps to authored station coordinate sign*x+offset.
Handle(Law_Function) radius_law(std::vector<RadiusKnot>, bool cubic, bool periodic = false,
                                int sign = 1, double offset = 0);
} // namespace blcad::geometry::detail
