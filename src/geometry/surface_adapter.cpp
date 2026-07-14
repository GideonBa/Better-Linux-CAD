#include "blcad/geometry/surface_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFill_Filling.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAbs_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr double kTolerance = 1.0e-7;

[[nodiscard]] Error surface_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

[[nodiscard]] gp_Pnt point(Point3 value) {
  return {value.x, value.y, value.z};
}

[[nodiscard]] double distance(Point3 left, Point3 right) {
  return std::hypot(std::hypot(left.x - right.x, left.y - right.y), left.z - right.z);
}

[[nodiscard]] bool same_point(Point3 left, Point3 right) {
  return distance(left, right) <= kTolerance;
}

[[nodiscard]] std::string failure_message(const Standard_Failure& failure) {
  const char* message = failure.GetMessageString();
  return message == nullptr || *message == '\0' ? "OCCT surface operation failed" : message;
}

[[nodiscard]] Result<TopoDS_Edge> make_edge(const FeatureId& id, const SweepPathSegment& segment) {
  if (same_point(segment.start, segment.end))
    return Result<TopoDS_Edge>::failure(
        surface_error(id, "surface boundary segment endpoints must be distinct"));
  if (segment.kind == SweepPathSegmentKind::Line) {
    BRepBuilderAPI_MakeEdge edge(point(segment.start), point(segment.end));
    if (!edge.IsDone())
      return Result<TopoDS_Edge>::failure(
          surface_error(id, "could not build surface boundary line"));
    return Result<TopoDS_Edge>::success(edge.Edge());
  }
  if (same_point(segment.start, segment.mid) || same_point(segment.mid, segment.end))
    return Result<TopoDS_Edge>::failure(
        surface_error(id, "surface boundary arc points must be distinct"));
  GC_MakeArcOfCircle arc(point(segment.start), point(segment.mid), point(segment.end));
  if (!arc.IsDone())
    return Result<TopoDS_Edge>::failure(surface_error(id, "could not build surface boundary arc"));
  BRepBuilderAPI_MakeEdge edge(arc.Value());
  if (!edge.IsDone())
    return Result<TopoDS_Edge>::failure(
        surface_error(id, "could not build surface boundary arc edge"));
  return Result<TopoDS_Edge>::success(edge.Edge());
}

[[nodiscard]] Result<std::vector<TopoDS_Edge>>
ordered_edges(const FeatureId& id, const std::vector<std::vector<SweepPathSegment>>& boundaries,
              bool require_closed) {
  if (boundaries.empty())
    return Result<std::vector<TopoDS_Edge>>::failure(
        surface_error(id, "surface requires at least one boundary"));
  std::vector<TopoDS_Edge> edges;
  for (std::size_t boundary_index = 0U; boundary_index < boundaries.size(); ++boundary_index) {
    const auto& boundary = boundaries[boundary_index];
    if (boundary.empty())
      return Result<std::vector<TopoDS_Edge>>::failure(
          surface_error(id, "surface boundary must contain at least one segment"));
    for (std::size_t segment_index = 1U; segment_index < boundary.size(); ++segment_index)
      if (!same_point(boundary[segment_index - 1U].end, boundary[segment_index].start))
        return Result<std::vector<TopoDS_Edge>>::failure(
            surface_error(id, "surface boundary segments must be connected in order"));
    if (boundary_index > 0U &&
        !same_point(boundaries[boundary_index - 1U].back().end, boundary.front().start))
      return Result<std::vector<TopoDS_Edge>>::failure(
          surface_error(id, "ordered surface boundaries must form one connected contour"));
    for (const auto& segment : boundary) {
      auto edge = make_edge(id, segment);
      if (edge.has_error())
        return Result<std::vector<TopoDS_Edge>>::failure(edge.error());
      edges.push_back(edge.value());
    }
  }
  if (require_closed && !same_point(boundaries.back().back().end, boundaries.front().front().start))
    return Result<std::vector<TopoDS_Edge>>::failure(
        surface_error(id, "surface boundary contour must be closed"));
  return Result<std::vector<TopoDS_Edge>>::success(std::move(edges));
}

