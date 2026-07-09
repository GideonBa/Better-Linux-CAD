#include "blcad/geometry/reference_generated_profile_resolver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

void add_projected_point_constraint(Sketch& sketch, const char* id, const char* line_id,
                                    bool start, const char* point_id) {
  auto constrained = start ? SketchReferenceTarget::create_line_segment_start(SketchEntityId(line_id))
                           : SketchReferenceTarget::create_line_segment_end(SketchEntityId(line_id));
  REQUIRE(constrained);
  auto projected = SketchReferenceTarget::create_projected_point(SketchEntityId(point_id));
  REQUIRE(projected);
  auto constraint = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId(id), constrained.value(), projected.value());
  REQUIRE(constraint);
  REQUIRE(sketch.add_constraint(constraint.value()));
}

PartDocument make_reference_generated_profile_document(bool add_y_axis) {
  auto document = PartDocument::create(DocumentId("part.reference_generated_profile"),
                                       "ReferenceGeneratedProfile");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto point_a = ConstructionPoint::create_explicit(ConstructionPointId("point.a"), "A",
                                                    Point3{0.0, 0.0, 0.0});
  auto point_b = ConstructionPoint::create_explicit(ConstructionPointId("point.b"), "B",
                                                    Point3{10.0, 0.0, 0.0});
  auto point_c = ConstructionPoint::create_explicit(ConstructionPointId("point.c"), "C",
                                                    Point3{0.0, 10.0, 0.0});
  REQUIRE(point_a);
  REQUIRE(point_b);
  REQUIRE(point_c);
  REQUIRE(document.value().add_construction_point(point_a.value()));
  REQUIRE(document.value().add_construction_point(point_b.value()));
  REQUIRE(document.value().add_construction_point(point_c.value()));

  auto axis_x = ConstructionLine::create_explicit(ConstructionLineId("line.axis_x"), "AxisX",
                                                  Point3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0});
  REQUIRE(axis_x);
  REQUIRE(document.value().add_construction_line(axis_x.value()));
  if (add_y_axis) {
    auto axis_y = ConstructionLine::create_explicit(ConstructionLineId("line.axis_y"), "AxisY",
                                                    Point3{0.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0});
    REQUIRE(axis_y);
    REQUIRE(document.value().add_construction_line(axis_y.value()));
  }

  auto sketch = Sketch::create(SketchId("sketch.profile"), "Profile", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto line_ab = LineSegment::create(SketchEntityId("helper.ab"), Point2{0.0, 0.0}, Point2{10.0, 0.0});
  auto line_bc = LineSegment::create(SketchEntityId("helper.bc"), Point2{10.0, 0.0}, Point2{0.0, 10.0});
  auto line_ca = LineSegment::create(SketchEntityId("helper.ca"), Point2{0.0, 10.0}, Point2{0.0, 0.0});
  REQUIRE(line_ab);
  REQUIRE(line_bc);
  REQUIRE(line_ca);
  REQUIRE(sketch.value().add_entity(line_ab.value()));
  REQUIRE(sketch.value().add_entity(line_bc.value()));
  REQUIRE(sketch.value().add_entity(line_ca.value()));

  for (const auto& pair : {std::pair{"ref.a", "point.a"}, std::pair{"ref.b", "point.b"},
                           std::pair{"ref.c", "point.c"}}) {
    auto projected = ProjectedSketchPoint::create_from_construction_point(
        SketchEntityId(pair.first), ConstructionPointId(pair.second));
    REQUIRE(projected);
    REQUIRE(sketch.value().add_reference(projected.value()));
  }
  auto projected_axis_x = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("ref.axis_x"), ConstructionLineId("line.axis_x"));
  REQUIRE(projected_axis_x);
  REQUIRE(sketch.value().add_reference(projected_axis_x.value()));
  if (add_y_axis) {
    auto projected_axis_y = ProjectedSketchLine::create_from_construction_line(
        SketchEntityId("ref.axis_y"), ConstructionLineId("line.axis_y"));
    REQUIRE(projected_axis_y);
    REQUIRE(sketch.value().add_reference(projected_axis_y.value()));
  }

  add_projected_point_constraint(sketch.value(), "constraint.ab.start", "helper.ab", true, "ref.a");
  add_projected_point_constraint(sketch.value(), "constraint.ab.end", "helper.ab", false, "ref.b");
  add_projected_point_constraint(sketch.value(), "constraint.bc.start", "helper.bc", true, "ref.b");
  add_projected_point_constraint(sketch.value(), "constraint.bc.end", "helper.bc", false, "ref.c");
  add_projected_point_constraint(sketch.value(), "constraint.ca.start", "helper.ca", true, "ref.c");
  add_projected_point_constraint(sketch.value(), "constraint.ca.end", "helper.ca", false, "ref.a");

  auto line_target = SketchReferenceTarget::create_line_segment(SketchEntityId("helper.ab"));
  REQUIRE(line_target);
  auto projected_x = SketchReferenceTarget::create_projected_line(SketchEntityId("ref.axis_x"));
  REQUIRE(projected_x);
  auto parallel_x = SketchConstraint::create_parallel_to_projected_line(
      SketchConstraintId("constraint.ab.parallel_x"), line_target.value(), projected_x.value());
  REQUIRE(parallel_x);
  REQUIRE(sketch.value().add_constraint(parallel_x.value()));
  if (add_y_axis) {
    auto projected_y = SketchReferenceTarget::create_projected_line(SketchEntityId("ref.axis_y"));
    REQUIRE(projected_y);
    auto parallel_y = SketchConstraint::create_parallel_to_projected_line(
        SketchConstraintId("constraint.ab.parallel_y"), line_target.value(), projected_y.value());
    REQUIRE(parallel_y);
    REQUIRE(sketch.value().add_constraint(parallel_y.value()));
  }

  auto profile = ClosedProfile::create(ProfileId("profile.reference_triangle"),
                                       {SketchEntityId("helper.ab"), SketchEntityId("helper.bc"),
                                        SketchEntityId("helper.ca")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  return document.value();
}

std::vector<ReferenceGeneratedLine> make_reference_lines(bool mismatched_direction) {
  auto ab = mismatched_direction
                ? ReferenceGeneratedLine::create_with_projected_line_direction(
                      SketchEntityId("helper.ab"), SketchConstraintId("constraint.ab.start"),
                      SketchConstraintId("constraint.ab.end"),
                      SketchConstraintId("constraint.ab.parallel_y"))
                : ReferenceGeneratedLine::create_with_projected_line_direction(
                      SketchEntityId("helper.ab"), SketchConstraintId("constraint.ab.start"),
                      SketchConstraintId("constraint.ab.end"),
                      SketchConstraintId("constraint.ab.parallel_x"));
  auto bc = ReferenceGeneratedLine::create_from_projected_point_constraints(
      SketchEntityId("helper.bc"), SketchConstraintId("constraint.bc.start"),
      SketchConstraintId("constraint.bc.end"));
  auto ca = ReferenceGeneratedLine::create_from_projected_point_constraints(
      SketchEntityId("helper.ca"), SketchConstraintId("constraint.ca.start"),
      SketchConstraintId("constraint.ca.end"));
  REQUIRE(ab);
  REQUIRE(bc);
  REQUIRE(ca);
  return {ab.value(), bc.value(), ca.value()};
}

} // namespace

