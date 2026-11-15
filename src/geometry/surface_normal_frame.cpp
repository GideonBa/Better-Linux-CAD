#include "blcad/geometry/surface_selection_geometry.hpp"
#include "geometry_shape_internal.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <limits>
namespace blcad::geometry {
Result<SurfaceNormalFrame> SurfaceSelectionGeometry::normal_frame(const GeometryShape& shape) {
  const auto fail = [](std::string message) {
    return Result<SurfaceNormalFrame>::failure(
        Error::geometry("surface.normal", std::move(message)));
  };
  if (shape.empty())
    return fail("Select an owned Surface face or quilt");
  try {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(shape.impl_->shape, properties);
    const auto center = properties.CentreOfMass();
    double best = std::numeric_limits<double>::infinity();
    std::optional<SurfaceNormalFrame> frame;
    int count = 0;
    // This representative station is a transient direction aid. It never
    // chooses or persists a face identity, nor changes the per-face signed
    // side.
    for (TopExp_Explorer e(shape.impl_->shape, TopAbs_FACE); e.More(); e.Next()) {
      if (++count > 64)
        return fail("Surface direction exceeds the 64-face budget");
      const auto face = TopoDS::Face(e.Current());
      BRepAdaptor_Surface surface(face, true);
      double u0, u1, v0, v1;
      BRepTools::UVBounds(face, u0, u1, v0, v1);
      if (!std::isfinite(u0) || !std::isfinite(u1) || !std::isfinite(v0) || !std::isfinite(v1))
        return fail("Surface direction requires finite native bounds");
      for (const double u : {.5, .25, .75, .125, .875})
        for (const double v : {.5, .25, .75, .125, .875}) {
          const double a = std::lerp(u0, u1, u), b = std::lerp(v0, v1, v);
          if (BRepClass_FaceClassifier(face, gp_Pnt2d(a, b), 1e-9).State() != TopAbs_IN)
            continue;
          gp_Pnt point;
          gp_Vec du, dv;
          surface.D1(a, b, point, du, dv);
          auto normal = du.Crossed(dv);
          if (normal.SquareMagnitude() < 1e-24)
            continue;
          normal.Normalize();
          if (face.Orientation() == TopAbs_REVERSED)
            normal.Reverse();
          const double distance = point.SquareDistance(center);
          if (!std::isfinite(distance) || distance >= best)
            continue;
          best = distance;
          frame = SurfaceNormalFrame{{point.X(), point.Y(), point.Z()},
                                     {normal.X(), normal.Y(), normal.Z()}};
        }
    }
    if (!frame)
      return fail("No regular interior Surface normal station is available");
    return Result<SurfaceNormalFrame>::success(*frame);
  } catch (const Standard_Failure& error) {
    return fail(error.GetMessageString());
  }
}
} // namespace blcad::geometry
