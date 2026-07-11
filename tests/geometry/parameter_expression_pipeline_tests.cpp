#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

// A plate whose thickness is the expression `width / 10 - 2 mm`.
PartDocument make_expression_plate() {
  auto part = PartDocument::create(DocumentId("part.expression_plate"), "ExpressionPlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_expression_parameter(ParameterId("part.thickness"), "thickness",
                                                ParameterType::Length, "width / 10 - 2 mm"));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto sketch = Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(sketch.value()));

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(part.value().add_feature(feature.value()));
  return part.value();
}

} // namespace

TEST_CASE("A formula-driven length drives geometry recompute through the dependency graph",
          "[geometry][parameter-expression]") {
  PartDocument document = make_expression_plate();
  auto cache = ShapeCache::create(ShapeCacheId("cache.expression_plate"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  const RectangleExtrusionAdapter inspector;

  // Initial thickness: 120 / 10 - 2 = 10 mm.
  REQUIRE(executor.execute_document(document, cache.value()));
  CHECK(inspector.summarize(*cache.value().final_shape()).volume_mm3 ==
        Approx(120.0 * 80.0 * 10.0).margin(1.0e-6));

  // Changing width re-evaluates the thickness formula (160 / 10 - 2 = 14 mm)
  // and invalidates the extrude, so the incremental plan rebuilds the plate.
  document.mark_all_clean();
  auto new_width = Quantity::length_mm(160.0, "part.width");
  REQUIRE(new_width);
  REQUIRE(document.set_parameter_value(ParameterId("part.width"), new_width.value()));

  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK_FALSE(plan.value().steps().empty());
  REQUIRE(executor.execute_plan(document, plan.value(), cache.value()));
  CHECK(inspector.summarize(*cache.value().final_shape()).volume_mm3 ==
        Approx(160.0 * 80.0 * 14.0).margin(1.0e-6));
}
