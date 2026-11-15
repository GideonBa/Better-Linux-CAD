#include "constrained_loft_patch.hpp"
#include "native_loft_tensor.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace blcad::geometry::detail {
namespace {
double curve_error(const TopoDS_Edge& edge, const TopoDS_Face& face, double tolerance) {
  BRepAdaptor_Curve curve(edge);
  const double first = curve.FirstParameter(), last = curve.LastParameter();
  const double integration = std::max(1e-12, tolerance * .05);
  const double length = GCPnts_AbscissaPoint::Length(curve, first, last, integration);
  if (!std::isfinite(length) || length <= tolerance)
    throw std::runtime_error("Collapsed Loft section/rail cannot constrain a regular patch");
  double maximum = 0;
  for (int i = 0; i <= 128; ++i) {
    double parameter = i == 128 ? last : first;
    if (i > 0 && i < 128) {
      GCPnts_AbscissaPoint station(integration, curve, length * double(i) / 128, first);
      if (!station.IsDone())
        throw std::runtime_error("Native Loft section/rail station did not converge");
      parameter = station.Parameter();
    }
    const auto point = curve.Value(parameter);
    if (!std::isfinite(point.X()) || !std::isfinite(point.Y()) || !std::isfinite(point.Z()))
      throw std::runtime_error("Nonfinite native Loft section/rail point");
    BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(point).Vertex(), face);
    if (!distance.IsDone() || !std::isfinite(distance.Value()) || distance.Value() > tolerance)
      throw std::runtime_error(
          "Constrained Loft candidate does not satisfy its native section/rail");
    maximum = std::max(maximum, distance.Value());
  }
  return maximum;
}
} // namespace

