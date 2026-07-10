#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

SketchReferenceTarget line_target(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
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

void add_dimensioned_rectangle_intent(Sketch& sketch) {
  add_line(sketch, "line.bottom", Point2{0.0, 0.0}, Point2{1.0, 0.0});
  add_line(sketch, "line.right", Point2{1.0, 0.0}, Point2{1.0, 1.0});
  add_line(sketch, "line.top", Point2{1.0, 1.0}, Point2{0.0, 1.0});
  add_line(sketch, "line.left", Point2{0.0, 1.0}, Point2{0.0, 0.0});

  auto horizontal_bottom = SketchGeometricConstraint::create_horizontal(
      SketchConstraintId("constraint.bottom.horizontal"), line_target("line.bottom"));
  auto horizontal_top = SketchGeometricConstraint::create_horizontal(
      SketchConstraintId("constraint.top.horizontal"), line_target("line.top"));
  auto vertical_right = SketchGeometricConstraint::create_vertical(
      SketchConstraintId("constraint.right.vertical"), line_target("line.right"));
  auto vertical_left = SketchGeometricConstraint::create_vertical(
      SketchConstraintId("constraint.left.vertical"), line_target("line.left"));
  auto parallel_width = SketchGeometricConstraint::create_parallel(
      SketchConstraintId("constraint.width.parallel"), line_target("line.bottom"),
      line_target("line.top"));
  auto perpendicular_corner = SketchGeometricConstraint::create_perpendicular(
      SketchConstraintId("constraint.corner.perpendicular"), line_target("line.bottom"),
      line_target("line.right"));
  auto equal_width = SketchGeometricConstraint::create_equal_length(
      SketchConstraintId("constraint.width.equal"), line_target("line.bottom"),
      line_target("line.top"));
  REQUIRE(horizontal_bottom);
  REQUIRE(horizontal_top);
  REQUIRE(vertical_right);
  REQUIRE(vertical_left);
  REQUIRE(parallel_width);
  REQUIRE(perpendicular_corner);
  REQUIRE(equal_width);
  REQUIRE(sketch.add_constraint(horizontal_bottom.value()));
  REQUIRE(sketch.add_constraint(horizontal_top.value()));
  REQUIRE(sketch.add_constraint(vertical_right.value()));
  REQUIRE(sketch.add_constraint(vertical_left.value()));
  REQUIRE(sketch.add_constraint(parallel_width.value()));
  REQUIRE(sketch.add_constraint(perpendicular_corner.value()));
  REQUIRE(sketch.add_constraint(equal_width.value()));

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
  auto document = PartDocument::create(DocumentId("part.dimensioned"), "Dimensioned");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  auto sketch =
      Sketch::create(SketchId("sketch.dimensioned"), "Dimensioned", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_dimensioned_rectangle_intent(sketch.value());
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.dimensioned"), "Dimensioned",
                                       SketchId("sketch.dimensioned"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

bool contains(const std::vector<std::string>& values, const char* needle) {
  return std::find(values.begin(), values.end(), std::string(needle)) != values.end();
}

} // namespace

TEST_CASE("Sketch constraints and driving dimensions roundtrip through JSON",
          "[core][json][sketch-dimension]") {
  PartDocument document = make_dimensioned_document();
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("geometric_constraints") != std::string::npos);
  CHECK(serialized.value().find("driving_dimensions") != std::string::npos);
  CHECK(serialized.value().find("horizontal_distance") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const Sketch* sketch = restored.value().find_sketch(SketchId("sketch.dimensioned"));
  REQUIRE(sketch != nullptr);
  CHECK(sketch->geometric_constraints().size() == 7U);
  CHECK(sketch->driving_dimensions().size() == 4U);
  REQUIRE(sketch->find_driving_dimension(SketchDimensionId("dim.width.bottom")) != nullptr);
}

TEST_CASE("Driving dimension parameters dirty sketches and dependent features",
          "[core][dependency][sketch-dimension]") {
  PartDocument document = make_dimensioned_document();
  auto affected = document.mark_parameter_changed(ParameterId("part.width"));
  REQUIRE(affected);
  CHECK(contains(affected.value(), "part.width"));
  CHECK(contains(affected.value(), "sketch.dimensioned"));
  CHECK(contains(affected.value(), "feature.dimensioned"));
}
