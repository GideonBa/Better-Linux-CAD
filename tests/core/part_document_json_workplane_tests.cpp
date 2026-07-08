#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
}

PartDocument make_feature_face_cut_document(SemanticFace face, DatumPlaneId workplane_id,
                                            const char* workplane_name, SketchId hole_sketch_id,
                                            const char* hole_sketch_name, ProfileId profile_id,
                                            FeatureId cut_feature_id, const char* cut_name,
                                            double hole_diameter = 4.0) {
  auto document = PartDocument::create(DocumentId("part.face_plate"), "FacePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.hole_diameter", "hole_diameter", hole_diameter)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"), ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));

  auto base = Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto face_reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"), face);
  REQUIRE(face_reference);
  auto workplane = DerivedWorkplane::create_on_feature_face(workplane_id, workplane_name, face_reference.value());
  REQUIRE(workplane);
  REQUIRE(document.value().add_derived_workplane(workplane.value()));

  auto hole_sketch = Sketch::create(hole_sketch_id, hole_sketch_name, workplane_id);
  REQUIRE(hole_sketch);
  auto circle = CircleProfile::create(profile_id, ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.value().add_sketch(hole_sketch.value()));

  auto cut = Feature::create_subtractive_extrude(cut_feature_id, cut_name, hole_sketch_id, FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

PartDocument make_top_face_cut_document() {
  return make_feature_face_cut_document(SemanticFace::Top, DatumPlaneId("workplane.base_top"),
                                        "BaseTopFace", SketchId("sketch.top_hole"),
                                        "Sketch_TopHole", ProfileId("profile.top_hole"),
                                        FeatureId("feature.top_hole_cut"), "TopHoleCut", 20.0);
}

PartDocument make_bottom_face_cut_document() {
  return make_feature_face_cut_document(SemanticFace::Bottom, DatumPlaneId("workplane.base_bottom"),
                                        "BaseBottomFace", SketchId("sketch.bottom_hole"),
                                        "Sketch_BottomHole", ProfileId("profile.bottom_hole"),
                                        FeatureId("feature.bottom_hole_cut"), "BottomHoleCut", 20.0);
}

PartDocument make_right_face_cut_document() {
  return make_feature_face_cut_document(SemanticFace::Right, DatumPlaneId("workplane.base_right"),
                                        "BaseRightFace", SketchId("sketch.right_hole"),
                                        "Sketch_RightHole", ProfileId("profile.right_hole"),
                                        FeatureId("feature.right_hole_cut"), "RightHoleCut", 4.0);
}

void check_roundtrip(const PartDocument& document, DatumPlaneId workplane_id, SemanticFace face,
                     const char* serialized_face, const char* sketch_id, const char* cut_id) {
  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("derived_workplanes") != std::string::npos);
  CHECK(serialized.value().find(workplane_id.value()) != std::string::npos);
  CHECK(serialized.value().find(std::string("\"face\": \"") + serialized_face + "\"") !=
        std::string::npos);

  const auto restored = deserialize_part_document_from_json(serialized.value());

  REQUIRE(restored);
  CHECK(restored.value().derived_workplane_count() == 1);
  CHECK(restored.value().sketch_count() == 2);
  CHECK(restored.value().feature_count() == 2);

  const auto* workplane = restored.value().find_derived_workplane(workplane_id);
  REQUIRE(workplane != nullptr);
  CHECK(workplane->face_reference().source_feature().value() == "feature.base_extrude");
  CHECK(workplane->face_reference().face() == face);

  CHECK(restored.value().dependency_graph().has_dependency("feature.base_extrude",
                                                          workplane_id.value()));
  CHECK(restored.value().dependency_graph().has_dependency(workplane_id.value(), sketch_id));
  CHECK(restored.value().dependency_graph().has_dependency(sketch_id, cut_id));
}

} // namespace

TEST_CASE("PartDocument JSON round-trips top derived workplanes", "[core][json][workplane]") {
  check_roundtrip(make_top_face_cut_document(), DatumPlaneId("workplane.base_top"),
                  SemanticFace::Top, "top", "sketch.top_hole", "feature.top_hole_cut");
}

TEST_CASE("PartDocument JSON round-trips bottom derived workplanes", "[core][json][workplane]") {
  check_roundtrip(make_bottom_face_cut_document(), DatumPlaneId("workplane.base_bottom"),
                  SemanticFace::Bottom, "bottom", "sketch.bottom_hole",
                  "feature.bottom_hole_cut");
}

TEST_CASE("PartDocument JSON round-trips right derived workplanes", "[core][json][workplane]") {
  check_roundtrip(make_right_face_cut_document(), DatumPlaneId("workplane.base_right"),
                  SemanticFace::Right, "right", "sketch.right_hole", "feature.right_hole_cut");
}
