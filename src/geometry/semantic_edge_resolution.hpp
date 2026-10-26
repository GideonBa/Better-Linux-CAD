#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include <string_view>

namespace blcad::geometry {

class ShapeCache;

// Shared semantic edge resolution for the edge-driven Part features (Fillet,
// Chamfer). Both resolve a persisted `EdgeReference` — semantic linear or
// semantic circular, or a producer-anchored result witness — against a freshly computed target
// shape, and both fail closed when the reference no longer matches exactly one edge.
//
// `feature_label` only selects the feature wording inside the returned
// diagnostics ("fillet", "chamfer"); it carries no resolution policy.
[[nodiscard]] Result<ResultEdgeReference> make_result_edge_reference(const FeatureId& producer,
                                                                     const TopoDS_Edge& edge);

[[nodiscard]] Result<TopoDS_Edge>
resolve_semantic_edge(const PartDocument& document, const ShapeCache& cache,
                      const TopoDS_Shape& target, const EdgeReference& reference,
                      const FeatureId& id, std::string_view feature_label);

} // namespace blcad::geometry
