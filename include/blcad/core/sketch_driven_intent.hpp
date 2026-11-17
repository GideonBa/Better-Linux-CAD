#pragma once
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/sketch_driven_feature.hpp"
#include <optional>
#include <variant>
namespace blcad {
enum class RibWebKind { Rib, Web };
enum class RibExtrusionPlane { PerpendicularToSketch, ParallelToSketch };
enum class RibThicknessSide { Symmetric, First, Second, Asymmetric };
enum class RibCrossingPolicy { Union, Reject };
struct RibSketchSource {
  SketchId sketch;
  std::vector<SketchEntityId> curves;
  std::optional<ProfileId> profile;
};
struct RibDistanceExtent {
  ParameterId distance;
};
struct RibFaceExtent {
  FaceReference face;
};
struct RibBodyExtent {
  BodyId body;
};
using RibExtent = std::variant<RibDistanceExtent, RibFaceExtent, RibBodyExtent>;
struct RibWebIntent {
  RibWebKind kind{RibWebKind::Rib};
  RibSketchSource source;
  RibExtrusionPlane plane{RibExtrusionPlane::PerpendicularToSketch};
  // Components in the source Sketch frame; magnitude is not an extrusion length.
  Vector3 direction{0, 0, 1};
  RibThicknessSide side{RibThicknessSide::Symmetric};
  ParameterId thickness;
  std::optional<ParameterId> second_thickness;
  std::optional<ParameterId> draft;
  RibExtent extent;
  RibCrossingPolicy crossings{RibCrossingPolicy::Union};
};
struct SketchDrivenDefinition {
  std::variant<RibWebIntent> intent;
};

} // namespace blcad
