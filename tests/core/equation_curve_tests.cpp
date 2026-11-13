#include "blcad/core/equation_curve.hpp"
#include "blcad/core/parameter_expression.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using namespace blcad;
namespace {
PartDocument document(const char* id = "curve.part", const char* radius_name = "Radius") {
  auto part = PartDocument::create(DocumentId(id), "Equations").value();
  REQUIRE(part.add_parameter(
      Parameter::create_length(ParameterId("radius"), radius_name, Quantity::length_mm(5).value())
          .value()));
  REQUIRE(part.add_parameter(
      Parameter::create_angle(ParameterId("angle"), "Angle", Quantity::angle_deg(45).value())
          .value()));
  REQUIRE(part.add_parameter(
      Parameter::create_count(ParameterId("count"), "Count", Quantity::count(3).value()).value()));
  REQUIRE(part.add_parameter(Parameter::create_length(ParameterId("u"), "LocalNameParameter",
                                                      Quantity::length_mm(7).value())
                                 .value()));
  return part;
}
void near(double actual, double expected) {
  CHECK(actual == Catch::Approx(expected).margin(1e-10 * (1 + std::abs(expected))));
}
void near(Point3 actual, Point3 expected) {
  near(actual.x, expected.x);
  near(actual.y, expected.y);
  near(actual.z, expected.z);
}
CurveExpression expression(const PartDocument& part, std::string_view source,
                           CurveUnit unit = CurveUnit::Scalar) {
  auto parsed = CurveExpression::compile(part, source, CurveVariable{"u", unit});
  INFO((parsed ? "compiled" : parsed.error().message()));
  REQUIRE(parsed);
  return parsed.value();
}
EquationCurve curve(EquationCurveMode mode, CurveUnit unit, std::string lower, std::string upper,
                    std::array<std::string, 3> coordinates) {
  auto result = EquationCurve::create(SketchEntityId("equation"), mode, {"u", unit}, lower, upper,
                                      coordinates);
  INFO((result ? "intent" : result.error().message()));
  REQUIRE(result);
  return result.value();
}
} // namespace

TEST_CASE("Curve-local expressions retain units and independently differentiated real derivatives",
          "[core][parity-b160][curve-expression]") {
  const auto part = document();
  const auto before = serialize_part_document_to_json(part).value();
  auto polynomial = expression(part, "u^2/(10 mm)", CurveUnit::Length);
  CHECK(polynomial.unit() == CurveExpressionUnit{1, 0});
  for (double u : {-3.0, -.5, 0.0, 2.0, 5.0}) {
    const auto jet = polynomial.evaluate(part, u);
    REQUIRE(jet);
    near(jet.value().value, u * u / 10);
    near(jet.value().first, u / 5);
    near(jet.value().second, .2);
    const auto trig = expression(part, "sin(u)^2+cos(u)^2").evaluate(part, u);
    REQUIRE(trig);
    near(trig.value().value, 1);
    near(trig.value().first, 0);
    near(trig.value().second, 0);
  }
  const auto reciprocal = expression(part, "u^-2").evaluate(part, 2);
  REQUIRE(reciprocal);
  near(reciprocal.value().value, .25);
  near(reciprocal.value().first, -.25);
  near(reciprocal.value().second, .375);
  const auto square_root = expression(part, "sqrt(u)").evaluate(part, 4);
  REQUIRE(square_root);
  near(square_root.value().value, 2);
  near(square_root.value().first, .25);
  near(square_root.value().second, -1.0 / 32);
  const auto exponential = expression(part, "exp(u)+log(u)").evaluate(part, 2);
  REQUIRE(exponential);
  near(exponential.value().first, std::exp(2) + .5);
  near(exponential.value().second, std::exp(2) - .25);
  CHECK(serialize_part_document_to_json(part).value() == before);
}

