#include "blcad/geometry/surface_adapter.hpp"

#include "blcad/geometry/semantic_reference_evaluator.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFill_Filling.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_Shape.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <array>
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

[[nodiscard]] double dot(Vector3 left, Vector3 right) {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vector3 normalized(Vector3 value) {
  const double magnitude = std::sqrt(dot(value, value));
  return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

[[nodiscard]] Vector3 cross(Vector3 left, Vector3 right) {
  return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

[[nodiscard]] Point3 point3(const gp_Pnt& value) {
  return {value.X(), value.Y(), value.Z()};
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

[[nodiscard]] std::size_t face_count(const TopoDS_Shape& shape) {
  std::size_t count = 0U;
  for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next())
    ++count;
  return count;
}

[[nodiscard]] Result<TopoDS_Face>
single_surface_face(const FeatureId& id, const TopoDS_Shape& shape, std::string_view role) {
  if (shape.IsNull())
    return Result<TopoDS_Face>::failure(surface_error(id, std::string(role) + " is empty"));
  std::vector<TopoDS_Face> faces;
  for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next())
    faces.push_back(TopoDS::Face(explorer.Current()));
  if (faces.size() != 1U)
    return Result<TopoDS_Face>::failure(
        surface_error(id, std::string(role) + " must resolve to exactly one unambiguous face"));
  return Result<TopoDS_Face>::success(faces.front());
}

[[nodiscard]] std::array<SemanticVertex, 3> face_vertices(SemanticFace face) {
  switch (face) {
  case SemanticFace::Top:
    return {SemanticVertex::TopFrontRight, SemanticVertex::TopFrontLeft,
            SemanticVertex::TopBackRight};
  case SemanticFace::Bottom:
    return {SemanticVertex::BottomFrontRight, SemanticVertex::BottomBackRight,
            SemanticVertex::BottomFrontLeft};
  case SemanticFace::Right:
    return {SemanticVertex::BottomFrontRight, SemanticVertex::TopFrontRight,
            SemanticVertex::BottomBackRight};
  case SemanticFace::Left:
    return {SemanticVertex::BottomFrontLeft, SemanticVertex::BottomBackLeft,
            SemanticVertex::TopFrontLeft};
  case SemanticFace::Front:
    return {SemanticVertex::BottomFrontRight, SemanticVertex::BottomFrontLeft,
            SemanticVertex::TopFrontRight};
  case SemanticFace::Back:
    return {SemanticVertex::BottomBackRight, SemanticVertex::TopBackRight,
            SemanticVertex::BottomBackLeft};
  }
  return {SemanticVertex::TopFrontRight, SemanticVertex::TopFrontLeft,
          SemanticVertex::TopBackRight};
}

[[nodiscard]] Result<std::pair<Point3, Vector3>>
expected_plane(const FeatureId& id, const PartDocument& document, const ShapeCache& cache,
               const SemanticFaceReference& reference) {
  const auto vertices = face_vertices(reference.face());
  std::array<Point3, 3> points;
  for (std::size_t index = 0U; index < vertices.size(); ++index) {
    auto semantic = SemanticVertexReference::create(reference.source_feature(), vertices[index]);
    if (semantic.has_error())
      return Result<std::pair<Point3, Vector3>>::failure(semantic.error());
    auto resolved = SemanticReferenceEvaluator{}.resolve_vertex(document, semantic.value(), cache);
    if (resolved.has_error())
      return Result<std::pair<Point3, Vector3>>::failure(resolved.error());
    points[index] = resolved.value().position;
  }
  const Vector3 first{points[1].x - points[0].x, points[1].y - points[0].y,
                      points[1].z - points[0].z};
  const Vector3 second{points[2].x - points[0].x, points[2].y - points[0].y,
                       points[2].z - points[0].z};
  const Vector3 normal = cross(first, second);
  if (dot(normal, normal) <= kTolerance * kTolerance)
    return Result<std::pair<Point3, Vector3>>::failure(
        surface_error(id, "semantic surface face has degenerate source geometry"));
  return Result<std::pair<Point3, Vector3>>::success({points[0], normalized(normal)});
}

[[nodiscard]] Result<TopoDS_Face> planar_trim_face(const FeatureId& id,
                                                   const std::vector<SweepPathSegment>& boundary) {
  auto edges = ordered_edges(id, {boundary}, true);
  if (edges.has_error())
    return Result<TopoDS_Face>::failure(edges.error());
  BRepBuilderAPI_MakeWire wire;
  for (const auto& edge : edges.value())
    wire.Add(edge);
  if (!wire.IsDone())
    return Result<TopoDS_Face>::failure(
        surface_error(id, "could not build the closed trimming wire"));
  BRepBuilderAPI_MakeFace face(wire.Wire());
  if (!face.IsDone())
    return Result<TopoDS_Face>::failure(
        surface_error(id, "trimming boundary must form one planar closed face"));
  return Result<TopoDS_Face>::success(face.Face());
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

// Gathers every face across the ordered stitch input shapes, rejecting empty
// inputs, solids, and faceless surfaces so that stitching only ever sees faces.
[[nodiscard]] Result<std::vector<TopoDS_Face>>
stitch_input_faces(const FeatureId& id, const std::vector<TopoDS_Shape>& shapes) {
  std::vector<TopoDS_Face> faces;
  for (const auto& shape : shapes) {
    if (shape.IsNull())
      return Result<std::vector<TopoDS_Face>>::failure(
          surface_error(id, "surface stitch input must not be empty"));
    if (TopExp_Explorer(shape, TopAbs_SOLID).More())
      return Result<std::vector<TopoDS_Face>>::failure(
          surface_error(id, "surface stitch inputs must be surfaces, not solids"));
    const std::size_t before = faces.size();
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next())
      faces.push_back(TopoDS::Face(explorer.Current()));
    if (faces.size() == before)
      return Result<std::vector<TopoDS_Face>>::failure(
          surface_error(id, "surface stitch input contains no face"));
  }
  return Result<std::vector<TopoDS_Face>>::success(std::move(faces));
}

// True when any edge of the shell is shared by more than two faces.
[[nodiscard]] bool is_non_manifold_shell(const TopoDS_Shell& shell) {
  TopTools_IndexedDataMapOfShapeListOfShape edge_faces;
  TopExp::MapShapesAndAncestors(shell, TopAbs_EDGE, TopAbs_FACE, edge_faces);
  for (Standard_Integer index = 1; index <= edge_faces.Extent(); ++index)
    if (edge_faces.FindFromIndex(index).Extent() > 2)
      return true;
  return false;
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

Result<GeometryShape>
SurfaceAdapter::extract_semantic_face(FeatureId feature_id, const PartDocument& document,
                                      const ShapeCache& shape_cache,
                                      const SemanticFaceReference& reference) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("surface_face", "feature id must not be empty"));
  const GeometryShape* source = shape_cache.find_feature_shape(reference.source_feature());
  if (source == nullptr || source->empty())
    return Result<GeometryShape>::failure(surface_error(
        feature_id, "semantic surface source feature shape must exist in the shape cache"));
  auto expected = expected_plane(feature_id, document, shape_cache, reference);
  if (expected.has_error())
    return Result<GeometryShape>::failure(expected.error());

  std::vector<TopoDS_Face> matches;
  for (TopExp_Explorer explorer(source->impl_->shape, TopAbs_FACE); explorer.More();
       explorer.Next()) {
    const TopoDS_Face face = TopoDS::Face(explorer.Current());
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane)
      continue;
    const gp_Pln plane = surface.Plane();
    const gp_Dir direction = plane.Axis().Direction();
    const Vector3 normal{direction.X(), direction.Y(), direction.Z()};
    if (std::abs(std::abs(dot(normal, expected.value().second)) - 1.0) > kTolerance)
      continue;
    const gp_Pnt location = plane.Location();
    const Vector3 delta{expected.value().first.x - location.X(),
                        expected.value().first.y - location.Y(),
                        expected.value().first.z - location.Z()};
    if (std::abs(dot(delta, normal)) <= kTolerance)
      matches.push_back(face);
  }
  if (matches.size() != 1U)
    return Result<GeometryShape>::failure(surface_error(
        feature_id, matches.empty() ? "semantic surface face no longer exists on source feature"
                                    : "semantic surface face is ambiguous on source feature"));
  TopoDS_Shape face = matches.front();
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(face))));
}

