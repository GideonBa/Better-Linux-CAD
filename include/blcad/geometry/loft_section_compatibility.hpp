#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/sweep_adapter.hpp"

namespace blcad::geometry {

// Derived refinement only. Original Sketch entities, section order and seams stay authored intent.
// source_vertices[section][vertex] identifies the same vertex in the refined section.
template <typename Segment> struct LoftCompatibleSections {
  std::vector<std::vector<Segment>> sections;
  std::vector<std::vector<std::size_t>> source_vertices;
  bool adjusted = false;
};

[[nodiscard]] Result<LoftCompatibleSections<ClosedProfileCurveSegment>>
make_loft_sections_compatible(const FeatureId&,
                              const std::vector<std::vector<ClosedProfileCurveSegment>>&);

[[nodiscard]] Result<LoftCompatibleSections<SweepPathSegment>>
make_loft_sections_compatible(const FeatureId&, const std::vector<std::vector<SweepPathSegment>>&);

} // namespace blcad::geometry
