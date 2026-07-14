#include "blcad/geometry/chamfer_adapter.hpp"

#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {
constexpr double kTolerance = 1.0e-6;
constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] Error chamfer_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}
[[nodiscard]] double dot(Vector3 a, Vector3 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
[[nodiscard]] double distance(Point3 a, Point3 b) noexcept {
  return std::hypot(std::hypot(a.x - b.x, a.y - b.y), a.z - b.z);
}
[[nodiscard]] Point3 point(const TopoDS_Vertex& vertex) {
  const gp_Pnt value = BRep_Tool::Pnt(vertex);
  return {value.X(), value.Y(), value.Z()};
}
[[nodiscard]] bool same_endpoints(const TopoDS_Edge& edge, Point3 first, Point3 second) {
  TopoDS_Vertex start;
  TopoDS_Vertex end;
  TopExp::Vertices(edge, start, end, true);
  if (start.IsNull() || end.IsNull())
    return false;
  const Point3 a = point(start);
  const Point3 b = point(end);
  return (distance(a, first) <= kTolerance && distance(b, second) <= kTolerance) ||
         (distance(a, second) <= kTolerance && distance(b, first) <= kTolerance);
}

[[nodiscard]] Result<TopoDS_Edge>
resolve_linear(const PartDocument& document, const ShapeCache& cache, const TopoDS_Shape& target,
               const SemanticEdgeReference& reference, const FeatureId& id) {
  auto semantic = SemanticReferenceEvaluator{}.resolve_edge(document, reference, cache);
  if (semantic.has_error())
    return Result<TopoDS_Edge>::failure(semantic.error());
  std::vector<TopoDS_Edge> matches;
  TopTools_IndexedMapOfShape edges;
  TopExp::MapShapes(target, TopAbs_EDGE, edges);
  for (int index = 1; index <= edges.Extent(); ++index) {
    const TopoDS_Edge edge = TopoDS::Edge(edges(index));
    if (same_endpoints(edge, semantic.value().start, semantic.value().end))
      matches.push_back(edge);
  }
  if (matches.size() != 1U)
    return Result<TopoDS_Edge>::failure(
        chamfer_error(id, matches.empty() ? "semantic chamfer edge no longer exists on target body"
                                          : "semantic chamfer edge is ambiguous on target body"));
  return Result<TopoDS_Edge>::success(matches.front());
}

struct ExpectedCircle {
  Point3 source_center;
  Vector3 axis;
  double radius_mm;
};

[[nodiscard]] Result<ExpectedCircle> expected_circle(const PartDocument& document,
                                                     const SemanticCircularEdgeReference& reference,
                                                     const FeatureId& id) {
  const Feature* source = document.find_feature(reference.source_feature());
  if (source == nullptr || source->type() != FeatureType::SubtractiveExtrude)
    return Result<ExpectedCircle>::failure(
        chamfer_error(id, "circular chamfer edge producer must be a subtractive extrude"));
  const Sketch* sketch = document.find_sketch(source->input_sketch());
  const CircleProfile* profile =
      sketch == nullptr ? nullptr : sketch->find_circle_profile(reference.source_profile());
  if (sketch == nullptr || profile == nullptr)
    return Result<ExpectedCircle>::failure(
        chamfer_error(id, "circular chamfer edge profile must exist"));
  const Parameter* diameter = document.find_parameter(profile->diameter_parameter());
  if (diameter == nullptr || diameter->type() != ParameterType::Length)
    return Result<ExpectedCircle>::failure(
        chamfer_error(id, "circular chamfer edge diameter must resolve to Length"));
  auto workplane = WorkplaneResolver{}.resolve_for_sketch(document, *sketch);
  if (workplane.has_error())
    return Result<ExpectedCircle>::failure(workplane.error());
  Vector3 axis = workplane.value().normal;
  if (source->direction() == ExtrudeDirection::OppositeSketchNormal)
    axis = {-axis.x, -axis.y, -axis.z};
  return Result<ExpectedCircle>::success(
      {WorkplaneResolver{}.evaluate_point(workplane.value(), profile->center()), axis,
       diameter->value().millimeters() / 2.0});
}

[[nodiscard]] Result<TopoDS_Edge> resolve_circular(const PartDocument& document,
                                                   const TopoDS_Shape& target,
                                                   const SemanticCircularEdgeReference& reference,
                                                   const FeatureId& id) {
  auto expected = expected_circle(document, reference, id);
  if (expected.has_error())
    return Result<TopoDS_Edge>::failure(expected.error());
  struct Candidate {
    TopoDS_Edge edge;
    double offset;
  };
  std::vector<Candidate> matches;
  TopTools_IndexedMapOfShape edges;
  TopExp::MapShapes(target, TopAbs_EDGE, edges);
  for (int index = 1; index <= edges.Extent(); ++index) {
    const TopoDS_Edge edge = TopoDS::Edge(edges(index));
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Circle)
      continue;
    const gp_Circ circle = curve.Circle();
    if (std::abs(circle.Radius() - expected.value().radius_mm) > kTolerance)
      continue;
    const gp_Dir direction = circle.Axis().Direction();
    const Vector3 curve_axis{direction.X(), direction.Y(), direction.Z()};
    if (std::abs(std::abs(dot(curve_axis, expected.value().axis)) - 1.0) > kTolerance)
      continue;
    const gp_Pnt location = circle.Location();
    const Vector3 delta{location.X() - expected.value().source_center.x,
                        location.Y() - expected.value().source_center.y,
                        location.Z() - expected.value().source_center.z};
    const double along = dot(delta, expected.value().axis);
    const Vector3 radial{delta.x - expected.value().axis.x * along,
                         delta.y - expected.value().axis.y * along,
                         delta.z - expected.value().axis.z * along};
    if (std::sqrt(dot(radial, radial)) <= kTolerance)
      matches.push_back({edge, along});
  }
  if (matches.empty())
    return Result<TopoDS_Edge>::failure(
        chamfer_error(id, "semantic circular chamfer edge no longer exists on target body"));
  std::sort(matches.begin(), matches.end(), [](const Candidate& a, const Candidate& b) {
    return std::abs(a.offset) < std::abs(b.offset);
  });
  const std::size_t index =
      reference.edge() == SemanticCircularEdge::SourceRim ? 0U : matches.size() - 1U;
  if (matches.size() > 1U) {
    const std::size_t neighbor = index == 0U ? 1U : matches.size() - 2U;
    if (std::abs(std::abs(matches[index].offset) - std::abs(matches[neighbor].offset)) <=
        kTolerance)
      return Result<TopoDS_Edge>::failure(
          chamfer_error(id, "semantic circular chamfer edge is ambiguous on target body"));
  }
  return Result<TopoDS_Edge>::success(matches[index].edge);
}

