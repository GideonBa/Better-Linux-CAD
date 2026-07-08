#include "blcad/geometry/circular_cut_adapter.hpp"

#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

GeometryShape make_base_plate() {
  const auto width = Quantity::length_mm(120.0, "plate.width");
  const auto height = Quantity::length_mm(80.0, "plate.height");
  const auto thickness = Quantity::length_mm(8.0, "plate.thickness");
  REQUIRE(width);
  REQUIRE(height);
  REQUIRE(thickness);

  const RectangleExtrusionAdapter adapter;
  auto plate = adapter.make_extruded_rectangle(width.value(), height.value(), thickness.value());
  REQUIRE(plate);

  return plate.value();
}

} // namespace

TEST_CASE("CircularCutAdapter cuts a through-all hole and keeps a single solid",
          "[geometry][circular_cut]") {
  const GeometryShape plate = make_base_plate();
  const auto diameter = Quantity::length_mm(20.0, "plate.hole_diameter");
  REQUIRE(diameter);

  const CircularCutAdapter adapter;
  const auto cut = adapter.cut_circular_hole(plate, diameter.value());

  REQUIRE(cut);
  CHECK_FALSE(cut.value().empty());

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary cut_summary = inspector.summarize(cut.value());
  CHECK_FALSE(cut_summary.is_null);
  CHECK(cut_summary.solid_count == 1);

  const ShapeSummary plate_summary = inspector.summarize(plate);
  CHECK(cut_summary.volume_mm3 < plate_summary.volume_mm3);
  CHECK(cut_summary.volume_mm3 > 0.0);
}

TEST_CASE("CircularCutAdapter rejects an empty target shape", "[geometry][circular_cut]") {
  const GeometryShape empty_target;
  const auto diameter = Quantity::length_mm(20.0, "plate.hole_diameter");
  REQUIRE(diameter);

  const CircularCutAdapter adapter;
  const auto cut = adapter.cut_circular_hole(empty_target, diameter.value());

  REQUIRE(cut.has_error());
  CHECK(cut.error().category() == ErrorCategory::Geometry);
  CHECK(cut.error().object_id() == "geometry.circular_cut");
  CHECK(cut.error().message() == "circular cut requires a non-empty target shape");
}
