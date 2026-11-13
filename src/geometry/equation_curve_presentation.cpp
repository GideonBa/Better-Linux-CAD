#include "blcad/geometry/equation_curve_presentation.hpp"
#include "derived_curve_internal.hpp"
#include <GeomAdaptor_Curve.hxx>
#include <GeomConvert_BSplineCurveToBezierCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <numbers>
#include <stdexcept>

namespace blcad::geometry {
namespace {
Point3 point(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}
Point3 mix(Point3 a, Point3 b, double t) {
  return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
}
using Controls = std::array<Point3, 4>;
Controls controls(const Handle(Geom_BezierCurve) & curve) {
  if (curve.IsNull() || curve->Degree() != 3 || curve->IsRational())
    throw std::runtime_error("Equation presentation requires its supported cubic Geometry");
  Controls p;
  for (int i = 1; i <= 4; ++i)
    p[static_cast<std::size_t>(i - 1)] = point(curve->Pole(i));
  return p;
}
std::pair<Controls, Controls> split(const Controls& p) {
  const auto a = mix(p[0], p[1], .5), b = mix(p[1], p[2], .5), c = mix(p[2], p[3], .5);
  const auto d = mix(a, b, .5), e = mix(b, c, .5), f = mix(d, e, .5);
  return {{p[0], a, d, f}, {f, e, c, p[3]}};
}
} // namespace
Result<std::vector<Point3>>
sample_equation_curve_for_presentation(const EquationCurveGeometry& source, double deflection_mm,
                                       std::size_t maximum_points) {
  const auto fail = [&](std::string message) {
    return Result<std::vector<Point3>>::failure(
        Error::geometry(source.source.value(), std::move(message)));
  };
  if (!std::isfinite(deflection_mm) || deflection_mm <= 0 || maximum_points < 2 ||
      maximum_points > 32769)
    return fail("Equation presentation requires positive deflection and 2 to 32769 points");
  if (!source.curve || source.curve->curve.IsNull())
    return fail("Equation presentation requires resolved Geometry");
  try {
    GeomAdaptor_Curve curve(source.curve->curve);
    std::vector<Point3> points{point(curve.Value(curve.FirstParameter()))};
    const auto append = [&](Point3 p) {
      if (points.size() >= maximum_points)
        throw std::runtime_error("Equation presentation point budget exceeded");
      points.push_back(p);
    };
    if (curve.GetType() == GeomAbs_Line)
      append(point(curve.Value(curve.LastParameter())));
    else if (curve.GetType() == GeomAbs_Circle) {
      const double step =
          4 * std::asin(std::sqrt(std::min(1., deflection_mm / (2 * curve.Circle().Radius()))));
      const double count =
          std::max(source.curve->closure_proven ? 4. : 1.,
                   std::ceil((curve.LastParameter() - curve.FirstParameter()) / step));
      if (!std::isfinite(count) || count > static_cast<double>(maximum_points - 1))
        return fail("Equation presentation point budget exceeded");
      const auto n = static_cast<std::size_t>(count);
      for (std::size_t i = 1; i <= n; ++i)
        append(point(curve.Value(
            std::lerp(curve.FirstParameter(), curve.LastParameter(), static_cast<double>(i) / n))));
    } else {
      std::vector<Controls> arcs;
      if (curve.GetType() == GeomAbs_BezierCurve)
        arcs.push_back(controls(curve.Bezier()));
      else if (curve.GetType() == GeomAbs_BSplineCurve) {
        GeomConvert_BSplineCurveToBezierCurve converted(curve.BSpline());
        if (static_cast<std::size_t>(converted.NbArcs()) >= maximum_points)
          return fail("Equation presentation point budget exceeded");
        for (int i = 1; i <= converted.NbArcs(); ++i)
          arcs.push_back(controls(converted.Arc(i)));
      } else
        return fail("Unsupported equation presentation Geometry");
      struct Piece {
        Controls p;
        std::size_t depth;
      };
      for (const auto& arc : arcs) {
        std::vector<Piece> pending{{arc, 0}};
        while (!pending.empty()) {
          const auto piece = pending.back();
          pending.pop_back();
          const auto& p = piece.p;
          const double error = std::max(distance(p[1], mix(p[0], p[3], 1. / 3)),
                                        distance(p[2], mix(p[0], p[3], 2. / 3)));
          if (std::isfinite(error) && error <= deflection_mm) {
            append(p[3]);
            continue;
          }
          if (piece.depth >= 24 || points.size() + pending.size() + 2 > maximum_points)
            return fail("Equation presentation deflection could not be resolved within its budget");
          const auto [left, right] = split(p);
          pending.push_back({right, piece.depth + 1});
          pending.push_back({left, piece.depth + 1});
        }
      }
    }
    if (source.curve->closure_proven)
      points.back() = points.front();
    return Result<std::vector<Point3>>::success(std::move(points));
  } catch (const Standard_Failure& failure) {
    return fail(failure.GetMessageString() ? failure.GetMessageString()
                                           : "Equation presentation failed");
  } catch (const std::exception& failure) {
    return fail(failure.what());
  }
}
} // namespace blcad::geometry
