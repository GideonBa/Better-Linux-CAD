#include "blcad/geometry/loft_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
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

constexpr double kTolerance = 1.0e-8;

[[nodiscard]] Error loft_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

[[nodiscard]] gp_Pnt point(Point3 value) {
  return {value.x, value.y, value.z};
}

[[nodiscard]] double distance(Point3 left, Point3 right) {
  return std::hypot(std::hypot(left.x - right.x, left.y - right.y), left.z - right.z);
}

[[nodiscard]] std::string failure_message(const Standard_Failure& failure) {
  const char* message = failure.GetMessageString();
  return message == nullptr || *message == '\0' ? "OCCT loft operation failed" : message;
}

[[nodiscard]] Result<TopoDS_Edge> closed_edge(const FeatureId& id,
                                              const ClosedProfileCurveSegment& segment) {
  if (distance(segment.start, segment.end) <= kTolerance)
    return Result<TopoDS_Edge>::failure(loft_error(id, "loft section edge is degenerate"));
  if (segment.kind == ClosedProfileCurveKind::Line) {
    BRepBuilderAPI_MakeEdge edge(point(segment.start), point(segment.end));
    if (!edge.IsDone())
      return Result<TopoDS_Edge>::failure(loft_error(id, "could not build loft line edge"));
    return Result<TopoDS_Edge>::success(edge.Edge());
  }
  if (segment.kind == ClosedProfileCurveKind::CircularArc) {
    GC_MakeArcOfCircle arc(point(segment.start), point(segment.mid), point(segment.end));
    if (!arc.IsDone())
      return Result<TopoDS_Edge>::failure(loft_error(id, "could not build loft arc"));
    BRepBuilderAPI_MakeEdge edge(arc.Value());
    if (!edge.IsDone())
      return Result<TopoDS_Edge>::failure(loft_error(id, "could not build loft arc edge"));
    return Result<TopoDS_Edge>::success(edge.Edge());
  }
  TColgp_Array1OfPnt poles(1, 4);
  poles.SetValue(1, point(segment.start));
  poles.SetValue(2, point(segment.mid));
  poles.SetValue(3, point(segment.control2));
  poles.SetValue(4, point(segment.end));
  Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles);
  BRepBuilderAPI_MakeEdge edge(curve);
  if (!edge.IsDone())
    return Result<TopoDS_Edge>::failure(loft_error(id, "could not build loft spline edge"));
  return Result<TopoDS_Edge>::success(edge.Edge());
}

[[nodiscard]] Result<TopoDS_Edge> open_edge(const FeatureId& id, const SweepPathSegment& segment) {
  if (distance(segment.start, segment.end) <= kTolerance)
    return Result<TopoDS_Edge>::failure(loft_error(id, "open loft section edge is degenerate"));
  if (segment.kind == SweepPathSegmentKind::Line) {
    BRepBuilderAPI_MakeEdge edge(point(segment.start), point(segment.end));
    if (!edge.IsDone())
      return Result<TopoDS_Edge>::failure(loft_error(id, "could not build open loft line"));
    return Result<TopoDS_Edge>::success(edge.Edge());
  }
  GC_MakeArcOfCircle arc(point(segment.start), point(segment.mid), point(segment.end));
  if (!arc.IsDone())
    return Result<TopoDS_Edge>::failure(loft_error(id, "could not build open loft arc"));
  BRepBuilderAPI_MakeEdge edge(arc.Value());
  if (!edge.IsDone())
    return Result<TopoDS_Edge>::failure(loft_error(id, "could not build open loft arc edge"));
  return Result<TopoDS_Edge>::success(edge.Edge());
}

