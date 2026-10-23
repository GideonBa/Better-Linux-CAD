#pragma once
#include "blcad/core/part_document.hpp"
#include <catch2/catch_test_macros.hpp>

namespace blcad::test {
template <typename T> T reference_checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
// Imported intent boundary: unsupported circular-source semantic references are
// structurally valid, loadable, and unresolved by the rectangular seed resolver.
inline PartDocument reference_repair_fixture(bool broken = true, bool plane = false,
                                             bool downstream = false) {
  auto part = reference_checked(PartDocument::create(DocumentId("part.repair"), "Repair"));
  for (const auto& [id, value] : {std::pair{"width", 20.0}, {"height", 10.0}, {"depth", 8.0}})
    REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
        ParameterId(id), id, reference_checked(Quantity::length_mm(value, id))))));
  REQUIRE(part.add_datum_plane(reference_checked(DatumPlane::xy())));
  for (const std::string id : {"old", "new"}) {
    auto sketch =
        reference_checked(Sketch::create(SketchId("sketch." + id), id, DatumPlaneId("datum.xy")));
    if (id == "old" && broken)
      REQUIRE(sketch.add_profile(reference_checked(
          CircleProfile::create(ProfileId("profile." + id), ParameterId("width")))));
    else
      REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
          ProfileId("profile." + id), ParameterId("width"), ParameterId("height"), {30, 0}))));
    REQUIRE(part.add_sketch(sketch));
    REQUIRE(part.add_feature(reference_checked(Feature::create_additive_extrude(
        FeatureId("feature." + id), id, sketch.id(), ParameterId("depth")))));
  }
  auto sketch = reference_checked(
      Sketch::create(SketchId("sketch.refs"), "References", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_entity(
      reference_checked(LineSegment::create(SketchEntityId("line"), {0, 0}, {5, 0}))));
  REQUIRE(sketch.add_reference(reference_checked(ProjectedSketchPoint::create_from_semantic_vertex(
      SketchEntityId("point"), reference_checked(SemanticVertexReference::create(
                                   FeatureId("feature.old"), SemanticVertex::TopFrontRight))))));
  REQUIRE(sketch.add_reference(reference_checked(ProjectedSketchLine::create_from_semantic_edge(
      SketchEntityId("edge"), reference_checked(SemanticEdgeReference::create(
                                  FeatureId("feature.old"), SemanticEdge::TopFront))))));
  REQUIRE(sketch.add_constraint(
      reference_checked(SketchConstraint::create_coincident_to_projected_point(
          SketchConstraintId("coincident"),
          reference_checked(
              SketchReferenceTarget::create_line_segment_start(SketchEntityId("line"))),
          reference_checked(
              SketchReferenceTarget::create_projected_point(SketchEntityId("point")))))));
  REQUIRE(
      sketch.add_constraint(reference_checked(SketchConstraint::create_parallel_to_projected_line(
          SketchConstraintId("parallel"),
          reference_checked(SketchReferenceTarget::create_line_segment(SketchEntityId("line"))),
          reference_checked(
              SketchReferenceTarget::create_projected_line(SketchEntityId("edge")))))));
  if (downstream)
    REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
        ProfileId("profile.refs"), ParameterId("width"), ParameterId("height"), {60, 0}))));
  REQUIRE(part.add_sketch(sketch));
  if (downstream)
    REQUIRE(part.add_feature(reference_checked(Feature::create_additive_extrude(
        FeatureId("feature.consumer"), "Consumer", sketch.id(), ParameterId("depth")))));
  if (plane)
    REQUIRE(part.add_derived_workplane(reference_checked(DerivedWorkplane::create_on_feature_face(
        DatumPlaneId("plane"), "Face plane",
        reference_checked(
            SemanticFaceReference::create(FeatureId("feature.old"), SemanticFace::Top))))));
  REQUIRE(part.add_construction_point(reference_checked(ConstructionPoint::create_explicit(
      ConstructionPointId("point.fallback"), "Fallback point", {42, 3, 0}))));
  REQUIRE(part.add_construction_line(reference_checked(ConstructionLine::create_explicit(
      ConstructionLineId("line.fallback"), "Fallback line", {10, 2, 0}, {1, 0, 0}))));
  return part;
}
} // namespace blcad::test
