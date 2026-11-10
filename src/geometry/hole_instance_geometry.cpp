#include "blcad/geometry/hole_instance_geometry.hpp"
#include "geometry_shape_internal.hpp"
#include "occt_failure_message.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pln.hxx>
#include <numbers>
#include <type_traits>

namespace blcad::geometry {
namespace {
constexpr double tol = 1e-7;
std::vector<const detail::FaceLineage*> collect(const TopoDS_Shape& body,
                                                const std::vector<detail::FaceLineage>& lineage,
                                                const ResultFaceOrigin& origin) {
  std::vector<const detail::FaceLineage*> result;
  for (const auto& entry : lineage) {
    if (entry.origin != origin)
      continue;
    bool present = false;
    for (TopExp_Explorer it(body, TopAbs_FACE); it.More(); it.Next())
      present |= it.Current().IsSame(entry.face);
    if (present && std::none_of(result.begin(), result.end(),
                                [&](auto* e) { return e->face.IsSame(entry.face); }))
      result.push_back(&entry);
  }
  return result;
}
} // namespace
Result<HoleInstanceFrame> HoleInstanceGeometry::resolve(const PartDocument& p,
                                                        const HoleInstanceReference& ref,
                                                        const ShapeCache& cache) {
  auto valid = cache.validate_complete_geometry_result(p);
  if (!valid)
    return Result<HoleInstanceFrame>::failure(valid.error());
  auto body = validate_hole_instance_reference(p, ref);
  if (!body)
    return Result<HoleInstanceFrame>::failure(body.error());
  const auto* shape = cache.find_body_shape(body.value());
  if (!shape)
    return Result<HoleInstanceFrame>::failure(
        Error::geometry(ref.producer().value(), "Hole Body result is unavailable"));
  return resolve_shape(p, ref, *shape);
}
Result<HoleInstanceFrame> HoleInstanceGeometry::resolve_shape(const PartDocument& p,
                                                              const HoleInstanceReference& ref,
                                                              const GeometryShape& shape) {
  const auto fail = [&](std::string message) {
    return Result<HoleInstanceFrame>::failure(
        Error::geometry(ref.producer().value(), "Hole instance: " + std::move(message)));
  };
  auto valid = validate_hole_instance_reference(p, ref);
  if (!valid)
    return Result<HoleInstanceFrame>::failure(valid.error());
  if (shape.empty())
    return fail("Body result is unavailable");
  try {
    const auto bore = collect(shape.impl_->shape, shape.impl_->face_lineage, ref.origin("wall"));
    if (bore.empty())
      return fail("bore descendants are missing");
    const auto& first = *bore.front();
    if (!first.support_axis || length(*first.support_axis) <= tol || length(first.x_axis) <= tol)
      return fail("authored bore frame is unavailable");
    auto axis = normalized(*first.support_axis), x = normalized(first.x_axis);
    if (std::abs(dot(axis, x)) > tol)
      return fail("bore frame is not orthogonal");
    auto origin = first.frame_origin;
    double radius = 0;
    for (const auto* e : bore) {
      BRepAdaptor_Surface surface(e->face);
      if (surface.GetType() != GeomAbs_Cylinder || !e->support_axis)
        return fail("bore is not cylindrical");
      const auto cylinder = surface.Cylinder();
      const auto n = cylinder.Axis().Direction();
      const auto o = cylinder.Location();
      if (radius == 0)
        radius = cylinder.Radius();
      if (std::abs(radius - cylinder.Radius()) > tol || distance(e->frame_origin, origin) > tol ||
          std::abs(dot(axis, *e->support_axis) - 1) > tol ||
          std::abs(std::abs(dot(axis, {n.X(), n.Y(), n.Z()})) - 1) > tol ||
          length(cross({o.X() - origin.x, o.Y() - origin.y, o.Z() - origin.z}, axis)) > tol)
        return fail("bore descendants have incompatible axes or radii");
    }
    auto faces = bore;
    double opening = radius;
    const auto* hole = p.find_hole_feature(ref.hole());
    if (ref.role() == HoleReferenceRole::Entry) {
      std::string role = "wall";
      const double nominal =
          resolve_hole_diameter(p, hole->diameter()).value().diameter.millimeters() / 2;
      const double scale = radius / nominal;
      std::visit(
          [&](const auto& recipe) {
            using T = std::decay_t<decltype(recipe)>;
            if constexpr (std::is_same_v<T, HoleCounterbore> || std::is_same_v<T, HoleSpotface> ||
                          std::is_same_v<T, HoleCountersink> || std::is_same_v<T, HoleCenter>) {
              role = "seat/wall";
              opening = resolve_hole_dimension(p, recipe.diameter).value().diameter.millimeters() *
                        scale / 2;
            } else if constexpr (std::is_same_v<T, HoleStepped>) {
              const HoleStep* first = nullptr;
              double depth = 0;
              for (const auto& step : recipe.steps) {
                const double z =
                    resolve_hole_dimension(p, step.depth).value().diameter.millimeters();
                if (!first || z < depth) {
                  first = &step;
                  depth = z;
                }
              }
              if (first) {
                role = "step/" + std::to_string(first->id.size()) + ":" + first->id + "/wall";
                opening =
                    resolve_hole_dimension(p, first->diameter).value().diameter.millimeters() *
                    scale / 2;
              }
            }
          },
          hole->manufacturing());
      faces = collect(shape.impl_->shape, shape.impl_->face_lineage, ref.origin(role));
      bool ring = false;
      for (auto* e : faces)
        for (TopExp_Explorer it(e->face, TopAbs_EDGE); it.More(); it.Next()) {
          BRepAdaptor_Curve curve(TopoDS::Edge(it.Current()));
          if (curve.GetType() != GeomAbs_Circle ||
              std::abs(std::abs(curve.LastParameter() - curve.FirstParameter()) -
                       2 * std::numbers::pi) > tol)
            continue;
          const auto circle = curve.Circle();
          const auto c = circle.Location();
          const auto n = circle.Axis().Direction();
          ring |= std::abs(circle.Radius() - opening) <= tol &&
                  distance(origin, {c.X(), c.Y(), c.Z()}) <= tol &&
                  std::abs(std::abs(dot(axis, {n.X(), n.Y(), n.Z()})) - 1) <= tol;
        }
      if (!ring)
        return fail("complete entry rim on the authored support is missing or consumed");
    } else if (ref.role() == HoleReferenceRole::Seat || ref.role() == HoleReferenceRole::Step) {
      const auto role =
          ref.role() == HoleReferenceRole::Seat
              ? std::string("seat/bottom")
              : "step/" + std::to_string(ref.step().size()) + ":" + ref.step() + "/bottom";
      faces = collect(shape.impl_->shape, shape.impl_->face_lineage, ref.origin(role));
      if (faces.size() != 1)
        return fail("seating face is missing or ambiguous");
      BRepAdaptor_Surface surface(faces.front()->face);
      if (surface.GetType() != GeomAbs_Plane)
        return fail("seating face is not planar");
      const auto plane = surface.Plane();
      const auto n = plane.Axis().Direction();
      const auto q = plane.Location();
      const Vector3 normal{n.X(), n.Y(), n.Z()};
      if (std::abs(std::abs(dot(normal, axis)) - 1) > tol)
        return fail("seat is not normal to the bore axis");
      const double z =
          dot({q.X() - origin.x, q.Y() - origin.y, q.Z() - origin.z}, normal) / dot(axis, normal);
      origin = {origin.x + z * axis.x, origin.y + z * axis.y, origin.z + z * axis.z};
    }
    TopoDS_Compound support;
    BRep_Builder builder;
    builder.MakeCompound(support);
    for (const auto* e : faces)
      builder.Add(support, e->face);
    return Result<HoleInstanceFrame>::success(
        {ResolvedWorkplane{{}, origin, x, cross(axis, x), axis, {}}, radius,
         GeometryShape(std::make_shared<GeometryShape::Impl>(support)), opening});
  } catch (const Standard_Failure& e) {
    return fail(standard_failure_message(e, "cannot resolve current geometry"));
  }
}
} // namespace blcad::geometry
