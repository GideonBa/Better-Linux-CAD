#include "blcad/geometry/recompute_executor.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/step_exporter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
}

// Build the MVP-1 reference part from the specification: a 120 x 80 x 8 mm
// plate with a centered 20 mm through-hole.
PartDocument make_reference_part_document() {
  auto document = PartDocument::create(DocumentId("part.rectangular_plate"), "RectangularPlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));

  auto hole_sketch =
      Sketch::create(SketchId("sketch.hole"), "Sketch_CenterHole", DatumPlaneId("datum.xy"));
  REQUIRE(hole_sketch);
  auto circle =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.value().add_sketch(hole_sketch.value()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                 "CenterHoleCut", SketchId("sketch.hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

ShapeCache make_shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));
  REQUIRE(cache);

  return cache.value();
}

} // namespace

TEST_CASE("MVP-1 reference part recomputes the whole document and exports a STEP file",
          "[geometry][pipeline]") {
  const PartDocument document = make_reference_part_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  REQUIRE(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.center_hole_cut");

  const RectangleExtrusionAdapter inspector;

  const GeometryShape* base_shape = cache.find_feature_shape(FeatureId("feature.base_extrude"));
  REQUIRE(base_shape != nullptr);
  const double base_volume = inspector.summarize(*base_shape).volume_mm3;

  REQUIRE(cache.final_shape() != nullptr);
  const ShapeSummary final_summary = inspector.summarize(*cache.final_shape());
  CHECK(final_summary.solid_count == 1);
  CHECK(final_summary.volume_mm3 < base_volume);

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "blcad_reference_part.step";
  std::filesystem::remove(path);

  const StepExporter exporter;
  const auto written = exporter.write_step(*cache.final_shape(), path.string());

  REQUIRE(written);
  CHECK(written.value() > 0U);
  REQUIRE(std::filesystem::exists(path));

  std::ifstream file(path);
  std::string first_line;
  std::getline(file, first_line);
  CHECK(first_line.rfind("ISO-10303-21", 0) == 0);

  std::filesystem::remove(path);
}

TEST_CASE("MVP-1 reference part recomputes a larger hole incrementally", "[geometry][pipeline]") {
  auto document = make_reference_part_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;
  const RectangleExtrusionAdapter inspector;

  REQUIRE(executor.execute_document(document, cache));
  REQUIRE(cache.final_shape() != nullptr);
  const double small_hole_volume = inspector.summarize(*cache.final_shape()).volume_mm3;

  // After a full recompute, the caller marks the document as clean.
  document.mark_all_clean();
  auto clean_plan = document.create_recompute_plan();
  REQUIRE(clean_plan);
  CHECK(clean_plan.value().step_count() == 0);

  // A real value change creates an incremental plan that contains only the
  // affected cut. The base body remains clean and stays in the cache.
  const auto larger_diameter = Quantity::length_mm(40.0, "part.hole_diameter");
  REQUIRE(larger_diameter);
  REQUIRE(document.set_parameter_value(ParameterId("part.hole_diameter"), larger_diameter.value()));

  auto incremental_plan = document.create_recompute_plan();
  REQUIRE(incremental_plan);
  CHECK(incremental_plan.value().contains("feature.center_hole_cut"));
  CHECK_FALSE(incremental_plan.value().contains("feature.base_extrude"));

  const auto incremental_summary = executor.execute_plan(document, incremental_plan.value(), cache);
  REQUIRE(incremental_summary);
  CHECK(incremental_summary.value().executed_feature_count == 1);
  CHECK(cache.final_feature_id().value() == "feature.center_hole_cut");

  // A larger hole removes more material, so the final volume becomes smaller.
  REQUIRE(cache.final_shape() != nullptr);
  const double large_hole_volume = inspector.summarize(*cache.final_shape()).volume_mm3;
  CHECK(large_hole_volume < small_hole_volume);

  // The clean state clears the recompute plan again.
  document.mark_all_clean();
  auto plan_after_clean = document.create_recompute_plan();
  REQUIRE(plan_after_clean);
  CHECK(plan_after_clean.value().step_count() == 0);
}

TEST_CASE("MVP-1 reference part can be restored from JSON and recomputed", "[geometry][pipeline]") {
  const PartDocument original = make_reference_part_document();
  const auto serialized = serialize_part_document_to_json(original);
  REQUIRE(serialized);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);

  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;
  const auto summary = executor.execute_document(restored.value(), cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  REQUIRE(cache.final_shape() != nullptr);
  CHECK(cache.final_feature_id().value() == "feature.center_hole_cut");

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary final_summary = inspector.summarize(*cache.final_shape());
  CHECK(final_summary.solid_count == 1);

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "blcad_reference_part_from_json.step";
  std::filesystem::remove(path);

  const StepExporter exporter;
  const auto written = exporter.write_step(*cache.final_shape(), path.string());

  REQUIRE(written);
  CHECK(written.value() > 0U);
  REQUIRE(std::filesystem::exists(path));

  std::filesystem::remove(path);
}
