#include "blcad/geometry/dimension_driven_profile_resolver.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

SketchReferenceTarget line_start(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment_start(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
}

SketchReferenceTarget line_end(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment_end(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
}

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

void add_dimensioned_rectangle(Sketch& sketch) {
  add_line(sketch, "line.bottom", Point2{0.0, 0.0}, Point2{1.0, 0.0});
  add_line(sketch, "line.right", Point2{1.0, 0.0}, Point2{1.0, 1.0});
  add_line(sketch, "line.top", Point2{1.0, 1.0}, Point2{0.0, 1.0});
  add_line(sketch, "line.left", Point2{0.0, 1.0}, Point2{0.0, 0.0});

  auto dim_bottom = SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("dim.width.bottom"), line_start("line.bottom"), line_end("line.bottom"),
      ParameterId("part.width"));
  auto dim_right = SketchDrivingDimension::create_vertical_distance(
      SketchDimensionId("dim.height.right"), line_start("line.right"), line_end("line.right"),
      ParameterId("part.height"));
  auto dim_top = SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("dim.width.top"), line_start("line.top"), line_end("line.top"),
      ParameterId("part.width"));
  auto dim_left = SketchDrivingDimension::create_vertical_distance(
      SketchDimensionId("dim.height.left"), line_start("line.left"), line_end("line.left"),
      ParameterId("part.height"));
  REQUIRE(dim_bottom);
  REQUIRE(dim_right);
  REQUIRE(dim_top);
  REQUIRE(dim_left);
  REQUIRE(sketch.add_dimension(dim_bottom.value()));
  REQUIRE(sketch.add_dimension(dim_right.value()));
  REQUIRE(sketch.add_dimension(dim_top.value()));
  REQUIRE(sketch.add_dimension(dim_left.value()));

  auto profile = ClosedProfile::create(ProfileId("profile.dimensioned"),
                                       {SketchEntityId("line.bottom"), SketchEntityId("line.right"),
                                        SketchEntityId("line.top"), SketchEntityId("line.left")});
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
}

PartDocument make_dimensioned_document() {
  auto document = PartDocument::create(DocumentId("part.dimensioned.geometry"),
                                       "DimensionedGeometry");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  auto sketch = Sketch::create(SketchId("sketch.dimensioned"), "Dimensioned", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_dimensioned_rectangle(sketch.value());
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.dimensioned"), "Dimensioned",
                                                  SketchId("sketch.dimensioned"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

} // namespace

TEST_CASE("DimensionDrivenProfileResolver resolves dimensioned closed profile vertices",
          "[geometry][sketch-dimension]") {
  PartDocument document = make_dimensioned_document();
  const Sketch* sketch = document.find_sketch(SketchId("sketch.dimensioned"));
  REQUIRE(sketch != nullptr);
  const ClosedProfile* profile = sketch->find_closed_profile(ProfileId("profile.dimensioned"));
  REQUIRE(profile != nullptr);

  const DimensionDrivenProfileResolver resolver;
  auto vertices = resolver.resolve_closed_profile_vertices(document, *sketch, *profile);
  REQUIRE(vertices);
  REQUIRE(vertices.value().size() == 4U);
  CHECK(vertices.value()[0].x == Catch::Approx(0.0));
  CHECK(vertices.value()[0].y == Catch::Approx(0.0));
  CHECK(vertices.value()[1].x == Catch::Approx(40.0));
  CHECK(vertices.value()[1].y == Catch::Approx(0.0));
  CHECK(vertices.value()[2].x == Catch::Approx(40.0));
  CHECK(vertices.value()[2].y == Catch::Approx(20.0));
  CHECK(vertices.value()[3].x == Catch::Approx(0.0));
  CHECK(vertices.value()[3].y == Catch::Approx(20.0));

  auto width = Quantity::length_mm(60.0, "part.width");
  REQUIRE(width);
  auto affected = document.set_parameter_value(ParameterId("part.width"), width.value());
  REQUIRE(affected);

  auto updated_vertices = resolver.resolve_closed_profile_vertices(document, *sketch, *profile);
  REQUIRE(updated_vertices);
  CHECK(updated_vertices.value()[1].x == Catch::Approx(60.0));
  CHECK(updated_vertices.value()[2].x == Catch::Approx(60.0));
}

TEST_CASE("Geometry recomputes additive extrude from dimension-driven profile",
          "[geometry][sketch-dimension]") {
  PartDocument document = make_dimensioned_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.dimensioned"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.dimensioned"));
}