template <typename Segment, typename Factory>
[[nodiscard]] Result<TopoDS_Wire> make_wire(const FeatureId& id,
                                            const std::vector<Segment>& section, bool closed,
                                            Factory&& factory) {
  if (section.empty())
    return Result<TopoDS_Wire>::failure(loft_error(id, "loft section must not be empty"));
  for (std::size_t index = 1U; index < section.size(); ++index)
    if (distance(section[index - 1U].end, section[index].start) > kTolerance)
      return Result<TopoDS_Wire>::failure(
          loft_error(id, "loft section edges must remain connected in authored order"));
  if (closed && distance(section.back().end, section.front().start) > kTolerance)
    return Result<TopoDS_Wire>::failure(loft_error(id, "solid loft section must be closed"));
  if (!closed && distance(section.back().end, section.front().start) <= kTolerance)
    return Result<TopoDS_Wire>::failure(loft_error(id, "open loft section must remain open"));
  BRepBuilderAPI_MakeWire wire;
  for (const auto& segment : section) {
    auto edge = factory(segment);
    if (edge.has_error())
      return Result<TopoDS_Wire>::failure(edge.error());
    wire.Add(edge.value());
  }
  if (!wire.IsDone())
    return Result<TopoDS_Wire>::failure(loft_error(id, "could not build loft section wire"));
  return Result<TopoDS_Wire>::success(wire.Wire());
}

template <typename Segment, typename Factory>
[[nodiscard]] Result<TopoDS_Shape> execute_loft(const FeatureId& id,
                                                const std::vector<std::vector<Segment>>& sections,
                                                bool closed, bool solid, Factory&& factory) {
  if (sections.size() < 2U)
    return Result<TopoDS_Shape>::failure(
        loft_error(id, "loft requires at least two ordered sections"));
  const std::size_t authored_edge_count = sections.front().size();
  for (const auto& section : sections)
    if (section.size() != authored_edge_count)
      return Result<TopoDS_Shape>::failure(
          loft_error(id, "all loft sections require matching authored edge counts"));
  try {
    BRepOffsetAPI_ThruSections builder(solid, Standard_False, kTolerance);
    builder.CheckCompatibility(Standard_False);
    for (const auto& section : sections) {
      auto wire = make_wire(id, section, closed, factory);
      if (wire.has_error())
        return Result<TopoDS_Shape>::failure(wire.error());
      builder.AddWire(wire.value());
    }
    builder.Build();
    if (!builder.IsDone())
      return Result<TopoDS_Shape>::failure(loft_error(id, "OCCT loft did not complete"));
    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull() || !BRepCheck_Analyzer(shape, true).IsValid())
      return Result<TopoDS_Shape>::failure(loft_error(id, "loft result is invalid"));
    if (solid) {
      if (!TopExp_Explorer(shape, TopAbs_SOLID).More())
        return Result<TopoDS_Shape>::failure(loft_error(id, "loft produced no solid"));
      GProp_GProps properties;
      BRepGProp::VolumeProperties(shape, properties);
      if (!std::isfinite(properties.Mass()) || properties.Mass() <= kTolerance)
        return Result<TopoDS_Shape>::failure(loft_error(id, "loft has no positive volume"));
    } else if (!TopExp_Explorer(shape, TopAbs_FACE).More() ||
               TopExp_Explorer(shape, TopAbs_SOLID).More()) {
      return Result<TopoDS_Shape>::failure(
          loft_error(id, "surface loft must produce faces without a solid"));
    }
    return Result<TopoDS_Shape>::success(std::move(shape));
  } catch (const Standard_Failure& failure) {
    return Result<TopoDS_Shape>::failure(loft_error(id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<TopoDS_Shape>::failure(loft_error(id, exception.what()));
  } catch (...) {
    return Result<TopoDS_Shape>::failure(loft_error(id, "unknown loft geometry error"));
  }
}

} // namespace

Result<GeometryShape> LoftAdapter::loft_closed_sections(
    FeatureId feature_id,
    const std::vector<std::vector<ClosedProfileCurveSegment>>& ordered_sections,
    bool make_solid) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("loft", "feature id must not be empty"));
  auto shape =
      execute_loft(feature_id, ordered_sections, true, make_solid,
                   [&feature_id](const auto& segment) { return closed_edge(feature_id, segment); });
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

Result<GeometryShape> LoftAdapter::loft_open_sections(
    FeatureId feature_id,
    const std::vector<std::vector<SweepPathSegment>>& ordered_sections) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("loft", "feature id must not be empty"));
  auto shape =
      execute_loft(feature_id, ordered_sections, false, false,
                   [&feature_id](const auto& segment) { return open_edge(feature_id, segment); });
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

} // namespace blcad::geometry
