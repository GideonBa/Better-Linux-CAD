#include "blcad/geometry/equation_curve_geometry.hpp"
#include "derived_curve_internal.hpp"
#include "rib_native.hpp"
#include <BOPAlgo_ArgumentAnalyzer.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_Circle.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}
template <class T> T take(Result<T> value) {
  if (!value)
    throw std::runtime_error(value.error().message());
  return std::move(value.value());
}
double size(const PartDocument& p, ParameterId id) {
  const auto* v = p.find_parameter(id);
  require(v && v->type() == ParameterType::Length, "Rib profile dimension is missing");
  return v->value().millimeters();
}
} // namespace
std::vector<RibSpine> rib_spines(const PartDocument& p, const RibWebIntent& r,
                                 const ResolvedWorkplane& frame) {
  const auto* sketch = p.find_sketch(r.source.sketch);
  require(sketch, "Rib source Sketch is unavailable");
  const auto point = [&](Point2 v) {
    auto g = WorkplaneResolver{}.evaluate_point(frame, v);
    return gp_Pnt(g.x, g.y, g.z);
  };
  const auto edge = [&](SketchEntityId id) -> TopoDS_Edge {
    if (const auto* line = sketch->find_line_segment(id))
      return BRepBuilderAPI_MakeEdge(point(line->start()), point(line->end())).Edge();
    if (const auto* arc = sketch->find_arc_segment(id))
      return BRepBuilderAPI_MakeEdge(
                 GC_MakeArcOfCircle(point(arc->start()), point(arc->mid()), point(arc->end()))
                     .Value())
          .Edge();
    if (const auto* spline = sketch->find_spline_segment(id)) {
      TColgp_Array1OfPnt poles(1, 4);
      TColStd_Array1OfReal weights(1, 4);
      const auto curve = spline->curve();
      for (int i = 1; i <= 4; ++i) {
        poles(i) = point(curve.poles[i - 1]);
        weights(i) = curve.weights[i - 1];
      }
      return BRepBuilderAPI_MakeEdge(Handle(Geom_Curve)(new Geom_BezierCurve(poles, weights)))
          .Edge();
    }
    const auto* equation = sketch->find_equation_curve(id);
    require(equation, "Rib source curve is missing");
    const auto local = take(EquationCurveGeometryAdapter{}.convert(p, *equation));
    return take(
        derived_curve_edge(transformed_derived_curve(local.curve, frame.origin, frame.x_axis,
                                                     frame.y_axis, frame.normal),
                           id.value()));
  };
  std::vector<std::vector<std::pair<TopoDS_Edge, std::string>>> contours;
  if (!r.source.profile) {
    std::vector<std::pair<TopoDS_Edge, std::string>> members;
    for (const auto& id : r.source.curves)
      members.emplace_back(edge(id), "curve/" + id.value());
    contours.push_back(std::move(members));
  } else {
    const auto id = *r.source.profile;
    const std::string prefix = "profile/" + id.value() + "/";
    if (const auto* rect = sketch->find_rectangle_profile(id)) {
      const auto c = rect->center();
      const auto w = size(p, rect->width_parameter()) / 2,
                 h = size(p, rect->height_parameter()) / 2;
      const std::vector<Point2> points{
          {c.x - w, c.y - h}, {c.x + w, c.y - h}, {c.x + w, c.y + h}, {c.x - w, c.y + h}};
      std::vector<std::pair<TopoDS_Edge, std::string>> members;
      const char* names[]{"bottom", "right", "top", "left"};
      for (int i = 0; i < 4; ++i)
        members.emplace_back(
            BRepBuilderAPI_MakeEdge(point(points[i]), point(points[(i + 1) % 4])).Edge(),
            prefix + names[i]);
      contours.push_back(std::move(members));
    } else if (const auto* circle = sketch->find_circle_profile(id)) {
      const auto c = point(circle->center());
      const gp_Ax2 axis(c, gp_Dir(frame.normal.x, frame.normal.y, frame.normal.z),
                        gp_Dir(frame.x_axis.x, frame.x_axis.y, frame.x_axis.z));
      contours.push_back(
          {{BRepBuilderAPI_MakeEdge(gp_Circ(axis, size(p, circle->diameter_parameter()) / 2))
                .Edge(),
            prefix + "circle"}});
    } else {
      std::vector<std::vector<SketchEntityId>> ids;
      if (const auto* closed = sketch->find_closed_profile(id))
        ids.push_back(closed->line_segments());
      else if (const auto* closed = sketch->find_arc_closed_profile(id))
        ids.push_back(closed->curve_segments());
      else if (const auto* composite = sketch->find_composite_closed_profile(id)) {
        ids.push_back(composite->outer_contour());
        for (const auto& hole : composite->inner_contours())
          ids.push_back(hole);
      }
      require(!ids.empty(), "Rib profile source is unavailable");
      for (const auto& loop : ids) {
        std::vector<std::pair<TopoDS_Edge, std::string>> members;
        for (const auto& curve : loop)
          members.emplace_back(edge(curve), prefix + curve.value());
        contours.push_back(std::move(members));
      }
    }
  }
  std::vector<RibSpine> result;
  const auto ends = [](const TopoDS_Edge& edge) {
    TopoDS_Vertex a, b;
    TopExp::Vertices(edge, a, b, true);
    return std::pair{BRep_Tool::Pnt(a), BRep_Tool::Pnt(b)};
  };
  for (auto members : contours) {
    // Branches at authored endpoints are ambiguous even when the insertion order looks like a
    // chain.
    for (const auto& [edge, name] : members) {
      const auto [a, b] = ends(edge);
      if (a.Distance(b) < 1e-7)
        continue;
      for (const auto& endpoint : {a, b}) {
        int degree = 0;
        for (const auto& [other, unused] : members) {
          const auto [c, d] = ends(other);
          degree += endpoint.Distance(c) < 1e-7;
          degree += endpoint.Distance(d) < 1e-7;
        }
        require(degree <= 2, "Rib skeleton has an ambiguous endpoint branch");
      }
    }
    std::vector<std::pair<TopoDS_Edge, std::string>> chain;
    const auto flush = [&] {
      if (chain.empty())
        return;
      BRepBuilderAPI_MakeWire wire;
      std::vector<std::pair<TopoDS_Edge, std::string>> connected;
      for (const auto& member : chain) {
        wire.Add(member.first);
        require(wire.IsDone(), "Rib skeleton wire connection failed");
        connected.emplace_back(wire.Edge(), member.second);
      }
      require(wire.IsDone(), "Rib skeleton is not connected");
      BOPAlgo_ArgumentAnalyzer check;
      check.SetShape1(wire.Wire());
      check.SelfInterMode() = true;
      check.Perform();
      require(!check.HasFaulty(), "Rib skeleton self-intersects");
      const auto [a, unused] = ends(chain.front().first);
      const auto [unused2, b] = ends(chain.back().first);
      result.push_back({wire.Wire(), std::move(connected), a.Distance(b) < 1e-7});
      chain.clear();
    };
    for (auto member : members) {
      if (!chain.empty()) {
        auto end = ends(chain.back().first).second;
        const auto next = ends(member.first);
        if (end.Distance(next.first) > 1e-7 && end.Distance(next.second) > 1e-7 &&
            chain.size() == 1) {
          auto first = ends(chain.front().first).first;
          if (first.Distance(next.first) < 1e-7 || first.Distance(next.second) < 1e-7) {
            chain.front().first.Reverse();
            end = first;
          }
        }
        if (end.Distance(next.first) < 1e-7) {
        } else if (end.Distance(next.second) < 1e-7)
          member.first.Reverse();
        else
          flush();
      }
      chain.push_back(std::move(member));
    }
    flush();
  }
  require(!result.empty(), "Rib source has no usable skeleton");
  return result;
}
} // namespace blcad::geometry::detail
