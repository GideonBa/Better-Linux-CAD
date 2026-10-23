#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
PartDocument document() {
  auto part = checked(PartDocument::create(DocumentId("part"), "Construction updates"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  for (const auto* id : {"length", "other"})
    REQUIRE(part.add_parameter(
        checked(Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(10))))));
  for (auto [id, position] : {std::pair{"a", Point3{0, 0, 0}}, std::pair{"b", Point3{10, 0, 0}},
                              std::pair{"c", Point3{0, 10, 0}}})
    REQUIRE(part.add_construction_point(
        checked(ConstructionPoint::create_explicit(ConstructionPointId(id), id, position))));
  REQUIRE(part.add_construction_line(checked(ConstructionLine::create_through_two_points(
      ConstructionLineId("line"), "Line",
      checked(ConstructionRelation::create_line_through_two_points(
          ConstructionRelationId("line.relation"), ConstructionPointId("a"),
          ConstructionPointId("b")))))));
  REQUIRE(part.add_datum_axis(checked(DatumAxis::create_from_construction_line(
      DatumAxisId("axis"), "Axis", ConstructionLineId("line")))));
  REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("plane"), "Plane",
      checked(ConstructionRelation::create_plane_offset_from_plane(
          ConstructionRelationId("plane.relation"), DatumPlaneId("datum.xy"),
          ParameterId("length")))))));
  REQUIRE(part.add_sketch(
      checked(Sketch::create(SketchId("sketch"), "Sketch", DatumPlaneId("plane")))));
  auto sketch = *part.find_sketch(SketchId("sketch"));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("profile"), ParameterId("length"), ParameterId("length")))));
  REQUIRE(part.update_sketch(std::move(sketch)));
  REQUIRE(part.add_feature(checked(Feature::create_additive_extrude(
      FeatureId("feature"), "Extrude", SketchId("sketch"), ParameterId("length")))));
  REQUIRE(part.add_derived_workplane(checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("face"), "Face",
      checked(SemanticFaceReference::create(FeatureId("feature"), SemanticFace::Top))))));
  part.mark_all_clean();
  return part;
}
std::string json(const PartDocument& part) {
  return checked(serialize_part_document_to_json(part));
}
void roundtrip(const PartDocument& part) {
  const auto saved = json(part);
  CHECK(json(checked(deserialize_part_document_from_json(saved))) == saved);
}
} // namespace

TEST_CASE("Construction updates preserve identity order and downstream invalidation",
          "[core][construction-update]") {
  auto part = document();
  CHECK(checked(part.update_construction_point(checked(ConstructionPoint::create_explicit(
            ConstructionPointId("b"), "Moved", {20, 4, 3}, {ParameterId("other")})))) == 1);
  CHECK(part.construction_point_count() == 3);
  CHECK(part.construction_points()[1].id() == ConstructionPointId("b"));
  CHECK(part.construction_points()[1].position() == Point3{20, 4, 3});
  CHECK(part.dependency_graph().has_dependency("other", "b"));
  const auto plan = checked(part.create_recompute_plan());
  CHECK(part.invalidation_state().find("b")->status == InvalidationStatus::Changed);
  CHECK_FALSE(plan.contains("b")); // Recompute plans contain dirty consumers, not changed inputs.
  CHECK(plan.contains("line"));
  CHECK(plan.contains("axis"));
  CHECK_FALSE(plan.contains("a"));
  roundtrip(part);

  REQUIRE(part.update_construction_line(checked(ConstructionLine::create_through_two_points(
      ConstructionLineId("line"), "Rebound",
      checked(ConstructionRelation::create_line_through_two_points(
          ConstructionRelationId("line.relation"), ConstructionPointId("a"),
          ConstructionPointId("c")))))));
  CHECK_FALSE(part.dependency_graph().has_dependency("b", "line"));
  CHECK(part.dependency_graph().has_dependency("c", "line"));
  CHECK(part.dependency_graph().has_dependency("line", "axis"));
  REQUIRE(part.update_datum_axis(checked(DatumAxis::create_explicit(
      DatumAxisId("axis"), "Explicit", {3, 4, 5}, {0, 0, 1}, {ParameterId("other")}))));
  CHECK_FALSE(part.dependency_graph().has_dependency("line", "axis"));
  CHECK(part.dependency_graph().has_dependency("other", "axis"));
  CHECK(part.datum_axis_count() == 1);
  roundtrip(part);
}

TEST_CASE("Workplane updates rebind parameters and retain Sketch and feature consumers",
          "[core][construction-update]") {
  auto part = document();
  REQUIRE(part.update_construction_plane(checked(ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("plane"), "Rebound",
      checked(ConstructionRelation::create_plane_offset_from_plane(
          ConstructionRelationId("plane.relation"), DatumPlaneId("datum.xy"),
          ParameterId("other")))))));
  CHECK_FALSE(part.dependency_graph().has_dependency("length", "plane"));
  CHECK(part.dependency_graph().has_dependency("other", "plane"));
  CHECK(part.dependency_graph().has_dependency("plane", "sketch"));
  const auto plan = checked(part.create_recompute_plan());
  CHECK(plan.contains("sketch"));
  CHECK(plan.contains("feature"));
  CHECK(plan.contains("face"));
  CHECK(part.find_sketch(SketchId("sketch"))->workplane() == DatumPlaneId("plane"));
  REQUIRE(part.update_derived_workplane(checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("face"), "Bottom",
      checked(SemanticFaceReference::create(FeatureId("feature"), SemanticFace::Bottom))))));
  CHECK(part.derived_workplane_count() == 1);
  CHECK(part.derived_workplanes()[0].face_reference().face() == SemanticFace::Bottom);
  roundtrip(part);
}

