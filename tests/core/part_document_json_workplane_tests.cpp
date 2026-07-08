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

PartDocument make_top_face_cut_document() {
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

  auto face_reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"),
                                                      SemanticFace::Top);
  REQUIRE(face_reference);
  auto workplane = DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("workplane.base_top"), "BaseTopFace", face_reference.value());
  REQUIRE(workplane);
  REQUIRE(document.value().add_derived_workplane(workplane.value()));

  auto hole_sketch = Sketch::create(SketchId("sketch.top_hole"), "Sketch_TopHole",
                                    DatumPlaneId("workplane.base_top"));
  REQUIRE(hole_sketch);
  auto circle = CircleProfile::create(ProfileId("profile.top_hole"),
                                      ParameterId("part.top_hole_diameter"));
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.value().add_sketch(hole_sketch.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.top_hole_cut"), "TopHoleCut",
                                                 SketchId("sketch.top_hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

} // namespace

TEST_CASE("PartDocument JSON round-trips derived workplanes", "[core][json][workplane]") {
  const PartDocument document = make_top_face_cut_document();

  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("derived_workplanes") != std::string::npos);
  CHECK(serialized.value().find("workplane.base_top") != std::string::npos);
  CHECK(serialized.value().find("\"face\": \"top\"") != std::string::npos);

  const auto restored = deserialize_part_document_from_json(serialized.value());

  REQUIRE(restored);
  CHECK(restored.value().derived_workplane_count() == 1);
  CHECK(restored.value().sketch_count() == 2);
  CHECK(restored.value().feature_count() == 2);

  const auto* workplane = restored.value().find_derived_workplane(DatumPlaneId("workplane.base_top"));
  REQUIRE(workplane != nullptr);
  CHECK(workplane->face_reference().source_feature().value() == "feature.base_extrude");
  CHECK(workplane->face_reference().face() == SemanticFace::Top);

  CHECK(restored.value().dependency_graph().has_dependency("feature.base_extrude",
                                                          "workplane.base_top"));
  CHECK(restored.value().dependency_graph().has_dependency("workplane.base_top",
                                                          "sketch.top_hole"));
  CHECK(restored.value().dependency_graph().has_dependency("sketch.top_hole",
                                                          "feature.top_hole_cut"));
}
