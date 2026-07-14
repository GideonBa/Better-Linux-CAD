#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"

#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/construction_point_resolver.hpp"
#include "blcad/geometry/sketch_reference_projector.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepLib.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom2d_Line.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr double k_pi = 3.14159265358979323846;

[[nodiscard]] Error geometry_error(std::string object_id, std::string message) {
  return Error::geometry(std::move(object_id), std::move(message));
}

[[nodiscard]] std::string failure_message(const Standard_Failure& failure) {
  const char* message = failure.GetMessageString();
  return message == nullptr || *message == '\0' ? "OCCT operation failed" : message;
}

[[nodiscard]] gp_Pnt point(Point3 value) {
  return gp_Pnt(value.x, value.y, value.z);
}

[[nodiscard]] Result<double> coordinate_value(const PartDocument& document,
                                              const SketchCoordinate3D& coordinate,
                                              const SketchEntityId& point_id) {
  if (coordinate.source() == SketchCoordinate3DSource::Explicit)
    return Result<double>::success(coordinate.explicit_coordinate()->millimeters());
  const Parameter* parameter = document.find_parameter(*coordinate.parameter());
  if (parameter == nullptr || parameter->type() != ParameterType::Length)
    return Result<double>::failure(geometry_error(
        point_id.value(), "3D point coordinate requires an existing Length parameter"));
  return Result<double>::success(parameter->value().millimeters());
}

[[nodiscard]] Result<Point3> resolve_owned_point(const PartDocument& document,
                                                 const SketchPoint3D& source) {
  auto x = coordinate_value(document, source.x(), source.id());
  auto y = coordinate_value(document, source.y(), source.id());
  auto z = coordinate_value(document, source.z(), source.id());
  if (x.has_error() || y.has_error() || z.has_error())
    return Result<Point3>::failure(x.has_error()   ? x.error()
                                   : y.has_error() ? y.error()
                                                   : z.error());
  return Result<Point3>::success(Point3{x.value(), y.value(), z.value()});
}

[[nodiscard]] const ResolvedSketchPoint3D*
find_resolved_point(const std::vector<ResolvedSketchPoint3D>& points, const SketchEntityId& id) {
  const auto found = std::find_if(points.begin(), points.end(), [&id](const auto& candidate) {
    return candidate.entity_id == id;
  });
  return found == points.end() ? nullptr : &*found;
}

[[nodiscard]] Result<Point3>
resolve_point_reference(const PartDocument& document,
                        const std::vector<ResolvedSketchPoint3D>& local_points,
                        const SketchPointReference3D& reference) {
  if (reference.source() == SketchPointReference3DSource::LocalPoint) {
    const auto* resolved = find_resolved_point(local_points, *reference.local_point());
    if (resolved == nullptr)
      return Result<Point3>::failure(
          geometry_error(reference.local_point()->value(), "local 3D point must be resolved"));
    return Result<Point3>::success(resolved->position);
  }
  if (reference.source() == SketchPointReference3DSource::ConstructionPoint) {
    auto resolved = ConstructionPointResolver{}.resolve(document, *reference.construction_point());
    if (resolved.has_error())
      return Result<Point3>::failure(resolved.error());
    return Result<Point3>::success(resolved.value().position);
  }

  const Sketch* sketch = document.find_sketch(*reference.planar_sketch());
  if (sketch == nullptr)
    return Result<Point3>::failure(
        geometry_error(reference.planar_sketch()->value(), "planar source sketch must exist"));
  auto workplane = WorkplaneResolver{}.resolve_for_sketch(document, *sketch);
  if (workplane.has_error())
    return Result<Point3>::failure(workplane.error());
  const SketchReferenceTarget& target = *reference.planar_point();
  Point2 local{};
  if (target.kind() == SketchReferenceTargetKind::ProjectedPoint) {
    const ProjectedSketchPoint* projected = sketch->find_projected_point(target.entity());
    if (projected == nullptr)
      return Result<Point3>::failure(
          geometry_error(target.entity().value(), "projected planar point must exist"));
    auto resolved = SketchReferenceProjector{}.resolve_point(document, *sketch, *projected);
    if (resolved.has_error())
      return Result<Point3>::failure(resolved.error());
    local = resolved.value().position;
  } else {
    const LineSegment* line = sketch->find_line_segment(target.entity());
    if (line == nullptr)
      return Result<Point3>::failure(
          geometry_error(target.entity().value(), "planar line endpoint must exist"));
    local =
        target.kind() == SketchReferenceTargetKind::LineSegmentStart ? line->start() : line->end();
  }
  return Result<Point3>::success(WorkplaneResolver{}.evaluate_point(workplane.value(), local));
}

