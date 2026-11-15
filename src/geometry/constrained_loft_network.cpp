#include "constrained_loft_network.hpp"
#include "blcad/core/sketch_contour.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "derived_curve_internal.hpp"
#include "loft_section_alignment_internal.hpp"
#include "native_path_curve.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAPI_IntCS.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomConvert.hxx>
#include <GeomConvert_CompCurveToBSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Plane.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Pln.hxx>
#include <limits>
#include <stdexcept>

namespace blcad::geometry::detail {
namespace {
template <class T> T checked(Result<T> value) {
  if (!value)
    throw std::runtime_error(value.error().message());
  return std::move(value.value());
}
Handle(Geom_Curve) native_curve(const TopoDS_Edge& edge) {
  double first, last;
  const auto source = BRep_Tool::Curve(edge, first, last);
  if (source.IsNull())
    throw std::runtime_error("Loft requires native bounded curves");
  Handle(Geom_TrimmedCurve) curve = new Geom_TrimmedCurve(source, first, last);
  if (edge.Orientation() == TopAbs_REVERSED)
    curve->Reverse();
  return curve;
}
struct Contour {
  std::vector<Handle(Geom_Curve)> curves;
  std::vector<double> lengths;
  double length = 0;
  explicit Contour(const TopoDS_Wire& wire) {
    for (BRepTools_WireExplorer edge(wire); edge.More(); edge.Next()) {
      auto curve = native_curve(edge.Current());
      BRepAdaptor_Curve adaptor(edge.Current());
      const double size = GCPnts_AbscissaPoint::Length(adaptor);
      if (!std::isfinite(size) || size <= 1e-9)
        throw std::runtime_error("Collapsed native Loft contour");
      curves.push_back(curve);
      lengths.push_back(size);
      length += size;
    }
    if (curves.empty() || curves.size() > 512)
      throw std::runtime_error("Native Loft contour exceeds its edge budget");
  }
  double parameter(std::size_t i, double fraction) const {
    const auto& curve = curves[i];
    if (fraction <= 0)
      return curve->FirstParameter();
    if (fraction >= 1)
      return curve->LastParameter();
    GeomAdaptor_Curve adaptor(curve);
    GCPnts_AbscissaPoint point(1e-11, adaptor, lengths[i] * fraction, curve->FirstParameter());
    if (!point.IsDone())
      throw std::runtime_error("Native Loft length station did not converge");
    return point.Parameter();
  }
  gp_Pnt at(double distance) const {
    for (std::size_t i = 0; i < curves.size(); ++i) {
      if (distance <= lengths[i] || i + 1 == curves.size())
        return curves[i]->Value(parameter(i, std::clamp(distance / lengths[i], 0.0, 1.0)));
      distance -= lengths[i];
    }
    throw std::runtime_error("Missing native Loft contour station");
  }
  TopoDS_Edge span(double start, double end, bool closed) const {
    if (closed && end <= start)
      end += length;
    if (end - start <= 1e-9 || end - start > length + 1e-8)
      throw std::runtime_error("Loft guides do not delimit a distinct native section span");
    Handle(Geom_BSplineCurve) combined;
    double base = 0;
    for (int turn = 0; turn < (closed ? 2 : 1); ++turn)
      for (std::size_t i = 0; i < curves.size(); ++i) {
        const double a = std::max(start, base), b = std::min(end, base + lengths[i]);
        if (b - a > 1e-10) {
          const Handle(Geom_TrimmedCurve) trimmed =
              new Geom_TrimmedCurve(curves[i], parameter(i, (a - base) / lengths[i]),
                                    parameter(i, (b - base) / lengths[i]));
          auto next = GeomConvert::CurveToBSplineCurve(trimmed);
          if (combined.IsNull())
            combined = next;
          else {
            GeomConvert_CompCurveToBSplineCurve join(combined);
            if (!join.Add(next, 1e-9, true, true, 0))
              throw std::runtime_error("Native Loft section pieces cannot be concatenated exactly");
            combined = join.BSplineCurve();
          }
        }
        base += lengths[i];
      }
    if (combined.IsNull())
      throw std::runtime_error("Missing native Loft section span");
    return BRepBuilderAPI_MakeEdge(combined).Edge();
  }
};
TopoDS_Wire wire(const std::vector<SweepPathSegment>& segments, const std::string& id,
                 bool closed) {
  auto shape = checked(SweepAdapter{}.path_shape(PathCurveId(id), segments, closed));
  const auto* native = OcctShapeView::get(shape);
  if (!native || native->ShapeType() != TopAbs_WIRE)
    throw std::runtime_error("Loft requires one ordered native wire");
  return TopoDS::Wire(*native);
}
TopoDS_Wire closed_wire(const std::vector<ClosedProfileCurveSegment>& curves,
                        const std::string& id) {
  std::vector<SweepPathSegment> result;
  for (const auto& curve : curves) {
    SweepPathSegment span{SweepPathSegmentKind::Line, curve.start, curve.mid, curve.end};
    if (curve.kind == ClosedProfileCurveKind::CircularArc)
      span.kind = SweepPathSegmentKind::CircularArc;
    else if (curve.kind == ClosedProfileCurveKind::DerivedNative) {
      span.kind = SweepPathSegmentKind::DerivedNative;
      span.derived_curve = curve.derived_curve;
    } else if (curve.kind == ClosedProfileCurveKind::CubicBezierSpline) {
      span.kind = SweepPathSegmentKind::DerivedNative;
      span.derived_curve = checked(native_rational_bezier(
          {curve.start, curve.mid, curve.control2, curve.end}, curve.weights, id));
    }
    result.push_back(std::move(span));
  }
  return wire(result, id, true);
}
double anchor_distance(const PartDocument& document, const ProfileSectionReference& section,
                       const LoftGuideAnchor& anchor, const Contour& contour, const ShapeCache&) {
  if (!anchor.boundary_entity)
    return anchor.boundary_fraction * contour.length;
  const auto& source = std::get<ProfileRegionReference>(section.source());
  const auto* sketch = document.find_sketch(source.sketch());
  std::vector<SketchEntityId> entities;
  if (const auto* p = sketch->find_arc_closed_profile(source.profile()))
    entities = p->curve_segments();
  else if (const auto* p = sketch->find_closed_profile(source.profile()))
    entities = p->line_segments();
  else
    throw std::runtime_error("This Loft section has no original entity anchor capability");
  auto traversal = checked(resolve_sketch_contour(*sketch, entities));
  if (section.alignment_reference()) {
    auto found = std::find(entities.begin(), entities.end(), *section.alignment_reference());
    if (found == entities.end())
      throw std::runtime_error("Missing authored Loft seam entity");
    const auto offset = found - entities.begin();
    std::rotate(entities.begin(), found, entities.end());
    std::rotate(traversal.begin(), traversal.begin() + offset, traversal.end());
  }
  const auto found = std::find(entities.begin(), entities.end(), *anchor.boundary_entity);
  if (found == entities.end() || entities.size() != contour.curves.size())
    throw std::runtime_error("Loft anchor must retain its original boundary entity");
  const auto index = std::size_t(found - entities.begin());
  const bool forward = !traversal[index].reversed;
  double result = 0;
  for (std::size_t i = 0; i < index; ++i)
    result += contour.lengths[i];
  return result + contour.lengths[index] *
                      (forward ? anchor.boundary_fraction : 1 - anchor.boundary_fraction);
}
} // namespace
Result<NativeLoftNetwork> resolve_constrained_loft_network(const PartDocument& document,
                                                           const LoftFeature& feature,
                                                           const ShapeCache& cache,
                                                           StepImportTopologyContext* imports,
                                                           double tolerance) {
  const auto fail = [&](std::string message) {
    return Result<NativeLoftNetwork>::failure(
        Error::geometry(feature.id().value(), std::move(message)));
  };
  try {
    if (!feature.constraints() || feature.guide_curves().size() < 2 ||
        feature.guide_curves().size() > 32)
      return fail(
          "Verified Loft currently requires two to thirty-two explicitly anchored native rails");
    const auto& intent = *feature.constraints();
    if (std::any_of(feature.sections().begin(), feature.sections().end(), [&](const auto& section) {
          return section.flip_normal() != feature.sections().front().flip_normal();
        }))
      return fail(
          "Verified Loft requires consistent section traversal; mixed normal flips are ambiguous");
    const bool closed = feature.sections().front().kind() == LoftSectionKind::ClosedRegion;
    GeometryRecomputeExecutor executor(imports);
    NativeLoftNetwork network{closed, {}, {}, {}};
    std::vector<Contour> rails;
    for (const auto& binding : intent.guides) {
      const auto* path = document.find_path_curve(binding.guide);
      if (path->execution_policy().kind() != PathGeometryPolicyKind::CurvePreserving)
        return fail("Verified Loft rails require explicit curve-preserving execution");
      auto segments = checked(executor.resolve_path_segments(document, binding.guide, cache));
      rails.emplace_back(wire(segments, binding.guide.value(), false));
      auto edge = rails.back().span(
          std::min(binding.anchors.front().guide_fraction, binding.anchors.back().guide_fraction) *
              rails.back().length,
          std::max(binding.anchors.front().guide_fraction, binding.anchors.back().guide_fraction) *
              rails.back().length,
          false);
      if (binding.reversed)
        edge.Reverse();
      network.rails.push_back(edge);
    }
    for (std::size_t s = 0; s < feature.sections().size(); ++s) {
      const auto& section = feature.sections()[s];
      TopoDS_Wire source;
      if (closed) {
        auto canonical = checked(ProfileSectionReference::create_closed_region(
            std::get<ProfileRegionReference>(section.source()), section.alignment_reference(),
            false, section.rotation_offset()));
        source = closed_wire(checked(resolve_loft_alignment_section(document, canonical, cache)),
                             feature.id().value());
      } else {
        if (section.rotation_offset())
          return fail("Verified open Loft does not support authored section rotation");
        source = wire(checked(executor.resolve_path_segments(
                          document, std::get<PathCurveId>(section.source()), cache)),
                      feature.id().value(), false);
      }
      network.contours.push_back(source);
      Contour contour(source);
      std::vector<double> distances;
      for (std::size_t g = 0; g < rails.size(); ++g) {
        const auto& anchor = intent.guides[g].anchors[s];
        double station = anchor_distance(document, section, anchor, contour, cache);
        if (closed && station == contour.length)
          station = 0;
        const auto point = contour.at(station);
        if (point.Distance(rails[g].at(anchor.guide_fraction * rails[g].length)) > tolerance)
          return fail(
              "Authored Loft guide anchor is not incident to its original section in model units");
        distances.push_back(station);
      }
      double covered = 0;
      std::vector<TopoDS_Edge> spans;
      for (std::size_t g = 0; g < (closed ? rails.size() : rails.size() - 1); ++g) {
        const double a = distances[g], b = distances[(g + 1) % rails.size()];
        if (!closed && b <= a)
          return fail("Open Loft guide order must increase along every section");
        covered += b > a ? b - a : b + contour.length - a;
        spans.push_back(contour.span(a, b, closed));
      }
      if (closed && std::abs(covered - contour.length) > tolerance)
        return fail("Loft guide order must describe exactly one traversal of every closed section");
      if (!closed &&
          (distances.front() > tolerance || contour.length - distances.back() > tolerance))
        return fail("Open verified Loft requires rails on both section endpoints");
      if (section.flip_normal())
        for (auto& span : spans)
          span.Reverse();
      network.sections.push_back(std::move(spans));
    }
    if (feature.path_curve()) {
      if (!closed)
        return fail("Verified center paths currently require closed section interiors");
      Contour center(
          wire(checked(executor.resolve_path_segments(document, *feature.path_curve(), cache)),
               feature.path_curve()->value(), false));
      const auto curve = native_curve(center.span(0, center.length, false));
      double previous = -std::numeric_limits<double>::infinity();
      for (const auto& contour : network.contours) {
        BRepBuilderAPI_MakeFace made(contour, true);
        if (!made.IsDone())
          return fail("Center path requires finite planar section interiors");
        const auto plane = BRepAdaptor_Surface(made.Face()).Plane();
        GeomAPI_IntCS intersection(curve, new Geom_Plane(plane));
        if (!intersection.IsDone() || intersection.NbSegments() != 0)
          return fail("Loft center path/section station is ambiguous");
        std::vector<double> matches;
        for (int i = 1; i <= intersection.NbPoints(); ++i) {
          BRepClass_FaceClassifier classifier(made.Face(), intersection.Point(i), tolerance);
          if (classifier.State() != TopAbs_IN && classifier.State() != TopAbs_ON)
            continue;
          double u, v, t;
          intersection.Parameters(i, u, v, t);
          matches.push_back(t);
        }
        if (matches.size() != 1 || matches.front() <= previous)
          return fail("Loft center path must cross each finite section interior once in order");
        previous = matches.front();
      }
    }
    return Result<NativeLoftNetwork>::success(std::move(network));
  } catch (const Standard_Failure& e) {
    return fail(e.GetMessageString() ? e.GetMessageString() : "Native Loft network failed");
  } catch (const std::exception& e) {
    return fail(e.what());
  }
}
} // namespace blcad::geometry::detail
