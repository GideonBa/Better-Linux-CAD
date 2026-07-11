#include "blcad/core/parameter_expression.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace blcad;
using Catch::Approx;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Parameter make_count_parameter(const char* id, const char* name, double value) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_document() {
  auto document = PartDocument::create(DocumentId("part.expression"), "ExpressionPart");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(document.value().add_parameter(make_count_parameter("part.bolt_count", "bolt_count", 6)));
  return document.value();
}

} // namespace

TEST_CASE("Parameter expressions evaluate unit-aware formulas", "[core][parameter-expression]") {
  const PartDocument document = make_document();
  const ParameterExpressionEvaluator evaluator;

  SECTION("length arithmetic with parameter references and mm literals") {
    const auto evaluated =
        evaluator.evaluate(document, "width / 2 - 15 mm", ParameterType::Length, "test");
    REQUIRE(evaluated);
    CHECK(evaluated.value().value.millimeters() == Approx(45.0));
    REQUIRE(evaluated.value().referenced_parameters.size() == 1U);
    CHECK(evaluated.value().referenced_parameters.front().value() == "part.width");
  }

  SECTION("parentheses, unary minus, and repeated references") {
    const auto evaluated = evaluator.evaluate(
        document, "(width + height) / 2 + -(width - height) / 4", ParameterType::Length, "test");
    REQUIRE(evaluated);
    CHECK(evaluated.value().value.millimeters() == Approx(90.0));
    CHECK(evaluated.value().referenced_parameters.size() == 2U);
  }

  SECTION("count parameters are dimensionless inputs") {
    const auto evaluated =
        evaluator.evaluate(document, "bolt_count * 2", ParameterType::Count, "test");
    REQUIRE(evaluated);
    CHECK(evaluated.value().value.count_value() == 12U);
  }

  SECTION("unit violations are rejected") {
    CHECK(evaluator.evaluate(document, "width + 2", ParameterType::Length, "test").has_error());
    CHECK(
        evaluator.evaluate(document, "width * height", ParameterType::Length, "test").has_error());
    CHECK(evaluator.evaluate(document, "2 / width", ParameterType::Length, "test").has_error());
    CHECK(evaluator.evaluate(document, "width", ParameterType::Count, "test").has_error());
    CHECK(evaluator.evaluate(document, "5", ParameterType::Length, "test").has_error());
  }

  SECTION("malformed formulas are rejected") {
    CHECK(evaluator.evaluate(document, "width +", ParameterType::Length, "test").has_error());
    CHECK(evaluator.evaluate(document, "(width", ParameterType::Length, "test").has_error());
    CHECK(evaluator.evaluate(document, "width 2", ParameterType::Length, "test").has_error());
    CHECK(
        evaluator.evaluate(document, "unknown + 1 mm", ParameterType::Length, "test").has_error());
    CHECK(evaluator.evaluate(document, "width / 0", ParameterType::Length, "test").has_error());
  }
}

TEST_CASE("PartDocument owns expression parameters through the dependency graph",
          "[core][parameter-expression]") {
  PartDocument document = make_document();
  REQUIRE(document.add_expression_parameter(ParameterId("part.margin"), "margin",
                                            ParameterType::Length, "width / 2 - 15 mm"));

  const Parameter* margin = document.find_parameter(ParameterId("part.margin"));
  REQUIRE(margin != nullptr);
  CHECK(margin->is_expression());
  CHECK(margin->value().millimeters() == Approx(45.0));
  CHECK(document.dependency_graph().has_dependency("part.width", "part.margin"));

  SECTION("direct writes to expression parameters are rejected") {
    auto value = Quantity::length_mm(10.0, "part.margin");
    REQUIRE(value);
    const auto set = document.set_parameter_value(ParameterId("part.margin"), value.value());
    REQUIRE(set.has_error());
    CHECK(set.error().message() == "expression-driven parameter value cannot be set directly");
  }

  SECTION("changing an input re-evaluates the expression and marks dependents") {
    document.mark_all_clean();
    auto new_width = Quantity::length_mm(200.0, "part.width");
    REQUIRE(new_width);
    const auto affected =
        document.set_parameter_value(ParameterId("part.width"), new_width.value());
    REQUIRE(affected);
    CHECK(std::find(affected.value().begin(), affected.value().end(), "part.margin") !=
          affected.value().end());
    CHECK(document.find_parameter(ParameterId("part.margin"))->value().millimeters() ==
          Approx(85.0));
  }

  SECTION("chained expressions re-evaluate in topological order") {
    REQUIRE(document.add_expression_parameter(ParameterId("part.half_margin"), "half_margin",
                                              ParameterType::Length, "margin / 2"));
    auto new_width = Quantity::length_mm(150.0, "part.width");
    REQUIRE(new_width);
    REQUIRE(document.set_parameter_value(ParameterId("part.width"), new_width.value()));
    CHECK(document.find_parameter(ParameterId("part.margin"))->value().millimeters() ==
          Approx(60.0));
    CHECK(document.find_parameter(ParameterId("part.half_margin"))->value().millimeters() ==
          Approx(30.0));
  }

  SECTION("duplicate ids, self references, and invalid formulas are rejected") {
    CHECK(document
              .add_expression_parameter(ParameterId("part.margin"), "margin_again",
                                        ParameterType::Length, "width / 2")
              .has_error());
    CHECK(document
              .add_expression_parameter(ParameterId("part.bad"), "bad", ParameterType::Length,
                                        "bad + 1 mm")
              .has_error());
    CHECK(document
              .add_expression_parameter(ParameterId("part.scalar"), "scalar", ParameterType::Length,
                                        "3 * 4")
              .has_error());
  }
}

TEST_CASE("Expression parameters survive JSON roundtrip and re-derive edges",
          "[core][parameter-expression]") {
  PartDocument document = make_document();
  REQUIRE(document.add_expression_parameter(ParameterId("part.margin"), "margin",
                                            ParameterType::Length, "width / 2 - 15 mm"));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("\"formula\"") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const Parameter* margin = restored.value().find_parameter(ParameterId("part.margin"));
  REQUIRE(margin != nullptr);
  CHECK(margin->is_expression());
  CHECK(margin->formula().value() == "width / 2 - 15 mm");
  CHECK(margin->value().millimeters() == Approx(45.0));
  CHECK(restored.value().dependency_graph().has_dependency("part.width", "part.margin"));

  auto new_width = Quantity::length_mm(180.0, "part.width");
  REQUIRE(new_width);
  REQUIRE(restored.value().set_parameter_value(ParameterId("part.width"), new_width.value()));
  CHECK(restored.value().find_parameter(ParameterId("part.margin"))->value().millimeters() ==
        Approx(75.0));
}
