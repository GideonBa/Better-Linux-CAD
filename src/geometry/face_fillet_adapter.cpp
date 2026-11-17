#include "face_fillet_adapter.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "semantic_face_resolution.hpp"
#include "surface_join_verification.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <array>
#include <cmath>
#include <gp_Lin.hxx>
#include <numbers>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
constexpr double tol = 1e-6;
void require(bool condition, const std::string& message) {
  if (!condition)
    throw std::runtime_error(message);
}
double mass(const TopoDS_Shape& shape, bool volume = false) {
  GProp_GProps p;
  if (volume)
    BRepGProp::VolumeProperties(shape, p);
  else
    BRepGProp::LinearProperties(shape, p);
  return p.Mass();
}
int solids(const TopoDS_Shape& shape) {
  TopTools_IndexedMapOfShape m;
  TopExp::MapShapes(shape, TopAbs_SOLID, m);
  return m.Extent();
}
TopoDS_Compound compound(const std::vector<TopoDS_Edge>& edges) {
  TopoDS_Compound c;
  BRep_Builder b;
  b.MakeCompound(c);
  for (const auto& e : edges)
    b.Add(c, e);
  return c;
}
TopoDS_Shape common(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  BRepAlgoAPI_Common op(a, b);
  op.SetNonDestructive(true);
  op.Build();
  require(op.IsDone(), "Contact intersection failed");
  return op.Shape();
}
gp_Vec normal(const TopoDS_Face& face) {
  BRepAdaptor_Surface s(face);
  require(s.GetType() == GeomAbs_Plane, "This finishing combination requires planar supports");
  gp_Vec n(s.Plane().Axis().Direction());
  if (face.Orientation() == TopAbs_REVERSED)
    n.Reverse();
  return n;
}
struct Frame {
  gp_Pnt origin;
  gp_Vec x, y, z;
  gp_Pnt point(double a, double b, double c) const {
    return origin.Translated(x * a + y * b + z * c);
  }
};
struct Cap {
  TopoDS_Shape solid, slab;
  std::vector<FaceLineage> generated;
};
TopoDS_Edge quintic(const std::array<gp_Pnt, 6>& points) {
  TColgp_Array1OfPnt poles(1, 6);
  for (int i = 0; i < 6; ++i)
    poles(i + 1) = points[i];
  Handle(Geom_BezierCurve) bezier = new Geom_BezierCurve(poles);
  return BRepBuilderAPI_MakeEdge(GeomConvert::CurveToBSplineCurve(bezier)).Edge();
}
TopoDS_Face profile(const TopoDS_Edge& curve, const std::vector<gp_Pnt>& closing) {
  BRepBuilderAPI_MakeWire w;
  w.Add(curve);
  gp_Pnt a = BRep_Tool::Pnt(TopExp::LastVertex(curve));
  for (const auto& b : closing) {
    w.Add(BRepBuilderAPI_MakeEdge(a, b).Edge());
    a = b;
  }
  require(w.IsDone(), "Cannot close finishing profile");
  BRepBuilderAPI_MakeFace f(w.Wire(), true);
  require(f.IsDone(), "Cannot construct finishing section");
  return f.Face();
}
TopoDS_Face rectangle(const Frame& f, double width, double height) {
  auto a = f.origin, b = f.point(width, 0, 0), c = f.point(width, 0, height),
       d = f.point(0, 0, height);
  BRepBuilderAPI_MakeWire w;
  for (auto e : {BRepBuilderAPI_MakeEdge(a, b).Edge(), BRepBuilderAPI_MakeEdge(b, c).Edge(),
                 BRepBuilderAPI_MakeEdge(c, d).Edge(), BRepBuilderAPI_MakeEdge(d, a).Edge()})
    w.Add(e);
  return BRepBuilderAPI_MakeFace(w.Wire()).Face();
}
std::vector<TopoDS_Face> resolve(const PartDocument& doc, const FilletFeature& feature,
                                 const FilletFaceSet& set, const TopoDS_Shape& target,
                                 const ShapeCache& cache, StepImportTopologyContext* context) {
  std::vector<TopoDS_Face> result;
  for (const auto& r : set.faces) {
    auto face = resolve_semantic_face(doc, cache, target, r, feature.id(), "face Fillet", context);
    if (!face)
      throw std::runtime_error(set.id + ": " + face.error().message());
    for (const auto& old : result)
      require(!old.IsSame(face.value()), set.id + ": repeated resolved face");
    result.push_back(face.value());
  }
  return result;
}
std::vector<FaceLineage> marked(const std::vector<TopoDS_Face>& faces, const FeatureId& id,
                                const std::string& role) {
  std::vector<FaceLineage> result;
  for (const auto& face : faces)
    result.push_back({{id, role}, face, {}, {1, 0, 0}});
  return result;
}
void append(std::vector<FaceLineage>& a, const std::vector<FaceLineage>& b) {
  a.insert(a.end(), b.begin(), b.end());
}
bool contains(const std::vector<TopoDS_Face>& faces, const TopoDS_Shape& face) {
  return std::any_of(faces.begin(), faces.end(), [&](const auto& f) { return f.IsSame(face); });
}
std::array<std::vector<TopoDS_Edge>, 2> verify(const FeatureId& id, const TopoDS_Shape& shape,
                                               const std::vector<FaceLineage>& generated,
                                               const std::vector<FaceLineage>& supports,
                                               FilletContinuity continuity) {
  TopTools_IndexedDataMapOfShapeListOfShape adjacent;
  TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, adjacent);
  std::array<std::vector<TopoDS_Edge>, 2> contact;
  std::vector<bool> touched(supports.size(), false);
  for (int i = 1; i <= adjacent.Extent(); ++i) {
    auto edge = TopoDS::Edge(adjacent.FindKey(i));
    for (TopTools_ListIteratorOfListOfShape it(adjacent.FindFromIndex(i)); it.More(); it.Next()) {
      const auto candidate = TopoDS::Face(it.Value());
      bool is_generated = std::any_of(generated.begin(), generated.end(),
                                      [&](const auto& f) { return f.face.IsSame(candidate); });
      if (!is_generated)
        continue;
      for (const auto& support : supports) {
        bool adjacent_support = false;
        for (TopTools_ListIteratorOfListOfShape neighbour(adjacent.FindFromIndex(i));
             neighbour.More(); neighbour.Next())
          adjacent_support |= neighbour.Value().IsSame(support.face);
        if (!adjacent_support)
          continue;
        auto checked = verify_surface_join(id, edge, candidate, support.face,
                                           continuity == FilletContinuity::Curvature
                                               ? LoftBoundaryContinuity::G2
                                               : LoftBoundaryContinuity::G1,
                                           2e-5, 1e-4, 1e-4);
        require(bool(checked), checked ? "" : checked.error().message());
        contact[support.origin.role == "first" ? 0 : 1].push_back(edge);
        touched[&support - supports.data()] = true;
      }
    }
  }
  require(std::all_of(touched.begin(), touched.end(), [](bool value) { return value; }),
          "A selected support descendant has no verified blend contact");
  require(!contact[0].empty() && !contact[1].empty(),
          "Generated blend has no verified contact with both selected support sets");
  return contact;
}
void hold_lines(const PartDocument& doc, const ShapeCache& cache,
                StepImportTopologyContext* context, const std::vector<FilletHoldLine>& holds,
                const std::array<std::vector<TopoDS_Edge>, 2>& contacts) {
  for (const auto& hold : holds) {
    auto path = GeometryRecomputeExecutor(context).preview_path_curve(doc, hold.path, cache);
    require(bool(path), path ? "" : hold.id + ": " + path.error().message());
    auto wire = OcctShapeView::get(path.value());
    require(wire, "Hold line has no native curve");
    auto contact = compound(contacts[hold.side == FilletSide::First ? 0 : 1]);
    const double overlap = mass(common(*wire, contact));
    require(std::abs(overlap - mass(*wire)) < tol && std::abs(overlap - mass(contact)) < tol,
            hold.id + ": hold line must coincide with the complete actual support contact");
  }
}
// Complete native edge/face intersection, rather than untrimmed surface projection.
void contact_coverage(const TopoDS_Edge& line, const std::vector<TopoDS_Face>& faces) {
  double covered = 0;
  for (const auto& f : faces)
    covered += mass(common(line, f));
  require(std::abs(covered - mass(line)) < tol,
          "Selected trimmed supports do not cover the complete contact line");
}
Cap planar_round(const FullRoundIntent& intent, const std::vector<TopoDS_Face>& first,
                 const std::vector<TopoDS_Face>& second, const std::vector<TopoDS_Face>& middle,
                 const FeatureId& id) {
  require(first.size() == 1 && second.size() == 1 && middle.size() == 1,
          "Planar full-round requires one continuous face per role");
  const auto z = normal(middle[0]), x = -normal(first[0]), opposed = normal(second[0]);
  require(x.Dot(opposed) > 1 - 1e-10 && std::abs(x.Dot(z)) < 1e-10,
          "Full-round side planes must be opposed and perpendicular to the middle");
  auto p = BRepAdaptor_Surface(first[0]).Plane().Location();
  auto top = BRepAdaptor_Surface(middle[0]).Plane();
  p.Translate(z * gp_Vec(p, top.Location()).Dot(z));
  const double width = gp_Vec(p, BRepAdaptor_Surface(second[0]).Plane().Location()).Dot(x),
               r = width / 2;
  require(width > 2 * tol, "Full-round middle width is unresolved: " + std::to_string(width));
  Frame frame{p, x, z.Crossed(x), z};
  double lo = 1e100, hi = -1e100;
  for (TopExp_Explorer it(middle[0], TopAbs_VERTEX); it.More(); it.Next()) {
    auto v = gp_Vec(p, BRep_Tool::Pnt(TopoDS::Vertex(it.Current())));
    require(v.Dot(x) >= -tol && v.Dot(x) <= width + tol,
            "Middle face extends outside the selected side planes");
    lo = std::min(lo, v.Dot(frame.y));
    hi = std::max(hi, v.Dot(frame.y));
  }
  require(hi - lo > tol, "Full-round middle length is unresolved");
  frame.origin.Translate(frame.y * lo);
  const double length = hi - lo;
  for (double a : {0., width})
    contact_coverage(
        BRepBuilderAPI_MakeEdge(frame.point(a, 0, -r), frame.point(a, length, -r)).Edge(),
        a == 0 ? first : second);
  TopoDS_Edge curve;
  if (intent.continuity == FilletContinuity::Tangent)
    curve = BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(frame.point(0, 0, -r), frame.point(r, 0, 0),
                                                       frame.point(width, 0, -r))
                                        .Value())
                .Edge();
  else
    curve = quintic({frame.point(0, 0, -r), frame.point(0, 0, -.2 * r), frame.point(0, 0, .2 * r),
                     frame.point(width, 0, .2 * r), frame.point(width, 0, -.2 * r),
                     frame.point(width, 0, -r)});
  BRepPrimAPI_MakePrism cap(profile(curve, {frame.point(0, 0, -r)}), frame.y * length);
  require(cap.IsDone(), "Full-round cap extrusion failed");
  auto low = frame;
  low.origin.Translate(z * (-r));
  BRepPrimAPI_MakePrism slab(rectangle(low, width, r), frame.y * length);
  Cap result{cap.Shape(), slab.Shape(), {}};
  for (TopTools_ListIteratorOfListOfShape it(cap.Generated(curve)); it.More(); it.Next())
    if (it.Value().ShapeType() == TopAbs_FACE)
      result.generated.push_back(
          {{id, "fillet.full_round/" + intent.middle.id}, TopoDS::Face(it.Value()), {}, {1, 0, 0}});
  return result;
}
Cap cylindrical_round(const FullRoundIntent& intent, const std::vector<TopoDS_Face>& first,
                      const std::vector<TopoDS_Face>& second,
                      const std::vector<TopoDS_Face>& middle, const FeatureId& id) {
  require(middle.size() == 1, "Cylindrical full-round requires a single annular middle face");
  auto z = normal(middle[0]);
  BRepAdaptor_Surface a(first[0]), b(second[0]);
  require(a.GetType() == GeomAbs_Cylinder && b.GetType() == GeomAbs_Cylinder,
          "Full-round curved sides must be coaxial cylinders");
  auto c = a.Cylinder(), d = b.Cylinder();
  require(std::abs(c.Axis().Direction().Dot(d.Axis().Direction())) > 1 - 1e-10 &&
              std::abs(c.Axis().Direction().Dot(gp_Dir(z))) > 1 - 1e-10 &&
              gp_Lin(c.Axis()).Distance(d.Axis().Location()) < tol,
          "Full-round cylinders and middle plane must share an axis");
  for (const auto& set : {first, second})
    for (const auto& face : set) {
      BRepAdaptor_Surface s(face);
      require(s.GetType() == GeomAbs_Cylinder,
              "Full-round side set includes a noncylindrical face");
      auto cylinder = s.Cylinder();
      require(gp_Lin(c.Axis()).Distance(cylinder.Axis().Location()) < tol &&
                  std::abs(std::abs(c.Axis().Direction().Dot(cylinder.Axis().Direction())) - 1) <
                      1e-10 &&
                  std::abs(cylinder.Radius() - (contains(first, face) ? c.Radius() : d.Radius())) <
                      tol,
              "Full-round side set contains incompatible cylinders");
    }
  const double inner = std::min(c.Radius(), d.Radius()), outer = std::max(c.Radius(), d.Radius()),
               r = (outer - inner) / 2, major = (outer + inner) / 2;
  require(inner > tol && r > tol, "Full-round annular width is unresolved");
  auto axis = c.Axis().Location();
  auto plane = BRepAdaptor_Surface(middle[0]).Plane();
  axis.Translate(z * (gp_Vec(axis, plane.Location()).Dot(z)));
  Frame frame{axis, gp_Vec(c.Position().XDirection()), {}, z};
  frame.y = z.Crossed(frame.x);
  for (double radius : {inner, outer}) {
    auto circle =
        BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(frame.point(0, 0, -r), gp_Dir(z)), radius)).Edge();
    contact_coverage(circle, std::abs(radius - c.Radius()) < tol ? first : second);
  }
  TopoDS_Edge curve;
  if (intent.continuity == FilletContinuity::Tangent)
    curve = BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(frame.point(inner, 0, -r),
                                                       frame.point(major, 0, 0),
                                                       frame.point(outer, 0, -r))
                                        .Value())
                .Edge();
  else
    curve = quintic({frame.point(inner, 0, -r), frame.point(inner, 0, -.2 * r),
                     frame.point(inner, 0, .2 * r), frame.point(outer, 0, .2 * r),
                     frame.point(outer, 0, -.2 * r), frame.point(outer, 0, -r)});
  BRepPrimAPI_MakeRevol cap(profile(curve, {frame.point(inner, 0, -r)}), gp_Ax1(axis, gp_Dir(z)));
  require(cap.IsDone(), "Full-round cap revolution failed");
  auto low = frame;
  low.origin = frame.point(inner, 0, -r);
  BRepPrimAPI_MakeRevol slab(rectangle(low, outer - inner, r), gp_Ax1(axis, gp_Dir(z)));
  Cap result{cap.Shape(), slab.Shape(), {}};
  for (TopTools_ListIteratorOfListOfShape it(cap.Generated(curve)); it.More(); it.Next())
    if (it.Value().ShapeType() == TopAbs_FACE)
      result.generated.push_back(
          {{id, "fillet.full_round/" + intent.middle.id}, TopoDS::Face(it.Value()), {}, {1, 0, 0}});
  return result;
}
} // namespace
Result<NativeFilletResult> face_fillet(const PartDocument& doc, const FilletFeature& feature,
                                       const TopoDS_Shape& target, const ShapeCache& cache,
                                       StepImportTopologyContext* context,
                                       const std::vector<FaceLineage>& lineage) {
  try {
    const auto& definition = feature.advanced_intent()->definition;
    const auto* blend = std::get_if<FaceBlendIntent>(&definition);
    const auto* round = std::get_if<FullRoundIntent>(&definition);
    require(blend || round, "Face finishing mode is missing");
    auto first = resolve(doc, feature, blend ? blend->first : round->first, target, cache, context),
         second =
             resolve(doc, feature, blend ? blend->second : round->second, target, cache, context);
    auto middle = round ? resolve(doc, feature, round->middle, target, cache, context)
                        : std::vector<TopoDS_Face>{};
    for (const auto& a : first)
      require(!contains(second, a) && !contains(middle, a),
              "Resolved face appears in multiple finishing roles");
    for (const auto& a : second)
      require(!contains(middle, a), "Resolved face appears in multiple finishing roles");
    BRepBuilderAPI_Copy copy(target, true, false);
    require(copy.IsDone(), "Cannot isolate face finishing target");
    const auto working = copy.Shape();
    auto mapped = map_lineage(copy, working, lineage);
    for (auto* set : {&first, &second, &middle})
      for (auto& f : *set) {
        const auto orientation = f.Orientation();
        f = TopoDS::Face(copy.ModifiedShape(f));
        f.Orientation(orientation);
      }
    auto supports = marked(first, feature.id(), "first");
    append(supports, marked(second, feature.id(), "second"));
    std::vector<FaceLineage> generated;
    TopoDS_Shape result;
    auto continuity = blend ? blend->continuity : round->continuity;
    if (blend && continuity == FilletContinuity::Tangent) {
      const double radius = doc.find_parameter(blend->size)->value().millimeters();
      BRepFilletAPI_MakeFillet builder(working);
      builder.SetParams(1e-2, 1e-7, 1e-8, 1e-7, 1e-8, 1e-4);
      TopTools_IndexedDataMapOfShapeListOfShape adjacent;
      TopExp::MapShapesAndAncestors(working, TopAbs_EDGE, TopAbs_FACE, adjacent);
      std::vector<TopoDS_Edge> selected;
      for (int i = 1; i <= adjacent.Extent(); ++i) {
        bool one = false, two = false;
        for (TopTools_ListIteratorOfListOfShape it(adjacent.FindFromIndex(i)); it.More();
             it.Next()) {
          one |= contains(first, it.Value());
          two |= contains(second, it.Value());
        }
        if (one && two) {
          auto edge = TopoDS::Edge(adjacent.FindKey(i));
          builder.Add(radius, edge);
          selected.push_back(edge);
        }
      }
      require(!selected.empty(), "Face sets have no shared sharp edge");
      for (int c = 1; c <= builder.NbContours(); ++c)
        for (int j = 1; j <= builder.NbEdges(c); ++j) {
          auto edge = builder.Edge(c, j);
          require(std::any_of(selected.begin(), selected.end(),
                              [&](const auto& e) { return e.IsSame(edge); }),
                  "Tangent propagation leaves the selected support sets");
        }
      builder.Build();
      require(builder.IsDone(), "Face blend failed; inspect the size and selected supports");
      result = builder.Shape();
      mapped = map_lineage(builder, result, mapped);
      supports = map_lineage(builder, result, supports);
      for (const auto& edge : selected)
        for (TopTools_ListIteratorOfListOfShape it(builder.Generated(edge)); it.More(); it.Next())
          if (it.Value().ShapeType() == TopAbs_FACE)
            append_lineage(
                generated,
                {{feature.id(), "fillet.face_blend/" + blend->first.id + "/" + blend->second.id},
                 TopoDS::Face(it.Value()),
                 {},
                 {1, 0, 0}},
                result);
    } else {
      Cap cap;
      if (round) {
        if (BRepAdaptor_Surface(first[0]).GetType() == GeomAbs_Cylinder)
          cap = cylindrical_round(*round, first, second, middle, feature.id());
        else
          cap = planar_round(*round, first, second, middle, feature.id());
      } else {
        require(first.size() == 1 && second.size() == 1,
                "G2 face blend requires one continuous face per side");
        const auto x = -normal(first[0]), z = -normal(second[0]);
        require(std::abs(x.Dot(z)) < 1e-10, "G2 face blend requires orthogonal convex planes");
        std::vector<TopoDS_Edge> edges;
        for (TopExp_Explorer it(first[0], TopAbs_EDGE); it.More(); it.Next())
          for (TopExp_Explorer jt(second[0], TopAbs_EDGE); jt.More(); jt.Next())
            if (it.Current().IsSame(jt.Current()))
              edges.push_back(TopoDS::Edge(it.Current()));
        require(edges.size() == 1, "G2 face blend requires one shared straight edge");
        BRepAdaptor_Curve edge(edges[0]);
        require(edge.GetType() == GeomAbs_Line, "G2 planar blend requires a straight shared edge");
        const auto start = edge.Value(edge.FirstParameter()),
                   end = edge.Value(edge.LastParameter());
        const double length = start.Distance(end),
                     r = doc.find_parameter(blend->size)->value().millimeters();
        Frame f{start, x, gp_Vec(start, end).Normalized(), z};
        for (bool side : {false, true})
          contact_coverage(BRepBuilderAPI_MakeEdge(f.point(side ? r : 0, 0, side ? 0 : r),
                                                   f.point(side ? r : 0, length, side ? 0 : r))
                               .Edge(),
                           side ? second : first);
        auto curve = quintic({f.point(0, 0, r), f.point(0, 0, .6 * r), f.point(0, 0, .2 * r),
                              f.point(.2 * r, 0, 0), f.point(.6 * r, 0, 0), f.point(r, 0, 0)});
        auto face = profile(curve, {start, f.point(0, 0, r)});
        BRepPrimAPI_MakePrism tool(face, f.y * length);
        require(tool.IsDone(), "G2 profile extrusion failed");
        cap.solid = tool.Shape();
        for (TopTools_ListIteratorOfListOfShape it(tool.Generated(curve)); it.More(); it.Next())
          if (it.Value().ShapeType() == TopAbs_FACE)
            cap.generated.push_back(
                {{feature.id(), "fillet.face_blend/" + blend->first.id + "/" + blend->second.id},
                 TopoDS::Face(it.Value()),
                 {},
                 {1, 0, 0}});
      }
      TopoDS_Shape tool = cap.solid;
      generated = cap.generated;
      if (round) {
        const auto middle_plane = BRepAdaptor_Surface(middle.front()).Plane();
        for (TopExp_Explorer it(working, TopAbs_FACE); it.More(); it.Next()) {
          const auto face = TopoDS::Face(it.Current());
          if (contains(middle, face))
            continue;
          BRepAdaptor_Surface support(face);
          if (support.GetType() != GeomAbs_Plane)
            continue;
          const auto plane = support.Plane();
          if (std::abs(plane.Axis().Direction().Dot(middle_plane.Axis().Direction())) < 1 - 1e-10 ||
              middle_plane.Distance(plane.Location()) > tol)
            continue;
          GProp_GProps overlap;
          BRepGProp::SurfaceProperties(common(face, cap.slab), overlap);
          require(overlap.Mass() < tol * tol,
                  "Full-round removal overlaps an unselected middle face");
        }
        BRepAlgoAPI_Cut tool_cut(cap.slab, cap.solid);
        tool_cut.SetNonDestructive(true);
        tool_cut.Build();
        require(tool_cut.IsDone(), "Cannot construct local full-round removal");
        tool = tool_cut.Shape();
        generated = map_lineage(tool_cut, tool, generated);
      }
      BRepAlgoAPI_Cut operation(working, tool);
      operation.SetNonDestructive(true);
      operation.Build();
      require(operation.IsDone(), "Finishing Boolean failed");
      result = operation.Shape();
      mapped = map_lineage(operation, result, mapped);
      supports = map_lineage(operation, result, supports);
      generated = map_lineage(operation, result, generated);
      if (round)
        for (const auto& face : middle) {
          auto remaining = common(result, face);
          GProp_GProps area;
          BRepGProp::SurfaceProperties(remaining, area);
          require(area.Mass() < tol * tol,
                  "Full-round did not replace the complete selected middle region");
        }
    }
    require(!result.IsNull() && solids(result) == solids(target) &&
                BRepCheck_Analyzer(result, true).IsValid() &&
                BRepAlgoAPI_Check(result, false, true).IsValid(),
            "Face finishing produced invalid, self-interfering or disconnected material");
    const double before = mass(target, true), after = mass(result, true);
    require(std::isfinite(after) && after > tol && after < before - tol * tol * tol,
            "Face finishing must remove finite positive material");
    require(!generated.empty(), "Face finishing generated no identified blend surface");
    auto contacts = verify(feature.id(), result, generated, supports, continuity);
    hold_lines(doc, cache, context, blend ? blend->hold_lines : round->hold_lines, contacts);
    append(mapped, generated);
    return Result<NativeFilletResult>::success({result, std::move(mapped)});
  } catch (const Standard_Failure& e) {
    return Result<NativeFilletResult>::failure(Error::geometry(
        feature.id().value(),
        e.GetMessageString() ? e.GetMessageString() : "Native face finishing failed"));
  } catch (const std::exception& e) {
    return Result<NativeFilletResult>::failure(Error::geometry(feature.id().value(), e.what()));
  }
}
} // namespace blcad::geometry::detail
