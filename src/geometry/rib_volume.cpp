#include "rib_native.hpp"
#include <BOPAlgo_ArgumentAnalyzer.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <gp_Pln.hxx>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct Boundary {
  TopoDS_Wire wire;
  std::vector<std::pair<TopoDS_Edge, std::string>> edges;
};
gp_Pnt first(const TopoDS_Wire& w) {
  BRepTools_WireExplorer it(w);
  require(it.More(), "Empty Rib boundary");
  return BRep_Tool::Pnt(TopExp::FirstVertex(it.Current(), true));
}
gp_Pnt last(const TopoDS_Wire& w) {
  BRepTools_WireExplorer it(w);
  TopoDS_Edge edge;
  for (; it.More(); it.Next())
    edge = it.Current();
  require(!edge.IsNull(), "Empty Rib boundary");
  return BRep_Tool::Pnt(TopExp::LastVertex(edge, true));
}
Boundary offset(const RibSpine& s, const ResolvedWorkplane& f, double distance,
                const std::string& side) {
  Boundary result;
  if (std::abs(distance) < 1e-12) {
    result.wire = s.wire;
    for (const auto& [edge, name] : s.edges)
      result.edges.emplace_back(edge, name + "/" + side);
    return result;
  }
  const gp_Dir n(f.normal.x, f.normal.y, f.normal.z);
  const gp_Pln plane(gp_Ax3(gp_Pnt(f.origin.x, f.origin.y, f.origin.z), n,
                            gp_Dir(f.x_axis.x, f.x_axis.y, f.x_axis.z)));
  BRepAdaptor_Curve curve(s.edges.front().first);
  gp_Pnt p;
  gp_Vec tangent;
  curve.D1((curve.FirstParameter() + curve.LastParameter()) * .5, p, tangent);
  if (s.edges.front().first.Orientation() == TopAbs_REVERSED)
    tangent.Reverse();
  const gp_Vec left = gp_Vec(n).Crossed(tangent).Normalized();
  const gp_Pnt expected = p.Translated(left * distance);
  for (const auto sign : {1., -1.}) {
    // OCCT derives left/right domain reversal from the face's source wire.
    // Retain that wire on the transient planar carrier for both signed offsets.
    BRepOffsetAPI_MakeOffset operation(BRepBuilderAPI_MakeFace(plane, s.wire, false).Face(),
                                       GeomAbs_Intersection, !s.closed);
    operation.Perform(sign * distance);
    if (!operation.IsDone() || operation.Shape().IsNull())
      continue;
    TopExp_Explorer wires(operation.Shape(), TopAbs_WIRE);
    if (!wires.More())
      continue;
    auto wire = TopoDS::Wire(wires.Current());
    wires.Next();
    if (wires.More())
      continue;
    BRepExtrema_DistShapeShape near(BRepBuilderAPI_MakeVertex(expected).Vertex(), wire);
    if (!near.IsDone() || near.Value() > 1e-6)
      continue;
    require(result.wire.IsNull(), "Rib offset side is ambiguous");
    if (!s.closed && first(wire).Distance(first(s.wire)) > last(wire).Distance(first(s.wire)))
      wire.Reverse();
    result.wire = wire;
    for (const auto& [source, name] : s.edges)
      for (TopTools_ListIteratorOfListOfShape it(operation.Generated(source)); it.More();
           it.Next()) {
        if (it.Value().ShapeType() == TopAbs_EDGE)
          result.edges.emplace_back(TopoDS::Edge(it.Value()), name + "/" + side);
      }
  }
  require(!result.wire.IsNull(), "Rib offset does not resolve the authored physical side");
  require(BRepCheck_Analyzer(result.wire).IsValid(), "Rib offset produced an invalid boundary");
  require(!result.edges.empty(), "Rib offset lost authored curve provenance");
  return result;
}
Boundary translated(const RibSpine& s, gp_Vec vector, const std::string& side) {
  gp_Trsf t;
  t.SetTranslation(vector);
  BRepBuilderAPI_Transform operation(s.wire, t, true);
  Boundary b{TopoDS::Wire(operation.Shape()), {}};
  for (const auto& [edge, name] : s.edges)
    b.edges.emplace_back(TopoDS::Edge(operation.ModifiedShape(edge)), name + "/" + side);
  return b;
}
Boundary ribbon(const RibSpine& s, const RibWebIntent& r, const ResolvedWorkplane& f, double a,
                double b) {
  auto left = r.plane == RibExtrusionPlane::PerpendicularToSketch
                  ? offset(s, f, a, "first")
                  : translated(s, gp_Vec(f.normal.x, f.normal.y, f.normal.z) * a, "first");
  auto right = r.plane == RibExtrusionPlane::PerpendicularToSketch
                   ? offset(s, f, b, "second")
                   : translated(s, gp_Vec(f.normal.x, f.normal.y, f.normal.z) * b, "second");
  require(!s.closed, "Closed Rib bands are built from independent outer and inner wires");
  BRepBuilderAPI_MakeWire loop;
  loop.Add(left.wire);
  const auto end = BRepBuilderAPI_MakeEdge(last(left.wire), last(right.wire)).Edge();
  loop.Add(end);
  auto reverse = right.wire;
  reverse.Reverse();
  loop.Add(reverse);
  const auto start = BRepBuilderAPI_MakeEdge(first(right.wire), first(left.wire)).Edge();
  loop.Add(start);
  require(loop.IsDone(), "Rib ribbon could not close");
  Boundary result{loop.Wire(), left.edges};
  result.edges.insert(result.edges.end(), right.edges.begin(), right.edges.end());
  result.edges.emplace_back(start, s.edges.front().second + "/start");
  result.edges.emplace_back(end, s.edges.back().second + "/end");
  return result;
}
double area(TopoDS_Wire w) {
  GProp_GProps prop;
  BRepGProp::SurfaceProperties(BRepBuilderAPI_MakeFace(w, true).Face(), prop);
  return std::abs(prop.Mass());
}
TopoDS_Wire moved(TopoDS_Wire w, gp_Vec v) {
  gp_Trsf t;
  t.SetTranslation(v);
  return TopoDS::Wire(BRepBuilderAPI_Transform(w, t, true).Shape());
}
template <class Algorithm> void setup(Algorithm& op, const TopoDS_Shape& a, const TopoDS_Shape& b) {
  TopTools_ListOfShape arguments, tools;
  arguments.Append(a);
  tools.Append(b);
  op.SetArguments(arguments);
  op.SetTools(tools);
  op.SetNonDestructive(true);
  op.Build();
  require(op.IsDone() && !op.HasErrors(), "Rib Boolean operation failed");
}
void append_faces(RibNative& out, const TopoDS_Shape& shape, const FeatureId& id,
                  const std::string& role, const ResolvedWorkplane& f) {
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
    auto face = TopoDS::Face(it.Current());
    Vector3 x = f.x_axis;
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() == GeomAbs_Plane) {
      const auto n = surface.Plane().Axis().Direction();
      if (std::abs(n.Dot(gp_Dir(x.x, x.y, x.z))) > .99)
        x = f.y_axis;
    }
    append_lineage(out.lineage, {{id, role}, face, f.origin, x}, out.shape);
  }
}
} // namespace
double rib_volume(const TopoDS_Shape& s) {
  GProp_GProps props;
  BRepGProp::VolumeProperties(s, props);
  return props.Mass();
}
void validate_rib_solid(const TopoDS_Shape& s) {
  require(!s.IsNull() && TopExp_Explorer(s, TopAbs_SOLID).More() &&
              BRepCheck_Analyzer(s, true).IsValid(),
          "Rib produced an invalid solid");
  BOPAlgo_ArgumentAnalyzer check;
  check.SetShape1(s);
  check.SelfInterMode() = true;
  check.Perform();
  require(!check.HasFaulty(), "Rib contains self-interference");
}
RibNative rib_tool(const RibSpine& s, const RibWebIntent& r, const FeatureId& id,
                   const ResolvedWorkplane& f, double a, double b, double draft, double length,
                   gp_Vec d) {
  require(a - b > 1e-6 && a - b + 2 * length * draft > 1e-6,
          "Rib minimum thickness collapses along its depth");
  if (r.plane == RibExtrusionPlane::ParallelToSketch) {
    require(!s.closed, "Parallel closed curtains are unsupported");
    for (const auto& edge : s.edges)
      require(BRepAdaptor_Curve(edge.first).GetType() == GeomAbs_Line,
              "Parallel Rib currently requires straight skeleton segments");
  }
  if (draft != 0)
    for (const auto& edge : s.edges) {
      auto type = BRepAdaptor_Curve(edge.first).GetType();
      require(type == GeomAbs_Line || type == GeomAbs_Circle,
              "Drafted Rib requires straight or circular skeletons");
    }
  const gp_Vec extrusion = d * length;
  const auto make_solid = [&](Boundary base, Boundary top) -> RibNative {
    RibNative out;
    if (draft == 0) {
      BRepBuilderAPI_MakeFace face(base.wire, true);
      require(face.IsDone(), "Rib ribbon is not planar or self-intersects");
      BRepPrimAPI_MakePrism prism(face.Face(), extrusion, true);
      require(prism.IsDone(), "Rib extrusion failed");
      out.shape = prism.Shape();
      for (const auto& [edge, role] : base.edges)
        for (TopTools_ListIteratorOfListOfShape it(prism.Generated(edge)); it.More(); it.Next())
          append_faces(out, it.Value(), id, role, f);
      append_faces(out, prism.FirstShape(), id, s.edges.front().second + "/base", f);
      append_faces(out, prism.LastShape(), id, s.edges.front().second + "/cap", f);
    } else {
      BRepOffsetAPI_ThruSections loft(true, true, 1e-7);
      loft.CheckCompatibility(false);
      loft.AddWire(base.wire);
      loft.AddWire(moved(top.wire, extrusion));
      loft.Build();
      require(loft.IsDone(), "Rib drafted volume failed");
      out.shape = loft.Shape();
      for (const auto& [edge, role] : base.edges) {
        auto face = loft.GeneratedFace(edge);
        if (!face.IsNull())
          append_faces(out, face, id, role, f);
      }
      append_faces(out, loft.FirstShape(), id, s.edges.front().second + "/base", f);
      append_faces(out, loft.LastShape(), id, s.edges.front().second + "/cap", f);
    }
    validate_rib_solid(out.shape);
    return out;
  };
  if (!s.closed)
    return make_solid(ribbon(s, r, f, a, b),
                      ribbon(s, r, f, a + length * draft, b - length * draft));
  auto first = offset(s, f, a, "first"), second = offset(s, f, b, "second");
  auto top_first = offset(s, f, a + length * draft, "first"),
       top_second = offset(s, f, b - length * draft, "second");
  if (area(first.wire) < area(second.wire)) {
    std::swap(first, second);
    std::swap(top_first, top_second);
  }
  if (draft == 0) {
    BRepBuilderAPI_MakeFace face(first.wire, true);
    auto inner = second.wire;
    inner.Reverse();
    face.Add(inner);
    require(face.IsDone() && BRepCheck_Analyzer(face.Face()).IsValid(),
            "Rib closed band collapsed");
    BRepPrimAPI_MakePrism prism(face.Face(), extrusion, true);
    RibNative out{prism.Shape(), {}};
    for (const auto& boundaries : {first.edges, second.edges})
      for (const auto& [edge, role] : boundaries)
        for (TopTools_ListIteratorOfListOfShape it(prism.Generated(edge)); it.More(); it.Next())
          append_faces(out, it.Value(), id, role, f);
    append_faces(out, prism.FirstShape(), id, s.edges.front().second + "/base", f);
    append_faces(out, prism.LastShape(), id, s.edges.front().second + "/cap", f);
    validate_rib_solid(out.shape);
    return out;
  }
  auto outer = make_solid(first, top_first), inner = make_solid(second, top_second);
  BRepAlgoAPI_Cut cut;
  setup(cut, outer.shape, inner.shape);
  auto lineage = outer.lineage;
  lineage.insert(lineage.end(), inner.lineage.begin(), inner.lineage.end());
  return {cut.Shape(), map_lineage(cut, cut.Shape(), lineage)};
}
RibNative join_rib(const RibNative& a, const RibNative& b, const FeatureId&, bool reject_crossing) {
  if (reject_crossing) {
    BRepAlgoAPI_Common common;
    setup(common, a.shape, b.shape);
    require(rib_volume(common.Shape()) < 1e-7,
            "Rib/Web crossing policy rejects overlapping skeleton bands");
  }
  BRepAlgoAPI_Fuse fuse;
  setup(fuse, a.shape, b.shape);
  auto lineage = a.lineage;
  lineage.insert(lineage.end(), b.lineage.begin(), b.lineage.end());
  RibNative out{fuse.Shape(), map_lineage(fuse, fuse.Shape(), lineage)};
  validate_rib_solid(out.shape);
  return out;
}
} // namespace blcad::geometry::detail
