#pragma once

#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <TopoDS_Shape.hxx>

#include <utility>

// Interne, nicht oeffentliche Definition des OCCT-Backings von GeometryShape.
// Dieser Header wird nur von Geometry-Adapter-Uebersetzungseinheiten inkludiert,
// die als friend von GeometryShape auf die OCCT-Shape zugreifen duerfen. Der
// oeffentliche Header haelt GeometryShape absichtlich OCCT-frei.
namespace blcad::geometry {

struct GeometryShape::Impl {
  explicit Impl(TopoDS_Shape shape_in) : shape(std::move(shape_in)) {}

  TopoDS_Shape shape;
};

} // namespace blcad::geometry