[[nodiscard]] Result<std::vector<Point3>>
resolve_references(const PartDocument& document,
                   const std::vector<ResolvedSketchPoint3D>& local_points,
                   const std::vector<SketchPointReference3D>& references) {
  std::vector<Point3> result;
  result.reserve(references.size());
  for (const auto& reference : references) {
    auto resolved = resolve_point_reference(document, local_points, reference);
    if (resolved.has_error())
      return Result<std::vector<Point3>>::failure(resolved.error());
    result.push_back(resolved.value());
  }
  return Result<std::vector<Point3>>::success(std::move(result));
}

[[nodiscard]] Result<TopoDS_Shape> make_line(Point3 start, Point3 end, const SketchEntityId& id) {
  BRepBuilderAPI_MakeEdge builder(point(start), point(end));
  if (!builder.IsDone())
    return Result<TopoDS_Shape>::failure(
        geometry_error(id.value(), "could not build 3D sketch line edge"));
  return Result<TopoDS_Shape>::success(builder.Edge());
}

[[nodiscard]] Result<TopoDS_Shape> make_polyline(const std::vector<Point3>& points,
                                                 const SketchEntityId& id) {
  BRepBuilderAPI_MakePolygon builder;
  for (Point3 value : points)
    builder.Add(point(value));
  if (!builder.IsDone())
    return Result<TopoDS_Shape>::failure(
        geometry_error(id.value(), "could not build 3D sketch polyline wire"));
  return Result<TopoDS_Shape>::success(builder.Wire());
}

[[nodiscard]] Result<TopoDS_Shape> make_arc(const std::vector<Point3>& points,
                                            const SketchEntityId& id) {
  GC_MakeArcOfCircle curve(point(points[0]), point(points[1]), point(points[2]));
  if (!curve.IsDone())
    return Result<TopoDS_Shape>::failure(
        geometry_error(id.value(), "could not build three-point 3D sketch arc"));
  BRepBuilderAPI_MakeEdge builder(curve.Value());
  if (!builder.IsDone())
    return Result<TopoDS_Shape>::failure(
        geometry_error(id.value(), "could not build 3D sketch arc edge"));
  return Result<TopoDS_Shape>::success(builder.Edge());
}

[[nodiscard]] GeomAbs_Shape continuity(SketchSpline3DContinuity value) {
  switch (value) {
  case SketchSpline3DContinuity::Positional:
    return GeomAbs_C0;
  case SketchSpline3DContinuity::Tangent:
    return GeomAbs_C1;
  case SketchSpline3DContinuity::Curvature:
    return GeomAbs_C2;
  }
  return GeomAbs_C0;
}

[[nodiscard]] Result<Handle(Geom_BSplineCurve)>
make_control_spline(const std::vector<Point3>& points, std::size_t degree,
                    const SketchEntityId& id) {
  const Standard_Integer pole_count = static_cast<Standard_Integer>(points.size());
  const Standard_Integer knot_count = pole_count - static_cast<Standard_Integer>(degree) + 1;
  TColgp_Array1OfPnt poles(1, pole_count);
  for (Standard_Integer index = 1; index <= pole_count; ++index)
    poles.SetValue(index, point(points[static_cast<std::size_t>(index - 1)]));
  TColStd_Array1OfReal knots(1, knot_count);
  TColStd_Array1OfInteger multiplicities(1, knot_count);
  for (Standard_Integer index = 1; index <= knot_count; ++index) {
    knots.SetValue(index, static_cast<double>(index - 1));
    multiplicities.SetValue(
        index, index == 1 || index == knot_count ? static_cast<Standard_Integer>(degree) + 1 : 1);
  }
  try {
    return Result<Handle(Geom_BSplineCurve)>::success(new Geom_BSplineCurve(
        poles, knots, multiplicities, static_cast<Standard_Integer>(degree), false));
  } catch (const Standard_Failure& failure) {
    return Result<Handle(Geom_BSplineCurve)>::failure(
        geometry_error(id.value(), failure_message(failure)));
  }
}

[[nodiscard]] Result<TopoDS_Shape> make_spline(const std::vector<Point3>& points,
                                               const SketchSpline3D& spline) {
  Handle(Geom_BSplineCurve) curve;
  if (spline.representation() == SketchSpline3DRepresentation::ControlPoints) {
    auto built = make_control_spline(points, spline.degree(), spline.id());
    if (built.has_error())
      return Result<TopoDS_Shape>::failure(built.error());
    curve = built.value();
  } else {
    TColgp_Array1OfPnt fit_points(1, static_cast<Standard_Integer>(points.size()));
    for (Standard_Integer index = 1; index <= fit_points.Length(); ++index)
      fit_points.SetValue(index, point(points[static_cast<std::size_t>(index - 1)]));
    GeomAPI_PointsToBSpline builder(fit_points, static_cast<Standard_Integer>(spline.degree()),
                                    static_cast<Standard_Integer>(spline.degree()),
                                    continuity(spline.continuity()), 1.0e-7);
    if (!builder.IsDone())
      return Result<TopoDS_Shape>::failure(
          geometry_error(spline.id().value(), "could not build fit-point 3D spline"));
    curve = builder.Curve();
  }
  BRepBuilderAPI_MakeEdge edge(curve);
  if (!edge.IsDone())
    return Result<TopoDS_Shape>::failure(
        geometry_error(spline.id().value(), "could not build 3D spline edge"));
  return Result<TopoDS_Shape>::success(edge.Edge());
}

