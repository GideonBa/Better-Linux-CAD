#include "blcad/geometry/step_exporter.hpp"

#include "blcad/geometry/circular_cut_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace blcad;
using namespace blcad::geometry;

namespace {

GeometryShape make_plate_with_hole() {
  const auto width = Quantity::length_mm(120.0, "plate.width");
  const auto height = Quantity::length_mm(80.0, "plate.height");
  const auto thickness = Quantity::length_mm(8.0, "plate.thickness");
  const auto diameter = Quantity::length_mm(20.0, "plate.hole_diameter");
  REQUIRE(width);
  REQUIRE(height);
  REQUIRE(thickness);
  REQUIRE(diameter);

  const RectangleExtrusionAdapter extrusion;
  auto plate = extrusion.make_extruded_rectangle(width.value(), height.value(), thickness.value());
  REQUIRE(plate);

  const CircularCutAdapter cut;
  auto cut_plate = cut.cut_circular_hole(plate.value(), diameter.value());
  REQUIRE(cut_plate);

  return cut_plate.value();
}

} // namespace

TEST_CASE("StepExporter writes a valid non-empty STEP file for a cut plate",
          "[geometry][step_export]") {
  const GeometryShape shape = make_plate_with_hole();

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "blcad_plate_with_hole.step";
  std::filesystem::remove(path);

  const StepExporter exporter;
  const auto written = exporter.write_step(shape, path.string());

  REQUIRE(written);
  CHECK(written.value() > 0U);
  REQUIRE(std::filesystem::exists(path));
  CHECK(std::filesystem::file_size(path) == written.value());

  std::ifstream file(path);
  std::string first_line;
  std::getline(file, first_line);
  CHECK(first_line.rfind("ISO-10303-21", 0) == 0);

  std::filesystem::remove(path);
}

TEST_CASE("StepExporter rejects an empty shape", "[geometry][step_export]") {
  const GeometryShape empty_shape;
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "blcad_should_not_exist.step";
  std::filesystem::remove(path);

  const StepExporter exporter;
  const auto written = exporter.write_step(empty_shape, path.string());

  REQUIRE(written.has_error());
  CHECK(written.error().category() == ErrorCategory::Export);
  CHECK(written.error().object_id() == "geometry.step_exporter");
  CHECK(written.error().message() == "step export requires a non-empty shape");
  CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("StepExporter rejects an empty path", "[geometry][step_export]") {
  const GeometryShape shape = make_plate_with_hole();

  const StepExporter exporter;
  const auto written = exporter.write_step(shape, "");

  REQUIRE(written.has_error());
  CHECK(written.error().category() == ErrorCategory::Export);
  CHECK(written.error().message() == "step export path must not be empty");
}
