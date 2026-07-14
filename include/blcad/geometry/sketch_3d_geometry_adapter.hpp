#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry {

enum class Sketch3DGeometryProductKind { Point, Line, Polyline, Arc, Spline, Helix };

struct Sketch3DGeometryProduct {
  SketchEntityId entity_id;
  Sketch3DGeometryProductKind kind = Sketch3DGeometryProductKind::Point;
  GeometryShape shape;
  std::size_t vertex_count = 0;
  std::size_t edge_count = 0;
  std::size_t wire_count = 0;
};

struct ResolvedSketchPoint3D {
  SketchEntityId entity_id;
  Point3 position;
};

class Sketch3DGeometryResult {
public:
  Sketch3DGeometryResult(Sketch3DId sketch_id, std::vector<ResolvedSketchPoint3D> points,
                         std::vector<Sketch3DGeometryProduct> products);

  [[nodiscard]] const Sketch3DId& sketch_id() const noexcept;
  [[nodiscard]] const std::vector<ResolvedSketchPoint3D>& points() const noexcept;
  [[nodiscard]] const std::vector<Sketch3DGeometryProduct>& products() const noexcept;
  [[nodiscard]] const ResolvedSketchPoint3D* find_point(SketchEntityId id) const noexcept;
  [[nodiscard]] const Sketch3DGeometryProduct* find_product(SketchEntityId id) const noexcept;

private:
  Sketch3DId sketch_id_;
  std::vector<ResolvedSketchPoint3D> points_;
  std::vector<Sketch3DGeometryProduct> products_;
};

class Sketch3DGeometryAdapter {
public:
  [[nodiscard]] Result<Sketch3DGeometryResult> convert(const PartDocument& document,
                                                       Sketch3DId sketch_id) const;
};

} // namespace blcad::geometry