TEST_CASE("Curve references use stable IDs across names values scopes and local variable spelling",
          "[core][parity-b160][curve-expression]") {
  auto part = document();
  const auto program = expression(part, "@{radius}*cos(u)+2mm");
  CHECK(program.parameter_dependencies() == std::vector<ParameterId>{ParameterId("radius")});
  near(program.evaluate(part, 0).value().value, 7);
  REQUIRE(part.set_parameter_value(ParameterId("radius"), Quantity::length_mm(8).value()));
  near(program.evaluate(part, 0).value().value, 10);
  auto renamed = document("curve.part", "RenamedRadius");
  near(program.evaluate(renamed, 0).value().value, 7);
  const auto another = document("another.part");
  CHECK_FALSE(program.evaluate(another, 0));
  const auto distinct = expression(part, "u*1mm+@{u}");
  near(distinct.evaluate(part, -2).value().value, 5);
  CHECK_FALSE(
      CurveExpression::compile(part, "Radius*cos(u)", CurveVariable{"u", CurveUnit::Scalar}));
  const auto angle = expression(part, "sin(@{angle})+cos(45deg)");
  near(angle.evaluate(part, 0).value().value, std::sqrt(2.0));
  CHECK_FALSE(ParameterExpressionEvaluator{}.evaluate(part, "Radius*Radius", ParameterType::Length,
                                                      "legacy"));
  CHECK_FALSE(
      ParameterExpressionEvaluator{}.evaluate(part, "sin(1)", ParameterType::Count, "legacy"));
  CHECK_FALSE(
      ParameterExpressionEvaluator{}.evaluate(part, "Angle", ParameterType::Angle, "legacy"));
}

TEST_CASE("Curve parsing rejects unsafe malformed untyped and excessive input deterministically",
          "[core][parity-b160][curve-expression]") {
  const auto part = document();
  const auto before = serialize_part_document_to_json(part).value();
  const CurveVariable local{"u", CurveUnit::Scalar};
  for (const std::string source :
       {"", "1+", "sin()", "sin(u,1)", "u^9", "u^1.5", "1e999", "u;system(1)", "system(1)",
        "__import__(1)", "@{missing}", "@{}", "@{radius", "sin(1mm)", "sqrt(1mm)", "exp(1rad)",
        "1mm+1", "unknown(u)", "1m"}) {
    CAPTURE(source);
    const auto result = CurveExpression::compile(part, source, local, "unsafe");
    CHECK_FALSE(result);
    const auto repeated = CurveExpression::compile(part, source, local, "unsafe");
    REQUIRE(repeated.has_error());
    CHECK(repeated.error().message() == result.error().message());
  }
  std::string nodes = "1";
  for (int i = 0; i < 300; ++i)
    nodes += "+1";
  for (const auto& source : std::vector<std::string>{
           std::string(4097, '1'), std::string(70, '(') + "u" + std::string(70, ')'), nodes,
           std::string("1\0+u", 4)})
    CHECK_FALSE(CurveExpression::compile(part, source, local));
  CHECK_FALSE(CurveExpression::compile(part, "1mm", CurveVariable{"pi", CurveUnit::Scalar}));
  CHECK_FALSE(
      CurveExpression::compile(part, "1mm", CurveVariable{"u", static_cast<CurveUnit>(99)}));
  CHECK(serialize_part_document_to_json(part).value() == before);
}

TEST_CASE("Curve evaluation fails explicitly at singularities without inventing derivatives",
          "[core][parity-b160][curve-expression]") {
  const auto part = document();
  for (const auto& [source, value] :
       std::vector<std::pair<std::string, double>>{{"1/(u-.3)", .3},
                                                   {"u^-1", 0},
                                                   {"sqrt(u)", 0},
                                                   {"sqrt(u^4)", 0},
                                                   {"sqrt(u)", -1},
                                                   {"log(u)", 0},
                                                   {"abs(u)", 0},
                                                   {"tan(u)", std::numbers::pi / 2},
                                                   {"exp(u)", 1000}}) {
    CAPTURE(source, value);
    CHECK_FALSE(expression(part, source).evaluate(part, value));
  }
  for (const auto& source : {"sqrt(0mm*0mm)", "abs(0mm)", "0mm"}) {
    const auto value = expression(part, source).evaluate(part, 0);
    REQUIRE(value);
    near(value.value().value, 0);
    near(value.value().first, 0);
    near(value.value().second, 0);
  }
  const auto negative = expression(part, "abs(u)").evaluate(part, -2);
  REQUIRE(negative);
  near(negative.value().value, 2);
  near(negative.value().first, -1);
  near(negative.value().second, 0);
}

