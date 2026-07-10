#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/reference_generated_profile_resolver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

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

void add_projected_point_constraint(Sketch& sketch, const char* id, const char* line_id, bool start,
                                    const char* point_id) {
  auto constrained = start
                         ? SketchReferenceTarget::create_line_segment_start(SketchEntityId(line_id))
                         : SketchReferenceTarget::create_line_segment_end(SketchEntityId(line_id));
  REQUIRE(constrained);
  auto projected = SketchReferenceTarget::create_projected_point(SketchEntityId(point_id));
  REQUIRE(projected);
  auto constraint = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId(id), constrained.value(), projected.value());
  REQUIRE(constraint);
  REQUIRE(sketch.add_constraint(constraint.value()));
}

void add_reference_generated_line(Sketch& sketch, const char* line_id, const char* start_constraint,
                                  const char* end_constraint,
                                  const char* direction_constraint = nullptr) {
  auto line =
      direction_constraint == nullptr
          ? ReferenceGeneratedLine::create_from_projected_point_constraints(
                SketchEntityId(line_id), SketchConstraintId(start_constraint),
                SketchConstraintId(end_constraint))
          : ReferenceGeneratedLine::create_with_projected_line_direction(
                SketchEntityId(line_id), SketchConstraintId(start_constraint),
                SketchConstraintId(end_constraint), SketchConstraintId(direction_constraint));
  REQUIRE(line);
  REQUIRE(sketch.add_reference(line.value()));
}

Sketch make_reference_triangle_sketch(bool add_mismatched_direction) {
  auto sketch = Sketch::create(SketchId("sketch.profile"), "Profile", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

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

  if (add_mismatched_direction) {
    auto projected_axis_y = ProjectedSketchLine::create_from_construction_line(
        SketchEntityId("ref.axis_y"), ConstructionLineId("line.axis_y"));
    REQUIRE(projected_axis_y);
    REQUIRE(sketch.value().add_reference(projected_axis_y.value()));
  }

  add_reference_generated_line(
      sketch.value(), "helper.ab", "constraint.ab.start", "constraint.ab.end",
      add_mismatched_direction ? "constraint.ab.parallel_y" : "constraint.ab.parallel_x");
  add_reference_generated_line(sketch.value(), "helper.bc", "constraint.bc.start",
                               "constraint.bc.end");
  add_reference_generated_line(sketch.value(), "helper.ca", "constraint.ca.start",
                               "constraint.ca.end");

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
  if (!add_mismatched_direction) {
    REQUIRE(sketch.value().add_constraint(parallel_x.value()));
  }

  if (add_mismatched_direction) {
    auto projected_y = SketchReferenceTarget::create_projected_line(SketchEntityId("ref.axis_y"));
    REQUIRE(projected_y);
    auto parallel_y = SketchConstraint::create_parallel_to_projected_line(
        SketchConstraintId("constraint.ab.parallel_y"), line_target.value(), projected_y.value());
    REQUIRE(parallel_y);
    REQUIRE(sketch.value().add_constraint(parallel_y.value()));
  }

  auto profile = ClosedProfile::create(
      ProfileId("profile.reference_triangle"),
      {SketchEntityId("helper.ab"), SketchEntityId("helper.bc"), SketchEntityId("helper.ca")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

PartDocument make_reference_generated_profile_document(bool add_mismatched_direction = false) {
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
  if (add_mismatched_direction) {
    auto axis_y = ConstructionLine::create_explicit(ConstructionLineId("line.axis_y"), "AxisY",
                                                    Point3{0.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0});
    REQUIRE(axis_y);
    REQUIRE(document.value().add_construction_line(axis_y.value()));
  }

  auto sketch = make_reference_triangle_sketch(add_mismatched_direction);
  REQUIRE(document.value().add_sketch(sketch));
  return document.value();
}

PartDocument make_reference_generated_additive_document() {
  PartDocument document = make_reference_generated_profile_document(false);
  REQUIRE(document.add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.reference_triangle"), "ReferenceTriangle",
                                       SketchId("sketch.profile"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.add_feature(feature.value()));
  return document;
}

PartDocument make_reference_generated_subtractive_document() {
  auto document =
      PartDocument::create(DocumentId("part.reference_generated_cut"), "ReferenceGeneratedCut");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto base_profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                               ParameterId("part.height"));
  REQUIRE(base_profile);
  REQUIRE(base_sketch.value().add_profile(base_profile.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));
  auto base_feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(base_feature);
  REQUIRE(document.value().add_feature(base_feature.value()));

  auto point_a = ConstructionPoint::create_explicit(ConstructionPointId("point.a"), "A",
                                                    Point3{-5.0, -5.0, 0.0});
  auto point_b = ConstructionPoint::create_explicit(ConstructionPointId("point.b"), "B",
                                                    Point3{5.0, -5.0, 0.0});
  auto point_c = ConstructionPoint::create_explicit(ConstructionPointId("point.c"), "C",
                                                    Point3{-5.0, 5.0, 0.0});
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

  auto cut_sketch = make_reference_triangle_sketch(false);
  REQUIRE(document.value().add_sketch(cut_sketch));
  auto cut_feature = Feature::create_subtractive_extrude(
      FeatureId("feature.cut"), "Cut", SketchId("sketch.profile"), FeatureId("feature.base"));
  REQUIRE(cut_feature);
  REQUIRE(document.value().add_feature(cut_feature.value()));
  return document.value();
}

} // namespace

TEST_CASE("ReferenceGeneratedProfileResolver resolves first-class helper profile vertices",
          "[geometry][reference-generated-profile]") {
  PartDocument document = make_reference_generated_profile_document(false);
  const Sketch* sketch = document.find_sketch(SketchId("sketch.profile"));
  REQUIRE(sketch != nullptr);
  const ClosedProfile* profile =
      sketch->find_closed_profile(ProfileId("profile.reference_triangle"));
  REQUIRE(profile != nullptr);

  const ReferenceGeneratedProfileResolver resolver;
  auto vertices = resolver.resolve_closed_profile_vertices(document, *sketch, *profile,
                                                           sketch->reference_generated_lines());
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
  const ClosedProfile* profile =
      sketch->find_closed_profile(ProfileId("profile.reference_triangle"));
  REQUIRE(profile != nullptr);

  const ReferenceGeneratedProfileResolver resolver;
  auto vertices = resolver.resolve_closed_profile_vertices(document, *sketch, *profile,
                                                           sketch->reference_generated_lines());
  REQUIRE_FALSE(vertices);
  CHECK(vertices.error().message() ==
        "reference-generated line direction must match projected-line constraint");
}

TEST_CASE("Geometry recomputes additive extrude from reference-generated helper profile",
          "[geometry][reference-generated-profile]") {
  PartDocument document = make_reference_generated_additive_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.reference_additive"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.reference_triangle"));
}

TEST_CASE("Geometry recomputes subtractive cut from reference-generated helper profile",
          "[geometry][reference-generated-profile]") {
  PartDocument document = make_reference_generated_subtractive_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.reference_cut"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.cut"));
}
