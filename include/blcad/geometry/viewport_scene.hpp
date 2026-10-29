#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <string>
#include <variant>
#include <vector>

namespace blcad::geometry {
class StepImportTopologyContext;

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
  std::string owner_body_id{};
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
  ViewportSceneBuilder() = default;
  explicit ViewportSceneBuilder(StepImportTopologyContext* context) : import_context_(context) {}
  [[nodiscard]] Result<std::vector<ViewportSceneItem>>
  build_part(const PartDocument& document, const ShapeCache& shape_cache) const;
  // Disposable, typed presentations of supported construction sources. Never
  // infer persistent semantic references from kernel subshape ordinals.
  [[nodiscard]] Result<std::vector<ConstructionSelectionTarget>>
  build_construction_sources(const PartDocument& document, const ShapeCache& shape_cache) const;
  // Supported semantic edge axes at the same cache-aware positions used by Body Rotate.
  [[nodiscard]] Result<std::vector<ConstructionSelectionTarget>>
  build_body_rotation_sources(const PartDocument& document, const ShapeCache& shape_cache) const;
  [[nodiscard]] Result<std::vector<ViewportSceneItem>> build_project(const Project& project) const;

private:
  StepImportTopologyContext* import_context_{};
};

} // namespace blcad::geometry
