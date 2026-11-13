#include "blcad/geometry/equation_curve_geometry.hpp"
#include "blcad/geometry/sketch_region_finder.hpp"
#include "derived_curve_internal.hpp"
#include <GeomAPI.hxx>
#include <gp_Pln.hxx>

#include "blcad/core/sketch_contour.hpp"
#include "occt_failure_message.hpp"

#include <GCE2d_MakeArcOfCircle.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2d_BezierCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <TColStd_Array1OfReal.hxx>

#include <algorithm>
#include <cmath>
#include <exception>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr double kConnectivityTolerance = 1e-9;
constexpr double kIntersectionTolerance = 1e-8;
constexpr double kPickDeflection = 1e-3;

struct RegionCurve {
  SketchEntityId id;
  Point2 start;
  Point2 end;
  Handle(Geom2d_Curve) curve;
  bool spline{false};
  bool closed{false};
};

bool same(Point2 a, Point2 b, double tolerance = kConnectivityTolerance) {
  return std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance;
}
Point2 point(gp_Pnt2d p) { return {p.X(), p.Y()}; }

bool allowed_joint(const RegionCurve& a, const RegionCurve& b, Point2 p) {
  for (const auto first : {a.start, a.end})
    for (const auto second : {b.start, b.end})
      if (same(first, second) && same(first, p, 16 * kIntersectionTolerance))
        return true;
  return false;
}

Result<std::size_t> validate_intersections(const std::vector<RegionCurve>& curves,
                                         const std::string& object_id) {
  const auto fail = [&](const char* message) {
    return Result<std::size_t>::failure(Error::validation(object_id, message));
  };
  for (std::size_t i = 0; i < curves.size(); ++i) {
    if (curves[i].spline) {
      Geom2dAPI_InterCurveCurve self(curves[i].curve, kIntersectionTolerance);
      if (!self.Intersector().IsDone())
        return fail("automatic curved region intersection validation did not converge");
      const auto seam_pair = [&](const IntRes2d_IntersectionPoint& witness) {
        const double first = curves[i].curve->FirstParameter(),
                     last = curves[i].curve->LastParameter();
        const double tolerance = 1e-10 * std::max(1., last - first);
        const double a = witness.ParamOnFirst(), b = witness.ParamOnSecond();
        const bool endpoints =
            (std::abs(a - first) <= tolerance && std::abs(b - last) <= tolerance) ||
            (std::abs(b - first) <= tolerance && std::abs(a - last) <= tolerance);
        return curves[i].closed && endpoints &&
               same(point(witness.Value()), curves[i].start, 16 * kIntersectionTolerance);
      };
      for (int n = 1; n <= self.NbPoints(); ++n)
        if (!seam_pair(self.Intersector().Point(n)))
          return fail("automatic region detection rejects self-intersecting spline edges");
      for (int n = 1; n <= self.NbSegments(); ++n) {
        const auto& segment = self.Intersector().Segment(n);
        if (!curves[i].closed || !segment.HasFirstPoint() || !segment.HasLastPoint())
          return fail("automatic region detection rejects overlapping equation spans");
        const auto& first = segment.FirstPoint();
        const auto& last = segment.LastPoint();
        if (!seam_pair(first) || !seam_pair(last))
          return fail("automatic region detection rejects overlapping equation spans");
        for (const auto p :
             {point(first.Value()), point(last.Value()),
              point(curves[i].curve->Value((first.ParamOnFirst() + last.ParamOnFirst()) * .5)),
              point(curves[i].curve->Value((first.ParamOnSecond() + last.ParamOnSecond()) * .5))})
          if (!same(p, curves[i].start, 16 * kIntersectionTolerance))
            return fail("automatic region detection rejects overlapping equation spans");
      }
    }
    for (std::size_t j = i + 1; j < curves.size(); ++j) {
      Geom2dAPI_InterCurveCurve intersection(curves[i].curve, curves[j].curve,
                                            kIntersectionTolerance);
      if (!intersection.Intersector().IsDone())
        return fail("automatic curved region intersection validation did not converge");
      for (int n = 1; n <= intersection.NbPoints(); ++n)
        if (!allowed_joint(curves[i], curves[j], point(intersection.Point(n))))
          return fail("automatic region detection rejects self-intersecting regions");
      for (int n = 1; n <= intersection.NbSegments(); ++n) {
        const auto& segment = intersection.Intersector().Segment(n);
        // Tangential contacts can be returned as a short interval around an
        // intended joint, including zero-length intervals on coincident
        // circles. Inspect parameters directly instead of constructing a
        // trimmed curve whose equal parameter bounds OCCT rejects.
        if (!segment.HasFirstPoint() || !segment.HasLastPoint())
          return fail("automatic region detection rejects overlapping or self-intersecting edges");
        const auto& first = segment.FirstPoint();
        const auto& last = segment.LastPoint();
        for (const auto p : {point(first.Value()), point(last.Value()),
                             point(curves[i].curve->Value((first.ParamOnFirst() + last.ParamOnFirst()) * .5)),
                             point(curves[j].curve->Value((first.ParamOnSecond() + last.ParamOnSecond()) * .5))})
          if (!allowed_joint(curves[i], curves[j], p))
            return fail("automatic region detection rejects overlapping or self-intersecting edges");
      }
    }
  }
  return Result<std::size_t>::success(curves.size());
}

