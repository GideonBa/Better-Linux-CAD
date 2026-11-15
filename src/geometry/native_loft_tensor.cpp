#include "native_loft_tensor.hpp"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRep_Tool.hxx>
#include <BSplCLib.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array2OfReal.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace blcad::geometry::detail {
namespace {
class MissingRailStation : public std::runtime_error {
public:
  MissingRailStation() : std::runtime_error("Native Loft section is not incident to its rail") {}
};
Handle(Geom_BSplineCurve) spline(const TopoDS_Edge& edge, bool reverse = false) {
  if (edge.IsNull() ||
      (edge.Orientation() != TopAbs_FORWARD && edge.Orientation() != TopAbs_REVERSED))
    throw std::runtime_error("Native Loft curve must exist");
  double first, last;
  const auto source = BRep_Tool::Curve(edge, first, last);
  if (source.IsNull() || !std::isfinite(first) || !std::isfinite(last) || first >= last)
    throw std::runtime_error("Verified Loft requires bounded native spline-compatible curves");
  Handle(Geom_TrimmedCurve) trimmed =
      new Geom_TrimmedCurve(Handle(Geom_Curve)::DownCast(source->Copy()), first, last);
  if ((edge.Orientation() == TopAbs_REVERSED) != reverse)
    trimmed->Reverse();
  auto result = GeomConvert::CurveToBSplineCurve(trimmed);
  if (result.IsNull() || result->Degree() > 14 || result->NbPoles() > 512)
    throw std::runtime_error("Native Loft curve exceeds its spline budget");
  for (int i = 1; i <= result->NbPoles(); ++i) {
    const auto point = result->Pole(i);
    if (!std::isfinite(point.X()) || !std::isfinite(point.Y()) || !std::isfinite(point.Z()) ||
        !std::isfinite(result->Weight(i)) || result->Weight(i) <= 0)
      throw std::runtime_error("Native Loft requires finite poles and positive finite weights");
  }
  TColStd_Array1OfReal knots(1, result->NbKnots());
  result->Knots(knots);
  BSplCLib::Reparametrize(0, 1, knots);
  result->SetKnots(knots);
  return result;
}
void compatible(std::vector<Handle(Geom_BSplineCurve)>& curves, double tolerance) {
  int degree = 1;
  for (const auto& curve : curves)
    degree = std::max(degree, curve->Degree());
  std::map<double, int> union_knots;
  for (auto& curve : curves) {
    curve->IncreaseDegree(degree);
    TColStd_Array1OfReal canonical(1, curve->NbKnots());
    double delta = 0, minimum_span = 1;
    for (int i = 1; i <= curve->NbKnots(); ++i) {
      const double knot = curve->Knot(i);
      const auto equivalent = [&](double value) {
        return std::abs(value - knot) <= 64 * std::numeric_limits<double>::epsilon() *
                                             std::max({1.0, std::abs(value), std::abs(knot)});
      };
      auto match = union_knots.lower_bound(knot);
      if (match == union_knots.end() || !equivalent(match->first)) {
        if (match != union_knots.begin() && equivalent(std::prev(match)->first))
          --match;
        else
          match = union_knots.emplace(knot, 0).first;
      }
      canonical(i) = match->first;
      match->second = std::max(match->second, curve->Multiplicity(i));
      delta = std::max(delta, std::abs(canonical(i) - knot));
      if (i > 1) {
        minimum_span =
            std::min({minimum_span, knot - curve->Knot(i - 1), canonical(i) - canonical(i - 1)});
        if (canonical(i) <= canonical(i - 1))
          throw std::runtime_error(
              "Distinct knots cannot collapse during native basis normalization");
      }
    }
    // With corresponding spans, each de Boor interpolation coefficient changes
    // by at most 4*delta/minimum_span. Accumulate over degree levels in centered
    // homogeneous coordinates, then divide by the positive minimum weight.
    double radius = 0, minimum_weight = curve->Weight(1), maximum_weight = minimum_weight;
    for (int i = 1; i <= curve->NbPoles(); ++i) {
      radius = std::max(radius, curve->Pole(i).Distance(curve->Pole(1)));
      minimum_weight = std::min(minimum_weight, curve->Weight(i));
      maximum_weight = std::max(maximum_weight, curve->Weight(i));
    }
    const double error =
        4 * degree * delta / minimum_span * radius * (3 * maximum_weight / minimum_weight - 1);
    if (!std::isfinite(error) || error > .05 * tolerance)
      throw std::runtime_error("Native knot normalization exceeds its geometric error budget");
    curve->SetKnots(canonical);
  }
  if (union_knots.size() > 256)
    throw std::runtime_error("Native Loft common spline basis exceeds its knot budget");
  TColStd_Array1OfReal knots(1, int(union_knots.size()));
  TColStd_Array1OfInteger multiplicities(1, int(union_knots.size()));
  int i = 1;
  double previous = -1;
  for (const auto& [knot, multiplicity] : union_knots) {
    if (previous >= 0 && knot - previous < 1e-12)
      throw std::runtime_error("Native Loft common basis has indistinguishable distinct knots");
    knots.SetValue(i, knot);
    multiplicities.SetValue(i, multiplicity);
    ++i;
    previous = knot;
  }
  for (auto& curve : curves)
    curve->InsertKnots(knots, multiplicities, 0, false);
  if (curves.front()->NbPoles() > 512)
    throw std::runtime_error("Native Loft common basis exceeds its pole budget");
  for (const auto& curve : curves)
    if (curve->NbPoles() != curves.front()->NbPoles())
      throw std::runtime_error("Native Loft common basis is inconsistent");
}
double station(const Handle(Geom_BSplineCurve) & rail, gp_Pnt point, double tolerance) {
  std::vector<double> matches;
  for (const auto u : {0.0, 1.0})
    if (rail->Value(u).Distance(point) <= tolerance)
      matches.push_back(u);
  GeomAPI_ProjectPointOnCurve projection(point, rail, 0, 1);
  for (int i = 1; i <= projection.NbPoints(); ++i)
    if (projection.Distance(i) <= tolerance) {
      const double u = projection.Parameter(i);
      if (std::none_of(matches.begin(), matches.end(),
                       [u](double v) { return std::abs(u - v) <= 1e-12; }))
        matches.push_back(u);
    }
  if (matches.empty())
    throw MissingRailStation();
  if (matches.size() != 1)
    throw std::runtime_error("Native Loft section has an ambiguous rail station");
  return matches.front();
}
double binomial(int n, int k) {
  if (k < 0 || k > n)
    return 0;
  double result = 1;
  for (int i = 1; i <= k; ++i)
    result *= double(n + 1 - i) / i;
  return result;
}
double bernstein(int n, int i, double t) {
  if (i < 0 || i > n)
    return 0;
  return binomial(n, i) * std::pow(t, i) * std::pow(1 - t, n - i);
}
double basis(int degree, int i, double t, int derivative) {
  if (derivative == 0)
    return bernstein(degree, i, t);
  if (derivative == 1)
    return degree * (bernstein(degree - 1, i - 1, t) - bernstein(degree - 1, i, t));
  return degree * (degree - 1) *
         (bernstein(degree - 2, i - 2, t) - 2 * bernstein(degree - 2, i - 1, t) +
          bernstein(degree - 2, i, t));
}
std::vector<std::vector<double>> inverse(std::vector<std::vector<double>> matrix) {
  const auto n = matrix.size();
  std::vector<std::vector<double>> result(n, std::vector<double>(n));
  for (std::size_t i = 0; i < n; ++i)
    result[i][i] = 1;
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t pivot = i;
    for (std::size_t j = i + 1; j < n; ++j)
      if (std::abs(matrix[j][i]) > std::abs(matrix[pivot][i]))
        pivot = j;
    if (std::abs(matrix[pivot][i]) < 1e-11)
      throw std::runtime_error("Native Loft Hermite stations are ill-conditioned");
    std::swap(matrix[i], matrix[pivot]);
    std::swap(result[i], result[pivot]);
    const double scale = matrix[i][i];
    for (std::size_t j = 0; j < n; ++j) {
      matrix[i][j] /= scale;
      result[i][j] /= scale;
    }
    for (std::size_t row = 0; row < n; ++row)
      if (row != i) {
        const double factor = matrix[row][i];
        for (std::size_t j = 0; j < n; ++j) {
          matrix[row][j] -= factor * matrix[i][j];
          result[row][j] -= factor * result[i][j];
        }
      }
  }
  return result;
}
} // namespace
Result<TopoDS_Face> native_loft_tensor(FeatureId id, const std::array<TopoDS_Edge, 4>& bounds,
                                       const std::vector<TopoDS_Edge>& sections, double tolerance) {
  const auto fail = [&](std::string message) {
    return Result<TopoDS_Face>::failure(Error::geometry(id.value(), std::move(message)));
  };
  if (id.empty() || !std::isfinite(tolerance) || tolerance < 1e-9 || tolerance > 1)
    return fail("Native Loft tensor requires identity and a bounded modeling tolerance");
  if (sections.size() > 9)
    return fail("Verified Loft supports at most eleven section stations per patch");
  try {
    const auto left = spline(bounds[3], true), right = spline(bounds[1]);
    std::vector<Handle(Geom_BSplineCurve)> profiles{spline(bounds[0]), spline(bounds[2], true)};
    std::vector<double> stations;
    for (const auto& edge : sections) {
      auto curve = spline(edge);
      double first_station;
      try {
        first_station = station(left, curve->Value(0), tolerance);
      } catch (const MissingRailStation&) {
        curve->Reverse();
        first_station = station(left, curve->Value(0), tolerance);
      }
      const double second_station = station(right, curve->Value(1), tolerance);
      if (std::abs(first_station - second_station) > 1e-8 || first_station <= 1e-4 ||
          first_station >= 1 - 1e-4)
        return fail("Verified Loft requires compatible interior section parameters on both rails");
      if (!stations.empty() && first_station - stations.back() < 1e-4)
        return fail("Verified Loft interior section stations must increase distinctly");
      stations.push_back(first_station);
      profiles.push_back(curve);
    }
    compatible(profiles, tolerance);
    gp_Pnt p;
    gp_Vec first_left, first_right, second_left, second_right;
    left->D2(0, p, first_left, second_left);
    right->D2(0, p, first_right, second_right);
    if ((first_left - first_right).Magnitude() > tolerance ||
        (second_left - second_right).Magnitude() > tolerance)
      return fail("Verified Loft requires compatible constant root derivative fields");
    const auto start_d1 = first_left, start_d2 = second_left;
    left->D2(1, p, first_left, second_left);
    right->D2(1, p, first_right, second_right);
    if ((first_left - first_right).Magnitude() > tolerance ||
        (second_left - second_right).Magnitude() > tolerance)
      return fail("Verified Loft requires compatible constant tip derivative fields");
    const auto end_d1 = first_left, end_d2 = second_left;
    const int count = 6 + int(sections.size()), degree_v = count - 1;
    std::vector<std::pair<double, int>> conditions{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}};
    for (const auto value : stations)
      conditions.push_back({value, 0});
    std::vector<std::vector<double>> system(count, std::vector<double>(count));
    for (int row = 0; row < count; ++row)
      for (int col = 0; col < count; ++col)
        system[row][col] = basis(degree_v, col, conditions[row].first, conditions[row].second);
    const auto transform = inverse(system);
    const int poles_u = profiles.front()->NbPoles();
    TColgp_Array2OfPnt poles(1, poles_u, 1, count);
    TColStd_Array2OfReal weights(1, poles_u, 1, count);
    for (int i = 1; i <= poles_u; ++i) {
      const double weight = profiles.front()->Weight(i) / profiles.front()->Weight(1);
      if (!std::isfinite(weight) || weight <= 0)
        return fail("Native Loft normalized weight function is outside numerical capability");
      for (const auto& curve : profiles)
        if (std::abs(curve->Weight(i) / curve->Weight(1) - weight) > 1e-12)
          return fail("Verified Loft requires matching section weight functions");
      std::vector<gp_XYZ> values{profiles[0]->Pole(i).XYZ(), start_d1.XYZ(), start_d2.XYZ(),
                                 profiles[1]->Pole(i).XYZ(), end_d1.XYZ(),   end_d2.XYZ()};
      for (std::size_t k = 2; k < profiles.size(); ++k)
        values.push_back(profiles[k]->Pole(i).XYZ());
      for (int j = 0; j < count; ++j) {
        gp_XYZ value(0, 0, 0);
        for (int k = 0; k < count; ++k)
          value += values[k] * transform[j][k];
        if (!std::isfinite(value.X()) || !std::isfinite(value.Y()) || !std::isfinite(value.Z()))
          return fail("Native Loft tensor produced nonfinite control data");
        poles.SetValue(i, j + 1, gp_Pnt(value));
        weights.SetValue(i, j + 1, weight);
      }
    }
    TColStd_Array1OfReal u_knots(1, profiles.front()->NbKnots()), v_knots(1, 2);
    TColStd_Array1OfInteger u_mult(1, u_knots.Length()), v_mult(1, 2);
    profiles.front()->Knots(u_knots);
    profiles.front()->Multiplicities(u_mult);
    v_knots.SetValue(1, 0);
    v_knots.SetValue(2, 1);
    v_mult.Init(count);
    const Handle(Geom_BSplineSurface) surface =
        new Geom_BSplineSurface(poles, weights, u_knots, v_knots, u_mult, v_mult,
                                profiles.front()->Degree(), degree_v, false, false);
    // Exact rail correction in a shared V basis. The U blend is the rational
    // partition with Greville coefficients, so it retains the common weight
    // function and has endpoint values 0/1. Corrections vanish at every authored
    // section and through second order at the two terminal stations.
    if (left->IsRational() || right->IsRational())
      return fail("Verified Loft rails currently require polynomial native B-splines");
    std::vector<Handle(Geom_BSplineCurve)> rails{GeomConvert::CurveToBSplineCurve(surface->UIso(0)),
                                                 GeomConvert::CurveToBSplineCurve(surface->UIso(1)),
                                                 left, right};
    compatible(rails, tolerance);
    surface->IncreaseDegree(surface->UDegree(), rails.front()->Degree());
    TColStd_Array1OfReal rail_knots(1, rails.front()->NbKnots());
    TColStd_Array1OfInteger rail_mult(1, rail_knots.Length());
    rails.front()->Knots(rail_knots);
    rails.front()->Multiplicities(rail_mult);
    surface->InsertVKnots(rail_knots, rail_mult, 0, false);
    for (int k = 2; k < surface->NbVKnots(); ++k)
      if (surface->VDegree() - surface->VMultiplicity(k) < 2)
        return fail("Verified Loft currently requires C2 polynomial rail knot continuity");
    if (surface->NbVPoles() != rails.front()->NbPoles())
      return fail("Native Loft rail correction basis is inconsistent");
    TColStd_Array1OfReal sequence(1, surface->NbUPoles() + surface->UDegree() + 1);
    surface->UKnotSequence(sequence);
    for (int i = 1; i <= surface->NbUPoles(); ++i) {
      double blend = 0;
      for (int k = 1; k <= surface->UDegree(); ++k)
        blend += sequence(i + k);
      blend /= surface->UDegree();
      for (int j = 1; j <= surface->NbVPoles(); ++j) {
        const auto delta_left = rails[2]->Pole(j).XYZ() - rails[0]->Pole(j).XYZ();
        const auto delta_right = rails[3]->Pole(j).XYZ() - rails[1]->Pole(j).XYZ();
        surface->SetPole(
            i, j,
            gp_Pnt(surface->Pole(i, j).XYZ() + delta_left * (1 - blend) + delta_right * blend));
      }
    }
    BRepBuilderAPI_MakeFace made(surface, 0, 1, 0, 1, tolerance * .01);
    if (!made.IsDone() || !BRepCheck_Analyzer(made.Face(), true).IsValid())
      return fail("Native Hermite Loft face is invalid");
    return Result<TopoDS_Face>::success(made.Face());
  } catch (const Standard_Failure& error) {
    return fail(error.GetMessageString() ? error.GetMessageString() : "Native Loft tensor failed");
  } catch (const std::exception& error) {
    return fail(error.what());
  }
}
} // namespace blcad::geometry::detail