[[nodiscard]] Result<TopoDS_Edge> resolve_edge(const PartDocument& document,
                                               const ShapeCache& cache, const TopoDS_Shape& target,
                                               const EdgeReference& reference,
                                               const FeatureId& id) {
  return std::holds_alternative<SemanticEdgeReference>(reference.source())
             ? resolve_linear(document, cache, target,
                              std::get<SemanticEdgeReference>(reference.source()), id)
             : resolve_circular(document, target,
                                std::get<SemanticCircularEdgeReference>(reference.source()), id);
}

[[nodiscard]] Result<Vector3> preferred_linear_face_normal(const PartDocument& document,
                                                           const SemanticEdgeReference& reference,
                                                           const FeatureId& id) {
  const Feature* source = document.find_feature(reference.source_feature());
  const Sketch* sketch = source == nullptr ? nullptr : document.find_sketch(source->input_sketch());
  if (source == nullptr || sketch == nullptr)
    return Result<Vector3>::failure(
        chamfer_error(id, "linear chamfer edge source sketch must exist"));
  auto workplane = WorkplaneResolver{}.resolve_for_sketch(document, *sketch);
  if (workplane.has_error())
    return Result<Vector3>::failure(workplane.error());
  const auto negate = [](Vector3 value) { return Vector3{-value.x, -value.y, -value.z}; };
  switch (reference.edge()) {
  case SemanticEdge::TopFront:
  case SemanticEdge::TopBack:
  case SemanticEdge::TopRight:
  case SemanticEdge::TopLeft:
    return Result<Vector3>::success(workplane.value().normal);
  case SemanticEdge::BottomFront:
  case SemanticEdge::BottomBack:
  case SemanticEdge::BottomRight:
  case SemanticEdge::BottomLeft:
    return Result<Vector3>::success(negate(workplane.value().normal));
  case SemanticEdge::FrontRight:
  case SemanticEdge::FrontLeft:
    return Result<Vector3>::success(workplane.value().y_axis);
  case SemanticEdge::BackRight:
  case SemanticEdge::BackLeft:
    return Result<Vector3>::success(negate(workplane.value().y_axis));
  }
  return Result<Vector3>::failure(chamfer_error(id, "unsupported linear chamfer edge"));
}