Result<std::vector<SketchEntityId>> order_loop(const std::vector<RegionCurve>& curves,
                                             const std::string& object_id) {
  const auto fail = [&](const char* message) {
    return Result<std::vector<SketchEntityId>>::failure(Error::validation(object_id, message));
  };
  if (curves.empty() || (curves.size() < 2 && !curves.front().closed))
    return fail(
        "automatic curved region detection requires a closed equation or at least two edges");
  for (const auto& curve : curves) {
    for (const Point2 endpoint : {curve.start, curve.end}) {
      std::size_t degree = 0;
      for (const auto& other : curves) {
        degree += same(endpoint, other.start) ? 1 : 0;
        degree += same(endpoint, other.end) ? 1 : 0;
      }
      if (degree > 2)
        return fail("automatic region detection rejects branched regions");
      if (degree < 2)
        return fail("automatic region detection found a gap in the loop");
    }
  }
  std::vector<bool> used(curves.size());
  std::vector<SketchEntityId> ordered{curves.front().id};
  used.front() = true;
  auto cursor = curves.front().end;
  while (ordered.size() < curves.size()) {
    bool found = false;
    for (std::size_t i = 1; i < curves.size(); ++i) {
      if (used[i]) continue;
      if (same(cursor, curves[i].start) || same(cursor, curves[i].end)) {
        ordered.push_back(curves[i].id);
        used[i] = true;
        cursor = same(cursor, curves[i].start) ? curves[i].end : curves[i].start;
        found = true;
        break;
      }
    }
    if (!found)
      return fail("automatic region detection supports one single-contour loop only");
  }
  if (!same(cursor, curves.front().start))
    return fail("automatic region detection loop does not close");
  return Result<std::vector<SketchEntityId>>::success(std::move(ordered));
}

} // namespace

