#pragma once

#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include "face_lineage.hpp"

#include <TopoDS_Shape.hxx>

#include <utility>

// Internal, non-public definition of the OCCT backing of GeometryShape. Only
// Geometry adapter translation units include this header; they are friends of
// GeometryShape and may reach the OCCT shape. The public header deliberately
// keeps GeometryShape OCCT-free.
namespace blcad::geometry {

struct GeometryShape::Impl {
  explicit Impl(TopoDS_Shape shape_in) : shape(std::move(shape_in)) {}

  TopoDS_Shape shape;
  std::vector<detail::FaceLineage> face_lineage;
};

} // namespace blcad::geometry
