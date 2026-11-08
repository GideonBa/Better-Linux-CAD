#pragma once

#include "blcad/core/result_face_workplane.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <optional>
#include <string>
#include <vector>

namespace blcad::geometry {

class StepImportTopologyContext;

struct ResultFaceSelectionTarget {
  std::string semantic_id;
  std::string label;
  ResultFaceReference reference;
  GeometryShape shape;
  Point3 anchor;
};

struct ResolvedResultFace {
  ResolvedWorkplane plane;
  GeometryShape face;
};

// Resolve authored face origins through kernel history; no topology traversal
// position or geometric proximity becomes persistent identity.
class ResultFaceSupportGeometry {
public:
  [[nodiscard]] static GeometryShape tag_hole_wall(const GeometryShape& shape, FeatureId producer,
                                                   std::string role, Point3 center, Vector3 axis,
                                                   Vector3 x_axis, double radius_mm);
  [[nodiscard]] static Result<GeometryShape>
  resolve_face(const PartDocument& document, const ShapeCache& cache,
               const ResultFaceReference& reference,
               StepImportTopologyContext* import_context = nullptr);
  // Attach semantic roots to a fresh primitive. Returns detached metadata so
  // earlier cached shapes sharing the same OCCT topology remain immutable.
  [[nodiscard]] static GeometryShape tag_prismatic_caps(const GeometryShape& shape,
                                                        FeatureId producer,
                                                        const ResolvedWorkplane& frame);
  [[nodiscard]] static GeometryShape tag_plane(const GeometryShape& shape, FeatureId producer,
                                               std::string role, Point3 origin, Vector3 x_axis,
                                               Vector3 normal, bool selectable = true);
  [[nodiscard]] static GeometryShape tag_rectangular_supports(const GeometryShape& shape,
                                                              FeatureId producer,
                                                              const ResolvedWorkplane& frame,
                                                              Point2 center, double width_mm,
                                                              double height_mm);
  // Copy instances have distinct authored identities even when their faces fuse.
  [[nodiscard]] static GeometryShape scope_copy(const GeometryShape& shape, FeatureId producer,
                                                std::string instance);
  [[nodiscard]] static Result<std::vector<ResultFaceSelectionTarget>>
  selectable_faces(const PartDocument& document, const ShapeCache& cache, const BodyId& body,
                   StepImportTopologyContext* import_context = nullptr,
                   std::optional<std::size_t> history_prefix = std::nullopt);
  [[nodiscard]] static Result<ResolvedWorkplane>
  resolve(const PartDocument& document, const ResultFaceWorkplane& plane, const ShapeCache& cache,
          StepImportTopologyContext* import_context = nullptr);
  // Follow the same authored contributors on the current Body, including later
  // modifiers/placements. This does not author a synthetic workplane record.
  [[nodiscard]] static Result<ResolvedResultFace>
  resolve_current(const PartDocument& document, const ShapeCache& cache, const BodyId& body,
                  const std::vector<ResultFaceOrigin>& origins,
                  StepImportTopologyContext* import_context = nullptr);
};

} // namespace blcad::geometry
