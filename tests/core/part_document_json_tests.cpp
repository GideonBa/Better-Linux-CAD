#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_approx.hpp>
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

PartDocument make_reference_part_document() {
  auto document = PartDocument::create(DocumentId("part.rectangular_plate"), "RectangularPlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

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

  auto hole_sketch =
      Sketch::create(SketchId("sketch.hole"), "Sketch_CenterHole", DatumPlaneId("datum.xy"));
  REQUIRE(hole_sketch);
  auto circle =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.value().add_sketch(hole_sketch.value()));

  auto base = Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                               SketchId("sketch.base"),
                                               ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                 "CenterHoleCut", SketchId("sketch.hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

void replace_first(std::string& value, const std::string& needle, const std::string& replacement) {
  const auto position = value.find(needle);
  REQUIRE(position != std::string::npos);
  value.replace(position, needle.size(), replacement);
}

} // namespace

TEST_CASE("PartDocument JSON round-trips MVP-1 model intent", "[core][json]") {
  const PartDocument document = make_reference_part_document();

  const auto serialized = serialize_part_document_to_json(document);

  REQUIRE(serialized);
  CHECK(serialized.value().find("blcad.part_document.mvp1") != std::string::npos);
  CHECK(serialized.value().find("feature.center_hole_cut") != std::string::npos);

  const auto restored = deserialize_part_document_from_json(serialized.value());

  REQUIRE(restored);
  CHECK(restored.value().id().value() == "part.rectangular_plate");
  CHECK(restored.value().name() == "RectangularPlate");
  CHECK(restored.value().parameter_count() == 4);
  CHECK(restored.value().datum_plane_count() == 1);
  CHECK(restored.value().sketch_count() == 2);
  CHECK(restored.value().feature_count() == 2);

  const auto* width = restored.value().find_parameter(ParameterId("part.width"));
  REQUIRE(width != nullptr);
  CHECK(width->name() == "width");
  CHECK(width->value().millimeters() == Catch::Approx(120.0));

  const auto* hole_sketch = restored.value().find_sketch(SketchId("sketch.hole"));
  REQUIRE(hole_sketch != nullptr);
  CHECK(hole_sketch->circle_profiles().size() == 1);
  CHECK(hole_sketch->circle_profiles()[0].diameter_parameter().value() == "part.hole_diameter");

  const auto* cut = restored.value().find_feature(FeatureId("feature.center_hole_cut"));
  REQUIRE(cut != nullptr);
  CHECK(cut->type() == FeatureType::SubtractiveExtrude);
  CHECK(cut->target_feature().value() == "feature.base_extrude");

  CHECK(restored.value().dependency_graph().has_dependency("part.width", "sketch.base"));
  CHECK(restored.value().dependency_graph().has_dependency("sketch.base", "feature.base_extrude"));
  CHECK(restored.value().dependency_graph().has_dependency("feature.base_extrude",
                                                          "feature.center_hole_cut"));

  const auto clean_plan = restored.value().create_recompute_plan();
  REQUIRE(clean_plan);
  CHECK(clean_plan.value().step_count() == 0);
}

TEST_CASE("PartDocument JSON rejects unsupported schema", "[core][json]") {
  const auto restored = deserialize_part_document_from_json(R"({"schema":"wrong","version":1})");

  REQUIRE(restored.has_error());
  CHECK(restored.error().category() == ErrorCategory::Validation);
  CHECK(restored.error().object_id() == "part_document_json");
  CHECK(restored.error().message() == "unsupported part document json schema");
}

TEST_CASE("PartDocument JSON rejects unsupported feature types", "[core][json]") {
  const auto serialized = serialize_part_document_to_json(make_reference_part_document());
  REQUIRE(serialized);

  std::string unsupported = serialized.value();
  replace_first(unsupported, "additive_extrude", "revolve");

  const auto restored = deserialize_part_document_from_json(unsupported);

  REQUIRE(restored.has_error());
  CHECK(restored.error().category() == ErrorCategory::Validation);
  CHECK(restored.error().object_id() == "part_document_json");
  CHECK(restored.error().message() == "unsupported feature type in part document json");
}
