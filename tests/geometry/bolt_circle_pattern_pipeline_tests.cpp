#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace blcad;
using namespace blcad::geometry;

namespace {

constexpr double k_pi = 3.14159265358979323846;

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Parameter make_count_parameter(const char* id, const char* name, double value) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Sketch make_base_rectangle_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  return sketch.value();
}

Sketch make_bolt_circle_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.bolt_circle"), "Sketch_BoltCircle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto pattern = CircularHolePattern::create(
      ProfileId("pattern.bolt_circle"), ParameterId("part.bolt_circle_radius"),
      ParameterId("part.bolt_count"), ParameterId("part.bolt_hole_diameter"));
  REQUIRE(pattern);
  REQUIRE(sketch.value().add_profile(pattern.value()));
  return sketch.value();
}

PartDocument make_bolt_circle_document(double bolt_circle_radius_mm, double bolt_count) {
  auto document = PartDocument::create(DocumentId("part.flange_plate"), "FlangePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter(
      "part.bolt_circle_radius", "bolt_circle_radius", bolt_circle_radius_mm)));
  REQUIRE(document.value().add_parameter(
      make_count_parameter("part.bolt_count", "bolt_count", bolt_count)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_hole_diameter", "bolt_hole_diameter", 6.6)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_base_rectangle_sketch()));
  REQUIRE(document.value().add_sketch(make_bolt_circle_sketch()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.bolt_circle_cut"),
                                                 "BoltCircleCut", SketchId("sketch.bolt_circle"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

ShapeCache make_shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("cache.bolt_circle"));
  REQUIRE(cache);
  return cache.value();
}

double single_hole_volume_mm3(double hole_diameter_mm, double thickness_mm) {
  const double radius = hole_diameter_mm / 2.0;
  return k_pi * radius * radius * thickness_mm;
}

} // namespace

TEST_CASE("Bolt circle pattern cuts the parametric hole count", "[geometry][bolt-circle]") {
  const PartDocument document = make_bolt_circle_document(30.0, 6.0);
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  REQUIRE(cache.has_final_shape());

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);

  const double plate_volume = 120.0 * 80.0 * 8.0;
  const double expected_volume = plate_volume - 6.0 * single_hole_volume_mm3(6.6, 8.0);
  CHECK(std::abs(shape.volume_mm3 - expected_volume) < 1.0);
}

TEST_CASE("Changing the bolt count recomputes the bolt circle incrementally",
          "[geometry][bolt-circle]") {
  PartDocument document = make_bolt_circle_document(30.0, 6.0);
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  REQUIRE(executor.execute_document(document, cache));
  const RectangleExtrusionAdapter inspector;
  const double six_hole_volume = inspector.summarize(*cache.final_shape()).volume_mm3;

  document.mark_all_clean();
  auto new_count = Quantity::count(12.0, "part.bolt_count");
  REQUIRE(new_count);
  REQUIRE(document.set_parameter_value(ParameterId("part.bolt_count"), new_count.value()));

  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(executor.execute_plan(document, plan.value(), cache));

  const double twelve_hole_volume = inspector.summarize(*cache.final_shape()).volume_mm3;
  const double plate_volume = 120.0 * 80.0 * 8.0;
  CHECK(twelve_hole_volume < six_hole_volume);
  CHECK(std::abs(plate_volume - 12.0 * single_hole_volume_mm3(6.6, 8.0) - twelve_hole_volume) <
        1.0);
}

TEST_CASE("Changing the bolt circle radius recomputes hole positions", "[geometry][bolt-circle]") {
  PartDocument document = make_bolt_circle_document(20.0, 6.0);
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  REQUIRE(executor.execute_document(document, cache));
  const RectangleExtrusionAdapter inspector;
  const double small_radius_volume = inspector.summarize(*cache.final_shape()).volume_mm3;

  document.mark_all_clean();
  auto new_radius = Quantity::length_mm(30.0, "part.bolt_circle_radius");
  REQUIRE(new_radius);
  REQUIRE(document.set_parameter_value(ParameterId("part.bolt_circle_radius"), new_radius.value()));

  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(executor.execute_plan(document, plan.value(), cache));

  // The hole volume stays the same; only positions change.
  const double large_radius_volume = inspector.summarize(*cache.final_shape()).volume_mm3;
  CHECK(std::abs(large_radius_volume - small_radius_volume) < 1.0);
}

TEST_CASE("Bolt circle pattern rejects holes outside the resolved bounds",
          "[geometry][bolt-circle]") {
  auto document = PartDocument::create(DocumentId("part.flange_plate"), "FlangePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  // radius 45 + hole radius 3.3 exceeds the 80 mm plate height (half height 40).
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_circle_radius", "bolt_circle_radius", 45.0)));
  REQUIRE(
      document.value().add_parameter(make_count_parameter("part.bolt_count", "bolt_count", 6.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_hole_diameter", "bolt_hole_diameter", 6.6)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_base_rectangle_sketch()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto face_reference =
      SemanticFaceReference::create(FeatureId("feature.base_extrude"), SemanticFace::Top);
  REQUIRE(face_reference);
  auto workplane = DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("workplane.base_top"), "BaseTopWorkplane", face_reference.value());
  REQUIRE(workplane);
  REQUIRE(document.value().add_derived_workplane(workplane.value()));

  auto sketch = Sketch::create(SketchId("sketch.bolt_circle"), "Sketch_BoltCircle",
                               DatumPlaneId("workplane.base_top"));
  REQUIRE(sketch);
  auto pattern = CircularHolePattern::create(
      ProfileId("pattern.bolt_circle"), ParameterId("part.bolt_circle_radius"),
      ParameterId("part.bolt_count"), ParameterId("part.bolt_hole_diameter"));
  REQUIRE(pattern);
  REQUIRE(sketch.value().add_profile(pattern.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.bolt_circle_cut"),
                                                 "BoltCircleCut", SketchId("sketch.bolt_circle"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document.value(), cache);
  REQUIRE(summary.has_error());
  CHECK(summary.error().category() == ErrorCategory::Validation);
  CHECK(summary.error().message() ==
        "circle profile must lie fully inside resolved workplane bounds");
}