Result<GeometryShape>
SurfaceAdapter::trim_surface(FeatureId feature_id, const GeometryShape& target,
                             const std::vector<SweepPathSegment>& closed_boundary) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("trim_surface", "feature id must not be empty"));
  auto target_face = single_surface_face(feature_id, target.impl_->shape, "trim target");
  if (target_face.has_error())
    return Result<GeometryShape>::failure(target_face.error());
  auto trimming_face = planar_trim_face(feature_id, closed_boundary);
  if (trimming_face.has_error())
    return Result<GeometryShape>::failure(trimming_face.error());
  try {
    BRepAlgoAPI_Common common(target_face.value(), trimming_face.value());
    common.Build();
    if (!common.IsDone())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "OCCT could not trim the target surface"));
    TopoDS_Shape result = common.Shape();
    if (face_count(result) != 1U)
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "trimming result is empty, disconnected, or ambiguous"));
    auto checked = checked_shape(feature_id, std::move(result));
    if (checked.has_error())
      return Result<GeometryShape>::failure(checked.error());
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(checked.value()))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(surface_error(feature_id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(surface_error(feature_id, exception.what()));
  }
}

Result<GeometryShape> SurfaceAdapter::extend_surface(FeatureId feature_id,
                                                     const GeometryShape& target,
                                                     const std::vector<SweepPathSegment>& boundary,
                                                     double distance_mm) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("extend_surface", "feature id must not be empty"));
  if (!std::isfinite(distance_mm) || distance_mm <= 0.0)
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "surface extension distance must be positive and finite"));
  if (boundary.size() != 1U || boundary.front().kind != SweepPathSegmentKind::Line)
    return Result<GeometryShape>::failure(surface_error(
        feature_id, "surface extension currently requires one linear boundary segment"));
  auto target_face = single_surface_face(feature_id, target.impl_->shape, "extend target");
  if (target_face.has_error())
    return Result<GeometryShape>::failure(target_face.error());

  try {
    std::vector<TopoDS_Edge> matches;
    for (TopExp_Explorer explorer(target_face.value(), TopAbs_EDGE); explorer.More();
         explorer.Next()) {
      const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
      TopoDS_Vertex first_vertex;
      TopoDS_Vertex second_vertex;
      TopExp::Vertices(edge, first_vertex, second_vertex);
      if (first_vertex.IsNull() || second_vertex.IsNull())
        continue;
      const Point3 first = point3(BRep_Tool::Pnt(first_vertex));
      const Point3 second = point3(BRep_Tool::Pnt(second_vertex));
      if ((same_point(first, boundary.front().start) && same_point(second, boundary.front().end)) ||
          (same_point(first, boundary.front().end) && same_point(second, boundary.front().start)))
        matches.push_back(edge);
    }
    if (matches.size() != 1U)
      return Result<GeometryShape>::failure(surface_error(
          feature_id, matches.empty() ? "extension boundary no longer matches the target surface"
                                      : "extension boundary is ambiguous on the target surface"));

    const Point3 start = boundary.front().start;
    const Point3 end = boundary.front().end;
    const Vector3 edge_direction = normalized({end.x - start.x, end.y - start.y, end.z - start.z});
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(target_face.value(), properties);
    const Point3 center = point3(properties.CentreOfMass());
    const Point3 middle{(start.x + end.x) / 2.0, (start.y + end.y) / 2.0, (start.z + end.z) / 2.0};
    const Vector3 toward_center{center.x - middle.x, center.y - middle.y, center.z - middle.z};
    const double along_edge = dot(toward_center, edge_direction);
    const Vector3 inward{toward_center.x - edge_direction.x * along_edge,
                         toward_center.y - edge_direction.y * along_edge,
                         toward_center.z - edge_direction.z * along_edge};
    if (dot(inward, inward) <= kTolerance * kTolerance)
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "extension boundary has no unique outward direction"));
    const Vector3 inward_direction = normalized(inward);
    const Vector3 outward{-inward_direction.x, -inward_direction.y, -inward_direction.z};
    const Vector3 plane_normal = normalized(cross(edge_direction, outward));
    for (TopExp_Explorer explorer(target_face.value(), TopAbs_VERTEX); explorer.More();
         explorer.Next()) {
      const Point3 vertex = point3(BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current())));
      const Vector3 delta{vertex.x - start.x, vertex.y - start.y, vertex.z - start.z};
      if (std::abs(dot(delta, plane_normal)) > kTolerance)
        return Result<GeometryShape>::failure(
            surface_error(feature_id, "extend target must be geometrically planar"));
    }
    const Point3 extended_start{start.x + outward.x * distance_mm,
                                start.y + outward.y * distance_mm,
                                start.z + outward.z * distance_mm};
    const Point3 extended_end{end.x + outward.x * distance_mm, end.y + outward.y * distance_mm,
                              end.z + outward.z * distance_mm};

    std::size_t wire_count = 0U;
    for (TopExp_Explorer explorer(target_face.value(), TopAbs_WIRE); explorer.More();
         explorer.Next())
      ++wire_count;
    if (wire_count != 1U)
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "surface extension requires one outer boundary wire"));
    const TopoDS_Wire outer = BRepTools::OuterWire(target_face.value());
    std::vector<Point3> vertices;
    std::vector<TopoDS_Edge> ordered_boundary_edges;
    for (BRepTools_WireExplorer explorer(outer, target_face.value()); explorer.More();
         explorer.Next()) {
      const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
      if (BRepAdaptor_Curve(edge).GetType() != GeomAbs_Line)
        return Result<GeometryShape>::failure(surface_error(
            feature_id, "surface extension currently requires a linear polygon boundary"));
      vertices.push_back(point3(BRep_Tool::Pnt(explorer.CurrentVertex())));
      ordered_boundary_edges.push_back(edge);
    }
    if (vertices.size() < 3U)
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "surface extension target boundary is incomplete"));
    const auto selected =
        std::find_if(ordered_boundary_edges.begin(), ordered_boundary_edges.end(),
                     [&matches](const TopoDS_Edge& edge) { return edge.IsSame(matches.front()); });
    if (selected == ordered_boundary_edges.end())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "extension edge is missing from the outer boundary"));
    const std::size_t selected_index =
        static_cast<std::size_t>(std::distance(ordered_boundary_edges.begin(), selected));
    const Point3 ordered_start = vertices[selected_index];
    const bool authored_order = same_point(ordered_start, start);

    BRepBuilderAPI_MakePolygon polygon;
    for (std::size_t index = 0U; index < vertices.size(); ++index) {
      polygon.Add(point(vertices[index]));
      if (index == selected_index) {
        polygon.Add(point(authored_order ? extended_start : extended_end));
        polygon.Add(point(authored_order ? extended_end : extended_start));
      }
    }
    polygon.Close();
    if (!polygon.IsDone())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "could not build the extended surface boundary"));
    BRepBuilderAPI_MakeFace extended_face(polygon.Wire());
    if (!extended_face.IsDone())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "could not build the extended surface face"));
    TopoDS_Shape result = extended_face.Face();
    auto checked = checked_shape(feature_id, std::move(result));
    if (checked.has_error())
      return Result<GeometryShape>::failure(checked.error());
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(checked.value()))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(surface_error(feature_id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(surface_error(feature_id, exception.what()));
  }
}

