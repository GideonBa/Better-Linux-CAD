#pragma once

#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <memory>

namespace blcad::geometry {

struct ShapeSummary {
  bool is_null = true;
  std::size_t solid_count = 0;
  double volume_mm3 = 0.0;
};

class RectangleExtrusionAdapter;
class BodyBooleanAdapter;
class CircularCutAdapter;
class ClosedProfileAdapter;
class BodyTransformAdapter;
class RevolveAdapter;
class SweepAdapter;
class SurfaceAdapter;
class StepExporter;
class AssemblyStepExporter;
class Sketch3DGeometryAdapter;
class OcctShapeView;
class ViewportSceneBuilder;

namespace detail {
class AssemblyPartShapeDefinitionBuilder;
class AssemblyPosedLeafShapeBuilder;
} // namespace detail

class GeometryShape {
public:
  GeometryShape();
  ~GeometryShape();

  GeometryShape(const GeometryShape& other);
  GeometryShape& operator=(const GeometryShape& other);
  GeometryShape(GeometryShape&& other) noexcept;
  GeometryShape& operator=(GeometryShape&& other) noexcept;

  [[nodiscard]] bool empty() const noexcept;

private:
  struct Impl;

  friend class RectangleExtrusionAdapter;
  friend class BodyBooleanAdapter;
  friend class CircularCutAdapter;
  friend class ClosedProfileAdapter;
  friend class BodyTransformAdapter;
  friend class RevolveAdapter;
  friend class SweepAdapter;
  friend class SurfaceAdapter;
  friend class LoftAdapter;
  friend class FilletAdapter;
  friend class ChamferAdapter;
  friend class ShellAdapter;
  friend class DraftAdapter;
  friend class StepExporter;
  friend class AssemblyStepExporter;
  friend class Sketch3DGeometryAdapter;
  friend class OcctShapeView;
  friend class ViewportSceneBuilder;
  friend class detail::AssemblyPartShapeDefinitionBuilder;
  friend class detail::AssemblyPosedLeafShapeBuilder;

  explicit GeometryShape(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> impl_;
};

class RectangleExtrusionAdapter {
public:
  [[nodiscard]] Result<GeometryShape> make_extruded_rectangle(const Quantity& width,
                                                              const Quantity& height,
                                                              const Quantity& thickness) const;

  [[nodiscard]] ShapeSummary summarize(const GeometryShape& shape) const;
};

} // namespace blcad::geometry
