#include "blcad/core/parameter.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("Length parameter stores identity, name, scope and millimeter value",
          "[core][parameter]") {
  const auto quantity = Quantity::length_mm(120.0, "part.width");
  REQUIRE(quantity);

  const auto parameter =
      Parameter::create_length(ParameterId("part.width"), "width", quantity.value());

  REQUIRE(parameter);
  CHECK(parameter.value().id().value() == "part.width");
  CHECK(parameter.value().name() == "width");
  CHECK(parameter.value().type() == ParameterType::Length);
  CHECK(to_string(parameter.value().type()) == "length");
  CHECK(parameter.value().scope() == ParameterScope::Part);
  CHECK(to_string(parameter.value().scope()) == "part");
  CHECK(parameter.value().value().millimeters() == Catch::Approx(120.0));
  CHECK(parameter.value().value().unit() == "mm");
}

TEST_CASE("Length parameter rejects empty ids", "[core][parameter]") {
  const auto quantity = Quantity::length_mm(120.0, "part.width");
  REQUIRE(quantity);

  const auto parameter = Parameter::create_length(ParameterId(), "width", quantity.value());

  REQUIRE(parameter.has_error());
  CHECK(parameter.error().category() == ErrorCategory::Validation);
  CHECK(parameter.error().object_id() == "parameter");
  CHECK(parameter.error().message() == "parameter id must not be empty");
}

TEST_CASE("Length parameter rejects empty names", "[core][parameter]") {
  const auto quantity = Quantity::length_mm(120.0, "part.width");
  REQUIRE(quantity);

  const auto parameter = Parameter::create_length(ParameterId("part.width"), "", quantity.value());

  REQUIRE(parameter.has_error());
  CHECK(parameter.error().category() == ErrorCategory::Validation);
  CHECK(parameter.error().object_id() == "part.width");
  CHECK(parameter.error().message() == "parameter name must not be empty");
}