Result<GeometryShape>
SurfaceAdapter::stitch_surfaces(FeatureId feature_id,
                                const std::vector<GeometryShape>& surfaces,
                                double tolerance_mm) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("surface_stitch", "feature id must not be empty"));
  if (!std::isfinite(tolerance_mm) || tolerance_mm <= 0.0)
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "surface stitch tolerance must be positive and finite"));
  if (surfaces.size() < 2U)
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "surface stitch requires at least two surfaces"));
  std::vector<TopoDS_Shape> shapes;
  shapes.reserve(surfaces.size());
  for (const auto& surface : surfaces) {
    if (surface.empty())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "surface stitch input must not be empty"));
    shapes.push_back(surface.impl_->shape);
  }
  auto faces = stitch_input_faces(feature_id, shapes);
  if (faces.has_error())
    return Result<GeometryShape>::failure(faces.error());
  const std::size_t input_face_count = faces.value().size();
  if (input_face_count < 2U)
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "surface stitch requires at least two faces"));

  try {
    BRepBuilderAPI_Sewing sewing(tolerance_mm);
    for (const auto& face : faces.value())
      sewing.Add(face);
    sewing.Perform();
    const TopoDS_Shape sewn = sewing.SewedShape();
    if (sewn.IsNull())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "OCCT could not stitch the surface set"));
    if (sewing.NbMultipleEdges() > 0)
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "stitched shell is non-manifold"));

    std::vector<TopoDS_Shell> shells;
    for (TopExp_Explorer explorer(sewn, TopAbs_SHELL); explorer.More(); explorer.Next())
      shells.push_back(TopoDS::Shell(explorer.Current()));
    if (shells.size() != 1U)
      return Result<GeometryShape>::failure(surface_error(
          feature_id, "stitched surfaces do not form one connected shell within tolerance"));
    const TopoDS_Shell& shell = shells.front();
    if (face_count(shell) != input_face_count || face_count(sewn) != input_face_count)
      return Result<GeometryShape>::failure(surface_error(
          feature_id, "stitched shell left free faces or gaps outside tolerance"));
    if (is_non_manifold_shell(shell))
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "stitched shell is non-manifold"));

    auto checked = checked_shape(feature_id, shell);
    if (checked.has_error())
      return Result<GeometryShape>::failure(checked.error());
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(checked.value()))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(surface_error(feature_id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(surface_error(feature_id, exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(surface_error(feature_id, "unknown surface stitch error"));
  }
}

Result<GeometryShape> SurfaceAdapter::close_shell_to_solid(FeatureId feature_id,
                                                           const GeometryShape& shell) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("closed_shell_to_solid", "feature id must not be empty"));
  if (shell.empty())
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "closed-shell-to-solid input must not be empty"));
  const TopoDS_Shape& input = shell.impl_->shape;
  if (TopExp_Explorer(input, TopAbs_SOLID).More())
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "closed-shell-to-solid input must be a surface shell, not a solid"));

  std::vector<TopoDS_Shell> shells;
  for (TopExp_Explorer explorer(input, TopAbs_SHELL); explorer.More(); explorer.Next())
    shells.push_back(TopoDS::Shell(explorer.Current()));
  if (shells.size() != 1U)
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "closed-shell-to-solid requires exactly one connected shell"));
  TopoDS_Shell shell_topology = shells.front();

  // A closed manifold shell shares every edge between exactly two faces: a free
  // edge means the shell is open, more than two faces means it is non-manifold.
  TopTools_IndexedDataMapOfShapeListOfShape edge_faces;
  TopExp::MapShapesAndAncestors(shell_topology, TopAbs_EDGE, TopAbs_FACE, edge_faces);
  for (Standard_Integer index = 1; index <= edge_faces.Extent(); ++index) {
    const Standard_Integer count = edge_faces.FindFromIndex(index).Extent();
    if (count < 2)
      return Result<GeometryShape>::failure(surface_error(
          feature_id, "closed-shell-to-solid requires a closed shell with no free edges"));
    if (count > 2)
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "closed-shell-to-solid rejects a non-manifold shell"));
  }
  if (!BRepCheck_Analyzer(shell_topology, true).IsValid())
    return Result<GeometryShape>::failure(surface_error(
        feature_id, "closed-shell-to-solid requires a consistently oriented valid shell"));

  try {
    shell_topology.Closed(Standard_True);
    BRepBuilderAPI_MakeSolid maker(shell_topology);
    if (!maker.IsDone())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "OCCT could not build a solid from the closed shell"));
    TopoDS_Solid solid = maker.Solid();
    BRepLib::OrientClosedSolid(solid);
    if (solid.IsNull() || !BRepCheck_Analyzer(solid, true).IsValid())
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "closed-shell-to-solid produced a topologically invalid solid"));
    std::size_t solid_count = 0U;
    for (TopExp_Explorer explorer(solid, TopAbs_SOLID); explorer.More(); explorer.Next())
      ++solid_count;
    if (solid_count != 1U)
      return Result<GeometryShape>::failure(
          surface_error(feature_id, "closed-shell-to-solid must produce exactly one solid"));
    GProp_GProps volume;
    BRepGProp::VolumeProperties(solid, volume);
    if (!(volume.Mass() > kTolerance))
      return Result<GeometryShape>::failure(surface_error(
          feature_id, "closed-shell-to-solid produced a degenerate zero-volume solid"));
    TopoDS_Shape result = solid;
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(result))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(surface_error(feature_id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(surface_error(feature_id, exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(
        surface_error(feature_id, "unknown closed-shell-to-solid error"));
  }
}

} // namespace blcad::geometry
