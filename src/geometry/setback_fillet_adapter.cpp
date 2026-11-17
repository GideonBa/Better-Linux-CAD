#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document.hpp"
#include "semantic_edge_resolution.hpp"
#include "surface_join_verification.hpp"
#include "variable_fillet_adapter.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <array>
#include <cmath>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
constexpr double position_tolerance = 2e-5, normal_tolerance = 1e-4;
void check(bool value, const std::string& message) {
  if (!value)
    throw std::runtime_error(message);
}
double volume(const TopoDS_Shape& shape) {
  GProp_GProps mass;
  BRepGProp::VolumeProperties(shape, mass);
  return mass.Mass();
}
int solids(const TopoDS_Shape& shape) {
  TopTools_IndexedMapOfShape components;
  TopExp::MapShapes(shape, TopAbs_SOLID, components);
  return components.Extent();
}
struct CornerTool {
  TopoDS_Shape shape;
};
CornerTool tetrahedron(const std::array<gp_Pnt, 4>& points) {
  BRepBuilderAPI_Sewing sew(1e-7);
  for (const auto& indices :
       std::array<std::array<int, 3>, 4>{{{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}}}) {
    BRepBuilderAPI_MakePolygon wire;
    for (int i : indices)
      wire.Add(points[i]);
    wire.Close();
    auto face = BRepBuilderAPI_MakeFace(wire.Wire()).Face();
    sew.Add(face);
  }
  sew.Perform();
  check(sew.SewedShape().ShapeType() == TopAbs_SHELL, "Cannot sew local setback tool");
  auto solid = BRepBuilderAPI_MakeSolid(TopoDS::Shell(sew.SewedShape())).Solid();
  check(BRepLib::OrientClosedSolid(solid), "Cannot orient local setback tool");
  return {solid};
}

} // namespace
Result<NativeFilletResult> setback_fillet(const PartDocument& doc, const FilletFeature& feature,
                                          const TopoDS_Shape& target, const ShapeCache& cache,
                                          StepImportTopologyContext* context,
                                          const std::vector<FaceLineage>& lineage) {
  const auto& intent = std::get<VariableFilletIntent>(feature.advanced_intent()->definition);
  const auto& policy = *intent.setback;
  std::string failing = policy.id;
  try {
    BRepBuilderAPI_Copy copy(target, true, false);
    check(copy.IsDone(), "Cannot isolate setback target");
    const auto working = copy.Shape();
    std::array<TopoDS_Edge, 3> edges;
    std::array<const FilletEdgeGroup*, 3> groups;
    for (int i = 0; i < 3; ++i) {
      auto found = std::find_if(intent.groups.begin(), intent.groups.end(),
                                [&](const auto& g) { return g.id == policy.groups[i]; });
      check(found != intent.groups.end(), "Setback group is missing");
      groups[i] = &*found;
      auto edge = resolve_semantic_edge(doc, cache, target, found->edges.front().edge, feature.id(),
                                        "setback Fillet", context);
      check(bool(edge), edge ? "" : edge.error().message());
      edges[i] = TopoDS::Edge(copy.ModifiedShape(edge.value()));
      check(BRepAdaptor_Curve(edges[i]).GetType() == GeomAbs_Line,
            "Setback requires three straight edge groups");
    }
    TopoDS_Vertex corner;
    for (const auto& candidate : {TopExp::FirstVertex(edges[0]), TopExp::LastVertex(edges[0])}) {
      bool shared = true;
      for (int i = 1; i < 3; ++i)
        shared &= candidate.IsSame(TopExp::FirstVertex(edges[i])) ||
                  candidate.IsSame(TopExp::LastVertex(edges[i]));
      if (shared) {
        check(corner.IsNull(), "Setback has ambiguous shared endpoints");
        corner = candidate;
      }
    }
    check(!corner.IsNull(), "Setback groups must meet at one shared vertex");
    TopTools_IndexedDataMapOfShapeListOfShape incident;
    TopExp::MapShapesAndAncestors(working, TopAbs_VERTEX, TopAbs_EDGE, incident);
    TopTools_IndexedMapOfShape outgoing;
    for (TopTools_ListIteratorOfListOfShape it(incident.FindFromKey(corner)); it.More(); it.Next())
      if (!BRep_Tool::Degenerated(TopoDS::Edge(it.Value())))
        outgoing.Add(it.Value());
    check(outgoing.Extent() == 3, "Setback requires an unbranched three-edge vertex");
    const auto origin = BRep_Tool::Pnt(corner);
    const double r = doc.find_parameter(groups[0]->stations.front().radius)->value().millimeters(),
                 setback = doc.find_parameter(policy.distance)->value().millimeters(),
                 trim = setback + (2 - std::sqrt(3.)) * r;
    check(setback > r + position_tolerance,
          "Setback distance must exceed the shared constant radius");
    std::array<gp_Vec, 3> axes;
    std::array<gp_Pnt, 4> points;
    points[0] = origin;
    for (int i = 0; i < 3; ++i) {
      const auto other = corner.IsSame(TopExp::FirstVertex(edges[i]))
                             ? TopExp::LastVertex(edges[i])
                             : TopExp::FirstVertex(edges[i]);
      const gp_Vec along(origin, BRep_Tool::Pnt(other));
      check(along.Magnitude() > trim + 2 * position_tolerance,
            "Setback leaves insufficient edge length");
      axes[i] = along.Normalized();
      for (int j = 0; j < i; ++j)
        check(std::abs(axes[i].Dot(axes[j])) < 1e-10,
              "Setback requires an orthogonal trihedral corner");
      points[i + 1] = origin.Translated(axes[i] * trim);
    }
    auto tool = tetrahedron(points);
    check(BRepCheck_Analyzer(tool.shape, true).IsValid(), "Invalid local setback tool");
    const auto witness = origin.Translated((axes[0] + axes[1] + axes[2]) * r);
    BRepClass3d_SolidClassifier inside(working, witness, 1e-7);
    check(inside.State() == TopAbs_IN, "Setback requires a convex material corner");
    BRepAlgoAPI_Cut cut(working, tool.shape);
    cut.SetNonDestructive(true);
    cut.Build();
    check(cut.IsDone(), "Local setback cut failed");
    const auto truncated = cut.Shape();
    std::array<TopoDS_Edge, 3> seeds;
    for (int i = 0; i < 3; ++i) {
      for (TopTools_ListIteratorOfListOfShape it(cut.Modified(edges[i])); it.More(); it.Next())
        if (it.Value().ShapeType() == TopAbs_EDGE) {
          check(seeds[i].IsNull(), "Setback edge split is ambiguous");
          seeds[i] = TopoDS::Edge(it.Value());
        }
      check(!seeds[i].IsNull(), "Setback did not retain the expected straight group");
    }
    std::vector<FaceLineage> mapped;
    TopoDS_Face cap;
    const auto direction = (axes[0] + axes[1] + axes[2]).Normalized();
    for (TopExp_Explorer it(truncated, TopAbs_FACE); it.More(); it.Next()) {
      auto face = TopoDS::Face(it.Current());
      BRepAdaptor_Surface support(face);
      if (support.GetType() != GeomAbs_Plane)
        continue;
      auto plane = support.Plane();
      if (std::abs(std::abs(plane.Axis().Direction().Dot(gp_Dir(direction))) - 1) > 1e-10 ||
          plane.Distance(points[1]) > 1e-6)
        continue;
      check(cap.IsNull(), "Setback cut face is ambiguous");
      cap = face;
    }
    check(!cap.IsNull(), "Setback cut face is missing");
    TopTools_IndexedMapOfShape cap_edges;
    TopExp::MapShapes(cap, TopAbs_EDGE, cap_edges);
    check(cap_edges.Extent() == 3, "Setback cut intersects another feature or support boundary");
    BRepFilletAPI_MakeFillet corner_builder(truncated);
    corner_builder.SetParams(1e-2, 1e-7, 1e-8, 1e-7, 1e-8, 1e-4);
    for (const auto& seed : seeds)
      corner_builder.Add(r, seed);
    for (int i = 1; i <= cap_edges.Extent(); ++i)
      corner_builder.Add(r, TopoDS::Edge(cap_edges(i)));
    check(corner_builder.NbContours() == 6, "Setback contours are ambiguous");
    for (int i = 1; i <= 6; ++i)
      check(corner_builder.NbEdges(i) == 1,
            "Setback tangent expansion leaves its declared local supports");
    corner_builder.Build();
    check(corner_builder.IsDone(), "Analytic setback rounds failed");
    auto result = corner_builder.Shape();
    check(solids(result) == solids(target) && BRepCheck_Analyzer(result, true).IsValid() &&
              BRepAlgoAPI_Check(result, false, true).IsValid(),
          "Setback produced invalid or self-interfering material");
    check(std::isfinite(volume(result)) && volume(result) > 0 && volume(result) < volume(target),
          "Setback must retain finite positive material");
    mapped = map_lineage(corner_builder, result,
                         map_lineage(cut, truncated, map_lineage(copy, working, lineage)));
    std::vector<FaceLineage> generated, primary;
    for (int i = 0; i < 3; ++i)
      for (TopTools_ListIteratorOfListOfShape it(corner_builder.Generated(seeds[i])); it.More();
           it.Next()) {
        if (it.Value().ShapeType() != TopAbs_FACE)
          continue;
        auto face = TopoDS::Face(it.Value());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() == GeomAbs_Cylinder &&
            std::abs(surface.Cylinder().Axis().Direction().Dot(gp_Dir(axes[i]))) > 1 - 1e-10)
          append_lineage(primary,
                         {{feature.id(), "fillet.group/" + groups[i]->id}, face, {}, {1, 0, 0}},
                         result);
      }
    check(primary.size() == 3, "Setback did not retain all three constant-radius edge blends");
    auto center = map_lineage(
        corner_builder, result,
        {{{feature.id(), "fillet.corner/" + policy.id + "/center"}, cap, {}, {1, 0, 0}}});
    check(center.size() == 1, "Setback center collapsed or split");
    generated = center;
    // Kernel-history descendants separate retained supports from newly constructed faces.
    // A shared corner role represents a face set; no traversal ordinal becomes identity.
    std::vector<FaceLineage> original_faces;
    for (TopExp_Explorer it(truncated, TopAbs_FACE); it.More(); it.Next())
      original_faces.push_back(
          {{feature.id(), "transient.support"}, TopoDS::Face(it.Current()), {}, {1, 0, 0}});
    auto retained = map_lineage(corner_builder, result, original_faces);
    for (TopExp_Explorer it(result, TopAbs_FACE); it.More(); it.Next()) {
      const auto face = TopoDS::Face(it.Current());
      const auto owns = [&](const auto& list) {
        return std::any_of(list.begin(), list.end(),
                           [&](const auto& f) { return f.face.IsSame(face); });
      };
      if (!owns(retained) && !owns(primary))
        append_lineage(generated,
                       {{feature.id(), "fillet.corner/" + policy.id}, face, {}, {1, 0, 0}}, result);
    }
    check(generated.size() >= 7, "Setback generated an incomplete transition network");
    TopTools_IndexedDataMapOfShapeListOfShape adjacent;
    TopExp::MapShapesAndAncestors(result, TopAbs_EDGE, TopAbs_FACE, adjacent);
    int contacts = 0;
    for (int i = 1; i <= adjacent.Extent(); ++i)
      for (const auto& candidate : generated) {
        bool incident = false;
        for (TopTools_ListIteratorOfListOfShape it(adjacent.FindFromIndex(i)); it.More(); it.Next())
          incident |= it.Value().IsSame(candidate.face);
        if (!incident)
          continue;
        for (TopTools_ListIteratorOfListOfShape it(adjacent.FindFromIndex(i)); it.More();
             it.Next()) {
          if (it.Value().IsSame(candidate.face))
            continue;
          auto checked =
              verify_surface_join(feature.id(), TopoDS::Edge(adjacent.FindKey(i)), candidate.face,
                                  TopoDS::Face(it.Value()), LoftBoundaryContinuity::G1,
                                  position_tolerance, normal_tolerance, 1e-4);
          check(bool(checked), checked ? "" : checked.error().message());
          ++contacts;
        }
      }
    check(contacts >= 12, "Setback has incomplete verified contacts");
    mapped.insert(mapped.end(), primary.begin(), primary.end());
    mapped.insert(mapped.end(), generated.begin(), generated.end());
    return Result<NativeFilletResult>::success({result, std::move(mapped)});
  } catch (const Standard_Failure& e) {
    return Result<NativeFilletResult>::failure(Error::geometry(
        failing, e.GetMessageString() ? e.GetMessageString() : "Native setback failed"));
  } catch (const std::exception& e) {
    return Result<NativeFilletResult>::failure(Error::geometry(failing, e.what()));
  }
}
} // namespace blcad::geometry::detail
