#include "variable_fillet_adapter.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "checked_fillet_builder.hpp"
#include "fillet_radius_law.hpp"
#include "semantic_edge_resolution.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <ChFiDS_Spine.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>
#include <tuple>
namespace blcad::geometry::detail {
namespace {
constexpr double tolerance = 1e-7;
Error error(const std::string& id, std::string message) {
  return Error::geometry(id, std::move(message));
}
// Persistent result witnesses have a canonical direction independent of OCCT edge orientation.
bool canonical_reverse(const TopoDS_Edge& edge) {
  BRepAdaptor_Curve curve(edge);
  std::array<std::array<double, 3>, ResultEdgeReference::sample_count> points;
  for (std::size_t i = 0; i < points.size(); ++i) {
    auto p = curve.Value(curve.FirstParameter() + (curve.LastParameter() - curve.FirstParameter()) *
                                                      double(i) / double(points.size() - 1));
    points[i] = {p.X(), p.Y(), p.Z()};
  }
  return std::lexicographical_compare(points.rbegin(), points.rend(), points.begin(), points.end());
}
Result<bool> authored_reverse(const PartDocument& document, const ShapeCache& cache,
                              const EdgeReference& reference, const TopoDS_Edge& edge) {
  if (const auto* semantic = std::get_if<SemanticEdgeReference>(&reference.source())) {
    auto resolved = SemanticReferenceEvaluator{}.resolve_edge(document, *semantic, cache);
    if (!resolved)
      return Result<bool>::failure(resolved.error());
    BRepAdaptor_Curve curve(edge);
    const auto p = resolved.value().start;
    return Result<bool>::success(
        curve.Value(curve.FirstParameter()).Distance(gp_Pnt(p.x, p.y, p.z)) > 1e-6);
  }
  // Result/import witnesses use geometric lexicographic direction. A semantic circle starts
  // at its source curve seam and follows its native parameter direction.
  return Result<bool>::success(
      std::holds_alternative<SemanticCircularEdgeReference>(reference.source())
          ? false
          : canonical_reverse(edge));
}
TopoDS_Vertex vertex(const TopoDS_Edge& edge, bool end) {
  return end ? TopExp::LastVertex(edge, true) : TopExp::FirstVertex(edge, true);
}
gp_Vec outgoing(const TopoDS_Edge& edge, const TopoDS_Vertex& at) {
  BRepAdaptor_Curve c(edge);
  gp_Pnt p;
  gp_Vec d;
  const bool first = TopExp::FirstVertex(edge, false).IsSame(at);
  c.D1(first ? c.FirstParameter() : c.LastParameter(), p, d);
  if (!first)
    d.Reverse();
  if (d.Magnitude() <= 1e-12)
    throw std::runtime_error("Singular Fillet contour tangent");
  return d.Normalized();
}
void check_contour(const ChFiDS_Spine& spine, const TopoDS_Shape& target) {
  TopTools_IndexedDataMapOfShapeListOfShape incident;
  TopExp::MapShapesAndAncestors(target, TopAbs_VERTEX, TopAbs_EDGE, incident);
  for (int i = 1; i <= spine.NbEdges(); ++i) {
    const auto& edge = spine.Edges(i);
    for (bool end : {false, true}) {
      auto at = vertex(edge, end);
      if (at.IsNull())
        throw std::runtime_error("Fillet contour has no semantic endpoint");
      const auto incoming = outgoing(edge, at);
      TopTools_IndexedMapOfShape candidates;
      if (incident.Contains(at))
        for (TopTools_ListIteratorOfListOfShape it(incident.FindFromKey(at)); it.More();
             it.Next()) {
          const auto other = TopoDS::Edge(it.Value());
          if (other.IsSame(edge) || BRep_Tool::Degenerated(other))
            continue;
          if (incoming.Dot(outgoing(other, at)) < -1 + 1e-7)
            candidates.Add(other);
        }
      if (candidates.Extent() > 1)
        throw std::runtime_error("Ambiguous tangent branch in Fillet contour");
    }
    if (i < spine.NbEdges() || spine.IsClosed()) {
      const auto& next = spine.Edges(i % spine.NbEdges() + 1);
      const auto end = vertex(edge, true), start = vertex(next, false);
      if (!end.IsSame(start))
        throw std::runtime_error("Disconnected Fillet contour");
      if (!edge.IsSame(next) && outgoing(edge, end).Dot(outgoing(next, start)) > -1 + 1e-7)
        throw std::runtime_error("Discontinuous Fillet contour tangent");
    }
  }
}
int solids(const TopoDS_Shape& shape) {
  TopTools_IndexedMapOfShape components;
  TopExp::MapShapes(shape, TopAbs_SOLID, components);
  return components.Extent();
}
} // namespace
Result<NativeFilletResult> variable_fillet(const PartDocument& document,
                                           const FilletFeature& feature, const TopoDS_Shape& target,
                                           const ShapeCache& cache,
                                           StepImportTopologyContext* context,
                                           const std::vector<FaceLineage>& lineage) {
  std::string failing = feature.id().value();
  try {
    const auto& intent = std::get<VariableFilletIntent>(feature.advanced_intent()->definition);
    BRepBuilderAPI_Copy copy(target, true, false);
    if (!copy.IsDone())
      return Result<NativeFilletResult>::failure(
          error(failing, "Cannot isolate the variable Fillet target"));
    const auto working_target = copy.Shape();
    CheckedFilletBuilder builder(working_target);
    builder.SetParams(1e-2, 1e-7, 1e-8, 1e-7, 1e-8, 1e-4);
    struct ResolvedGroup {
      const FilletEdgeGroup* authored;
      std::vector<TopoDS_Edge> edges;
    };
    std::vector<ResolvedGroup> resolved;
    TopTools_IndexedMapOfShape used;
    for (const auto& group : intent.groups) {
      failing = group.id;
      ResolvedGroup result{&group, {}};
      for (const auto& member : group.edges) {
        auto edge = resolve_semantic_edge(document, cache, target, member.edge, feature.id(),
                                          "variable Fillet", context);
        if (!edge)
          return Result<NativeFilletResult>::failure(edge.error());
        auto reverse = authored_reverse(document, cache, member.edge, edge.value());
        if (!reverse)
          return Result<NativeFilletResult>::failure(reverse.error());
        auto oriented = TopoDS::Edge(copy.ModifiedShape(edge.value()));
        oriented.Orientation(reverse.value() != member.reversed ? TopAbs_REVERSED : TopAbs_FORWARD);
        result.edges.push_back(oriented);
      }
      builder.Add(result.edges.front());
      const int contour = builder.Contains(result.edges.front());
      if (contour <= 0)
        return Result<NativeFilletResult>::failure(
            error(failing, "No supported sharp Fillet contour at the selected edge"));
      const auto spine = builder.Value(contour);
      if (group.propagation == FilletPropagation::Explicit &&
          spine->NbEdges() != int(result.edges.size()))
        return Result<NativeFilletResult>::failure(
            error(failing, "Explicit Fillet group must declare the complete tangent contour"));
      int first_index = 0;
      bool forward = true;
      int previous = 0;
      for (const auto& member : result.edges) {
        int index = 0;
        for (int i = 1; i <= spine->NbEdges(); ++i)
          if (spine->Edges(i).IsSame(member)) {
            index = i;
            break;
          }
        if (index == 0)
          return Result<NativeFilletResult>::failure(
              error(failing, "Fillet group contains disconnected or discontinuous members"));
        const bool same = member.Orientation() == spine->Edges(index).Orientation();
        if (!first_index) {
          first_index = index;
          forward = same;
        } else {
          int expected = previous + (forward ? 1 : -1);
          if (spine->IsClosed())
            expected = (expected + spine->NbEdges() - 1) % spine->NbEdges() + 1;
          if (same != forward || index != expected)
            return Result<NativeFilletResult>::failure(
                error(failing, "Fillet members must form one ordered oriented tangent chain"));
        }
        previous = index;
      }
      for (int i = 1; i <= spine->NbEdges(); ++i) {
        if (used.Contains(spine->Edges(i)))
          return Result<NativeFilletResult>::failure(
              error(failing, "Expanded Fillet groups overlap"));
        used.Add(spine->Edges(i));
      }
      check_contour(*spine, working_target);
      const double length = builder.Length(contour);
      if (!std::isfinite(length) || length <= tolerance)
        return Result<NativeFilletResult>::failure(
            error(failing, "Fillet contour length is numerically unresolved"));
      std::vector<RadiusKnot> knots;
      for (const auto& station : group.stations) {
        failing = station.id;
        const auto* parameter = document.find_parameter(station.radius);
        if (!parameter || parameter->type() != ParameterType::Length ||
            !parameter->value().is_positive_length())
          return Result<NativeFilletResult>::failure(
              error(failing, "Fillet station requires a positive Length radius"));
        const double position = station.normalized_length() * length;
        if (!knots.empty() &&
            position - knots.back().length_mm <= std::max(tolerance, length * 1e-12))
          return Result<NativeFilletResult>::failure(error(
              failing, "Fillet station spacing is numerically unresolved in true curve length"));
        knots.push_back({position, parameter->value().millimeters()});
      }
      failing = group.id;
      const bool closed = spine->IsClosed();
      if (closed && knots.front().radius_mm != knots.back().radius_mm)
        return Result<NativeFilletResult>::failure(
            error(failing, "Closed Fillet contour requires matching seam radii"));
      // Native -> group member coordinate, then -> original station basis. Open tangent
      // expansion defines endpoints of the complete expanded chain, not of the seed edge.
      const double phase = closed ? (forward ? spine->FirstParameter(first_index)
                                             : spine->LastParameter(first_index))
                                  : (forward ? 0 : length);
      int sign = forward ? 1 : -1;
      double offset = -sign * phase;
      if (group.station_basis_reversed) {
        sign = -sign;
        offset = length - offset;
      }
      auto law = radius_law(knots, group.interpolation == FilletRadiusInterpolation::MonotoneCubic,
                            closed, sign, offset);
      // Native acquisition must know the complete range before cap/extension analysis. This
      // table is never the final law; CheckedSpine installs exact interpolation on each span.
      for (const auto& knot : knots) {
        double x = (knot.length_mm - offset) / sign;
        if (closed) {
          x = std::fmod(x, length);
          if (x < 0)
            x += length;
        }
        builder.SetRadius(gp_XY(x, knot.radius_mm), contour, 0);
      }
      builder.SetRadius(gp_XY(0, law->Value(0)), contour, 0);
      builder.SetRadius(gp_XY(length, law->Value(length)), contour, 0);
      builder.install_law(contour, law);
      resolved.push_back(std::move(result));
    }
    failing = feature.id().value();
    builder.Compute();
    if (!builder.IsDone()) {
      if (builder.NbFaultyContours() > 0) {
        const int index = builder.FaultyContour(1);
        if (index > 0 && index <= int(resolved.size()))
          failing = resolved[index - 1].authored->id;
      }
      return Result<NativeFilletResult>::failure(error(
          failing,
          builder.law_failure().empty()
              ? "Variable Fillet construction failed; inspect this contour and its radius stations"
              : "Variable Fillet radius law failed: " + builder.law_failure()));
    }
    auto shape = builder.Shape();
    if (shape.IsNull() || solids(target) == 0 || solids(shape) != solids(target) ||
        !BRepCheck_Analyzer(shape, true).IsValid() ||
        !BRepAlgoAPI_Check(shape, false, true).IsValid())
      return Result<NativeFilletResult>::failure(error(
          failing, "Variable Fillet produced invalid, self-interfering or disconnected material"));
    GProp_GProps mass;
    BRepGProp::VolumeProperties(shape, mass);
    if (!std::isfinite(mass.Mass()) || mass.Mass() <= tolerance)
      return Result<NativeFilletResult>::failure(
          error(failing, "Variable Fillet produced no finite positive solid volume"));
    auto copied_lineage = map_lineage(copy, working_target, lineage);
    auto mapped = map_lineage(builder, shape, copied_lineage);
    for (const auto& group : resolved) {
      bool generated = false;
      for (const auto& edge : group.edges)
        for (TopTools_ListIteratorOfListOfShape it(builder.Generated(edge)); it.More(); it.Next())
          if (it.Value().ShapeType() == TopAbs_FACE) {
            generated = true;
            append_lineage(mapped,
                           {{feature.id(), "fillet.group/" + group.authored->id},
                            TopoDS::Face(it.Value()),
                            {},
                            {1, 0, 0}},
                           shape);
          }
      if (!generated)
        return Result<NativeFilletResult>::failure(
            error(group.authored->id, "Variable Fillet generated no blend faces"));
    }
    return Result<NativeFilletResult>::success({std::move(shape), std::move(mapped)});
  } catch (const Standard_Failure& ex) {
    return Result<NativeFilletResult>::failure(error(
        failing, ex.GetMessageString() ? ex.GetMessageString() : "Native variable Fillet failed"));
  } catch (const std::exception& ex) {
    return Result<NativeFilletResult>::failure(error(failing, ex.what()));
  }
}
} // namespace blcad::geometry::detail
