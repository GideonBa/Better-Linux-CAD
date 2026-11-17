#include "emboss_native.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepFill.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepLib_CheckCurveOnSurface.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Plane.hxx>
#include <ShapeAnalysis.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp.hxx>
#include <limits>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
void require(bool yes, const char* message) {
  if (!yes)
    throw std::runtime_error(message);
}
double area(const TopoDS_Shape& shape) {
  GProp_GProps p;
  BRepGProp::SurfaceProperties(shape, p);
  return std::abs(p.Mass());
}
TopoDS_Shape common(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  BRepAlgoAPI_Common op;
  TopTools_ListOfShape args, tools;
  args.Append(a);
  tools.Append(b);
  op.SetArguments(args);
  op.SetTools(tools);
  op.SetNonDestructive(true);
  op.Build();
  require(op.IsDone() && !op.HasErrors(), "Emboss trim coverage cannot be resolved");
  return op.Shape();
}
struct Loop {
  EmbossOutlineLoop source;
  TopoDS_Face uv;
  double area;
  int parent = -1, depth = 0;
  bool reverse = false;
};
struct Band {
  TopoDS_Wire wire;
  std::vector<TopoDS_Edge> edges;
};
Band wire(const EmbossOutlineLoop& source, const Handle(Geom_Surface) & surface, bool reverse) {
  BRepBuilderAPI_MakeWire builder;
  Band out;
  for (const auto& curve : source.curves) {
    auto edge =
        BRepBuilderAPI_MakeEdge(curve, surface, curve->FirstParameter(), curve->LastParameter())
            .Edge();
    require(BRepLib::BuildCurve3d(edge, 1e-8), "Emboss boundary has no controlled spatial curve");
    BRepLib_CheckCurveOnSurface check(edge, BRepBuilderAPI_MakeFace(surface, 1e-7).Face());
    check.Perform();
    require(check.IsDone() && check.MaxDistance() <= 1e-7,
            "Emboss boundary exceeds its spatial tolerance");
    builder.Add(edge);
    require(builder.IsDone(), "Emboss outline is not connected");
    out.edges.push_back(builder.Edge());
  }
  out.wire = builder.Wire();
  if (reverse)
    out.wire.Reverse();
  return out;
}
std::vector<Loop> nesting(const std::vector<EmbossOutlineLoop>& source) {
  Handle(Geom_Surface) plane = new Geom_Plane(gp::XOY());
  std::vector<Loop> loops;
  for (const auto& s : source) {
    auto boundary = wire(s, plane, false);
    BRepBuilderAPI_MakeFace face(plane, boundary.wire, false);
    require(face.IsDone(), "Emboss contour cannot bound a region");
    bool reverse = !ShapeAnalysis::IsOuterBound(face.Face());
    if (reverse) {
      boundary.wire.Reverse();
      face = BRepBuilderAPI_MakeFace(plane, boundary.wire, false);
    }
    require(BRepCheck_Analyzer(face.Face(), true).IsValid(),
            "Emboss contour is invalid or self-intersecting");
    const double a = area(face.Face());
    require(a > 1e-9, "Emboss contour has no area");
    loops.push_back({s, face.Face(), a, -1, 0, reverse});
  }
  for (std::size_t i = 0; i < loops.size(); ++i)
    for (std::size_t j = i + 1; j < loops.size(); ++j) {
      const double overlap = area(common(loops[i].uv, loops[j].uv));
      if (overlap < 1e-9)
        continue;
      require(std::abs(loops[i].area - loops[j].area) > 1e-8,
              "Emboss contours coincide ambiguously");
      require(std::abs(overlap - std::min(loops[i].area, loops[j].area)) < 1e-7,
              "Emboss contours overlap without proper nesting");
    }
  for (std::size_t i = 0; i < loops.size(); ++i) {
    double enclosing = std::numeric_limits<double>::infinity();
    auto p =
        loops[i].source.curves.front()->Value(loops[i].source.curves.front()->FirstParameter());
    for (std::size_t j = 0; j < loops.size(); ++j)
      if (i != j && loops[j].area > loops[i].area) {
        BRepClass_FaceClassifier classifier(loops[j].uv, p, 1e-8);
        require(classifier.State() != TopAbs_ON, "Emboss contours touch ambiguously");
        if (classifier.State() == TopAbs_IN && loops[j].area < enclosing) {
          loops[i].parent = int(j);
          enclosing = loops[j].area;
        }
      }
  }
  for (auto& loop : loops)
    for (int parent = loop.parent; parent >= 0; parent = loops[parent].parent)
      ++loop.depth;
  return loops;
}
} // namespace
RibNative emboss_tools(const std::vector<EmbossOutlineLoop>& outlines, const EmbossChart& chart,
                       const std::vector<TopoDS_Face>& supports, const FeatureId& id) {
  auto loops = nesting(outlines);
  BRep_Builder builder;
  TopoDS_Compound all, support;
  builder.MakeCompound(all);
  builder.MakeCompound(support);
  for (const auto& f : supports)
    builder.Add(support, f);
  RibNative out;
  for (std::size_t root = 0; root < loops.size(); ++root) {
    if (loops[root].depth % 2)
      continue;
    BRepBuilderAPI_Sewing sewing(1e-7, true, true, true, false);
    std::vector<FaceLineage> lineage;
    std::vector<Band> base, top;
    std::vector<std::size_t> members{root};
    for (std::size_t i = 0; i < loops.size(); ++i)
      if (loops[i].parent == int(root))
        members.push_back(i);
    for (auto i : members) {
      bool reverse = loops[i].reverse ^ (i != root);
      base.push_back(wire(loops[i].source, chart.base, reverse));
      top.push_back(wire(loops[i].source, chart.depth, reverse));
    }
    BRepBuilderAPI_MakeFace bottom(chart.base, base.front().wire, false),
        cap(chart.depth, top.front().wire, false);
    for (std::size_t i = 1; i < base.size(); ++i) {
      bottom.Add(base[i].wire);
      cap.Add(top[i].wire);
    }
    require(bottom.IsDone() && cap.IsDone() && BRepCheck_Analyzer(bottom.Face(), true).IsValid() &&
                BRepCheck_Analyzer(cap.Face(), true).IsValid(),
            "Emboss caps are not valid bounded regions");
    const double full = area(bottom.Face()), covered = area(common(bottom.Face(), support));
    require(std::abs(covered - full) <= std::max(1e-6, full * 1e-8),
            "Selected finite Emboss support does not cover the complete source region");
    const auto add = [&](TopoDS_Face face, const std::string& role) {
      sewing.Add(face);
      BRepAdaptor_Surface surface(face);
      Vector3 x{1, 0, 0};
      if (surface.GetType() == GeomAbs_Plane &&
          std::abs(surface.Plane().Axis().Direction().X()) > .99)
        x = {0, 1, 0};
      FaceLineage entry{{id, role}, face, {0, 0, 0}, x};
      entry.preserve_partition = true;
      lineage.push_back(std::move(entry));
    };
    add(bottom.Face(), loops[root].source.role + "/base");
    add(cap.Face(), loops[root].source.role + "/cap");
    for (std::size_t i = 0; i < base.size(); ++i)
      for (std::size_t edge = 0; edge < base[i].edges.size(); ++edge)
        add(BRepFill::Face(base[i].edges[edge], top[i].edges[edge]),
            loops[members[i]].source.role + "/side/" + std::to_string(edge));
    sewing.Perform();
    auto sewn = sewing.SewedShape();
    require(!sewn.IsNull() && sewn.ShapeType() == TopAbs_SHELL,
            "Emboss normal sides do not close a shell");
    BRepBuilderAPI_MakeSolid solid(TopoDS::Shell(sewn));
    require(solid.IsDone(), "Emboss tool cannot form a solid");
    auto shape = solid.Solid();
    require(BRepLib::OrientClosedSolid(shape),
            "Emboss tool has no consistent material orientation");
    validate_rib_solid(shape);
    require(rib_volume(shape) > 1e-8, "Emboss tool has no positive material");
    builder.Add(all, shape);
    for (auto entry : lineage) {
      if (sewing.IsModifiedSubShape(entry.face))
        entry.face = TopoDS::Face(sewing.ModifiedSubShape(entry.face));
      append_lineage(out.lineage, std::move(entry), shape);
    }
  }
  out.shape = all;
  require(TopExp_Explorer(all, TopAbs_SOLID).More(), "Emboss has no material tool");
  return out;
}
} // namespace blcad::geometry::detail
