#include "surface_join_verification.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom2d_Line.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_Surface.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <IntRes2d_IntersectionPoint.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Pnt2d.hxx>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace blcad::geometry::detail {
namespace {
struct Jet {
  gp_Pnt point;
  gp_Vec du, dv, normal;
  double E, F, G, determinant, e, f, g, u, v;
  double curvature(gp_Vec direction) const {
    const double rhs1 = du.Dot(direction), rhs2 = dv.Dot(direction);
    const double a = (G * rhs1 - F * rhs2) / determinant;
    const double b = (E * rhs2 - F * rhs1) / determinant;
    const double metric = a * a * E + 2 * a * b * F + b * b * G;
    if (!std::isfinite(metric) || metric <= 1e-24)
      throw std::runtime_error("Singular tangent projection in surface continuity verification");
    const double result = (a * a * e + 2 * a * b * f + b * b * g) / metric;
    if (!std::isfinite(result))
      throw std::runtime_error("Nonfinite signed normal curvature");
    return result;
  }
};
bool finite(const gp_Vec& v) {
  return std::isfinite(v.X()) && std::isfinite(v.Y()) && std::isfinite(v.Z());
}
std::vector<std::pair<int, int>> spans(const std::vector<double>& knots, double value,
                                       double minimum, double maximum) {
  if (knots.empty())
    return {{0, 0}};
  std::vector<std::pair<int, int>> result;
  const double rounding =
      64 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(value));
  for (std::size_t i = 0; i + 1 < knots.size(); ++i)
    if (value >= knots[i] - rounding && value <= knots[i + 1] + rounding && knots[i] < maximum &&
        knots[i + 1] > minimum)
      result.push_back({int(i) + 1, int(i) + 2});
  if (result.empty())
    throw std::runtime_error("Trim parameter is outside its native knot domain");
  return result;
}
struct TrimProjection {
  TopoDS_Face face;
  Handle(Geom_Surface) surface;
  std::vector<TopoDS_Edge> uses;
  BRepExtrema_DistShapeShape distance;
  Handle(Geom_BSplineSurface) spline;
  Handle(Geom_SurfaceOfLinearExtrusion) extrusion;
  Handle(Geom_BSplineCurve) basis;
  std::vector<double> uknots, vknots;
  double u_min, u_max, v_min, v_max;
  double u_period = 0, v_period = 0;
  void derivatives(double u, double v, int u0, int u1, int v0, int v1, gp_Pnt& p, gp_Vec& du,
                   gp_Vec& dv, gp_Vec& duu, gp_Vec& dvv, gp_Vec& duv, bool second_order) const {
    if (!spline.IsNull())
      spline->LocalD2(u, v, u0, u1, v0, v1, p, du, dv, duu, dvv, duv);
    else if (!basis.IsNull()) {
      basis->LocalD2(u, u0, u1, p, du, duu);
      dv = gp_Vec(extrusion->Direction());
      p.Translate(dv.Multiplied(v));
      dvv = gp_Vec(0, 0, 0);
      duv = gp_Vec(0, 0, 0);
    } else if (second_order)
      surface->D2(u, v, p, du, dv, duu, dvv, duv);
    else
      surface->D1(u, v, p, du, dv);
  }
  explicit TrimProjection(const TopoDS_Face& source, double tolerance) : face(source) {
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    u_min = u0;
    u_max = u1;
    v_min = v0;
    v_max = v1;
    if (face.Orientation() != TopAbs_FORWARD && face.Orientation() != TopAbs_REVERSED)
      throw std::runtime_error("Surface continuity requires an oriented manifold face");
    surface = BRep_Tool::Surface(face);
    if (surface.IsNull() || !std::isfinite(u0) || !std::isfinite(u1) || !std::isfinite(v0) ||
        !std::isfinite(v1))
      throw std::runtime_error("Surface continuity requires a finite native support domain");
    auto carrier = surface;
    if (const auto trimmed = Handle(Geom_RectangularTrimmedSurface)::DownCast(carrier);
        !trimmed.IsNull())
      carrier = trimmed->BasisSurface();
    // Finite rectangular trims can hide the periodicity of their analytic carrier.
    // The seam still identifies equivalent endpoint jets of that same carrier.
    u_period = carrier->IsUPeriodic() ? carrier->UPeriod() : 0;
    v_period = carrier->IsVPeriodic() ? carrier->VPeriod() : 0;
    spline = Handle(Geom_BSplineSurface)::DownCast(carrier);
    extrusion = Handle(Geom_SurfaceOfLinearExtrusion)::DownCast(carrier);
    if (!extrusion.IsNull()) {
      auto curve = extrusion->BasisCurve();
      if (const auto trimmed = Handle(Geom_TrimmedCurve)::DownCast(curve); !trimmed.IsNull())
        curve = trimmed->BasisCurve();
      basis = Handle(Geom_BSplineCurve)::DownCast(curve);
    }
    if (!spline.IsNull()) {
      for (int i = 1; i <= spline->NbUKnots(); ++i)
        uknots.push_back(spline->UKnot(i));
      for (int i = 1; i <= spline->NbVKnots(); ++i)
        vknots.push_back(spline->VKnot(i));
    } else if (!basis.IsNull())
      for (int i = 1; i <= basis->NbKnots(); ++i)
        uknots.push_back(basis->Knot(i));
    if (uknots.size() > 512 || vknots.size() > 512)
      throw std::runtime_error("Native continuity support exceeds its knot budget");
    BRep_Builder builder;
    TopoDS_Compound trim;
    builder.MakeCompound(trim);
    // Retain edge uses: the two sides of a periodic seam have distinct pcurves.
    for (TopExp_Explorer edge(face, TopAbs_EDGE); edge.More(); edge.Next()) {
      uses.push_back(TopoDS::Edge(edge.Current()));
      builder.Add(trim, edge.Current());
    }
    if (uses.empty())
      throw std::runtime_error("Surface continuity requires a finite trim boundary");
    distance.LoadS2(trim);
    distance.SetDeflection(std::max(1e-12, tolerance * .05));
  }
};
std::vector<Jet> projected_jets(TrimProjection& trim, const gp_Pnt& point, double tolerance,
                                bool second_order, bool reverse) {
  trim.distance.LoadS1(BRepBuilderAPI_MakeVertex(point).Vertex());
  if (!trim.distance.Perform() || !trim.distance.IsDone() ||
      !std::isfinite(trim.distance.Value()) || trim.distance.Value() > tolerance)
    throw std::runtime_error("Boundary is not incident to the finite trim in model units");
  const auto& face = trim.face;
  std::vector<Jet> jets;
  const auto append = [&](const TopoDS_Edge& edge, double parameter, const gp_Pnt& boundary_point) {
    double first, last;
    const auto pcurve = BRep_Tool::CurveOnSurface(edge, face, first, last);
    if (pcurve.IsNull() || parameter < first || parameter > last)
      throw std::runtime_error("Missing or inconsistent native trim pcurve");
    const auto uv = pcurve->Value(parameter);
    const double u = uv.X(), v = uv.Y();
    if (!std::isfinite(u) || !std::isfinite(v))
      throw std::runtime_error("Nonfinite native trim parameter");
    if (!jets.empty()) {
      const BRepAdaptor_Surface adaptor(face);
      const auto equivalent = [](double a, double b, double period, double resolution) {
        constexpr double rounding = 64 * std::numeric_limits<double>::epsilon();
        const double delta = a - b;
        const double uncertainty =
            std::max(rounding * std::max({1.0, std::abs(a), std::abs(b)}), resolution);
        return std::abs(delta) <= uncertainty ||
               (period > 0 && std::abs(delta) <= period + uncertainty &&
                std::abs(std::remainder(delta, period)) <= uncertainty);
      };
      // Native STEP trims can round a seam endpoint independently of the carrier period.
      // Limit chart equivalence to 0.1% of the physical incidence budget; every jet is
      // still evaluated and compared independently below, including curvature.
      if (!equivalent(u, jets.front().u, trim.u_period, adaptor.UResolution(tolerance * .001)) ||
          !equivalent(v, jets.front().v, trim.v_period, adaptor.VResolution(tolerance * .001)))
        throw std::runtime_error("Ambiguous native projection onto the trimmed support");
    }
    for (const auto& [u0, u1] : spans(trim.uknots, u, trim.u_min, trim.u_max))
      for (const auto& [v0, v1] : spans(trim.vknots, v, trim.v_min, trim.v_max)) {
        Jet jet{};
        jet.u = u;
        jet.v = v;
        gp_Vec duu, dvv, duv;
        trim.derivatives(u, v, u0, u1, v0, v1, jet.point, jet.du, jet.dv, duu, dvv, duv,
                         second_order);
        if (!std::isfinite(jet.point.X()) || !std::isfinite(jet.point.Y()) ||
            !std::isfinite(jet.point.Z()) || !finite(jet.du) || !finite(jet.dv) ||
            (second_order && (!finite(duu) || !finite(dvv) || !finite(duv))))
          throw std::runtime_error("Nonfinite native surface derivatives");
        jet.E = jet.du.SquareMagnitude();
        jet.F = jet.du.Dot(jet.dv);
        jet.G = jet.dv.SquareMagnitude();
        jet.normal = jet.du.Crossed(jet.dv);
        jet.determinant = jet.normal.SquareMagnitude();
        if (!std::isfinite(jet.determinant) || jet.E <= 1e-24 || jet.G <= 1e-24 ||
            jet.determinant <= 1e-20 * jet.E * jet.G)
          throw std::runtime_error(
              "Singular surface frame/pole cannot establish boundary continuity");
        jet.normal.Normalize();
        if ((face.Orientation() == TopAbs_REVERSED) != reverse)
          jet.normal.Reverse();
        jet.e = second_order ? duu.Dot(jet.normal) : 0.0;
        jet.f = second_order ? duv.Dot(jet.normal) : 0.0;
        jet.g = second_order ? dvv.Dot(jet.normal) : 0.0;
        if (jet.point.Distance(boundary_point) > tolerance || jet.point.Distance(point) > tolerance)
          throw std::runtime_error(
              "Native trim pcurve exceeds the model-space incidence tolerance");
        jets.push_back(std::move(jet));
      }
  };
  for (int i = 1; i <= trim.distance.NbSolution(); ++i) {
    const auto kind = trim.distance.SupportTypeShape2(i);
    const auto topology = trim.distance.SupportOnShape2(i);
    for (const auto& edge : trim.uses) {
      if (kind == BRepExtrema_IsOnEdge && edge.IsSame(topology)) {
        double parameter;
        trim.distance.ParOnEdgeS2(i, parameter);
        append(edge, parameter, trim.distance.PointOnShape2(i));
      } else if (kind == BRepExtrema_IsVertex) {
        double first, last;
        BRep_Tool::Range(edge, first, last);
        if (TopExp::FirstVertex(edge).IsSame(topology))
          append(edge, first, trim.distance.PointOnShape2(i));
        if (TopExp::LastVertex(edge).IsSame(topology))
          append(edge, last, trim.distance.PointOnShape2(i));
      }
    }
  }
  if (jets.empty())
    throw std::runtime_error("Boundary is not incident to the unique finite trimmed support");
  return jets;
}
void knot_stations(std::vector<double>& parameters, const TopoDS_Edge& boundary,
                   const TrimProjection& trim, double tolerance) {
  double first, last;
  const auto curve = BRep_Tool::Curve(boundary, first, last);
  if (curve.IsNull())
    throw std::runtime_error("Missing native boundary curve");
  for (const auto& edge : trim.uses) {
    double a, b;
    const auto pcurve = BRep_Tool::CurveOnSurface(edge, trim.face, a, b);
    if (pcurve.IsNull())
      throw std::runtime_error("Missing native trim pcurve");
    for (const bool u : {true, false}) {
      const auto& knots = u ? trim.uknots : trim.vknots;
      for (std::size_t k = 1; k + 1 < knots.size(); ++k) {
        const Handle(Geom2d_Line) isoline = new Geom2d_Line(
            gp_Pnt2d(u ? knots[k] : 0, u ? 0 : knots[k]), gp_Dir2d(u ? 0 : 1, u ? 1 : 0));
        Geom2dAPI_InterCurveCurve intersection(pcurve, isoline, 1e-12);
        if (!intersection.Intersector().IsDone())
          throw std::runtime_error("Native trim/spline-knot intersection did not converge");
        for (int i = 1; i <= intersection.NbPoints(); ++i) {
          const double t = intersection.Intersector().Point(i).ParamOnFirst();
          if (t < a || t > b)
            continue;
          const auto uv = intersection.Point(i);
          const auto point = trim.surface->Value(uv.X(), uv.Y());
          GeomAPI_ProjectPointOnCurve projection(point, curve, first, last);
          for (int j = 1; j <= projection.NbPoints(); ++j)
            if (projection.Distance(j) <= tolerance)
              parameters.push_back(projection.Parameter(j));
          if (parameters.size() > 4096)
            throw std::runtime_error("Native continuity knot station budget exceeded");
        }
      }
    }
  }
}
gp_Vec transport(const gp_Vec& value, const gp_Vec& from, const gp_Vec& to) {
  const auto axis = from.Crossed(to);
  const double sine_squared = axis.SquareMagnitude();
  const double cosine = std::clamp(from.Dot(to), -1.0, 1.0);
  if (sine_squared <= 1e-24) {
    if (cosine < 0)
      throw std::runtime_error("Opposed oriented support normals have no common tangent frame");
    return value;
  }
  auto result = value.Multiplied(cosine) + axis.Crossed(value) +
                axis.Multiplied(axis.Dot(value) * (1.0 - cosine) / sine_squared);
  result.Normalize();
  return result;
}
double angle(const gp_Vec& a, const gp_Vec& b) {
  return std::atan2(a.Crossed(b).Magnitude(), a.Dot(b));
}
} // namespace

