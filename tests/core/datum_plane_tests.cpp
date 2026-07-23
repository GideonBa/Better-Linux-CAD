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

TEST_CASE("DatumPlane creates the principal XZ and YZ origin planes", "[core][datum_plane]") {
  const auto xz = DatumPlane::xz();
  REQUIRE(xz);
  CHECK(xz.value().id().value() == "datum.xz");
  CHECK(xz.value().name() == "XZ");
  CHECK(xz.value().origin() == Point3{0.0, 0.0, 0.0});
  CHECK(xz.value().x_axis() == Vector3{1.0, 0.0, 0.0});
  CHECK(xz.value().y_axis() == Vector3{0.0, 0.0, 1.0});
  // Right-handed: x_axis × y_axis == normal.
  CHECK(xz.value().normal() == Vector3{0.0, -1.0, 0.0});

  const auto yz = DatumPlane::yz();
  REQUIRE(yz);
  CHECK(yz.value().id().value() == "datum.yz");
  CHECK(yz.value().name() == "YZ");
  CHECK(yz.value().x_axis() == Vector3{0.0, 1.0, 0.0});
  CHECK(yz.value().y_axis() == Vector3{0.0, 0.0, 1.0});
  CHECK(yz.value().normal() == Vector3{1.0, 0.0, 0.0});

  CHECK(DatumPlane::xz(DatumPlaneId(), "XZ").has_error());
  CHECK(DatumPlane::yz(DatumPlaneId("datum.yz"), "").has_error());
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

TEST_CASE("SemanticAxisReference exposes stable generated-axis identity",
          "[core][semantic-axis]") {
  const auto axis = SemanticAxisReference::create(FeatureId("feature.hole"));

  REQUIRE(axis);
  CHECK(axis.value().source_feature().value() == "feature.hole");
  CHECK(axis.value().axis() == SemanticAxis::Primary);
  CHECK(to_string(axis.value().axis()) == "axis");
  CHECK(axis.value().node_id() == "feature.hole.axis");

  const auto invalid = SemanticAxisReference::create(FeatureId());
  REQUIRE(invalid.has_error());
  CHECK(invalid.error().message() == "semantic axis source feature id must not be empty");
}
