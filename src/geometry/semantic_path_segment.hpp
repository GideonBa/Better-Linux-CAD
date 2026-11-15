#pragma once
#include "blcad/core/part_document.hpp"
#include "blcad/geometry/sweep_adapter.hpp"
namespace blcad::geometry {
class ShapeCache;
class StepImportTopologyContext;
namespace detail {
// Recover one producer-anchored native curve and orient it by persistent evidence.
// Imported paths select the immutable base output explicitly; placed results use a result witness.
[[nodiscard]] Result<SweepPathSegment> resolve_semantic_path_segment(const PartDocument&,
                                                                     const PathSegmentReference&,
                                                                     const ShapeCache&,
                                                                     StepImportTopologyContext*);
} // namespace detail
} // namespace blcad::geometry
