#include "blcad/core/assembly_document.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace blcad;
using namespace blcad::geometry;

namespace {

constexpr double k_pi = 3.14159265358979323846;

Parameter make_length_parameter(const char* id, const char* name, double value_mm,
                                ParameterScope scope = ParameterScope::Part) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value(), scope);
  REQUIRE(parameter);
  return parameter.value();
}

Parameter make_count_parameter(const char* id, const char* name, double value,
                               ParameterScope scope = ParameterScope::Part) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), name, quantity.value(), scope);
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_bolt_circle_part(const char* document_id, const char* document_name) {
  auto document = PartDocument::create(DocumentId(document_id), document_name);
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_circle_radius", "bolt_circle_radius", 25.0)));
  REQUIRE(document.value().add_parameter(make_count_parameter("part.bolt_count", "bolt_count", 4)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_hole_diameter", "bolt_hole_diameter", 6.6)));

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

  auto pattern_sketch =
      Sketch::create(SketchId("sketch.bolt_circle"), "Sketch_BoltCircle", DatumPlaneId("datum.xy"));
  REQUIRE(pattern_sketch);
  auto pattern = CircularHolePattern::create(
      ProfileId("pattern.bolt_circle"), ParameterId("part.bolt_circle_radius"),
      ParameterId("part.bolt_count"), ParameterId("part.bolt_hole_diameter"));
  REQUIRE(pattern);
  REQUIRE(pattern_sketch.value().add_profile(pattern.value()));
  REQUIRE(document.value().add_sketch(pattern_sketch.value()));

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

AssemblyDocument make_bound_flange_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.flange"), "FlangeAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_parameter(make_length_parameter(
      "assembly.bolt_circle_radius", "bolt_circle_radius", 30.0, ParameterScope::Assembly)));
  REQUIRE(assembly.value().add_parameter(
      make_count_parameter("assembly.bolt_count", "bolt_count", 6, ParameterScope::Assembly)));

  for (const char* part_id : {"part.base_plate", "part.cover_plate"}) {
    REQUIRE(assembly.value().add_member_part(DocumentId(part_id)));
    auto radius_binding = ParameterBinding::create(
        ParameterBindingId(std::string("binding.") + part_id + ".radius"), DocumentId(part_id),
        ParameterId("part.bolt_circle_radius"), ParameterId("assembly.bolt_circle_radius"));
    REQUIRE(radius_binding);
    REQUIRE(assembly.value().add_binding(radius_binding.value()));
    auto count_binding = ParameterBinding::create(
        ParameterBindingId(std::string("binding.") + part_id + ".count"), DocumentId(part_id),
        ParameterId("part.bolt_count"), ParameterId("assembly.bolt_count"));
    REQUIRE(count_binding);
    REQUIRE(assembly.value().add_binding(count_binding.value()));
  }

  return assembly.value();
}

double expected_plate_volume_mm3(double hole_count) {
  const double hole_radius = 6.6 / 2.0;
  return 120.0 * 80.0 * 8.0 - hole_count * k_pi * hole_radius * hole_radius * 8.0;
}

double recompute_and_measure(const GeometryRecomputeExecutor& executor,
                             const PartDocument& document, ShapeCache& cache) {
  const auto summary = executor.execute_document(document, cache);
  REQUIRE(summary);
  REQUIRE(cache.has_final_shape());
  const RectangleExtrusionAdapter inspector;
  return inspector.summarize(*cache.final_shape()).volume_mm3;
}

} // namespace

TEST_CASE("A shared assembly bolt count drives both member parts",
          "[geometry][assembly-parameters]") {
  AssemblyDocument assembly = make_bound_flange_assembly();
  PartDocument base_plate = make_bolt_circle_part("part.base_plate", "BasePlate");
  PartDocument cover_plate = make_bolt_circle_part("part.cover_plate", "CoverPlate");

  auto base_cache = ShapeCache::create(ShapeCacheId("cache.base_plate"));
  auto cover_cache = ShapeCache::create(ShapeCacheId("cache.cover_plate"));
  REQUIRE(base_cache);
  REQUIRE(cover_cache);
  const GeometryRecomputeExecutor executor;

  // Initial propagation: both parts follow the assembly values (radius 30, count 6).
  REQUIRE(assembly.apply_bindings_to(base_plate));
  REQUIRE(assembly.apply_bindings_to(cover_plate));

  const double base_volume = recompute_and_measure(executor, base_plate, base_cache.value());
  const double cover_volume = recompute_and_measure(executor, cover_plate, cover_cache.value());
  CHECK(std::abs(base_volume - expected_plate_volume_mm3(6.0)) < 1.0);
  CHECK(std::abs(cover_volume - expected_plate_volume_mm3(6.0)) < 1.0);

  // Change bolt_count 6 -> 8 on the assembly and re-propagate into both parts.
  auto new_count = Quantity::count(8.0, "assembly.bolt_count");
  REQUIRE(new_count);
  REQUIRE(assembly.set_parameter_value(ParameterId("assembly.bolt_count"), new_count.value()));

  for (PartDocument* part : {&base_plate, &cover_plate}) {
    part->mark_all_clean();
    auto applied = assembly.apply_bindings_to(*part);
    REQUIRE(applied);
    auto plan = part->create_recompute_plan();
    REQUIRE(plan);
    CHECK_FALSE(plan.value().steps().empty());
  }

  REQUIRE(executor.execute_plan(base_plate, base_plate.create_recompute_plan().value(),
                                base_cache.value()));
  REQUIRE(executor.execute_plan(cover_plate, cover_plate.create_recompute_plan().value(),
                                cover_cache.value()));

  const RectangleExtrusionAdapter inspector;
  const double base_after = inspector.summarize(*base_cache.value().final_shape()).volume_mm3;
  const double cover_after = inspector.summarize(*cover_cache.value().final_shape()).volume_mm3;
  CHECK(std::abs(base_after - expected_plate_volume_mm3(8.0)) < 1.0);
  CHECK(std::abs(cover_after - expected_plate_volume_mm3(8.0)) < 1.0);
}
