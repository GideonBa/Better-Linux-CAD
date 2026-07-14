#include "blcad/geometry/fillet_adapter.hpp"

#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
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

[[nodiscard]] Error fillet_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
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
        fillet_error(id, matches.empty() ? "semantic fillet edge no longer exists on target body"
                                         : "semantic fillet edge is ambiguous on target body"));
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
        fillet_error(id, "circular fillet edge producer must be a subtractive extrude"));
  const Sketch* sketch = document.find_sketch(source->input_sketch());
  const CircleProfile* profile =
      sketch == nullptr ? nullptr : sketch->find_circle_profile(reference.source_profile());
  if (sketch == nullptr || profile == nullptr)
    return Result<ExpectedCircle>::failure(
        fillet_error(id, "circular fillet edge profile must exist"));
  const Parameter* diameter = document.find_parameter(profile->diameter_parameter());
  if (diameter == nullptr || diameter->type() != ParameterType::Length)
    return Result<ExpectedCircle>::failure(
        fillet_error(id, "circular fillet edge diameter must resolve to Length"));
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
[[nodiscard]] double dot(Vector3 a, Vector3 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
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
        fillet_error(id, "semantic circular fillet edge no longer exists on target body"));
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
          fillet_error(id, "semantic circular fillet edge is ambiguous on target body"));
  }
  return Result<TopoDS_Edge>::success(matches[index].edge);
}
} // namespace

Result<GeometryShape> FilletAdapter::apply(const PartDocument& document,
                                           const FilletFeature& feature,
                                           const GeometryShape& target,
                                           const ShapeCache& shape_cache, double radius_mm) const {
  if (target.empty() || !std::isfinite(radius_mm) || radius_mm <= 0.0)
    return Result<GeometryShape>::failure(
        fillet_error(feature.id(), "fillet target and positive finite radius are required"));
  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;
    BRepFilletAPI_MakeFillet builder(target_shape);
    for (const auto& reference : feature.edges()) {
      auto edge =
          std::holds_alternative<SemanticEdgeReference>(reference.source())
              ? resolve_linear(document, shape_cache, target_shape,
                               std::get<SemanticEdgeReference>(reference.source()), feature.id())
              : resolve_circular(document, target_shape,
                                 std::get<SemanticCircularEdgeReference>(reference.source()),
                                 feature.id());
      if (edge.has_error())
        return Result<GeometryShape>::failure(edge.error());
      builder.Add(radius_mm, edge.value());
    }
    builder.Build();
    if (!builder.IsDone())
      return Result<GeometryShape>::failure(
          fillet_error(feature.id(), "OCCT fillet did not complete; radius may be too large"));
    TopoDS_Shape result = builder.Shape();
    if (result.IsNull() || !TopExp_Explorer(result, TopAbs_SOLID).More() ||
        !BRepCheck_Analyzer(result, true).IsValid())
      return Result<GeometryShape>::failure(
          fillet_error(feature.id(), "fillet produced an invalid solid result"));
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(result))));
  } catch (const Standard_Failure& failure) {
    const char* message = failure.GetMessageString();
    return Result<GeometryShape>::failure(fillet_error(
        feature.id(), message == nullptr ? "OCCT fillet failed" : std::string(message)));
  }
}
} // namespace blcad::geometry
