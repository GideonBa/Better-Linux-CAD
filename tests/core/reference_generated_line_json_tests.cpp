#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <utility>
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

Sketch make_reference_generated_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.profile"), "ReferenceProfile", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  for (const auto& pair : {std::pair{"ref.a", "point.a"}, std::pair{"ref.b", "point.b"},
                           std::pair{"ref.c", "point.c"}}) {
    auto projected = ProjectedSketchPoint::create_from_construction_point(
        SketchEntityId(pair.first), ConstructionPointId(pair.second));
    REQUIRE(projected);
    REQUIRE(sketch.value().add_reference(projected.value()));
  }

  auto ab = ReferenceGeneratedLine::create_from_projected_point_constraints(
      SketchEntityId("helper.ab"), SketchConstraintId("constraint.ab.start"),
      SketchConstraintId("constraint.ab.end"));
  auto bc = ReferenceGeneratedLine::create_from_projected_point_constraints(
      SketchEntityId("helper.bc"), SketchConstraintId("constraint.bc.start"),
      SketchConstraintId("constraint.bc.end"));
  auto ca = ReferenceGeneratedLine::create_from_projected_point_constraints(
      SketchEntityId("helper.ca"), SketchConstraintId("constraint.ca.start"),
      SketchConstraintId("constraint.ca.end"));
  REQUIRE(ab);
  REQUIRE(bc);
  REQUIRE(ca);
  REQUIRE(sketch.value().add_reference(ab.value()));
  REQUIRE(sketch.value().add_reference(bc.value()));
  REQUIRE(sketch.value().add_reference(ca.value()));

  add_projected_point_constraint(sketch.value(), "constraint.ab.start", "helper.ab", true, "ref.a");
  add_projected_point_constraint(sketch.value(), "constraint.ab.end", "helper.ab", false, "ref.b");
  add_projected_point_constraint(sketch.value(), "constraint.bc.start", "helper.bc", true, "ref.b");
  add_projected_point_constraint(sketch.value(), "constraint.bc.end", "helper.bc", false, "ref.c");
  add_projected_point_constraint(sketch.value(), "constraint.ca.start", "helper.ca", true, "ref.c");
  add_projected_point_constraint(sketch.value(), "constraint.ca.end", "helper.ca", false, "ref.a");

  auto profile = ClosedProfile::create(
      ProfileId("profile.reference_triangle"),
      {SketchEntityId("helper.ab"), SketchEntityId("helper.bc"), SketchEntityId("helper.ca")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

PartDocument make_reference_generated_document(bool parameter_driven_points) {
  auto document =
      PartDocument::create(DocumentId("part.reference_generated_lines"), "ReferenceGeneratedLines");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.anchor", "anchor", 1.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));

  const std::vector<ParameterId> deps = parameter_driven_points
                                            ? std::vector<ParameterId>{ParameterId("part.anchor")}
                                            : std::vector<ParameterId>{};
  auto point_a = ConstructionPoint::create_explicit(ConstructionPointId("point.a"), "A",
                                                    Point3{0.0, 0.0, 0.0}, deps);
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

  auto sketch = make_reference_generated_sketch();
  REQUIRE(document.value().add_sketch(sketch));
  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.reference_triangle"), "ReferenceTriangle",
                                       SketchId("sketch.profile"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

bool contains(const std::vector<std::string>& values, const std::string& needle) {
  return std::find(values.begin(), values.end(), needle) != values.end();
}

} // namespace

TEST_CASE("Reference-generated sketch lines roundtrip through JSON",
          "[core][json][reference-generated]") {
  PartDocument document = make_reference_generated_document(false);
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("reference_generated_lines") != std::string::npos);
  CHECK(serialized.value().find("helper.ab") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const Sketch* restored_sketch = restored.value().find_sketch(SketchId("sketch.profile"));
  REQUIRE(restored_sketch != nullptr);
  REQUIRE(restored_sketch->reference_generated_lines().size() == 3U);
  REQUIRE(restored_sketch->find_reference_generated_line(SketchEntityId("helper.ab")) != nullptr);
  REQUIRE(restored_sketch->find_closed_profile(ProfileId("profile.reference_triangle")) != nullptr);
}

TEST_CASE("Reference-generated helper line dependencies dirty sketches and dependent features",
          "[core][dependency][reference-generated]") {
  PartDocument document = make_reference_generated_document(true);
  auto affected = document.mark_parameter_changed(ParameterId("part.anchor"));
  REQUIRE(affected);
  CHECK(contains(affected.value(), "point.a"));
  CHECK(contains(affected.value(), "sketch.profile.reference_generated_line.helper.ab"));
  CHECK(contains(affected.value(), "sketch.profile.reference_generated_line.helper.ca"));
  CHECK(contains(affected.value(), "sketch.profile"));
  CHECK(contains(affected.value(), "feature.reference_triangle"));
}
