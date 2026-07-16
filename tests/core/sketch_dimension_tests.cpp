#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"
#include "blcad/core/sketch_dimension_catalog_system.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
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

Parameter make_angle_parameter(const char* id, const char* name, double value_deg) {
  auto quantity = Quantity::angle_deg(value_deg, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_angle(ParameterId(id), name, quantity.value());
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
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.dimensioned"), "Dimensioned", SketchId("sketch.dimensioned"),
      ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument make_block115_document() {
  auto document = PartDocument::create(DocumentId("part.block115"), "Block115");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("dim.length", "dim_length", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("dim.arc", "dim_arc", 15.7079632679)));
  REQUIRE(document.value().add_parameter(make_angle_parameter("dim.angle", "dim_angle", 90.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("base.length", "base_length", 10.0)));
  REQUIRE(document.value().add_expression_parameter(
      ParameterId("expr.length"), "expr_length", ParameterType::Length, "base_length * 2"));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  auto sketch = Sketch::create(SketchId("sketch.block115"), "Block115", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), {0.0, 0.0}, {0.0, 10.0}).value()));
  REQUIRE(sketch.value().add_entity(ArcSegment::create_three_point(
      SketchEntityId("arc.a"), {0.0, 0.0}, {5.0, 5.0}, {10.0, 0.0}).value()));
  REQUIRE(document.value().add_sketch(std::move(sketch.value())));
  return document.value();
}

SketchConstraintIntentTarget entity_target(const char* id) {
  auto target = SketchConstraintIntentTarget::entity(id);
  REQUIRE(target);
  return target.value();
}

SketchConstraintIntentTarget point_target(const SketchPointId& id) {
  auto target = SketchConstraintIntentTarget::point(id);
  REQUIRE(target);
  return target.value();
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

TEST_CASE("Block 115 dimension catalog validates all families and sidecar ordering",
          "[core][sketch-dimensions]") {
  PartDocument document = make_block115_document();
  const Sketch* sketch = document.find_sketch(SketchId("sketch.block115"));
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  const auto* line = topology.value().find_entity("entity/line.a");
  REQUIRE(line != nullptr);

  auto length = SketchDimensionIntent::create(
      SketchDimensionId("dimension.z.length"), SketchDimensionKind::Length,
      SketchDimensionMode::Driving, {entity_target("entity/line.a")},
      ParameterId("dim.length"));
  auto reference = SketchDimensionIntent::create(
      SketchDimensionId("dimension.a.reference"), SketchDimensionKind::PointToPointDistance,
      SketchDimensionMode::Reference,
      {point_target(line->points()[0]), point_target(line->points()[1])});
  REQUIRE(length);
  REQUIRE(reference);
  auto catalog = SketchDimensionCatalog::create(
      document.id(), sketch->id(), {length.value(), reference.value()});
  REQUIRE(catalog);
  CHECK(catalog.value().dimensions().front().id().value() == "dimension.a.reference");

  auto serialized = serialize_sketch_dimension_catalog_to_json(catalog.value());
  REQUIRE(serialized);
  CHECK(serialized.value().find("blcad.sketch_dimensions.mvp8") != std::string::npos);
  CHECK(serialized.value().find("point_to_point_distance") != std::string::npos);
  CHECK(serialized.value().find("reference") != std::string::npos);
  auto restored = deserialize_sketch_dimension_catalog_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value() == catalog.value());

  CHECK_FALSE(SketchDimensionIntent::create(
      SketchDimensionId("dimension.invalid"), SketchDimensionKind::Length,
      SketchDimensionMode::Reference, {entity_target("entity/line.a")},
      ParameterId("dim.length")));
}