Result<SurfaceJoinReport> verify_surface_join(FeatureId id, const TopoDS_Edge& boundary,
                                              const TopoDS_Face& candidate,
                                              const TopoDS_Face& support,
                                              LoftBoundaryContinuity continuity,
                                              double position_tolerance, double normal_tolerance,
                                              double curvature_tolerance, bool reverse_support) {
  const auto fail = [&](std::string message) {
    return Result<SurfaceJoinReport>::failure(Error::geometry(id.value(), std::move(message)));
  };
  if (boundary.IsNull() || candidate.IsNull() || support.IsNull() ||
      !std::isfinite(position_tolerance) || position_tolerance <= 0 ||
      !std::isfinite(normal_tolerance) || normal_tolerance <= 0 ||
      !std::isfinite(curvature_tolerance) || curvature_tolerance <= 0 ||
      (continuity != LoftBoundaryContinuity::G1 && continuity != LoftBoundaryContinuity::G2))
    return fail("Invalid surface continuity verification request");
  try {
    BRepAdaptor_Curve curve(boundary);
    const double first = curve.FirstParameter(), last = curve.LastParameter();
    const double integration_tolerance = std::max(1e-12, position_tolerance * .05);
    const double length = GCPnts_AbscissaPoint::Length(curve, first, last, integration_tolerance);
    if (!std::isfinite(length) || length <= position_tolerance)
      return fail("Collapsed Loft boundary/pole cannot establish continuity");
    const bool g2 = continuity == LoftBoundaryContinuity::G2;
    TrimProjection candidate_trim(candidate, position_tolerance);
    TrimProjection support_trim(support, position_tolerance);
    TColStd_Array1OfReal intervals(1, curve.NbIntervals(GeomAbs_C2) + 1);
    curve.Intervals(intervals, GeomAbs_C2);
    std::vector<double> mandatory;
    for (int i = intervals.Lower(); i <= intervals.Upper(); ++i)
      mandatory.push_back(intervals(i));
    knot_stations(mandatory, boundary, candidate_trim, position_tolerance);
    knot_stations(mandatory, boundary, support_trim, position_tolerance);
    SurfaceJoinReport previous, report;
    for (const int count : {33, 65, 129}) {
      report = {};
      std::vector<double> parameters = mandatory;
      for (int i = 0; i < count; ++i) {
        if (i == 0)
          parameters.push_back(first);
        else if (i == count - 1)
          parameters.push_back(last);
        else {
          GCPnts_AbscissaPoint station(integration_tolerance, curve,
                                       length * double(i) / double(count - 1), first);
          if (!station.IsDone())
            return fail("Native arc-length continuity station did not converge");
          parameters.push_back(station.Parameter());
        }
      }
      std::sort(parameters.begin(), parameters.end());
      parameters.erase(std::unique(parameters.begin(), parameters.end()), parameters.end());
      report.stations = parameters.size();
      for (const double parameter : parameters) {
        gp_Pnt point;
        gp_Vec tangent;
        curve.D1(parameter, point, tangent);
        if (!std::isfinite(point.X()) || !std::isfinite(point.Y()) || !std::isfinite(point.Z()) ||
            !finite(tangent) || tangent.SquareMagnitude() <= 1e-24)
          return fail("Singular native boundary tangent");
        tangent.Normalize();
        const auto a = projected_jets(candidate_trim, point, position_tolerance, g2, false);
        const auto b = projected_jets(support_trim, point, position_tolerance, g2, reverse_support);
        // Every admissible projection must agree; selecting the first branch is
        // not proof.
        for (const auto& candidate_jet : a)
          for (const auto& support_jet : b) {
            report.position_error_mm =
                std::max({report.position_error_mm, candidate_jet.point.Distance(point),
                          support_jet.point.Distance(point),
                          candidate_jet.point.Distance(support_jet.point)});
            const double normal_error = angle(candidate_jet.normal, support_jet.normal);
            report.normal_error_rad = std::max(report.normal_error_rad, normal_error);
            if (!g2 || normal_error > normal_tolerance)
              continue;
            // Compare complete signed second fundamental forms in orthonormal frames,
            // identified by the minimal rotation between the two oriented normals.
            auto along =
                tangent - candidate_jet.normal.Multiplied(candidate_jet.normal.Dot(tangent));
            if (along.SquareMagnitude() <= 1e-24)
              return fail("Boundary tangent is incompatible with the native surface frame");
            along.Normalize();
            auto across = candidate_jet.normal.Crossed(along);
            across.Normalize();
            auto diagonal = along + across;
            diagonal.Normalize();
            const auto support_along = transport(along, candidate_jet.normal, support_jet.normal);
            const auto support_across = transport(across, candidate_jet.normal, support_jet.normal);
            auto support_diagonal = support_along + support_across;
            support_diagonal.Normalize();
            const double xx = candidate_jet.curvature(along) - support_jet.curvature(support_along);
            const double yy =
                candidate_jet.curvature(across) - support_jet.curvature(support_across);
            const double xy = candidate_jet.curvature(diagonal) -
                              support_jet.curvature(support_diagonal) - .5 * (xx + yy);
            // Maximum absolute eigenvalue bounds every unit normal-curvature direction.
            const double error = std::abs(.5 * (xx + yy)) + std::hypot(.5 * (xx - yy), xy);
            if (!std::isfinite(error))
              return fail("Nonfinite signed normal-curvature tensor error");
            report.curvature_error_per_mm = std::max(report.curvature_error_per_mm, error);
          }
      }
      if (report.position_error_mm > position_tolerance ||
          report.normal_error_rad > normal_tolerance ||
          (g2 && report.curvature_error_per_mm > curvature_tolerance)) {
        std::ostringstream message;
        message << std::setprecision(10)
                << "Native boundary continuity verification failed: distance="
                << report.position_error_mm << " mm, oriented normal=" << report.normal_error_rad
                << " rad, signed normal curvature=" << report.curvature_error_per_mm << " /mm";
        return fail(message.str());
      }
      if (count == 129 &&
          (std::abs(report.position_error_mm - previous.position_error_mm) >
               .05 * position_tolerance ||
           std::abs(report.normal_error_rad - previous.normal_error_rad) > .05 * normal_tolerance ||
           (g2 && std::abs(report.curvature_error_per_mm - previous.curvature_error_per_mm) >
                      .05 * curvature_tolerance)))
        return fail("Native boundary continuity error did not stabilize under "
                    "station refinement");
      previous = report;
    }
    return Result<SurfaceJoinReport>::success(report);
  } catch (const Standard_Failure& failure) {
    const char* message = failure.GetMessageString();
    return fail(std::string("Native continuity verification failed: ") +
                (message ? message : "OCCT failure"));
  } catch (const std::exception& failure) {
    return fail(failure.what());
  }
}
} // namespace blcad::geometry::detail
