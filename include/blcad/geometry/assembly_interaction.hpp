#pragma once
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/viewport_scene.hpp"

namespace blcad::geometry {
struct AssemblyInteractionTarget {
  AssemblyConstraintTarget reference;
  AssemblyResolvedGeometricTarget resolved;
  std::string label;
  ViewportSceneItem presentation;
  Point3 anchor;
};
// Root-occurrence semantic inputs only. Native shapes/glyphs are disposable;
// persistent relationships retain the existing exact semantic target spelling.
class AssemblyInteractionGeometry {
public:
  [[nodiscard]] static Result<std::vector<AssemblyInteractionTarget>>
  targets(const Project& project);
};
} // namespace blcad::geometry