TEST_CASE("Construction updates reject missing objects references and cycles atomically",
          "[core][construction-update]") {
  auto part = document();
  const auto before = json(part);
  const auto edges = part.dependency_graph().dependencies();
  REQUIRE_FALSE(part.update_construction_point(checked(
      ConstructionPoint::create_explicit(ConstructionPointId("missing"), "Missing", {0, 0, 0}))));
  REQUIRE_FALSE(part.update_construction_point(checked(ConstructionPoint::create_explicit(
      ConstructionPointId("a"), "Bad dependency", {0, 0, 0}, {ParameterId("missing")}))));
  REQUIRE_FALSE(part.update_construction_line(checked(ConstructionLine::create_through_two_points(
      ConstructionLineId("line"), "Missing",
      checked(ConstructionRelation::create_line_through_two_points(
          ConstructionRelationId("line.relation"), ConstructionPointId("a"),
          ConstructionPointId("missing")))))));
  REQUIRE_FALSE(part.update_datum_axis(checked(DatumAxis::create_from_construction_line(
      DatumAxisId("axis"), "Missing", ConstructionLineId("missing")))));
  REQUIRE_FALSE(part.update_derived_workplane(checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("face"), "Missing",
      checked(SemanticFaceReference::create(FeatureId("missing"), SemanticFace::Top))))));
  // plane -> sketch -> feature -> face; rebinding plane to face would form a cycle.
  REQUIRE_FALSE(part.update_construction_plane(checked(ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("plane"), "Cycle",
      checked(ConstructionRelation::create_plane_offset_from_plane(
          ConstructionRelationId("plane.relation"), DatumPlaneId("face"),
          ParameterId("length")))))));
  CHECK(json(part) == before);
  CHECK(part.dependency_graph().dependencies() == edges);
  CHECK(checked(part.create_recompute_plan()).empty());
  // line -> parallel line; reverse the reference to exercise a different typed update.
  REQUIRE(
      part.add_construction_line(checked(ConstructionLine::create_parallel_to_line_through_point(
          ConstructionLineId("parallel"), "Parallel",
          checked(ConstructionRelation::create_line_parallel_to_line_through_point(
              ConstructionRelationId("parallel.relation"), ConstructionLineId("line"),
              ConstructionPointId("c")))))));
  const auto with_parallel = json(part);
  REQUIRE_FALSE(
      part.update_construction_line(checked(ConstructionLine::create_parallel_to_line_through_point(
          ConstructionLineId("line"), "Cycle",
          checked(ConstructionRelation::create_line_parallel_to_line_through_point(
              ConstructionRelationId("line.relation"), ConstructionLineId("parallel"),
              ConstructionPointId("a")))))));
  CHECK(json(part) == with_parallel);
  // All preceding rejected replacements left the original records untouched.
  auto expected = checked(deserialize_part_document_from_json(before));
  REQUIRE(expected.add_construction_line(part.construction_lines().back()));
  CHECK(json(part) == json(expected));
  CHECK(part.dependency_graph().has_dependency("line", "axis"));
  CHECK_FALSE(part.dependency_graph().has_cycle());
  roundtrip(part);
}

TEST_CASE("Rebound workplanes retain authored array order across dependency-ready JSON loading",
          "[core][construction-update][construction-update-json]") {
  auto part = document();
  REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId("later"), "Later", {0, 0, 25}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  REQUIRE(part.update_construction_plane(checked(ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("plane"), "Rebound to later",
      checked(ConstructionRelation::create_plane_offset_from_plane(
          ConstructionRelationId("plane.relation"), DatumPlaneId("later"),
          ParameterId("other")))))));
  roundtrip(part);
  auto loaded = checked(deserialize_part_document_from_json(json(part)));
  CHECK(loaded.construction_planes()[0].id() == ConstructionPlaneId("plane"));
  CHECK(loaded.construction_planes()[1].id() == ConstructionPlaneId("later"));
  // A separate plane may legally depend on a semantic face loaded with features,
  // while existing Sketch consumers retain the old plane identity.
  REQUIRE_FALSE(part.update_construction_plane(checked(ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("later"), "Rebound to face",
      checked(ConstructionRelation::create_plane_offset_from_plane(
          ConstructionRelationId("later.relation"), DatumPlaneId("face"),
          ParameterId("other")))))));
  // The preceding request is cyclic (later -> plane -> feature -> face) and atomic.
  roundtrip(part);
  REQUIRE(part.add_construction_plane(
      checked(ConstructionPlane::create_explicit(ConstructionPlaneId("independent"), "Independent",
                                                 {0, 0, 40}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  REQUIRE(part.update_construction_plane(checked(ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("independent"), "Rebound to face",
      checked(ConstructionRelation::create_plane_offset_from_plane(
          ConstructionRelationId("independent.relation"), DatumPlaneId("face"),
          ParameterId("other")))))));
  roundtrip(part);
  CHECK(checked(deserialize_part_document_from_json(json(part)))
            .dependency_graph()
            .has_dependency("face", "independent"));
}