struct ResolvedAxis {
  Point3 origin;
  Vector3 direction;
};

[[nodiscard]] Result<ResolvedAxis> resolve_axis(const PartDocument& document,
                                                const SketchHelixAxis3D& axis) {
  if (axis.source() == SketchHelixAxis3DSource::ConstructionLine) {
    auto line = ConstructionLineResolver{}.resolve(document, *axis.construction_line());
    if (line.has_error())
      return Result<ResolvedAxis>::failure(line.error());
    return Result<ResolvedAxis>::success({line.value().point, line.value().direction});
  }
  const DatumAxis* datum = document.find_datum_axis(*axis.datum_axis());
  if (datum == nullptr)
    return Result<ResolvedAxis>::failure(
        geometry_error(axis.datum_axis()->value(), "helix DatumAxis must exist"));
  if (datum->kind() == DatumAxisKind::Explicit)
    return Result<ResolvedAxis>::success({datum->origin(), datum->direction()});
  auto line = ConstructionLineResolver{}.resolve(document, datum->source_construction_line());
  if (line.has_error())
    return Result<ResolvedAxis>::failure(line.error());
  return Result<ResolvedAxis>::success({line.value().point, line.value().direction});
}

[[nodiscard]] Result<TopoDS_Shape> make_helix(const PartDocument& document,
                                              const SketchHelix3D& helix) {
  auto axis = resolve_axis(document, helix.axis());
  if (axis.has_error())
    return Result<TopoDS_Shape>::failure(axis.error());
  const Parameter* radius = document.find_parameter(helix.radius_parameter());
  const Parameter* pitch = document.find_parameter(helix.pitch_parameter());
  const Parameter* turns = document.find_parameter(helix.turns_parameter());
  if (radius == nullptr || pitch == nullptr || turns == nullptr)
    return Result<TopoDS_Shape>::failure(
        geometry_error(helix.id().value(), "helix parameters must exist"));
  const double pitch_per_radian = pitch->value().millimeters() / (2.0 * k_pi);
  const double sign = helix.handedness() == SketchHelix3DHandedness::RightHanded ? 1.0 : -1.0;
  const double direction_length = std::hypot(1.0, pitch_per_radian);
  Handle(Geom2d_Line) curve = new Geom2d_Line(
      gp_Pnt2d(0.0, 0.0), gp_Dir2d(sign / direction_length, pitch_per_radian / direction_length));
  Handle(Geom_CylindricalSurface) surface = new Geom_CylindricalSurface(
      gp_Ax3(point(axis.value().origin),
             gp_Dir(axis.value().direction.x, axis.value().direction.y, axis.value().direction.z)),
      radius->value().millimeters());
  const double last =
      direction_length * 2.0 * k_pi * static_cast<double>(turns->value().count_value());
  BRepBuilderAPI_MakeEdge builder(curve, surface, 0.0, last);
  if (!builder.IsDone())
    return Result<TopoDS_Shape>::failure(
        geometry_error(helix.id().value(), "could not build 3D helix edge"));
  TopoDS_Edge edge = builder.Edge();
  BRepLib::BuildCurves3d(edge);
  return Result<TopoDS_Shape>::success(edge);
}

} // namespace

Sketch3DGeometryResult::Sketch3DGeometryResult(Sketch3DId sketch_id,
                                               std::vector<ResolvedSketchPoint3D> points,
                                               std::vector<Sketch3DGeometryProduct> products)
    : sketch_id_(std::move(sketch_id)), points_(std::move(points)), products_(std::move(products)) {
}

const Sketch3DId& Sketch3DGeometryResult::sketch_id() const noexcept {
  return sketch_id_;
}
const std::vector<ResolvedSketchPoint3D>& Sketch3DGeometryResult::points() const noexcept {
  return points_;
}
const std::vector<Sketch3DGeometryProduct>& Sketch3DGeometryResult::products() const noexcept {
  return products_;
}
const ResolvedSketchPoint3D* Sketch3DGeometryResult::find_point(SketchEntityId id) const noexcept {
  return find_resolved_point(points_, id);
}
const Sketch3DGeometryProduct*
Sketch3DGeometryResult::find_product(SketchEntityId id) const noexcept {
  const auto found = std::find_if(products_.begin(), products_.end(),
                                  [&id](const auto& product) { return product.entity_id == id; });
  return found == products_.end() ? nullptr : &*found;
}

