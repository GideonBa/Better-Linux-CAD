#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <string>
#include <variant>
#include <vector>

namespace blcad::geometry {

enum class ViewportSceneKind {
  SolidBody,
  SurfaceBody,
  Face,
  Edge,
  Vertex,
  Datum,
  SketchEntity,
  Path,
  Component,
  AssemblyTarget,
};

struct ViewportSceneItem {
  ViewportSceneKind kind{ViewportSceneKind::SolidBody};
  std::string semantic_id;
  GeometryShape shape;
  SketchLineRole line_role{SketchLineRole::Geometry};
};

struct ConstructionSelectionTarget {
  ViewportSceneItem item;
  std::variant<SemanticFaceReference, SemanticEdgeReference, SemanticVertexReference> reference;
};

// Builds transient viewer input from authoritative Core intent and Geometry
// results. Scene identities are stable BLCAD ids; TopoDS identity is never used
// outside the presentation lifetime.
class ViewportSceneBuilder {
public:
  [[nodiscard]] Result<std::vector<ViewportSceneItem>>
  build_part(const PartDocument& document, const ShapeCache& shape_cache) const;
  // Disposable, typed presentations of supported construction sources. Never
  // infer persistent semantic references from kernel subshape ordinals.
  [[nodiscard]] Result<std::vector<ConstructionSelectionTarget>>
  build_construction_sources(const PartDocument& document, const ShapeCache& shape_cache) const;
  [[nodiscard]] Result<std::vector<ViewportSceneItem>> build_project(const Project& project) const;
};

} // namespace blcad::geometry
