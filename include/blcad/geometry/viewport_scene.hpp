#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <functional>
#include <optional>
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
  ThreadAnnotation,
};

struct ViewportSceneItem {
  ViewportSceneKind kind{ViewportSceneKind::SolidBody};
  std::string semantic_id;
  GeometryShape shape;
  SketchLineRole line_role{SketchLineRole::Geometry};
  std::string owner_body_id{};
  // Transient parent visibility, e.g. a component's derived Thread graphics.
  std::string owner_presentation_id{};
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
  build_part(const PartDocument& document, const ShapeCache& shape_cache,
             std::optional<std::size_t> history_prefix = std::nullopt) const;
  // Disposable, typed presentations of supported construction sources. Never
  // infer persistent semantic references from kernel subshape ordinals.
  [[nodiscard]] Result<std::vector<ConstructionSelectionTarget>>
  build_construction_sources(const PartDocument& document, const ShapeCache& shape_cache,
                             std::optional<std::size_t> history_prefix = std::nullopt) const;
  // Supported semantic edge axes at the same cache-aware positions used by Body Rotate.
  [[nodiscard]] Result<std::vector<ConstructionSelectionTarget>>
  build_body_rotation_sources(const PartDocument& document, const ShapeCache& shape_cache,
                              std::optional<std::size_t> history_prefix = std::nullopt) const;
  [[nodiscard]] Result<std::vector<ViewportSceneItem>> build_project(const Project& project) const;
  // Uses the session's complete member results, including separate visible Bodies.
  // Missing or stale supplied results fail rather than silently recomputing them.
  [[nodiscard]] Result<std::vector<ViewportSceneItem>>
  build_project(const Project& project,
                const std::function<const ShapeCache*(DocumentId)>& member_cache) const;

private:
  StepImportTopologyContext* import_context_{};
};

} // namespace blcad::geometry