[[nodiscard]] Result<TopoDS_Wire> make_wire(const FeatureId& id,
                                            const std::vector<SweepPathSegment>& boundary) {
  if (boundary.empty())
    return Result<TopoDS_Wire>::failure(
        surface_error(id, "surface section boundary must not be empty"));
  BRepBuilderAPI_MakeWire wire;
  for (const auto& segment : boundary) {
    auto edge = make_edge(id, segment);
    if (edge.has_error())
      return Result<TopoDS_Wire>::failure(edge.error());
    wire.Add(edge.value());
  }
  if (!wire.IsDone())
    return Result<TopoDS_Wire>::failure(surface_error(id, "could not build surface section wire"));
  return Result<TopoDS_Wire>::success(wire.Wire());
}

[[nodiscard]] Result<TopoDS_Shape> checked_shape(const FeatureId& id, TopoDS_Shape shape) {
  if (shape.IsNull() || !BRepCheck_Analyzer(shape, true).IsValid())
    return Result<TopoDS_Shape>::failure(
        surface_error(id, "surface result is null or topologically invalid"));
  if (!TopExp_Explorer(shape, TopAbs_FACE).More() || TopExp_Explorer(shape, TopAbs_SOLID).More())
    return Result<TopoDS_Shape>::failure(
        surface_error(id, "surface operation must produce faces without a solid"));
  return Result<TopoDS_Shape>::success(std::move(shape));
}

[[nodiscard]] Result<TopoDS_Shape>
fill(const FeatureId& id, const std::vector<std::vector<SweepPathSegment>>& boundaries) {
  auto edges = ordered_edges(id, boundaries, true);
  if (edges.has_error())
    return Result<TopoDS_Shape>::failure(edges.error());
  try {
    BRepFill_Filling filling;
    for (const auto& edge : edges.value())
      filling.Add(edge, GeomAbs_C0, Standard_True);
    filling.Build();
    if (!filling.IsDone())
      return Result<TopoDS_Shape>::failure(
          surface_error(id, "OCCT could not fill the closed surface boundary"));
    return checked_shape(id, filling.Face());
  } catch (const Standard_Failure& failure) {
    return Result<TopoDS_Shape>::failure(surface_error(id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<TopoDS_Shape>::failure(surface_error(id, exception.what()));
  } catch (...) {
    return Result<TopoDS_Shape>::failure(surface_error(id, "unknown surface filling error"));
  }
}

} // namespace

Result<GeometryShape> SurfaceAdapter::make_boundary_surface(
    FeatureId feature_id, const std::vector<std::vector<SweepPathSegment>>& boundaries) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("boundary_surface", "feature id must not be empty"));
  if (boundaries.size() < 2U || boundaries.size() > 4U)
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "boundary surface requires two to four boundary curves"));
  if (boundaries.size() != 2U) {
    auto shape = fill(feature_id, boundaries);
    if (shape.has_error())
      return Result<GeometryShape>::failure(shape.error());
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
  }

  try {
    auto first = make_wire(feature_id, boundaries[0]);
    auto second = make_wire(feature_id, boundaries[1]);
    if (first.has_error())
      return Result<GeometryShape>::failure(first.error());
    if (second.has_error())
      return Result<GeometryShape>::failure(second.error());
    BRepOffsetAPI_ThruSections loft(Standard_False, Standard_False, kTolerance);
    loft.CheckCompatibility(Standard_False);
    loft.AddWire(first.value());
    loft.AddWire(second.value());
    loft.Build();
    if (!loft.IsDone())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "OCCT could not build the two-boundary surface"));
    auto shape = checked_shape(feature_id, loft.Shape());
    if (shape.has_error())
      return Result<GeometryShape>::failure(shape.error());
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(surface_error(feature_id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(surface_error(feature_id, exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "unknown boundary surface error"));
  }
}

Result<GeometryShape> SurfaceAdapter::make_fill_surface(
    FeatureId feature_id, const std::vector<std::vector<SweepPathSegment>>& boundaries) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("fill_surface", "feature id must not be empty"));
  auto shape = fill(feature_id, boundaries);
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

} // namespace blcad::geometry
