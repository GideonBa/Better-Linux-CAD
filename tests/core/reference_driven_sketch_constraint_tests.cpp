#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("Sketch reference targets and constraints store projected reference intent",
          "[core][sketch]") {
  auto line = SketchReferenceTarget::create_line_segment(SketchEntityId("line.helper"));
  REQUIRE(line);
  CHECK(line.value().kind() == SketchReferenceTargetKind::LineSegment);
  CHECK(line.value().entity().value() == "line.helper");

  auto line_start = SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.helper"));
  REQUIRE(line_start);
  auto projected_point =
      SketchReferenceTarget::create_projected_point(SketchEntityId("ref.point.a"));
  REQUIRE(projected_point);
  auto coincident = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId("constraint.a"), line_start.value(), projected_point.value());
  REQUIRE(coincident);
  CHECK(coincident.value().kind() == SketchConstraintKind::CoincidentToProjectedPoint);
  CHECK(coincident.value().constrained_target().entity().value() == "line.helper");
  CHECK(coincident.value().reference_target().entity().value() == "ref.point.a");

  auto projected_line =
      SketchReferenceTarget::create_projected_line(SketchEntityId("ref.line.axis"));
  REQUIRE(projected_line);
  auto parallel = SketchConstraint::create_parallel_to_projected_line(
      SketchConstraintId("constraint.parallel"), line.value(), projected_line.value());
  REQUIRE(parallel);
  CHECK(parallel.value().kind() == SketchConstraintKind::ParallelToProjectedLine);

  auto collinear = SketchConstraint::create_collinear_with_projected_line(
      SketchConstraintId("constraint.collinear"), line.value(), projected_line.value());
  REQUIRE(collinear);
  CHECK(collinear.value().kind() == SketchConstraintKind::CollinearWithProjectedLine);
}

TEST_CASE("Sketch constraints reject incompatible target shapes", "[core][sketch]") {
  auto line = SketchReferenceTarget::create_line_segment(SketchEntityId("line.helper"));
  REQUIRE(line);
  auto projected_point =
      SketchReferenceTarget::create_projected_point(SketchEntityId("ref.point.a"));
  REQUIRE(projected_point);
  auto projected_line =
      SketchReferenceTarget::create_projected_line(SketchEntityId("ref.line.axis"));
  REQUIRE(projected_line);

  auto invalid_coincident = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId("constraint.bad"), line.value(), projected_point.value());
  REQUIRE(invalid_coincident.has_error());
  CHECK(invalid_coincident.error().message() ==
        "coincident projected-point constraint requires a line endpoint target");

  auto line_start = SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.helper"));
  REQUIRE(line_start);
  auto invalid_parallel = SketchConstraint::create_parallel_to_projected_line(
      SketchConstraintId("constraint.bad2"), line_start.value(), projected_line.value());
  REQUIRE(invalid_parallel.has_error());
  CHECK(invalid_parallel.error().message() ==
        "parallel projected-line constraint requires a line segment target");
}

TEST_CASE("Sketch stores validated projected reference constraints", "[core][sketch]") {
  auto sketch = Sketch::create(SketchId("sketch.refs"), "Sketch_Refs", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto helper =
      LineSegment::create(SketchEntityId("line.helper"), Point2{0.0, 0.0}, Point2{1.0, 0.0});
  REQUIRE(helper);
  REQUIRE(sketch.value().add_entity(helper.value()));

  auto point = ProjectedSketchPoint::create_from_construction_point(SketchEntityId("ref.point.a"),
                                                                    ConstructionPointId("point.a"));
  REQUIRE(point);
  REQUIRE(sketch.value().add_reference(point.value()));

  auto line = ProjectedSketchLine::create_from_construction_line(SketchEntityId("ref.line.axis"),
                                                                 ConstructionLineId("line.axis"));
  REQUIRE(line);
  REQUIRE(sketch.value().add_reference(line.value()));

  auto line_start = SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.helper"));
  REQUIRE(line_start);
  auto projected_point =
      SketchReferenceTarget::create_projected_point(SketchEntityId("ref.point.a"));
  REQUIRE(projected_point);
  auto coincident = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId("constraint.start"), line_start.value(), projected_point.value());
  REQUIRE(coincident);
  REQUIRE(sketch.value().add_constraint(coincident.value()));

  auto duplicate = sketch.value().add_constraint(coincident.value());
  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().message() == "sketch constraint id must be unique within sketch");

  auto missing_target =
      SketchReferenceTarget::create_projected_point(SketchEntityId("ref.missing"));
  REQUIRE(missing_target);
  auto invalid = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId("constraint.missing"), line_start.value(), missing_target.value());
  REQUIRE(invalid);
  auto added = sketch.value().add_constraint(invalid.value());
  REQUIRE(added.has_error());
  CHECK(added.error().message() == "sketch constraint projected point target must exist");
}