[[nodiscard]] Result<TopoDS_Face>
select_distance_face(const PartDocument& document, const TopoDS_Shape& target,
                     const EdgeReference& reference, const TopoDS_Edge& edge, const FeatureId& id) {
  TopTools_IndexedDataMapOfShapeListOfShape edge_faces;
  TopExp::MapShapesAndAncestors(target, TopAbs_EDGE, TopAbs_FACE, edge_faces);
  if (!edge_faces.Contains(edge))
    return Result<TopoDS_Face>::failure(
        chamfer_error(id, "chamfer edge has no adjacent target face"));

  std::vector<TopoDS_Face> planar_faces;
  for (TopTools_ListIteratorOfListOfShape iterator(edge_faces.FindFromKey(edge)); iterator.More();
       iterator.Next()) {
    const TopoDS_Face face = TopoDS::Face(iterator.Value());
    if (BRepAdaptor_Surface(face).GetType() == GeomAbs_Plane)
      planar_faces.push_back(face);
  }
  if (planar_faces.empty())
    return Result<TopoDS_Face>::failure(
        chamfer_error(id, "distance chamfer requires an adjacent planar reference face"));

  if (std::holds_alternative<SemanticCircularEdgeReference>(reference.source())) {
    if (planar_faces.size() != 1U)
      return Result<TopoDS_Face>::failure(
          chamfer_error(id, "circular chamfer reference face is ambiguous"));
    return Result<TopoDS_Face>::success(planar_faces.front());
  }

  auto preferred = preferred_linear_face_normal(
      document, std::get<SemanticEdgeReference>(reference.source()), id);
  if (preferred.has_error())
    return Result<TopoDS_Face>::failure(preferred.error());
  std::vector<TopoDS_Face> matches;
  for (const TopoDS_Face& face : planar_faces) {
    const gp_Dir normal = BRepAdaptor_Surface(face).Plane().Axis().Direction();
    const Vector3 value{normal.X(), normal.Y(), normal.Z()};
    if (std::abs(std::abs(dot(value, preferred.value())) - 1.0) <= kTolerance)
      matches.push_back(face);
  }
  if (matches.size() != 1U)
    return Result<TopoDS_Face>::failure(
        chamfer_error(id, matches.empty() ? "semantic chamfer reference face no longer exists"
                                          : "semantic chamfer reference face is ambiguous"));
  return Result<TopoDS_Face>::success(matches.front());
}
} // namespace

Result<GeometryShape> ChamferAdapter::apply(const PartDocument& document,
                                            const ChamferFeature& feature,
                                            const GeometryShape& target,
                                            const ShapeCache& shape_cache, double first_value,
                                            std::optional<double> second_value) const {
  if (target.empty() || !std::isfinite(first_value) || first_value <= 0.0 ||
      (second_value.has_value() && (!std::isfinite(*second_value) || *second_value <= 0.0)))
    return Result<GeometryShape>::failure(
        chamfer_error(feature.id(), "chamfer target and positive finite parameters are required"));
  if ((feature.mode() == ChamferMode::EqualDistance) != !second_value.has_value())
    return Result<GeometryShape>::failure(
        chamfer_error(feature.id(), "chamfer parameter count does not match its mode"));

  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;
    BRepFilletAPI_MakeChamfer builder(target_shape);
    for (const EdgeReference& reference : feature.edges()) {
      auto edge = resolve_edge(document, shape_cache, target_shape, reference, feature.id());
      if (edge.has_error())
        return Result<GeometryShape>::failure(edge.error());
      if (feature.mode() == ChamferMode::EqualDistance) {
        builder.Add(first_value, edge.value());
        continue;
      }
      auto face =
          select_distance_face(document, target_shape, reference, edge.value(), feature.id());
      if (face.has_error())
        return Result<GeometryShape>::failure(face.error());
      if (feature.mode() == ChamferMode::TwoDistance)
        builder.Add(first_value, *second_value, edge.value(), face.value());
      else
        builder.AddDA(first_value, *second_value * kPi / 180.0, edge.value(), face.value());
    }
    builder.Build();
    if (!builder.IsDone())
      return Result<GeometryShape>::failure(
          chamfer_error(feature.id(), "OCCT chamfer did not complete; distance may be too large"));
    TopoDS_Shape result = builder.Shape();
    if (result.IsNull() || !TopExp_Explorer(result, TopAbs_SOLID).More() ||
        !BRepCheck_Analyzer(result, true).IsValid())
      return Result<GeometryShape>::failure(
          chamfer_error(feature.id(), "chamfer produced an invalid solid result"));
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(result))));
  } catch (const Standard_Failure& failure) {
    const char* message = failure.GetMessageString();
    return Result<GeometryShape>::failure(chamfer_error(
        feature.id(), message == nullptr ? "OCCT chamfer failed" : std::string(message)));
  }
}

} // namespace blcad::geometry