TEST_CASE("Typed equation modes match analytic parabola circle and spatial helix jets",
          "[core][parity-b160][equation-curve]") {
  const auto part = document();
  const auto parabola = curve(EquationCurveMode::Cartesian2D, CurveUnit::Length, "-3mm", "4mm",
                              {"", "u*u/(10mm)", ""});
  const auto program = EquationCurveProgram::compile(part, parabola);
  REQUIRE(program);
  const auto jet = program.value().evaluate(part, -2);
  REQUIRE(jet);
  near(jet.value().value, {-2, .4, 0});
  near(jet.value().first, {1, -.4, 0});
  near(jet.value().second, {0, .2, 0});
  CHECK_FALSE(program.value().evaluate(part, 5));
  const auto polar =
      curve(EquationCurveMode::Polar2D, CurveUnit::Angle, "0rad", "360deg", {"@{radius}", "", ""});
  const auto circle = EquationCurveProgram::compile(part, polar);
  REQUIRE(circle);
  const auto top = circle.value().evaluate(part, std::numbers::pi / 2);
  REQUIRE(top);
  near(top.value().value, {0, 5, 0});
  near(top.value().first, {-5, 0, 0});
  near(top.value().second, {0, -5, 0});
  const auto helix = curve(EquationCurveMode::Parametric3D, CurveUnit::Scalar, "0", "2*pi",
                           {"@{radius}*cos(u)", "@{radius}*sin(u)", "2mm*u"});
  const auto spatial = EquationCurveProgram::compile(part, helix);
  REQUIRE(spatial);
  const auto halfway = spatial.value().evaluate(part, std::numbers::pi);
  REQUIRE(halfway);
  near(halfway.value().value, {-5, 0, 2 * std::numbers::pi});
  near(halfway.value().first, {0, -5, 2});
  near(halfway.value().second, {5, 0, 0});
  CHECK(helix.parameter_dependencies() == std::vector<ParameterId>{ParameterId("radius")});
}

TEST_CASE("Equation intent validates typed bounds modes tolerance and derived source identity",
          "[core][parity-b160][equation-curve]") {
  auto part = document();
  auto valid = curve(EquationCurveMode::Parametric2D, CurveUnit::Scalar, "0", "@{count}",
                     {"u*1mm", "-u*1mm", ""});
  const auto program = EquationCurveProgram::compile(part, valid);
  REQUIRE(program);
  REQUIRE(part.set_parameter_value(ParameterId("count"), Quantity::count(5).value()));
  near(program.value().domain(part).value().second, 5);
  CHECK(program.value().curve() == valid);
  CHECK(valid.with_construction(true).construction());
  for (const auto& bounds : std::vector<std::pair<std::string, std::string>>{
           {"0mm", "1mm"}, {"1", "1"}, {"2", "1"}, {"-1e308", "1e308"}}) {
    const auto c = curve(EquationCurveMode::Parametric2D, CurveUnit::Scalar, bounds.first,
                         bounds.second, {"u*1mm", "0mm", ""});
    CHECK_FALSE(EquationCurveProgram::compile(part, c));
  }
  const auto wrong =
      curve(EquationCurveMode::Parametric2D, CurveUnit::Scalar, "0", "1", {"u", "0mm", ""});
  CHECK_FALSE(EquationCurveProgram::compile(part, wrong));
  for (double tolerance : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
    CHECK_FALSE(EquationCurve::create(SketchEntityId("bad"), EquationCurveMode::Parametric2D,
                                      {"u", CurveUnit::Scalar}, "0", "1", {"u*1mm", "0mm", ""},
                                      tolerance));
  CHECK_FALSE(EquationCurve::create(SketchEntityId("bad"), EquationCurveMode::Polar2D,
                                    {"u", CurveUnit::Scalar}, "0", "1", {"1mm", "", ""}));
  CHECK_FALSE(EquationCurve::create(SketchEntityId("bad"), EquationCurveMode::Cartesian2D,
                                    {"u", CurveUnit::Length}, "0mm", "1mm", {"u", "1mm", ""}));
  CHECK_FALSE(EquationCurve::create(SketchEntityId("bad"), EquationCurveMode::Parametric2D,
                                    {"u", CurveUnit::Scalar}, "u", "1", {"u*1mm", "0mm", ""}));
}
