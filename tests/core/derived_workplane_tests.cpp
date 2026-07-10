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
  auto document = PartDocument::create(DocumentId("part.face_plate"), "FacePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 4.0)));

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

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
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

void add_hole_on_workplane(PartDocument& document, DatumPlaneId workplane_id, SketchId sketch_id,
                           ProfileId profile_id, FeatureId cut_id, const char* sketch_name,
                           const char* cut_name) {
  auto hole_sketch = Sketch::create(sketch_id, sketch_name, workplane_id);
  REQUIRE(hole_sketch);
  auto circle = CircleProfile::create(profile_id, ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.add_sketch(hole_sketch.value()));

  auto cut = Feature::create_subtractive_extrude(cut_id, cut_name, sketch_id,
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.add_feature(cut.value()));
}

} // namespace

TEST_CASE("Semantic face references store feature and face role", "[core][workplane]") {
  auto top_reference =
      SemanticFaceReference::create(FeatureId("feature.base_extrude"), SemanticFace::Top);
  REQUIRE(top_reference);
  CHECK(top_reference.value().source_feature().value() == "feature.base_extrude");
  CHECK(top_reference.value().face() == SemanticFace::Top);
  CHECK(to_string(top_reference.value().face()) == "top");

  auto bottom_reference =
      SemanticFaceReference::create(FeatureId("feature.base_extrude"), SemanticFace::Bottom);
  REQUIRE(bottom_reference);
  CHECK(bottom_reference.value().face() == SemanticFace::Bottom);
  CHECK(to_string(bottom_reference.value().face()) == "bottom");

  auto right_reference =
      SemanticFaceReference::create(FeatureId("feature.base_extrude"), SemanticFace::Right);
  REQUIRE(right_reference);
  CHECK(right_reference.value().face() == SemanticFace::Right);
  CHECK(to_string(right_reference.value().face()) == "right");

  auto left_reference =
      SemanticFaceReference::create(FeatureId("feature.base_extrude"), SemanticFace::Left);
  REQUIRE(left_reference);
  CHECK(left_reference.value().face() == SemanticFace::Left);
  CHECK(to_string(left_reference.value().face()) == "left");

  auto front_reference =
      SemanticFaceReference::create(FeatureId("feature.base_extrude"), SemanticFace::Front);
  REQUIRE(front_reference);
  CHECK(front_reference.value().face() == SemanticFace::Front);
  CHECK(to_string(front_reference.value().face()) == "front");

  auto back_reference =
      SemanticFaceReference::create(FeatureId("feature.base_extrude"), SemanticFace::Back);
  REQUIRE(back_reference);
  CHECK(back_reference.value().face() == SemanticFace::Back);
  CHECK(to_string(back_reference.value().face()) == "back");
}

TEST_CASE("Semantic face references reject empty source features", "[core][workplane]") {
  auto reference = SemanticFaceReference::create(FeatureId(), SemanticFace::Top);

  REQUIRE(reference.has_error());
  CHECK(reference.error().category() == ErrorCategory::Validation);
  CHECK(reference.error().message() == "semantic face source feature id must not be empty");
}

TEST_CASE("PartDocument accepts sketches on all controlled derived workplanes",
          "[core][workplane]") {
  auto document = make_base_extrude_document();

  REQUIRE(document.add_derived_workplane(make_feature_face_workplane(
      DatumPlaneId("workplane.base_top"), "BaseTopFace", SemanticFace::Top)));
  REQUIRE(document.add_derived_workplane(make_feature_face_workplane(
      DatumPlaneId("workplane.base_bottom"), "BaseBottomFace", SemanticFace::Bottom)));
  REQUIRE(document.add_derived_workplane(make_feature_face_workplane(
      DatumPlaneId("workplane.base_right"), "BaseRightFace", SemanticFace::Right)));
  REQUIRE(document.add_derived_workplane(make_feature_face_workplane(
      DatumPlaneId("workplane.base_left"), "BaseLeftFace", SemanticFace::Left)));
  REQUIRE(document.add_derived_workplane(make_feature_face_workplane(
      DatumPlaneId("workplane.base_front"), "BaseFrontFace", SemanticFace::Front)));
  REQUIRE(document.add_derived_workplane(make_feature_face_workplane(
      DatumPlaneId("workplane.base_back"), "BaseBackFace", SemanticFace::Back)));
  CHECK(document.derived_workplane_count() == 6);

  add_hole_on_workplane(document, DatumPlaneId("workplane.base_top"), SketchId("sketch.top_hole"),
                        ProfileId("profile.top_hole"), FeatureId("feature.top_hole_cut"),
                        "Sketch_TopHole", "TopHoleCut");
  add_hole_on_workplane(document, DatumPlaneId("workplane.base_bottom"),
                        SketchId("sketch.bottom_hole"), ProfileId("profile.bottom_hole"),
                        FeatureId("feature.bottom_hole_cut"), "Sketch_BottomHole", "BottomHoleCut");
  add_hole_on_workplane(document, DatumPlaneId("workplane.base_right"),
                        SketchId("sketch.right_hole"), ProfileId("profile.right_hole"),
                        FeatureId("feature.right_hole_cut"), "Sketch_RightHole", "RightHoleCut");
  add_hole_on_workplane(document, DatumPlaneId("workplane.base_left"), SketchId("sketch.left_hole"),
                        ProfileId("profile.left_hole"), FeatureId("feature.left_hole_cut"),
                        "Sketch_LeftHole", "LeftHoleCut");
  add_hole_on_workplane(document, DatumPlaneId("workplane.base_front"),
                        SketchId("sketch.front_hole"), ProfileId("profile.front_hole"),
                        FeatureId("feature.front_hole_cut"), "Sketch_FrontHole", "FrontHoleCut");
  add_hole_on_workplane(document, DatumPlaneId("workplane.base_back"), SketchId("sketch.back_hole"),
                        ProfileId("profile.back_hole"), FeatureId("feature.back_hole_cut"),
                        "Sketch_BackHole", "BackHoleCut");

  CHECK(document.dependency_graph().has_dependency("feature.base_extrude", "workplane.base_back"));
  CHECK(document.dependency_graph().has_dependency("workplane.base_back", "sketch.back_hole"));
  CHECK(document.dependency_graph().has_dependency("sketch.back_hole", "feature.back_hole_cut"));
  CHECK(
      document.dependency_graph().has_dependency("feature.base_extrude", "feature.back_hole_cut"));
}

TEST_CASE("PartDocument rejects derived workplanes without source feature", "[core][workplane]") {
  auto document = PartDocument::create(DocumentId("part.empty"), "EmptyPart");
  REQUIRE(document);

  auto face_reference =
      SemanticFaceReference::create(FeatureId("feature.missing"), SemanticFace::Back);
  REQUIRE(face_reference);
  auto workplane = DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("workplane.missing_back"), "MissingBackFace", face_reference.value());
  REQUIRE(workplane);

  auto added = document.value().add_derived_workplane(workplane.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().message() == "derived workplane source feature must exist in part document");
}
