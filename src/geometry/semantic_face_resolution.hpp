#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <string_view>

namespace blcad::geometry {

class ShapeCache;

// Shared semantic face resolution for the face-driven Part features (Draft,
// Shell). Both resolve a persisted `FaceReference` — semantic planar or
// semantic cylindrical — against a freshly computed target shape, and both fail
// closed when the reference no longer matches exactly one face.
//
// `feature_label` only selects the feature wording inside the returned
// diagnostics ("draft", "shell"); it carries no resolution policy.
[[nodiscard]] Result<TopoDS_Face>
resolve_semantic_face(const PartDocument& document, const ShapeCache& cache,
                      const TopoDS_Shape& target, const FaceReference& reference,
                      const FeatureId& id, std::string_view feature_label);

} // namespace blcad::geometry
