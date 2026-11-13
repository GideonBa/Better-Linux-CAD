#include "blcad/geometry/equation_curve_geometry.hpp"
#include "blcad/core/part_document.hpp"
#include "derived_curve_internal.hpp"
#include "geometry_shape_internal.hpp"
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Line.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Ax2.hxx>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <vector>

namespace blcad::geometry {
namespace {
template <class T> T checked(Result<T> result) {
  if (!result)
    throw result.error();
  return std::move(result.value());
}
gp_Pnt point(Point3 p) {
  return {p.x, p.y, p.z};
}
Point3 point(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}
double norm(Point3 p) {
  return std::hypot(p.x, p.y, p.z);
}
Point3 offset(Point3 p, Point3 direction, double scale) {
  return {p.x + direction.x * scale, p.y + direction.y * scale, p.z + direction.z * scale};
}
Point3 interpolate(Point3 a, Point3 b, double t) {
  return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
}
double resolution(std::initializer_list<Point3> points) {
  double magnitude = 1;
  for (const auto& p : points)
    magnitude = std::max({magnitude, std::abs(p.x), std::abs(p.y), std::abs(p.z)});
  return 128 * std::numeric_limits<double>::epsilon() * magnitude;
}
double upward(long double value) {
  if (value < 0 || !std::isfinite(value))
    return std::numeric_limits<double>::infinity();
  auto result = static_cast<double>(value);
  if (static_cast<long double>(result) < value)
    result = std::nextafter(result, std::numeric_limits<double>::infinity());
  return result;
}
struct PolynomialRange {
  long double lower{}, upper{};
};
PolynomialRange range(CurveInterval value) {
  return {value.lower, value.upper};
}
PolynomialRange add_range(PolynomialRange a, PolynomialRange b) {
  return {a.lower + b.lower, a.upper + b.upper};
}
PolynomialRange scale_range(PolynomialRange a, long double scale) {
  return scale >= 0 ? PolynomialRange{a.lower * scale, a.upper * scale}
                    : PolynomialRange{a.upper * scale, a.lower * scale};
}
PolynomialRange polynomial_range(const CurveAnalyticForm& form, long double u,
                                 bool derivative = false) {
  PolynomialRange value;
  for (int i = 3; i >= 0; --i) {
    if (derivative && i == 0)
      continue;
    value = add_range(scale_range(value, u),
                      scale_range(range(form.polynomial_bounds[static_cast<std::size_t>(i)]),
                                  derivative ? i : 1));
  }
  return value;
}
long double gap(PolynomialRange range, double value) {
  return std::max(std::abs(range.lower - value), std::abs(range.upper - value));
}
double polynomial_uniform_error(const std::array<std::optional<CurveAnalyticForm>, 3>& forms,
                                double lower, double upper, const std::array<Point3, 4>& controls) {
  std::array<std::array<long double, 3>, 4> errors{};
  for (std::size_t axis = 0; axis < 3; ++axis) {
    const auto& form = *forms[axis];
    const auto start = polynomial_range(form, lower), end = polynomial_range(form, upper);
    const long double span = static_cast<long double>(upper) - lower;
    const std::array<PolynomialRange, 4> exact{
        start, add_range(start, scale_range(polynomial_range(form, lower, true), span / 3)),
        add_range(end, scale_range(polynomial_range(form, upper, true), -span / 3)), end};
    for (std::size_t i = 0; i < 4; ++i) {
      const std::array<double, 3> value{controls[i].x, controls[i].y, controls[i].z};
      errors[i][axis] = gap(exact[i], value[axis]);
    }
  }
  long double maximum = 0;
  for (const auto& error : errors)
    maximum = std::max(maximum, std::hypot(error[0], error[1], error[2]));
  return upward(maximum + resolution({controls[0], controls[1], controls[2], controls[3]}));
}
double circle_uniform_error(const std::array<std::optional<CurveAnalyticForm>, 3>& forms,
                            const Handle(Geom_Circle) & circle, double lower, double upper,
                            double sweep) {
  const double frequency = sweep / (upper - lower), phase = frequency * lower,
               radius = circle->Radius();
  const auto& frame = circle->Position();
  const auto x = frame.XDirection(), y = frame.YDirection();
  const auto location = frame.Location();
  const std::array<double, 3> center{location.X(), location.Y(), location.Z()};
  const std::array<double, 3> first{x.X(), x.Y(), x.Z()}, second{y.X(), y.Y(), y.Z()};
  const long double maximum_u = std::max(std::abs(static_cast<long double>(lower)),
                                         std::abs(static_cast<long double>(upper)));
  std::array<long double, 3> errors{};
  for (std::size_t axis = 0; axis < 3; ++axis) {
    const auto& source = *forms[axis];
    const double cosine = radius * (first[axis] * std::cos(phase) - second[axis] * std::sin(phase));
    const double sine = radius * (first[axis] * std::sin(phase) + second[axis] * std::cos(phase));
    auto error = gap(range(source.polynomial_bounds[0]), center[axis]) +
                 gap(range(source.cosine_bounds), cosine) + gap(range(source.sine_bounds), sine);
    long double power = maximum_u;
    for (std::size_t i = 1; i < 4; ++i) {
      error += gap(range(source.polynomial_bounds[i]), 0) * power;
      power *= maximum_u;
    }
    error +=
        source.frequency_sensitivity * gap(range(source.frequency_bounds), frequency) * maximum_u;
    errors[axis] = error;
  }
  const long double numerical = resolution({{radius, 0, 0}, {center[0], center[1], center[2]}}) *
                                (1 + std::abs(static_cast<long double>(phase)));
  return upward(std::hypot(errors[0], errors[1], errors[2]) + numerical);
}

bool regular_cubic(const std::array<Point3, 4>& p) {
  std::array<Point3, 3> derivative;
  for (std::size_t i = 0; i < 3; ++i)
    derivative[i] = {p[i + 1].x - p[i].x, p[i + 1].y - p[i].y, p[i + 1].z - p[i].z};
  double lower_norm = 0;
  for (std::size_t axis = 0; axis < 3; ++axis) {
    std::array<double, 3> values;
    for (std::size_t i = 0; i < 3; ++i)
      values[i] = axis == 0 ? derivative[i].x : axis == 1 ? derivative[i].y : derivative[i].z;
    const auto [first, last] = std::minmax_element(values.begin(), values.end());
    const double minimum =
        *first <= 0 && *last >= 0 ? 0 : std::min(std::abs(*first), std::abs(*last));
    lower_norm = std::hypot(lower_norm, minimum);
  }
  return lower_norm > 0;
}
struct NativeModel {
  Handle(Geom_Curve) curve;
  EquationCurveGeometryKind kind;
  double second_normalized{};
  double lower{}, upper{};
  double uniform_error{std::numeric_limits<double>::infinity()};
  bool regular(double a, double b) const {
    const auto bezier = Handle(Geom_BezierCurve)::DownCast(curve);
    if (bezier.IsNull())
      return true; // Native circle or straight line.
    auto piece = Handle(Geom_BezierCurve)::DownCast(bezier->Copy());
    piece->Segment((a - lower) / (upper - lower), (b - lower) / (upper - lower));
    std::array<Point3, 4> p;
    for (int i = 1; i <= 4; ++i)
      p[static_cast<std::size_t>(i - 1)] = point(piece->Pole(i));
    return regular_cubic(p);
  }
  Point3 evaluate(double u) const {
    double t = u == lower ? 0 : u == upper ? 1 : (u - lower) / (upper - lower);
    const double parameter = std::lerp(curve->FirstParameter(), curve->LastParameter(), t);
    return point(curve->Value(parameter));
  }
};
std::array<std::optional<CurveAnalyticForm>, 3> analytic_forms(const PartDocument& document,
                                                               const EquationCurve& curve) {
  std::array<std::optional<CurveAnalyticForm>, 3> result;
  if (curve.mode() == EquationCurveMode::Polar2D)
    return result;
  for (std::size_t i = 0; i < 3; ++i) {
    if (curve.mode() == EquationCurveMode::Cartesian2D && i == 0) {
      CurveAnalyticForm form;
      form.polynomial[1] = 1;
      form.polynomial_bounds[1] = {1, 1};
      result[i] = form;
    } else if (curve.coordinates()[i].empty())
      result[i] = CurveAnalyticForm{};
    else
      result[i] = checked(checked(CurveExpression::compile(document, curve.coordinates()[i],
                                                           curve.variable(), curve.id().value()))
                              .analytic_form(document));
  }
  return result;
}
std::optional<NativeModel> native_model(const PartDocument& document, const EquationCurve& curve,
                                        double lower, double upper, const EquationCurveJet& first,
                                        const EquationCurveJet& last, bool closed) {
  auto forms = analytic_forms(document, curve);
  if (curve.mode() == EquationCurveMode::Polar2D) {
    const auto r = checked(checked(CurveExpression::compile(document, curve.coordinates()[0],
                                                            curve.variable(), curve.id().value()))
                               .analytic_form(document));
    if (r && r->polynomial[1] == 0 && r->polynomial[2] == 0 && r->polynomial[3] == 0 &&
        r->cosine_bounds.lower == 0 && r->cosine_bounds.upper == 0 && r->sine_bounds.lower == 0 &&
        r->sine_bounds.upper == 0 && r->polynomial_bounds[1].lower == 0 &&
        r->polynomial_bounds[1].upper == 0 && r->polynomial_bounds[2].lower == 0 &&
        r->polynomial_bounds[2].upper == 0 && r->polynomial_bounds[3].lower == 0 &&
        r->polynomial_bounds[3].upper == 0) {
      forms = {CurveAnalyticForm{}, CurveAnalyticForm{}, CurveAnalyticForm{}};
      forms[0]->frequency = 1;
      forms[0]->frequency_bounds = {1, 1};
      forms[0]->cosine = r->polynomial[0];
      forms[0]->cosine_bounds = r->polynomial_bounds[0];
      forms[0]->frequency_sensitivity = std::max(std::abs(r->polynomial_bounds[0].lower),
                                                 std::abs(r->polynomial_bounds[0].upper));
      forms[1]->frequency = 1;
      forms[1]->frequency_bounds = {1, 1};
      forms[1]->sine = r->polynomial[0];
      forms[1]->sine_bounds = r->polynomial_bounds[0];
      forms[1]->frequency_sensitivity = forms[0]->frequency_sensitivity;
    }
  }
  if (std::any_of(forms.begin(), forms.end(), [](const auto& f) { return !f; }))
    return std::nullopt;
  if (std::all_of(forms.begin(), forms.end(), [](const auto& f) {
        return f->cosine_bounds.lower == 0 && f->cosine_bounds.upper == 0 &&
               f->sine_bounds.lower == 0 && f->sine_bounds.upper == 0;
      })) {
    const double span = upper - lower;
    const auto modeled_last = closed ? first.value : last.value;
    const bool linear = std::all_of(forms.begin(), forms.end(), [](const auto& form) {
      return form->polynomial_bounds[2].lower == 0 && form->polynomial_bounds[2].upper == 0 &&
             form->polynomial_bounds[3].lower == 0 && form->polynomial_bounds[3].upper == 0;
    });
    if (linear) {
      const auto direction = displacement(first.value, modeled_last);
      const double extent = std::hypot(direction.x, direction.y, direction.z);
      if (extent <= Precision::Confusion())
        throw Error::geometry(curve.id().value(), "Equation line is below kernel resolution");
      Handle(Geom_Curve) line = new Geom_TrimmedCurve(
          new Geom_Line(point(first.value), gp_Dir(direction.x, direction.y, direction.z)), 0,
          extent);
      return NativeModel{
          line,
          EquationCurveGeometryKind::Polynomial,
          0,
          lower,
          upper,
          polynomial_uniform_error(forms, lower, upper,
                                   {first.value, interpolate(first.value, modeled_last, 1. / 3),
                                    interpolate(first.value, modeled_last, 2. / 3), modeled_last})};
    }
    const auto c1 = offset(first.value, first.first, span / 3),
               c2 = offset(modeled_last, last.first, -span / 3);
    TColgp_Array1OfPnt poles(1, 4);
    poles.SetValue(1, point(first.value));
    poles.SetValue(2, point(c1));
    poles.SetValue(3, point(c2));
    poles.SetValue(4, point(modeled_last));
    Handle(Geom_BezierCurve) bezier = new Geom_BezierCurve(poles);
    gp_Pnt p;
    gp_Vec d1, d2;
    bezier->D2(0, p, d1, d2);
    const double left = d2.Magnitude();
    bezier->D2(1, p, d1, d2);
    const double right = d2.Magnitude();
    return NativeModel{
        bezier,
        EquationCurveGeometryKind::Polynomial,
        std::max(left, right),
        lower,
        upper,
        polynomial_uniform_error(forms, lower, upper, {first.value, c1, c2, modeled_last})};
  }
  double frequency = 0;
  for (const auto& form : forms) {
    if (form->polynomial[1] != 0 || form->polynomial[2] != 0 || form->polynomial[3] != 0)
      return std::nullopt;
    if (form->cosine != 0 || form->sine != 0) {
      if (frequency != 0 && frequency != form->frequency)
        return std::nullopt;
      frequency = form->frequency;
    }
  }
  const Vector3 a{forms[0]->cosine, forms[1]->cosine, forms[2]->cosine};
  const Vector3 b{forms[0]->sine, forms[1]->sine, forms[2]->sine};
  const double radius = std::hypot(a.x, a.y, a.z), second_radius = std::hypot(b.x, b.y, b.z);
  if (radius == 0 || !std::isfinite(radius) || radius != second_radius || frequency <= 0 ||
      dot(a, b) != 0)
    return std::nullopt;
  const double requested_sweep = frequency * (upper - lower);
  if (!std::isfinite(requested_sweep) ||
      requested_sweep > 2 * std::numbers::pi * (1 + 64 * std::numeric_limits<double>::epsilon()))
    throw Error::geometry(curve.id().value(), "Equation circle domain overlaps more than one turn");
  if (radius <= Precision::Confusion())
    throw Error::geometry(curve.id().value(), "Equation circle is below kernel resolution");
  const auto normal =
      cross({a.x / radius, a.y / radius, a.z / radius}, {b.x / radius, b.y / radius, b.z / radius});
  const double phase = frequency * lower;
  const Vector3 x{(a.x * std::cos(phase) + b.x * std::sin(phase)) / radius,
                  (a.y * std::cos(phase) + b.y * std::sin(phase)) / radius,
                  (a.z * std::cos(phase) + b.z * std::sin(phase)) / radius};
  const Point3 center{forms[0]->polynomial[0], forms[1]->polynomial[0], forms[2]->polynomial[0]};
  Handle(Geom_Circle) circle = new Geom_Circle(
      gp_Ax2(point(center), gp_Dir(normal.x, normal.y, normal.z), gp_Dir(x.x, x.y, x.z)), radius);
  const double sweep = closed ? 2 * std::numbers::pi : requested_sweep;
  Handle(Geom_Curve) result = circle;
  if (!closed)
    result = new Geom_TrimmedCurve(circle, 0, sweep);
  return NativeModel{result,
                     EquationCurveGeometryKind::AnalyticCircle,
                     radius * sweep * sweep,
                     lower,
                     upper,
                     circle_uniform_error(forms, circle, lower, upper, sweep)};
}
struct Interval {
  double lower, upper;
  std::size_t depth;
};
struct HermiteSpan {
  double lower, upper;
  Point3 start, c1, c2, end;
};
Handle(Geom_BSplineCurve)
    smooth_chain(const std::vector<HermiteSpan>& spans, double lower, double upper) {
  const auto n = static_cast<int>(spans.size());
  TColgp_Array1OfPnt poles(1, 2 * n + 2);
  TColStd_Array1OfReal knots(1, n + 1);
  TColStd_Array1OfInteger multiplicities(1, n + 1);
  poles.SetValue(1, point(spans.front().start));
  for (int i = 0; i < n; ++i) {
    poles.SetValue(2 * i + 2, point(spans[i].c1));
    poles.SetValue(2 * i + 3, point(spans[i].c2));
    knots.SetValue(i + 1, i == 0 ? 0 : (spans[i].lower - lower) / (upper - lower));
    multiplicities.SetValue(i + 1, i == 0 ? 4 : 2);
  }
  poles.SetValue(2 * n + 2, point(spans.back().end));
  knots.SetValue(n + 1, 1);
  multiplicities.SetValue(n + 1, 4);
  return new Geom_BSplineCurve(poles, knots, multiplicities, 3, false);
}
} // namespace
Result<EquationCurveGeometry>
EquationCurveGeometryAdapter::convert(const PartDocument& document, const EquationCurve& curve,
                                      const EquationCurveGeometryLimits& limits) const {
  try {
    const auto fail = [&](std::string message) -> void {
      throw Error::geometry(curve.id().value(), std::move(message));
    };
    if (limits.maximum_segments == 0 || limits.maximum_segments > 8192 ||
        limits.maximum_depth == 0 || limits.maximum_depth > 28 || limits.maximum_evaluations == 0 ||
        limits.maximum_evaluations > 32768)
      fail("Equation work limits exceed adaptive_v1 policy");
    const auto program = checked(EquationCurveProgram::compile(document, curve));
    const auto [lower, upper] = checked(program.domain(document));
    const bool closed = curve.closure() == EquationCurveClosure::Periodic;
    const bool proven = checked(program.proves_closure(document));
    if (closed && !proven)
      fail("Equation domain has no supported exact closure proof; endpoint proximity cannot close "
           "a gap");
    std::size_t evaluations = 0;
    std::map<double, EquationCurveJet> cache;
    const auto at = [&](double u) -> EquationCurveJet {
      if (const auto found = cache.find(u); found != cache.end())
        return found->second;
      if (++evaluations > limits.maximum_evaluations)
        fail("Equation evaluation budget exceeded");
      const auto result = checked(program.evaluate(document, u));
      if (norm(result.first) == 0)
        fail("Equation has a zero-speed point");
      cache.emplace(u, result);
      return result;
    };
    const auto first = at(lower), last = at(upper);
    const double seam_adjustment = closed ? distance(first.value, last.value) : 0;
    auto model = native_model(document, curve, lower, upper, first, last, closed);
    if (model && model->curve->IsClosed() != closed)
      fail("Closed equation curves require explicit proven periodic intent");
    const double tolerance = curve.tolerance_mm() * .5;
    if (tolerance == 0)
      fail("Equation tolerance is below numeric resolution");
    std::vector<Interval> pending{{lower, upper, 0}};
    std::vector<HermiteSpan> accepted;
    std::size_t accepted_count = 0;
    double maximum_error = 0;
    while (!pending.empty()) {
      const auto interval = pending.back();
      pending.pop_back();
      if (++evaluations > limits.maximum_evaluations)
        fail("Equation interval/evaluation budget exceeded");
      auto analysis = program.analyze_interval(document, interval.lower, interval.upper);
      const double span = interval.upper - interval.lower;
      const auto start = at(interval.lower), end = at(interval.upper);
      const auto modeled_end = closed && interval.upper == upper ? first.value : end.value;
      HermiteSpan segment{interval.lower,
                          interval.upper,
                          start.value,
                          offset(start.value, start.first, span / 3),
                          offset(modeled_end, end.first, -span / 3),
                          modeled_end};
      double error = std::numeric_limits<double>::infinity();
      const bool modeled_regular =
          model ? model->regular(interval.lower, interval.upper)
                : regular_cubic({segment.start, segment.c1, segment.c2, segment.end});
      if (analysis && analysis.value().minimum_speed > 0 && modeled_regular) {
        const auto& a = analysis.value();
        const long double h = span;
        const double source_chord =
            upward(static_cast<long double>(a.maximum_second_derivative) * h * h / 8);
        double model_chord = 0, endpoint_error = 0, numeric = 0;
        if (model) {
          const long double normalized = h / (upper - lower);
          model_chord = upward(static_cast<long double>(model->second_normalized) * normalized *
                               normalized / 8);
          const auto left = model->evaluate(interval.lower),
                     right = model->evaluate(interval.upper);
          endpoint_error = std::max(distance(left, start.value), distance(right, end.value));
          numeric = resolution({left, right, start.value, end.value});
        } else {
          model_chord =
              std::max(distance(segment.c1, interpolate(segment.start, segment.end, 1. / 3)),
                       distance(segment.c2, interpolate(segment.start, segment.end, 2. / 3)));
          endpoint_error = interval.upper == upper ? seam_adjustment : 0;
          numeric = resolution({segment.start, segment.c1, segment.c2, segment.end});
        }
        error = upward(static_cast<long double>(source_chord) + model_chord + endpoint_error +
                       a.rounding_error_mm + numeric);
        if (model)
          error = std::min(error, upward(static_cast<long double>(model->uniform_error) +
                                         a.rounding_error_mm + numeric));
      }
      if (error <= tolerance) {
        ++accepted_count;
        if (accepted_count > limits.maximum_segments)
          fail("Equation segment budget exceeded");
        maximum_error = std::max(maximum_error, error);
        if (!model)
          accepted.push_back(segment);
        continue;
      }
      if (interval.depth >= limits.maximum_depth) {
        if (!analysis)
          throw analysis.error();
        if (analysis.value().minimum_speed == 0)
          fail("Equation regularity could not be proven within its depth budget");
        fail("Equation tolerance could not be resolved within its depth budget");
      }
      const double mid = interval.lower + (interval.upper - interval.lower) * .5;
      if (mid == interval.lower || mid == interval.upper)
        fail("Equation interval is below numeric resolution");
      if (accepted_count + pending.size() + 2 > limits.maximum_segments)
        fail("Equation segment budget exceeded");
      pending.push_back({mid, interval.upper, interval.depth + 1});
      pending.push_back({interval.lower, mid, interval.depth + 1});
    }
    const auto native =
        model ? model->curve : Handle(Geom_Curve)(smooth_chain(accepted, lower, upper));
    for (double parameter : {native->FirstParameter(), native->LastParameter()}) {
      gp_Pnt p;
      gp_Vec d;
      native->D1(parameter, p, d);
      const double speed = std::hypot(d.X(), d.Y(), d.Z());
      if (!std::isfinite(speed) || speed == 0)
        fail("Modeled equation endpoint is below tangent resolution");
    }
    const auto derived = std::make_shared<const DerivedCurve>(native, closed);
    const auto edge = checked(derived_curve_edge(derived, curve.id().value()));
    GeometryShape shape(std::make_shared<GeometryShape::Impl>(edge));
    return Result<EquationCurveGeometry>::success(
        {curve.id(),
         model ? model->kind : EquationCurveGeometryKind::Adaptive,
         derived,
         std::move(shape),
         {{{"start", point(native->Value(native->FirstParameter()))},
           {"end", closed ? point(native->Value(native->FirstParameter()))
                          : point(native->Value(native->LastParameter()))}}},
         maximum_error,
         accepted_count,
         evaluations});
  } catch (const Error& error) {
    return Result<EquationCurveGeometry>::failure(error);
  } catch (const Standard_Failure& failure) {
    return Result<EquationCurveGeometry>::failure(Error::geometry(
        curve.id().value(),
        failure.GetMessageString() ? failure.GetMessageString() : "OCCT equation curve failed"));
  } catch (const std::exception& failure) {
    return Result<EquationCurveGeometry>::failure(
        Error::geometry(curve.id().value(), failure.what()));
  }
}
} // namespace blcad::geometry
