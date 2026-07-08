#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
}

PartDocument make_base_extrude_document() {
  auto document = PartDocument::create(DocumentId("part.top_face_plate"), "TopFacePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.top_hole_diameter", "top_hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));

  auto base = Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                               SketchId("sketch.base"),
                                               ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  return document.value();
}

DerivedWorkplane make_feature_face_workplane(DatumPlaneId id, const char* name, SemanticFace face) {
  auto face_reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"), face);
  REQUIRE(face_reference);

  auto workplane = DerivedWorkplane::create_on_feature_face(id, name, face_reference.value());
  REQUIRE(workplane);

  return workplane.value();
}

DerivedWorkplane make_top_face_workplane() {
  return make_feature_face_workplane(DatumPlaneId("workplane.base_top"), "BaseTopFace",
                                     SemanticFace::Top);
}

DerivedWorkplane make_bottom_face_workplane() {
  return make_feature_face_workplane(DatumPlaneId("workplane.base_bottom"), "BaseBottomFace",
                                     SemanticFace::Bottom);
}

} // namespace

TEST_CASE("Semantic face references store feature and face role", "[core][workplane]") {
  auto top_reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"),
                                                     SemanticFace::Top);
  REQUIRE(top_reference);
  CHECK(top_reference.value().source_feature().value() == "feature.base_extrude");
  CHECK(top_reference.value().face() == SemanticFace::Top);
  CHECK(to_string(top_reference.value().face()) == "top");

  auto bottom_reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"),
                                                        SemanticFace::Bottom);
  REQUIRE(bottom_reference);
  CHECK(bottom_reference.value().source_feature().value() == "feature.base_extrude");
  CHECK(bottom_reference.value().face() == SemanticFace::Bottom);
  CHECK(to_string(bottom_reference.value().face()) == "bottom");
}

TEST_CASE("Semantic face references reject empty source features", "[core][workplane]") {
  auto reference = SemanticFaceReference::create(FeatureId(), SemanticFace::Top);

  REQUIRE(reference.has_error());
  CHECK(reference.error().category() == ErrorCategory::Validation);
  CHECK(reference.error().message() == "semantic face source feature id must not be empty");
}

TEST_CASE("PartDocument accepts sketch on derived top-face workplane", "[core][workplane]") {
  auto document = make_base_extrude_document();

  REQUIRE(document.add_derived_workplane(make_top_face_workplane()));
  CHECK(document.derived_workplane_count() == 1);
  REQUIRE(document.find_derived_workplane(DatumPlaneId("workplane.base_top")) != nullptr);

  auto hole_sketch = Sketch::create(SketchId("sketch.top_hole"), "Sketch_TopHole",
                                    DatumPlaneId("workplane.base_top"));
  REQUIRE(hole_sketch);
  auto circle = CircleProfile::create(ProfileId("profile.top_hole"),
                                      ParameterId("part.top_hole_diameter"));
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.add_sketch(hole_sketch.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.top_hole_cut"), "TopHoleCut",
                                                 SketchId("sketch.top_hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.add_feature(cut.value()));

  CHECK(document.dependency_graph().has_dependency("feature.base_extrude", "workplane.base_top"));
  CHECK(document.dependency_graph().has_dependency("workplane.base_top", "sketch.top_hole"));
  CHECK(document.dependency_graph().has_dependency("sketch.top_hole", "feature.top_hole_cut"));
  CHECK(document.dependency_graph().has_dependency("feature.base_extrude", "feature.top_hole_cut"));
  CHECK(document.dependency_graph().has_dependency("part.top_hole_diameter", "sketch.top_hole"));
}

TEST_CASE("PartDocument accepts sketch on derived bottom-face workplane", "[core][workplane]") {
  auto document = make_base_extrude_document();

  REQUIRE(document.add_derived_workplane(make_bottom_face_workplane()));
  CHECK(document.derived_workplane_count() == 1);
  REQUIRE(document.find_derived_workplane(DatumPlaneId("workplane.base_bottom")) != nullptr);

  auto hole_sketch = Sketch::create(SketchId("sketch.bottom_hole"), "Sketch_BottomHole",
                                    DatumPlaneId("workplane.base_bottom"));
  REQUIRE(hole_sketch);
  auto circle = CircleProfile::create(ProfileId("profile.bottom_hole"),
                                      ParameterId("part.top_hole_diameter"));
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.add_sketch(hole_sketch.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.bottom_hole_cut"),
                                                 "BottomHoleCut", SketchId("sketch.bottom_hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.add_feature(cut.value()));

  CHECK(document.dependency_graph().has_dependency("feature.base_extrude",
                                                  "workplane.base_bottom"));
  CHECK(document.dependency_graph().has_dependency("workplane.base_bottom",
                                                  "sketch.bottom_hole"));
  CHECK(document.dependency_graph().has_dependency("sketch.bottom_hole",
                                                  "feature.bottom_hole_cut"));
  CHECK(document.dependency_graph().has_dependency("feature.base_extrude",
                                                  "feature.bottom_hole_cut"));
}

TEST_CASE("PartDocument rejects derived workplanes without source feature", "[core][workplane]") {
  auto document = PartDocument::create(DocumentId("part.empty"), "EmptyPart");
  REQUIRE(document);

  auto face_reference = SemanticFaceReference::create(FeatureId("feature.missing"), SemanticFace::Top);
  REQUIRE(face_reference);
  auto workplane = DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("workplane.missing_top"), "MissingTopFace", face_reference.value());
  REQUIRE(workplane);

  auto added = document.value().add_derived_workplane(workplane.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().message() == "derived workplane source feature must exist in part document");
}
