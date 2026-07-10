#pragma once

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

namespace blcad::geometry {

struct ResolvedSemanticEdge {
  SemanticEdgeReference reference;
  Point3 start;
  Point3 end;
};

struct ResolvedSemanticVertex {
  SemanticVertexReference reference;
  Point3 position;
};

class SemanticReferenceEvaluator {
public:
  [[nodiscard]] Result<ResolvedSemanticEdge> resolve_edge(const PartDocument& document,
                                                          SemanticEdgeReference reference) const;

  [[nodiscard]] Result<ResolvedSemanticVertex>
  resolve_vertex(const PartDocument& document, SemanticVertexReference reference) const;
};

} // namespace blcad::geometry
