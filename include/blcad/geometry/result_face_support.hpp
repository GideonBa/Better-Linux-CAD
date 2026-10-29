#pragma once

#include "blcad/core/result_face_workplane.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <string>
#include <vector>

namespace blcad::geometry {

struct ResultFaceSelectionTarget {
  std::string semantic_id;
  std::string label;
  ResultFaceReference reference;
  GeometryShape shape;
  Point3 anchor;
};

// Resolve authored face origins through kernel history; no topology traversal
// position or geometric proximity becomes persistent identity.
class ResultFaceSupportGeometry {
public:
  // Attach semantic roots to a fresh primitive. Returns detached metadata so
  // earlier cached shapes sharing the same OCCT topology remain immutable.
  [[nodiscard]] static GeometryShape tag_prismatic_caps(const GeometryShape& shape,
                                                        FeatureId producer,
                                                        const ResolvedWorkplane& frame);
  [[nodiscard]] static GeometryShape tag_plane(const GeometryShape& shape, FeatureId producer,
                                               std::string role, Point3 origin, Vector3 x_axis,
                                               Vector3 normal);
  // Copy instances have distinct authored identities even when their faces fuse.
  [[nodiscard]] static GeometryShape scope_copy(const GeometryShape& shape, FeatureId producer,
                                                std::string instance);
  [[nodiscard]] static Result<std::vector<ResultFaceSelectionTarget>>
  selectable_faces(const PartDocument& document, const ShapeCache& cache, const BodyId& body);
  [[nodiscard]] static Result<ResolvedWorkplane>
  resolve(const PartDocument& document, const ResultFaceWorkplane& plane, const ShapeCache& cache);
};

} // namespace blcad::geometry
