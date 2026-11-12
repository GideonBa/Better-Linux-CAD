#pragma once

#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <optional>
#include <variant>
#include <vector>

namespace blcad::geometry {

class StepImportTopologyContext;

enum class FinishingSelectionVisibility { VisibleOnly, IncludeHiddenInputs };

struct FinishingSelectionTarget {
  std::string semantic_id;
  std::string label;
  std::variant<EdgeReference, FaceReference> reference;
  GeometryShape shape;
  Point3 anchor;
};

// Offers only stable semantic references that still match exactly one subshape
// on the current target Body. Missing and ambiguous roles are never guessed.
class FinishingSelectionBuilder {
public:
  [[nodiscard]] static Result<std::vector<FinishingSelectionTarget>>
  build(const PartDocument& document, const ShapeCache& cache, const BodyId& body,
        PartFeatureInputRole role, StepImportTopologyContext* import_context = nullptr,
        std::optional<std::size_t> history_prefix = std::nullopt,
        FinishingSelectionVisibility visibility = FinishingSelectionVisibility::VisibleOnly);
};

} // namespace blcad::geometry
