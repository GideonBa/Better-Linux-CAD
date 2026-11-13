#include "../support/equation_curve_fixture.hpp"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <numbers>

using namespace blcad;
namespace f = blcad::test::equation_fixture;
namespace {
CurveExpression expression(const PartDocument& part, const std::string& text,
                           CurveUnit unit = CurveUnit::Scalar) {
  return f::checked(CurveExpression::compile(part, text, CurveVariable{"u", unit}));
}
CurveExpression bound(const PartDocument& part, const std::string& text) {
  return f::checked(CurveExpression::compile(part, text));
}
void enclosed(CurveInterval interval, double value) {
  CHECK(interval.lower <= value);
  CHECK(value <= interval.upper);
}
} // namespace
TEST_CASE("Curve interval jets enclose independent analytic values and derivatives",
          "[core][parity-b161][equation-analysis]") {
  const auto part = f::part();
  struct Fixture {
    std::string source;
    std::function<std::array<double, 3>(double)> jet;
  };
  for (const auto& sample : std::array<Fixture, 6>{
           {{"sin(3*u)*1mm",
             [](double u) {
               return std::array{std::sin(3 * u), 3 * std::cos(3 * u), -9 * std::sin(3 * u)};
             }},
            {"exp(u)*1mm",
             [](double u) { return std::array{std::exp(u), std::exp(u), std::exp(u)}; }},
            {"log(u)*1mm", [](double u) { return std::array{std::log(u), 1 / u, -1 / (u * u)}; }},
            {"sqrt(u)*1mm",
             [](double u) {
               return std::array{std::sqrt(u), .5 / std::sqrt(u), -.25 / (u * std::sqrt(u))};
             }},
            {"1mm/u", [](double u) { return std::array{1 / u, -1 / (u * u), 2 / (u * u * u)}; }},
            {"u^3*1mm", [](double u) { return std::array{u * u * u, 3 * u * u, 6 * u}; }}}}) {
    INFO(sample.source);
    const auto range = f::checked(expression(part, sample.source).bounds(part, .25, 1.5));
    for (int i = 0; i <= 120; ++i) {
      const auto expected = sample.jet(.25 + 1.25 * i / 120.);
      enclosed(range.value, expected[0]);
      enclosed(range.first, expected[1]);
      enclosed(range.second, expected[2]);
    }
    CHECK(std::isfinite(range.rounding_error));
    CHECK(range.rounding_error >= 0);
  }
}
TEST_CASE("Curve intervals detect hidden oscillation singularities and lost numeric resolution",
          "[core][parity-b161][equation-analysis]") {
  const auto part = f::part();
  for (const auto& source :
       {"1mm/(u-.25)", "sqrt(u)*1mm", "log(u)*1mm", "abs(u)*1mm", "tan(u)*1mm"}) {
    INFO(source);
    CHECK_FALSE(expression(part, source).bounds(part, -1, 2));
  }
  CHECK(expression(part, "tan(u)*1mm").bounds(part, 0, .1));
  const auto high = f::checked(expression(part, "sin(2048*pi*u)*1mm").bounds(part, 0, 1));
  enclosed(high.value, -1);
  enclosed(high.value, 1);
  CHECK(std::max(std::abs(high.second.lower), std::abs(high.second.upper)) >=
        std::pow(2048 * std::numbers::pi, 2));
  const auto cancellation =
      f::checked(expression(part, "(1e16mm+u*1mm)-1e16mm").bounds(part, 0, 1));
  CHECK(cancellation.rounding_error > 1);
  CHECK_FALSE(expression(part, "exp(u)*1mm").bounds(part, 1000, 1001));
  const auto other = PartDocument::create(DocumentId("other"), "Other").value();
  CHECK_FALSE(expression(part, "u*1mm").bounds(other, 0, 1));
}
TEST_CASE(
    "Analytic curve recognition is conservative and retains polynomial and harmonic orientation",
    "[core][parity-b161][equation-analysis]") {
  const auto part = f::part();
  const auto polynomial = f::checked(expression(part, "u^3*1mm+2mm*u-3mm").analytic_form(part));
  REQUIRE(polynomial);
  CHECK(polynomial->polynomial == std::array{-3., 2., 0., 1.});
  const auto harmonic =
      f::checked(expression(part, "2mm*cos(-u)+3mm*sin(-u)+5mm").analytic_form(part));
  REQUIRE(harmonic);
  CHECK(harmonic->frequency == 1);
  CHECK(harmonic->cosine == 2);
  CHECK(harmonic->sine == -3);
  CHECK(harmonic->polynomial[0] == 5);
  CHECK_FALSE(f::checked(expression(part, "sin(u)^2*1mm").analytic_form(part)));
}
TEST_CASE("Curve closure uses exact symbolic pi or polynomial identities instead of a near seam",
          "[core][parity-b161][equation-analysis]") {
  auto part = f::part();
  const auto zero = bound(part, "0"), period = bound(part, "2*pi");
  const auto sine = expression(part, "sin(u)*1mm");
  CHECK(f::checked(sine.proves_period(part, zero, period)));
  CHECK(f::checked(expression(part, "exp(sin(u))*1mm").proves_period(part, zero, period)));
  CHECK(f::checked(expression(part, "sin(pi*u)*1mm").proves_period(part, zero, bound(part, "2"))));
  CHECK_FALSE(f::checked(sine.proves_period(part, zero, bound(part, "6.283185307179586"))));
  CHECK_FALSE(f::checked(sine.proves_period(part, zero, bound(part, "2*pi+1/1099511627776"))));
  CHECK_FALSE(f::checked(sine.proves_period(part, zero, bound(part, "2*pi+0.000000000000000001"))));
  CHECK(f::checked(expression(part, "sin(u/1mm)", CurveUnit::Length)
                       .proves_period(part, bound(part, "0mm"), bound(part, "2*pi*1mm"))));
  CHECK_FALSE(expression(part, "sin(u/1mm)", CurveUnit::Length).proves_period(part, zero, period));
  REQUIRE(part.add_parameter(f::checked(Parameter::create_angle(
      ParameterId("full.angle"), "Full angle", Quantity::angle_deg(360).value()))));
  CHECK(f::checked(expression(part, "cos(u)*1mm", CurveUnit::Angle)
                       .proves_period(part, bound(part, "0rad"), bound(part, "@{full.angle}"))));
  const auto polynomial = expression(part, "4mm*u*(1-u)");
  CHECK(f::checked(polynomial.proves_endpoint_identity(part, zero, bound(part, "1"))));
  CHECK_FALSE(f::checked(expression(part, "4mm*u*(1-u)+u*1mm/1099511627776")
                             .proves_endpoint_identity(part, zero, bound(part, "1"))));
}
TEST_CASE("Equation analysis includes domain uncertainty and proves only supported closed intent",
          "[core][parity-b161][equation-analysis]") {
  const auto part = f::part();
  const auto helix = f::checked(EquationCurveProgram::compile(part, f::spatial()));
  CHECK_FALSE(f::checked(helix.proves_closure(part)));
  const auto limits = f::checked(helix.domain(part));
  const auto metrics = f::checked(helix.analyze_interval(part, limits.first, limits.second));
  CHECK(metrics.minimum_speed > 2.9);
  CHECK(metrics.maximum_second_derivative >= 5);
  CHECK(metrics.rounding_error_mm > 0);
  const auto leaf = EquationCurve::create(SketchEntityId("leaf"), EquationCurveMode::Parametric2D,
                                          {"u", CurveUnit::Scalar}, "0", "1",
                                          {"4mm*u*(1-u)", "4mm*(2*u-1)*u*(1-u)", ""}, .001,
                                          EquationCurveClosure::Periodic)
                        .value();
  CHECK(f::checked(f::checked(EquationCurveProgram::compile(part, leaf)).proves_closure(part)));
}