Result<VerifiedLoftPatch>
build_constrained_loft_patch(FeatureId id, const std::vector<LoftPatchBoundary>& boundaries,
                             const std::vector<TopoDS_Edge>& internal_curves,
                             double linear_tolerance, double angular_tolerance,
                             double curvature_tolerance) {
  const auto fail = [&](std::string message) {
    return Result<VerifiedLoftPatch>::failure(Error::geometry(id.value(), std::move(message)));
  };
  if (id.empty() || boundaries.size() != 4 || boundaries.size() + internal_curves.size() > 24 ||
      !std::isfinite(linear_tolerance) || linear_tolerance < 1e-9 || linear_tolerance > 1 ||
      !std::isfinite(angular_tolerance) || angular_tolerance <= 0 || angular_tolerance > .1 ||
      !std::isfinite(curvature_tolerance) || curvature_tolerance <= 0 || curvature_tolerance > 1)
    return fail("Constrained Loft patch exceeds its curve budget or verification capability");
  try {
    if (std::any_of(boundaries.begin(), boundaries.end(),
                    [](const auto& boundary) { return boundary.curve.IsNull(); }))
      return fail("Constrained Loft native boundary must exist");
    for (std::size_t i = 0; i < boundaries.size(); ++i) {
      const auto& boundary = boundaries[i];
      if (boundary.curve.IsNull() ||
          (boundary.support &&
           (boundary.support->IsNull() || (boundary.continuity != LoftBoundaryContinuity::G1 &&
                                           boundary.continuity != LoftBoundaryContinuity::G2))))
        return fail("Constrained Loft boundary/support is malformed");
      const auto end = TopExp::LastVertex(boundary.curve, true);
      const auto next = TopExp::FirstVertex(boundaries[(i + 1) % boundaries.size()].curve, true);
      if (end.IsNull() || next.IsNull() ||
          BRep_Tool::Pnt(end).Distance(BRep_Tool::Pnt(next)) > linear_tolerance)
        return fail("Constrained Loft patch requires an explicitly ordered closed native boundary");
      for (std::size_t j = 0; j < i; ++j)
        if (boundary.curve.IsSame(boundaries[j].curve))
          return fail("Repeated/seam boundary requires an explicit nonperiodic Loft patch");
      if (boundary.support) {
        const auto incidence = verify_surface_join(
            id, boundary.curve, *boundary.support, *boundary.support, LoftBoundaryContinuity::G1,
            linear_tolerance, angular_tolerance, curvature_tolerance);
        if (!incidence)
          return fail("Loft support at boundary " + std::to_string(i + 1) + ": " +
                      incidence.error().message());
      }
    }
    const auto tensor = native_loft_tensor(
        id, {boundaries[0].curve, boundaries[1].curve, boundaries[2].curve, boundaries[3].curve},
        internal_curves, linear_tolerance);
    if (!tensor)
      return fail(tensor.error().message());
    const auto candidate = tensor.value();
    const auto native = Handle(Geom_BSplineSurface)::DownCast(BRep_Tool::Surface(candidate));
    if (native.IsNull())
      return fail("Verified Loft requires its native tensor surface");
    if ((native->NbUKnots() - 1) * (native->NbVKnots() - 1) > 1024)
      return fail("Verified Loft exceeds its native regularity cell budget");
    // Inspect every polynomial knot cell, including one-sided limits, with a
    // bounded 9x9 derivative grid. This is a stated numerical regularity witness.
    for (int u = 1; u < native->NbUKnots(); ++u)
      for (int v = 1; v < native->NbVKnots(); ++v)
        for (int i = 0; i <= 8; ++i)
          for (int j = 0; j <= 8; ++j) {
            gp_Pnt p;
            gp_Vec du, dv, duu, dvv, duv;
            native->LocalD2(std::lerp(native->UKnot(u), native->UKnot(u + 1), i / 8.0),
                            std::lerp(native->VKnot(v), native->VKnot(v + 1), j / 8.0), u, u + 1, v,
                            v + 1, p, du, dv, duu, dvv, duv);
            const double area = du.Crossed(dv).SquareMagnitude(),
                         scale = du.SquareMagnitude() * dv.SquareMagnitude();
            if (!std::isfinite(area) || !std::isfinite(scale) || scale <= 1e-24 ||
                area <= 1e-20 * scale)
              return fail("Constrained Loft has a singular native interior/knot frame");
          }

    std::string orientation_errors;
    for (const bool reverse : {false, true}) {
      auto oriented = candidate;
      if (reverse)
        oriented.Reverse();
      VerifiedLoftPatch verified{oriented, {}, 0};
      bool valid = true;
      for (std::size_t i = 0; i < boundaries.size(); ++i) {
        const auto& boundary = boundaries[i];
        const auto& support = boundary.support ? *boundary.support : oriented;
        const auto join = verify_surface_join(
            id, boundary.curve, oriented, support,
            boundary.support ? boundary.continuity : LoftBoundaryContinuity::G1, linear_tolerance,
            angular_tolerance, curvature_tolerance, boundary.support && boundary.reverse_normal);
        if (!join) {
          orientation_errors += (reverse ? " Reversed" : "Forward") +
                                std::string(" orientation, boundary ") + std::to_string(i + 1) +
                                ": " + join.error().message() + ".";
          valid = false;
          break;
        }
        verified.curve_error_mm = std::max(verified.curve_error_mm, join.value().position_error_mm);
        if (boundary.support)
          verified.joins.push_back(join.value());
      }
      if (!valid)
        continue;
      for (const auto& curve : internal_curves)
        verified.curve_error_mm =
            std::max(verified.curve_error_mm, curve_error(curve, oriented, linear_tolerance));
      return Result<VerifiedLoftPatch>::success(std::move(verified));
    }
    return fail("Constrained Loft failed native continuity verification: " + orientation_errors);
  } catch (const Standard_Failure& failure) {
    return fail(failure.GetMessageString() ? failure.GetMessageString()
                                           : "Native constrained Loft failed");
  } catch (const std::exception& failure) {
    return fail(failure.what());
  }
}
} // namespace blcad::geometry::detail