Result<GeneratedClosedProfileCandidate>
SketchRegionFinder::find_single_curve_region(const PartDocument& document, const Sketch& sketch) const {
  if (sketch.arc_segments().empty() && sketch.spline_segments().empty() &&
      sketch.equation_curves().empty())
    return find_single_region(document, sketch);
  const auto fail = [&](std::string message) {
    return Result<GeneratedClosedProfileCandidate>::failure(
        Error::validation(sketch.id().value(), std::move(message)));
  };
  try {
    std::vector<RegionCurve> curves;
    auto lines = resolved_lines(document, sketch);
    if (!lines) return Result<GeneratedClosedProfileCandidate>::failure(lines.error());
    for (const auto& line : lines.value()) {
      GCE2d_MakeSegment segment(gp_Pnt2d(line.start.x, line.start.y), gp_Pnt2d(line.end.x, line.end.y));
      if (!segment.IsDone()) return fail("automatic region detection rejects degenerate edges");
      curves.push_back({line.id, line.start, line.end, Handle(Geom2d_Curve)(segment.Value())});
    }
    for (const auto& arc : sketch.arc_segments()) {
      if (arc.construction())
        continue;
      GCE2d_MakeArcOfCircle circle(gp_Pnt2d(arc.start().x, arc.start().y),
                                  gp_Pnt2d(arc.mid().x, arc.mid().y),
                                  gp_Pnt2d(arc.end().x, arc.end().y));
      if (!circle.IsDone()) return fail("automatic region detection rejects degenerate arcs");
      curves.push_back({arc.id(), arc.start(), arc.end(), Handle(Geom2d_Curve)(circle.Value())});
    }
    for (const auto& spline : sketch.spline_segments()) {
      if (spline.construction())
        continue;
      TColgp_Array1OfPnt2d poles(1, 4);
      TColStd_Array1OfReal weights(1, 4);
      const auto controls = spline.curve().poles;
      for (int i = 1; i <= 4; ++i) {
        poles.SetValue(i, gp_Pnt2d(controls[i - 1].x, controls[i - 1].y));
        weights.SetValue(i, spline.weights()[i - 1]);
      }
      Handle(Geom2d_BezierCurve) curve = new Geom2d_BezierCurve(poles, weights);
      curves.push_back({spline.id(), spline.start(), spline.end(), curve, true});
    }
    for (const auto& equation : sketch.equation_curves()) {
      if (equation.construction())
        continue;
      auto resolved = EquationCurveGeometryAdapter{}.convert(document, equation);
      if (!resolved)
        return Result<GeneratedClosedProfileCandidate>::failure(resolved.error());
      const auto& value = resolved.value();
      const auto& ends = value.endpoints;
      auto projected = GeomAPI::To2d(value.curve->curve, gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
      if (projected.IsNull())
        return fail("Equation curve could not be resolved in its Sketch plane");
      curves.push_back({equation.id(),
                        {ends[0].position.x, ends[0].position.y},
                        {ends[1].position.x, ends[1].position.y},
                        projected,
                        value.kind != EquationCurveGeometryKind::AnalyticCircle &&
                            Geom2dAdaptor_Curve(projected).GetType() != GeomAbs_Line,
                        equation.closure() == EquationCurveClosure::Periodic});
    }
    std::sort(curves.begin(), curves.end(), [](const auto& a, const auto& b) {
      return a.id.value() < b.id.value();
    });
    auto ordered = order_loop(curves, sketch.id().value());
    if (!ordered) return Result<GeneratedClosedProfileCandidate>::failure(ordered.error());
    auto valid = validate_intersections(curves, sketch.id().value());
    if (!valid) return Result<GeneratedClosedProfileCandidate>::failure(valid.error());

    GeneratedClosedProfileCandidate candidate;
    candidate.id = ProfileId("generated.region." + sketch.id().value() + ".0");
    candidate.curve_segments = ordered.value();
    auto cursor = curves.front().start;
    for (const auto& id : ordered.value()) {
      const auto& source = *std::find_if(curves.begin(), curves.end(), [&](const auto& curve) {
        return curve.id == id;
      });
      const bool reversed = !same(cursor, source.start);
      Geom2dAdaptor_Curve adaptor(source.curve);
      GCPnts_QuasiUniformDeflection samples(adaptor, kPickDeflection);
      if (!samples.IsDone() || samples.NbPoints() > 100000)
        return fail("automatic curved region sampling did not converge within its point budget");
      std::vector<double> parameters;
      for (int i = 1; i <= samples.NbPoints(); ++i)
        parameters.push_back(samples.Parameter(i));
      // Keep inflected cubics represented even when their midpoint lies on
      // the endpoint chord. These points supplement deflection sampling.
      if (source.spline)
        for (int i = 1; i < 16; ++i)
          parameters.push_back(i / 16.0);
      std::sort(parameters.begin(), parameters.end());
      parameters.erase(std::unique(parameters.begin(), parameters.end()), parameters.end());
      if (reversed) std::reverse(parameters.begin(), parameters.end());
      for (std::size_t i = 0; i + 1 < parameters.size(); ++i)
        candidate.vertices.push_back(point(source.curve->Value(parameters[i])));
      cursor = reversed ? source.start : source.end;
    }
    double area_twice = 0;
    for (std::size_t i = 0; i < candidate.vertices.size(); ++i) {
      const auto a = candidate.vertices[i];
      const auto b = candidate.vertices[(i + 1) % candidate.vertices.size()];
      area_twice += a.x * b.y - a.y * b.x;
    }
    if (!std::isfinite(area_twice) || std::abs(area_twice) <= 1e-12)
      return fail("automatic region detection rejects zero-area contours");
    return Result<GeneratedClosedProfileCandidate>::success(std::move(candidate));
  } catch (const Standard_Failure& failure) {
    return fail("automatic curved region detection failed: " + standard_failure_message(failure));
  } catch (const std::exception& failure) {
    return fail("automatic curved region detection failed: " + std::string(failure.what()));
  }
}

} // namespace blcad::geometry