Result<Sketch3DGeometryResult> Sketch3DGeometryAdapter::convert(const PartDocument& document,
                                                                Sketch3DId sketch_id) const {
  const Sketch3D* sketch = document.find_sketch_3d(sketch_id);
  if (sketch == nullptr)
    return Result<Sketch3DGeometryResult>::failure(
        geometry_error(sketch_id.value(), "3D sketch must exist in part document"));
  try {
    const auto wrap_shape = [](TopoDS_Shape value) {
      return GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(value)));
    };
    std::vector<ResolvedSketchPoint3D> points;
    std::vector<Sketch3DGeometryProduct> products;
    points.reserve(sketch->points().size());
    products.reserve(sketch->entity_count() - sketch->guide_curves().size());
    for (const auto& source : sketch->points()) {
      auto resolved = resolve_owned_point(document, source);
      if (resolved.has_error())
        return Result<Sketch3DGeometryResult>::failure(resolved.error());
      BRepBuilderAPI_MakeVertex builder(point(resolved.value()));
      if (!builder.IsDone())
        return Result<Sketch3DGeometryResult>::failure(
            geometry_error(source.id().value(), "could not build 3D sketch point vertex"));
      points.push_back({source.id(), resolved.value()});
      products.push_back({source.id(), Sketch3DGeometryProductKind::Point,
                          wrap_shape(builder.Vertex()), 1U, 0U, 0U});
    }
    for (const auto& source : sketch->lines()) {
      const auto* start = find_resolved_point(points, source.start_point());
      const auto* end = find_resolved_point(points, source.end_point());
      if (start == nullptr || end == nullptr)
        return Result<Sketch3DGeometryResult>::failure(
            geometry_error(source.id().value(), "3D line endpoints must be resolved"));
      auto product = make_line(start->position, end->position, source.id());
      if (product.has_error())
        return Result<Sketch3DGeometryResult>::failure(product.error());
      products.push_back({source.id(), Sketch3DGeometryProductKind::Line,
                          wrap_shape(std::move(product.value())), 2U, 1U, 0U});
    }
    for (const auto& source : sketch->polylines()) {
      std::vector<Point3> vertices;
      for (const auto& id : source.ordered_vertices()) {
        const auto* resolved = find_resolved_point(points, id);
        if (resolved == nullptr)
          return Result<Sketch3DGeometryResult>::failure(
              geometry_error(source.id().value(), "3D polyline vertices must be resolved"));
        vertices.push_back(resolved->position);
      }
      auto product = make_polyline(vertices, source.id());
      if (product.has_error())
        return Result<Sketch3DGeometryResult>::failure(product.error());
      products.push_back({source.id(), Sketch3DGeometryProductKind::Polyline,
                          wrap_shape(std::move(product.value())), vertices.size(),
                          vertices.size() - 1U, 1U});
    }
    for (const auto& source : sketch->arcs()) {
      auto resolved = resolve_references(document, points, source.point_references());
      if (resolved.has_error())
        return Result<Sketch3DGeometryResult>::failure(resolved.error());
      auto product = make_arc(resolved.value(), source.id());
      if (product.has_error())
        return Result<Sketch3DGeometryResult>::failure(product.error());
      products.push_back({source.id(), Sketch3DGeometryProductKind::Arc,
                          wrap_shape(std::move(product.value())), 2U, 1U, 0U});
    }
    for (const auto& source : sketch->splines()) {
      auto resolved = resolve_references(document, points, source.ordered_points());
      if (resolved.has_error())
        return Result<Sketch3DGeometryResult>::failure(resolved.error());
      auto product = make_spline(resolved.value(), source);
      if (product.has_error())
        return Result<Sketch3DGeometryResult>::failure(product.error());
      products.push_back({source.id(), Sketch3DGeometryProductKind::Spline,
                          wrap_shape(std::move(product.value())), 2U, 1U, 0U});
    }
    for (const auto& source : sketch->helices()) {
      auto product = make_helix(document, source);
      if (product.has_error())
        return Result<Sketch3DGeometryResult>::failure(product.error());
      products.push_back({source.id(), Sketch3DGeometryProductKind::Helix,
                          wrap_shape(std::move(product.value())), 2U, 1U, 0U});
    }
    return Result<Sketch3DGeometryResult>::success(
        Sketch3DGeometryResult(sketch->id(), std::move(points), std::move(products)));
  } catch (const Standard_Failure& failure) {
    return Result<Sketch3DGeometryResult>::failure(
        geometry_error(sketch_id.value(), failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<Sketch3DGeometryResult>::failure(
        geometry_error(sketch_id.value(), exception.what()));
  }
}

} // namespace blcad::geometry
