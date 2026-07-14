#include "blcad/geometry/step_exporter.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/circular_cut_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/surface_adapter.hpp"

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

Body make_export_body(const std::string& id, BodyKind kind,
                      BodyVisibility visibility = BodyVisibility::Visible) {
  auto body = Body::create(BodyId(id), "Duplicate display name", kind, visibility);
  REQUIRE(body);
  return body.value();
}

GeometryShape make_surface_patch() {
  const std::vector<SweepPathSegment> boundary = {
      {SweepPathSegmentKind::Line, {0.0, 0.0, 0.0}, {}, {20.0, 0.0, 0.0}},
      {SweepPathSegmentKind::Line, {20.0, 0.0, 0.0}, {}, {20.0, 10.0, 0.0}},
      {SweepPathSegmentKind::Line, {20.0, 10.0, 0.0}, {}, {0.0, 10.0, 0.0}},
      {SweepPathSegmentKind::Line, {0.0, 10.0, 0.0}, {}, {0.0, 0.0, 0.0}}};
  auto surface =
      SurfaceAdapter{}.make_fill_surface(FeatureId("feature.surface"), {boundary});
  REQUIRE(surface);
  return surface.value();
}

ShapeCache make_part_export_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape-cache.part-step"));
  REQUIRE(cache);
  return cache.value();
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

TEST_CASE("StepExporter writes visible solid and surface Bodies in deterministic order",
          "[geometry][multi-body-step-export][integration][part-construction-mvp]") {
  auto document = PartDocument::create(DocumentId("part.multi-body"), "Multi Body");
  REQUIRE(document);
  REQUIRE(document.value().add_body(make_export_body("body/z solid", BodyKind::Solid)));
  REQUIRE(document.value().add_body(make_export_body("body%surface", BodyKind::Surface)));
  REQUIRE(document.value().add_body(
      make_export_body("body.hidden", BodyKind::Solid, BodyVisibility::Hidden)));

  ShapeCache cache = make_part_export_cache();
  REQUIRE(cache.store_body_shape(BodyId("body.hidden"), FeatureId("feature.hidden"),
                                 make_plate_with_hole()));
  REQUIRE(cache.store_body_shape(BodyId("body/z solid"), FeatureId("feature.solid"),
                                 make_plate_with_hole()));
  REQUIRE(cache.store_body_shape(BodyId("body%surface"), FeatureId("feature.surface"),
                                 make_surface_patch()));

  const auto before = serialize_part_document_to_json(document.value());
  REQUIRE(before);
  const std::size_t cached_body_count = cache.body_shape_count();
  const auto path =
      std::filesystem::temp_directory_path() / "blcad_multi_body_part.step";
  std::filesystem::remove(path);

  const auto exported =
      StepExporter{}.write_part_step(document.value(), cache, path.string());

  REQUIRE(exported);
  CHECK(exported.value().exported_body_count == 2U);
  CHECK(exported.value().exported_solid_body_count == 1U);
  CHECK(exported.value().exported_surface_body_count == 1U);
  REQUIRE(exported.value().bodies.size() == 2U);
  CHECK(exported.value().bodies[0].body_id.value() == "body%surface");
  CHECK(exported.value().bodies[0].exchange_name ==
        "blcad:body-definition:body%25surface");
  CHECK(exported.value().bodies[0].kind == BodyKind::Surface);
  CHECK(exported.value().bodies[1].body_id.value() == "body/z solid");
  CHECK(exported.value().bodies[1].exchange_name ==
        "blcad:body-definition:body%2Fz%20solid");
  CHECK(exported.value().bodies[1].kind == BodyKind::Solid);
  CHECK(exported.value().written_bytes == std::filesystem::file_size(path));

  std::ifstream step(path);
  const std::string content((std::istreambuf_iterator<char>(step)),
                            std::istreambuf_iterator<char>());
  CHECK(content.find("blcad:body-definition:body%25surface") != std::string::npos);
  CHECK(content.find("blcad:body-definition:body%2Fz%20solid") != std::string::npos);
  CHECK(content.find("body.hidden") == std::string::npos);

  const auto after = serialize_part_document_to_json(document.value());
  REQUIRE(after);
  CHECK(after.value() == before.value());
  CHECK(cache.body_shape_count() == cached_body_count);
  std::filesystem::remove(path);
}

TEST_CASE("StepExporter rejects unavailable or incompatible visible Body results",
          "[geometry][multi-body-step-export]") {
  auto missing = PartDocument::create(DocumentId("part.missing"), "Missing");
  REQUIRE(missing);
  REQUIRE(missing.value().add_body(make_export_body("body.missing", BodyKind::Solid)));
  ShapeCache empty_cache = make_part_export_cache();
  const auto path = std::filesystem::temp_directory_path() / "blcad_invalid_part.step";
  std::filesystem::remove(path);

  const auto missing_result =
      StepExporter{}.write_part_step(missing.value(), empty_cache, path.string());
  REQUIRE(missing_result.has_error());
  CHECK(missing_result.error().category() == ErrorCategory::Export);
  CHECK(missing_result.error().message() ==
        "visible body 'body.missing' has no cached result");
  CHECK_FALSE(std::filesystem::exists(path));

  auto incompatible = PartDocument::create(DocumentId("part.incompatible"), "Incompatible");
  REQUIRE(incompatible);
  REQUIRE(incompatible.value().add_body(
      make_export_body("body.surface", BodyKind::Surface)));
  ShapeCache solid_cache = make_part_export_cache();
  REQUIRE(solid_cache.store_body_shape(BodyId("body.surface"), FeatureId("feature.solid"),
                                       make_plate_with_hole()));
  const auto incompatible_result = StepExporter{}.write_part_step(
      incompatible.value(), solid_cache, path.string());
  REQUIRE(incompatible_result.has_error());
  CHECK(incompatible_result.error().message() ==
        "visible surface body 'body.surface' must contain face topology without solid topology");
  CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("StepExporter requires a visible Body and a non-empty Part path",
          "[geometry][multi-body-step-export]") {
  auto document = PartDocument::create(DocumentId("part.hidden"), "Hidden");
  REQUIRE(document);
  REQUIRE(document.value().add_body(
      make_export_body("body.hidden", BodyKind::Solid, BodyVisibility::Hidden)));
  ShapeCache cache = make_part_export_cache();
  const auto path = std::filesystem::temp_directory_path() / "blcad_no_visible_body.step";
  std::filesystem::remove(path);

  const auto no_visible =
      StepExporter{}.write_part_step(document.value(), cache, path.string());
  REQUIRE(no_visible.has_error());
  CHECK(no_visible.error().message() ==
        "part step export requires at least one visible body");
  CHECK_FALSE(std::filesystem::exists(path));

  const auto empty_path = StepExporter{}.write_part_step(document.value(), cache, "");
  REQUIRE(empty_path.has_error());
  CHECK(empty_path.error().message() == "part step export path must not be empty");
}
