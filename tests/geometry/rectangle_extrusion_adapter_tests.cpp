#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

TEST_CASE("RectangleExtrusionAdapter creates one OCCT solid from positive lengths",
          "[geometry][rectangle_extrusion]") {
  const auto width = Quantity::length_mm(80.0, "plate.width");
  const auto height = Quantity::length_mm(40.0, "plate.height");
  const auto thickness = Quantity::length_mm(10.0, "plate.thickness");

  REQUIRE(width);
  REQUIRE(height);
  REQUIRE(thickness);

  const RectangleExtrusionAdapter adapter;
  const auto shape =
      adapter.make_extruded_rectangle(width.value(), height.value(), thickness.value());

  REQUIRE(shape);
  CHECK_FALSE(shape.value().empty());

  const ShapeSummary summary = adapter.summarize(shape.value());
  CHECK_FALSE(summary.is_null);
  CHECK(summary.solid_count == 1);
}

TEST_CASE("GeometryShape defaults to an empty geometry handle", "[geometry][rectangle_extrusion]") {
  const GeometryShape shape;
  const RectangleExtrusionAdapter adapter;

  CHECK(shape.empty());

  const ShapeSummary summary = adapter.summarize(shape);
  CHECK(summary.is_null);
  CHECK(summary.solid_count == 0);
}
