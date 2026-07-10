#include "blcad/geometry/reference_driven_sketch_helper.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

PartDocument make_reference_helper_document() {
  auto document = PartDocument::create(DocumentId("part.reference_helper"), "ReferenceHelper");
  REQUIRE(document);

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto point_a = ConstructionPoint::create_explicit(ConstructionPointId("point.a"), "A",
                                                    Point3{0.0, 0.0, 0.0});
  REQUIRE(point_a);
  REQUIRE(document.value().add_construction_point(point_a.value()));

  auto point_b = ConstructionPoint::create_explicit(ConstructionPointId("point.b"), "B",
                                                    Point3{10.0, 0.0, 0.0});
  REQUIRE(point_b);
  REQUIRE(document.value().add_construction_point(point_b.value()));

  auto axis = ConstructionLine::create_explicit(ConstructionLineId("line.axis"), "Axis",
                                                Point3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_construction_line(axis.value()));

  auto sketch = Sketch::create(SketchId("sketch.refs"), "Refs", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto helper_line =
      LineSegment::create(SketchEntityId("line.helper"), Point2{-1.0, 0.0}, Point2{1.0, 0.0});
  REQUIRE(helper_line);
  REQUIRE(sketch.value().add_entity(helper_line.value()));

  auto projected_a = ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("ref.point.a"), ConstructionPointId("point.a"));
  REQUIRE(projected_a);
  REQUIRE(sketch.value().add_reference(projected_a.value()));

  auto projected_b = ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("ref.point.b"), ConstructionPointId("point.b"));
  REQUIRE(projected_b);
  REQUIRE(sketch.value().add_reference(projected_b.value()));

  auto projected_axis = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("ref.line.axis"), ConstructionLineId("line.axis"));
  REQUIRE(projected_axis);
  REQUIRE(sketch.value().add_reference(projected_axis.value()));

  auto start = SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.helper"));
  REQUIRE(start);
  auto end = SketchReferenceTarget::create_line_segment_end(SketchEntityId("line.helper"));
  REQUIRE(end);
  auto point_a_target =
      SketchReferenceTarget::create_projected_point(SketchEntityId("ref.point.a"));
  REQUIRE(point_a_target);
  auto point_b_target =
      SketchReferenceTarget::create_projected_point(SketchEntityId("ref.point.b"));
  REQUIRE(point_b_target);

  auto start_constraint = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId("constraint.start_on_a"), start.value(), point_a_target.value());
  REQUIRE(start_constraint);
  REQUIRE(sketch.value().add_constraint(start_constraint.value()));

  auto end_constraint = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId("constraint.end_on_b"), end.value(), point_b_target.value());
  REQUIRE(end_constraint);
  REQUIRE(sketch.value().add_constraint(end_constraint.value()));

  auto line_target = SketchReferenceTarget::create_line_segment(SketchEntityId("line.helper"));
  REQUIRE(line_target);
  auto projected_line_target =
      SketchReferenceTarget::create_projected_line(SketchEntityId("ref.line.axis"));
  REQUIRE(projected_line_target);
  auto parallel = SketchConstraint::create_parallel_to_projected_line(
      SketchConstraintId("constraint.parallel_axis"), line_target.value(),
      projected_line_target.value());
  REQUIRE(parallel);
  REQUIRE(sketch.value().add_constraint(parallel.value()));

  REQUIRE(document.value().add_sketch(sketch.value()));
  return document.value();
}

} // namespace

TEST_CASE("ReferenceDrivenSketchHelper resolves projected-point and projected-line constraints",
          "[geometry][sketch-reference]") {
  PartDocument document = make_reference_helper_document();
  const Sketch* sketch = document.find_sketch(SketchId("sketch.refs"));
  REQUIRE(sketch != nullptr);

  const SketchConstraint* start =
      sketch->find_constraint(SketchConstraintId("constraint.start_on_a"));
  REQUIRE(start != nullptr);
  const SketchConstraint* parallel =
      sketch->find_constraint(SketchConstraintId("constraint.parallel_axis"));
  REQUIRE(parallel != nullptr);

  const ReferenceDrivenSketchHelper helper;
  auto resolved_start = helper.resolve_coincident_point_constraint(document, *sketch, *start);
  REQUIRE(resolved_start);
  CHECK(resolved_start.value().point.x == Catch::Approx(0.0));
  CHECK(resolved_start.value().point.y == Catch::Approx(0.0));

  auto resolved_parallel = helper.resolve_projected_line_constraint(document, *sketch, *parallel);
  REQUIRE(resolved_parallel);
  CHECK(resolved_parallel.value().point.x == Catch::Approx(0.0));
  CHECK(resolved_parallel.value().point.y == Catch::Approx(0.0));
  CHECK(resolved_parallel.value().direction.x == Catch::Approx(1.0));
  CHECK(resolved_parallel.value().direction.y == Catch::Approx(0.0));
}

TEST_CASE(
    "ReferenceDrivenSketchHelper creates deterministic profile helper lines from projected points",
    "[geometry][sketch-reference]") {
  PartDocument document = make_reference_helper_document();
  const Sketch* sketch = document.find_sketch(SketchId("sketch.refs"));
  REQUIRE(sketch != nullptr);

  const SketchConstraint* start =
      sketch->find_constraint(SketchConstraintId("constraint.start_on_a"));
  REQUIRE(start != nullptr);
  const SketchConstraint* end = sketch->find_constraint(SketchConstraintId("constraint.end_on_b"));
  REQUIRE(end != nullptr);

  const ReferenceDrivenSketchHelper helper;
  auto line = helper.create_profile_helper_line_from_projected_point_constraints(
      document, *sketch, SketchEntityId("line.helper"), *start, *end);
  REQUIRE(line);
  CHECK(line.value().start().x == Catch::Approx(0.0));
  CHECK(line.value().start().y == Catch::Approx(0.0));
  CHECK(line.value().end().x == Catch::Approx(10.0));
  CHECK(line.value().end().y == Catch::Approx(0.0));
}
