#include "blcad/core/error.hpp"
#include "blcad/core/result.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("Error stores category, object id and message", "[core][error]") {
  const auto error = Error::validation("part.width", "length must be greater than 0 mm");

  CHECK(error.category() == ErrorCategory::Validation);
  CHECK(to_string(error.category()) == "validation");
  CHECK(error.object_id() == "part.width");
  CHECK(error.message() == "length must be greater than 0 mm");
}

TEST_CASE("Result stores successful values", "[core][result]") {
  const auto result = Result<int>::success(42);

  REQUIRE(result.has_value());
  CHECK_FALSE(result.has_error());
  CHECK(result);
  CHECK(result.value() == 42);
}

TEST_CASE("Result stores expected errors", "[core][result]") {
  const auto result = Result<int>::failure(Error::dependency("graph", "cycle detected"));

  REQUIRE(result.has_error());
  CHECK_FALSE(result.has_value());
  CHECK_FALSE(result);
  CHECK(result.error().category() == ErrorCategory::Dependency);
  CHECK(result.error().object_id() == "graph");
  CHECK(result.error().message() == "cycle detected");
}
