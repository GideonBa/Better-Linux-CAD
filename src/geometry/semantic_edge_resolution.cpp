#include "semantic_edge_resolution.hpp"

#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::geometry {
namespace {
constexpr double kTolerance = 1.0e-6;

[[nodiscard]] Error edge_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

[[nodiscard]] std::string concat(std::string_view prefix, std::string_view label,
                                 std::string_view suffix) {
  std::string message;
  message.reserve(prefix.size() + label.size() + suffix.size());
  message.append(prefix).append(label).append(suffix);
  return message;
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
               const SemanticEdgeReference& reference, const FeatureId& id,
               std::string_view feature_label) {
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
    return Result<TopoDS_Edge>::failure(edge_error(
        id, matches.empty()
                ? concat("semantic ", feature_label, " edge no longer exists on target body")
                : concat("semantic ", feature_label, " edge is ambiguous on target body")));
  return Result<TopoDS_Edge>::success(matches.front());
}

struct ExpectedCircle {
  Point3 source_center;
  Vector3 axis;
  double radius_mm;
};
[[nodiscard]] Result<ExpectedCircle> expected_circle(const PartDocument& document,
                                                     const SemanticCircularEdgeReference& reference,
                                                     const FeatureId& id,
                                                     std::string_view feature_label) {
  const Feature* source = document.find_feature(reference.source_feature());
  if (source == nullptr || source->type() != FeatureType::SubtractiveExtrude)
    return Result<ExpectedCircle>::failure(edge_error(
        id, concat("circular ", feature_label, " edge producer must be a subtractive extrude")));
  const Sketch* sketch = document.find_sketch(source->input_sketch());
  const CircleProfile* profile =
      sketch == nullptr ? nullptr : sketch->find_circle_profile(reference.source_profile());
  if (sketch == nullptr || profile == nullptr)
    return Result<ExpectedCircle>::failure(
        edge_error(id, concat("circular ", feature_label, " edge profile must exist")));
  const Parameter* diameter = document.find_parameter(profile->diameter_parameter());
  if (diameter == nullptr || diameter->type() != ParameterType::Length)
    return Result<ExpectedCircle>::failure(edge_error(
        id, concat("circular ", feature_label, " edge diameter must resolve to Length")));
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
                                                   const FeatureId& id,
                                                   std::string_view feature_label) {
  auto expected = expected_circle(document, reference, id, feature_label);
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
    return Result<TopoDS_Edge>::failure(edge_error(
        id, concat("semantic circular ", feature_label, " edge no longer exists on target body")));
  std::sort(matches.begin(), matches.end(), [](const Candidate& a, const Candidate& b) {
    return std::abs(a.offset) < std::abs(b.offset);
  });
  const std::size_t index =
      reference.edge() == SemanticCircularEdge::SourceRim ? 0U : matches.size() - 1U;
  if (matches.size() > 1U) {
    const std::size_t neighbor = index == 0U ? 1U : matches.size() - 2U;
    if (std::abs(std::abs(matches[index].offset) - std::abs(matches[neighbor].offset)) <=
        kTolerance)
      return Result<TopoDS_Edge>::failure(edge_error(
          id, concat("semantic circular ", feature_label, " edge is ambiguous on target body")));
  }
  return Result<TopoDS_Edge>::success(matches[index].edge);
}
} // namespace

Result<TopoDS_Edge> resolve_semantic_edge(const PartDocument& document, const ShapeCache& cache,
                                          const TopoDS_Shape& target,
                                          const EdgeReference& reference, const FeatureId& id,
                                          std::string_view feature_label) {
  if (std::holds_alternative<SemanticEdgeReference>(reference.source()))
    return resolve_linear(document, cache, target,
                          std::get<SemanticEdgeReference>(reference.source()), id, feature_label);
  return resolve_circular(document, target,
                          std::get<SemanticCircularEdgeReference>(reference.source()), id,
                          feature_label);
}

} // namespace blcad::geometry
