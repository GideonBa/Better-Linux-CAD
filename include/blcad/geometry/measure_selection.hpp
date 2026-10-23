#pragma once

#include "blcad/geometry/geometry_measure.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <optional>

namespace blcad::geometry {

enum class MeasureSelectionKind { Point, Line, Plane, Circle, Cylinder, Body };
struct MeasureSelectionOwner {
  std::string semantic_id;
  std::string label;
  GeometryShape shape;
  std::optional<BodyId> body;
};
struct MeasureSelectionTarget {
  std::string semantic_id;
  std::string label;
  MeasureSelectionKind kind;
  GeometryShape shape;
  Point3 anchor;
  std::optional<GeometryMeasureTarget> descriptor;
  std::optional<BodyId> body;
};

// Read-only, revision-scoped subshape catalogue. The owner shape is already in
// World/root-assembly coordinates. Subshape ordinals are transient viewer ids:
// callers discard the complete catalogue after model/freshness changes and
// never persist or remap these ids to a later topology generation.
class MeasureSelectionBuilder {
public:
  [[nodiscard]] static Result<std::vector<MeasureSelectionTarget>>
  build(const std::vector<MeasureSelectionOwner>& owners, std::size_t generation);
  [[nodiscard]] static Result<std::vector<MeasureSelectionTarget>>
  build_project(const Project& project, std::size_t generation);
};
} // namespace blcad::geometry
