#include "blcad/core/quantity.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace blcad;

TEST_CASE("Quantity stores positive millimeter lengths", "[core][quantity]") {
  const auto quantity = Quantity::length_mm(8.0, "part.thickness");

  REQUIRE(quantity);
  CHECK(quantity.value().millimeters() == Catch::Approx(8.0));
  CHECK(quantity.value().unit() == "mm");
  CHECK(quantity.value().is_positive_length());
}

TEST_CASE("Quantity rejects zero length", "[core][quantity]") {
  const auto quantity = Quantity::length_mm(0.0, "part.width");

  REQUIRE(quantity.has_error());
  CHECK(quantity.error().category() == ErrorCategory::Validation);
  CHECK(quantity.error().object_id() == "part.width");
  CHECK(quantity.error().message() == "length must be greater than 0 mm");
}

TEST_CASE("Quantity rejects negative length", "[core][quantity]") {
  const auto quantity = Quantity::length_mm(-1.0, "part.thickness");

  REQUIRE(quantity.has_error());
  CHECK(quantity.error().category() == ErrorCategory::Validation);
  CHECK(quantity.error().object_id() == "part.thickness");
  CHECK(quantity.error().message() == "length must be greater than 0 mm");
}

TEST_CASE("Quantity rejects non-finite length", "[core][quantity]") {
  const auto quantity = Quantity::length_mm(std::numeric_limits<double>::infinity(), "part.width");

  REQUIRE(quantity.has_error());
  CHECK(quantity.error().category() == ErrorCategory::Validation);
  CHECK(quantity.error().object_id() == "part.width");
  CHECK(quantity.error().message() == "length must be finite");
}
