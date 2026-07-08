#include "blcad/core/datum_plane.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("DatumPlane creates the standard XY plane", "[core][datum_plane]") {
  const auto plane = DatumPlane::xy();

  REQUIRE(plane);
  const Point3 expected_origin{0.0, 0.0, 0.0};
  const Vector3 expected_x_axis{1.0, 0.0, 0.0};
  const Vector3 expected_y_axis{0.0, 1.0, 0.0};
  const Vector3 expected_normal{0.0, 0.0, 1.0};

  CHECK(plane.value().id().value() == "datum.xy");
  CHECK(plane.value().name() == "XY");
  CHECK(plane.value().origin() == expected_origin);
  CHECK(plane.value().x_axis() == expected_x_axis);
  CHECK(plane.value().y_axis() == expected_y_axis);
  CHECK(plane.value().normal() == expected_normal);
}

TEST_CASE("DatumPlane rejects empty ids", "[core][datum_plane]") {
  const auto plane = DatumPlane::xy(DatumPlaneId(), "XY");

  REQUIRE(plane.has_error());
  CHECK(plane.error().category() == ErrorCategory::Validation);
  CHECK(plane.error().object_id() == "datum_plane");
  CHECK(plane.error().message() == "datum plane id must not be empty");
}

TEST_CASE("DatumPlane rejects empty names", "[core][datum_plane]") {
  const auto plane = DatumPlane::xy(DatumPlaneId("datum.xy"), "");

  REQUIRE(plane.has_error());
  CHECK(plane.error().category() == ErrorCategory::Validation);
  CHECK(plane.error().object_id() == "datum.xy");
  CHECK(plane.error().message() == "datum plane name must not be empty");
}