TEST_CASE("ReferenceGeneratedProfileResolver resolves deterministic helper profile vertices",
          "[geometry][reference-generated-profile]") {
  PartDocument document = make_reference_generated_profile_document(false);
  const Sketch* sketch = document.find_sketch(SketchId("sketch.profile"));
  REQUIRE(sketch != nullptr);
  const ClosedProfile* profile = sketch->find_closed_profile(ProfileId("profile.reference_triangle"));
  REQUIRE(profile != nullptr);

  const ReferenceGeneratedProfileResolver resolver;
  auto vertices = resolver.resolve_closed_profile_vertices(document, *sketch, *profile,
                                                           make_reference_lines(false));
  REQUIRE(vertices);
  REQUIRE(vertices.value().size() == 3U);
  CHECK(vertices.value()[0].x == Catch::Approx(0.0));
  CHECK(vertices.value()[0].y == Catch::Approx(0.0));
  CHECK(vertices.value()[1].x == Catch::Approx(10.0));
  CHECK(vertices.value()[1].y == Catch::Approx(0.0));
  CHECK(vertices.value()[2].x == Catch::Approx(0.0));
  CHECK(vertices.value()[2].y == Catch::Approx(10.0));
}

TEST_CASE("ReferenceGeneratedProfileResolver rejects mismatched projected-line direction",
          "[geometry][reference-generated-profile]") {
  PartDocument document = make_reference_generated_profile_document(true);
  const Sketch* sketch = document.find_sketch(SketchId("sketch.profile"));
  REQUIRE(sketch != nullptr);
  const ClosedProfile* profile = sketch->find_closed_profile(ProfileId("profile.reference_triangle"));
  REQUIRE(profile != nullptr);

  const ReferenceGeneratedProfileResolver resolver;
  auto vertices = resolver.resolve_closed_profile_vertices(document, *sketch, *profile,
                                                           make_reference_lines(true));
  REQUIRE_FALSE(vertices);
  CHECK(vertices.error().message() ==
        "reference-generated line direction must match projected-line constraint");
}