TEST_CASE("Block 115 driving length solves while reference distance only measures",
          "[core][sketch-dimensions]") {
  PartDocument document = make_block115_document();
  const Sketch* sketch = document.find_sketch(SketchId("sketch.block115"));
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  const auto* line = topology.value().find_entity("entity/line.a");
  REQUIRE(line != nullptr);
  auto driving = SketchDimensionIntent::create(
      SketchDimensionId("dimension.length"), SketchDimensionKind::Length,
      SketchDimensionMode::Driving, {entity_target("entity/line.a")},
      ParameterId("dim.length"));
  auto reference = SketchDimensionIntent::create(
      SketchDimensionId("dimension.reference"), SketchDimensionKind::PointToPointDistance,
      SketchDimensionMode::Reference,
      {point_target(line->points()[0]), point_target(line->points()[1])});
  REQUIRE(driving);
  REQUIRE(reference);
  auto dimensions = SketchDimensionCatalog::create(
      document.id(), sketch->id(), {driving.value(), reference.value()});
  auto constraints = SketchConstraintCatalog::create(document.id(), sketch->id());
  REQUIRE(dimensions);
  REQUIRE(constraints);

  auto solved = SketchDimensionCatalogSystemBuilder::solve(
      document, constraints.value(), dimensions.value());
  REQUIRE(solved);
  CHECK(solved.value().accepted);
  REQUIRE(solved.value().solved_sketch.has_value());
  const auto* solved_line = solved.value().solved_sketch->find_line_segment(
      SketchEntityId("line.a"));
  REQUIRE(solved_line != nullptr);
  CHECK(std::hypot(solved_line->end().x - solved_line->start().x,
                   solved_line->end().y - solved_line->start().y) ==
        Catch::Approx(20.0).margin(1.0e-6));
  auto measured = SketchDimensionMeasurementEvaluator::measure(
      solved.value().solve.topology, reference.value());
  REQUIRE(measured);
  CHECK(measured.value().value.millimeters() == Catch::Approx(20.0).margin(1.0e-6));
  CHECK(sketch->find_line_segment(SketchEntityId("line.a"))->end() == Point2{10.0, 0.0});
}

TEST_CASE("Block 115 arc length uses calibrated solve and validates actual measurement",
          "[core][sketch-dimensions]") {
  PartDocument document = make_block115_document();
  const Sketch* sketch = document.find_sketch(SketchId("sketch.block115"));
  REQUIRE(sketch != nullptr);
  auto arc_length = SketchDimensionIntent::create(
      SketchDimensionId("dimension.arc_length"), SketchDimensionKind::ArcLength,
      SketchDimensionMode::Driving, {entity_target("entity/arc.a")},
      ParameterId("dim.arc"));
  REQUIRE(arc_length);
  auto dimensions = SketchDimensionCatalog::create(
      document.id(), sketch->id(), {arc_length.value()});
  auto constraints = SketchConstraintCatalog::create(document.id(), sketch->id());
  REQUIRE(dimensions);
  REQUIRE(constraints);
  auto solved = SketchDimensionCatalogSystemBuilder::solve(
      document, constraints.value(), dimensions.value());
  REQUIRE(solved);
  CHECK(solved.value().accepted);
  auto measured = SketchDimensionMeasurementEvaluator::measure(
      solved.value().solve.topology, arc_length.value());
  REQUIRE(measured);
  CHECK(measured.value().value.millimeters() ==
        Catch::Approx(15.7079632679).margin(1.0e-6));
}

TEST_CASE("Block 115 literal and expression edits preserve typed parameter authority",
          "[core][sketch-dimensions][integration][sketch-expression-edit]") {
  PartDocument document = make_block115_document();
  auto direct = SketchDimensionIntent::create(
      SketchDimensionId("dimension.direct"), SketchDimensionKind::Length,
      SketchDimensionMode::Driving, {entity_target("entity/line.a")},
      ParameterId("dim.length"));
  auto expression = SketchDimensionIntent::create(
      SketchDimensionId("dimension.expression"), SketchDimensionKind::Length,
      SketchDimensionMode::Driving, {entity_target("entity/line.b")},
      ParameterId("expr.length"));
  REQUIRE(direct);
  REQUIRE(expression);
  auto catalog = SketchDimensionCatalog::create(
      document.id(), SketchId("sketch.block115"), {direct.value(), expression.value()});
  REQUIRE(catalog);

  auto literal = SketchDimensionEditService::set_literal(
      document, catalog.value(), direct.value().id(),
      Quantity::length_mm(25.0, "dimension.direct").value());
  REQUIRE(literal);
  CHECK(document.find_parameter(ParameterId("dim.length"))->value().millimeters() ==
        Catch::Approx(25.0));

  auto formula = SketchDimensionEditService::set_formula(
      document, catalog.value(), expression.value().id(), "base_length * 3");
  REQUIRE(formula);
  const Parameter* expression_parameter = document.find_parameter(ParameterId("expr.length"));
  REQUIRE(expression_parameter != nullptr);
  REQUIRE(expression_parameter->formula().has_value());
  CHECK(*expression_parameter->formula() == "base_length * 3");
  CHECK(expression_parameter->value().millimeters() == Catch::Approx(30.0));

  auto rejected = SketchDimensionEditService::set_literal(
      document, catalog.value(), expression.value().id(),
      Quantity::length_mm(12.0, "dimension.expression").value());
  CHECK_FALSE(rejected);
}
