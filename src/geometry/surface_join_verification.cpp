#include "surface_join_verification.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
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
std::vector<Jet> projected_jets(const TopoDS_Face& face, const gp_Pnt& point, double tolerance,
                                bool second_order, bool reverse) {
  double u0, u1, v0, v1;
  BRepTools::UVBounds(face, u0, u1, v0, v1);
  if (face.Orientation() != TopAbs_FORWARD && face.Orientation() != TopAbs_REVERSED)
    throw std::runtime_error("Surface continuity requires an oriented manifold face");
  const auto surface = BRep_Tool::Surface(face);
  if (surface.IsNull() || !std::isfinite(u0) || !std::isfinite(u1) || !std::isfinite(v0) ||
      !std::isfinite(v1))
    throw std::runtime_error("Surface continuity requires a finite native support domain");
  GeomAPI_ProjectPointOnSurf projection(point, surface, u0, u1, v0, v1,
                                        std::max(1e-12, tolerance * .05));
  std::vector<Jet> jets;
  for (int i = 1; i <= projection.NbPoints(); ++i) {
    if (projection.Distance(i) > tolerance)
      continue;
    double u, v;
    projection.Parameters(i, u, v);
    BRepClass_FaceClassifier domain(face, gp_Pnt2d(u, v), tolerance);
    // A boundary join must lie on the actual trim, not merely the infinite
    // carrier.
    if (domain.State() != TopAbs_ON)
      continue;
    if (!jets.empty()) {
      const auto equivalent = [](double delta, bool periodic, double period) {
        constexpr double rounding = 64 * std::numeric_limits<double>::epsilon();
        return delta == 0.0 || (periodic && std::abs(delta) <= period * (1.0 + rounding) &&
                                std::abs(std::remainder(delta, period)) <= rounding * period);
      };
      if (!equivalent(u - jets.front().u, surface->IsUPeriodic(),
                      surface->IsUPeriodic() ? surface->UPeriod() : 1.0) ||
          !equivalent(v - jets.front().v, surface->IsVPeriodic(),
                      surface->IsVPeriodic() ? surface->VPeriod() : 1.0))
        throw std::runtime_error("Ambiguous native projection onto the trimmed support");
    }
    Jet jet{};
    jet.u = u;
    jet.v = v;
    gp_Vec duu, dvv, duv;
    if (second_order)
      surface->D2(u, v, jet.point, jet.du, jet.dv, duu, dvv, duv);
    else
      surface->D1(u, v, jet.point, jet.du, jet.dv);
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
      throw std::runtime_error("Singular surface frame/pole cannot establish boundary continuity");
    jet.normal.Normalize();
    if ((face.Orientation() == TopAbs_REVERSED) != reverse)
      jet.normal.Reverse();
    jet.e = second_order ? duu.Dot(jet.normal) : 0.0;
    jet.f = second_order ? duv.Dot(jet.normal) : 0.0;
    jet.g = second_order ? dvv.Dot(jet.normal) : 0.0;
    jets.push_back(std::move(jet));
  }
  if (jets.empty())
    throw std::runtime_error("Boundary is not incident to the unique finite trimmed support");
  return jets;
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
    SurfaceJoinReport previous, report;
    for (const int count : {33, 65, 129}) {
      report = {};
      report.stations = std::size_t(count);
      for (int i = 0; i < count; ++i) {
        double parameter = i == count - 1 ? last : first;
        if (i > 0 && i < count - 1) {
          GCPnts_AbscissaPoint station(integration_tolerance, curve,
                                       length * double(i) / double(count - 1), first);
          if (!station.IsDone())
            return fail("Native arc-length continuity station did not converge");
          parameter = station.Parameter();
        }
        gp_Pnt point;
        gp_Vec tangent;
        curve.D1(parameter, point, tangent);
        if (!std::isfinite(point.X()) || !std::isfinite(point.Y()) || !std::isfinite(point.Z()) ||
            !finite(tangent) || tangent.SquareMagnitude() <= 1e-24)
          return fail("Singular native boundary tangent");
        tangent.Normalize();
        const auto a = projected_jets(candidate, point, position_tolerance, g2, false);
        const auto b = projected_jets(support, point, position_tolerance, g2, reverse_support);
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
