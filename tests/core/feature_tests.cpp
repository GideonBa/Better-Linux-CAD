#include "blcad/core/feature.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("AdditiveExtrude feature stores sketch and length parameter references",
          "[core][feature]") {
  const auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));

  REQUIRE(feature);
  CHECK(feature.value().id().value() == "feature.base_extrude");
  CHECK(feature.value().name() == "BaseExtrude");
  CHECK(feature.value().type() == FeatureType::AdditiveExtrude);
  CHECK(to_string(feature.value().type()) == "additive_extrude");
  CHECK(feature.value().input_sketch().value() == "sketch.base");
  CHECK(feature.value().length_parameter().value() == "part.thickness");
  CHECK(feature.value().target_feature().empty());
  CHECK(feature.value().direction() == ExtrudeDirection::SketchNormal);
  CHECK(to_string(feature.value().direction()) == "+Z");
}

TEST_CASE("AdditiveExtrude feature rejects missing required references", "[core][feature]") {
  const auto missing_id = Feature::create_additive_extrude(
      FeatureId(), "BaseExtrude", SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(missing_id.has_error());
  CHECK(missing_id.error().object_id() == "feature");
  CHECK(missing_id.error().message() == "feature id must not be empty");

  const auto missing_name =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(missing_name.has_error());
  CHECK(missing_name.error().object_id() == "feature.base_extrude");
  CHECK(missing_name.error().message() == "feature name must not be empty");

  const auto missing_sketch = Feature::create_additive_extrude(
      FeatureId("feature.base_extrude"), "BaseExtrude", SketchId(), ParameterId("part.thickness"));
  REQUIRE(missing_sketch.has_error());
  CHECK(missing_sketch.error().object_id() == "feature.base_extrude");
  CHECK(missing_sketch.error().message() == "feature input sketch id must not be empty");

  const auto missing_length = Feature::create_additive_extrude(
      FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"), ParameterId());
  REQUIRE(missing_length.has_error());
  CHECK(missing_length.error().object_id() == "feature.base_extrude");
  CHECK(missing_length.error().message() ==
        "additive extrude length parameter id must not be empty");
}

TEST_CASE("SubtractiveExtrude feature stores sketch and target feature references",
          "[core][feature]") {
  const auto feature = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                           "CenterHoleCut", SketchId("sketch.hole"),
                                                           FeatureId("feature.base_extrude"));

  REQUIRE(feature);
  CHECK(feature.value().id().value() == "feature.center_hole_cut");
  CHECK(feature.value().name() == "CenterHoleCut");
  CHECK(feature.value().type() == FeatureType::SubtractiveExtrude);
  CHECK(to_string(feature.value().type()) == "subtractive_extrude");
  CHECK(feature.value().input_sketch().value() == "sketch.hole");
  CHECK(feature.value().length_parameter().empty());
  CHECK(feature.value().target_feature().value() == "feature.base_extrude");
  CHECK(feature.value().subtractive_depth() == SubtractiveExtrudeDepth::ThroughAll);
  CHECK(to_string(feature.value().subtractive_depth()) == "through_all");
  CHECK(feature.value().direction() == ExtrudeDirection::SketchNormal);
}

TEST_CASE("SubtractiveExtrude feature supports opposite sketch-normal direction", "[core][feature]") {
  const auto feature = Feature::create_subtractive_extrude(
      FeatureId("feature.back_cut"), "BackCut", SketchId("sketch.back"),
      FeatureId("feature.base_extrude"), SubtractiveExtrudeDepth::ThroughAll,
      ExtrudeDirection::OppositeSketchNormal);

  REQUIRE(feature);
  CHECK(feature.value().direction() == ExtrudeDirection::OppositeSketchNormal);
  CHECK(to_string(feature.value().direction()) == "opposite_sketch_normal");
}

TEST_CASE("SubtractiveExtrude feature rejects missing required references", "[core][feature]") {
  const auto missing_id = Feature::create_subtractive_extrude(
      FeatureId(), "CenterHoleCut", SketchId("sketch.hole"), FeatureId("feature.base_extrude"));
  REQUIRE(missing_id.has_error());
  CHECK(missing_id.error().object_id() == "feature");
  CHECK(missing_id.error().message() == "feature id must not be empty");

  const auto missing_name = Feature::create_subtractive_extrude(
      FeatureId("feature.center_hole_cut"), "", SketchId("sketch.hole"),
      FeatureId("feature.base_extrude"));
  REQUIRE(missing_name.has_error());
  CHECK(missing_name.error().object_id() == "feature.center_hole_cut");
  CHECK(missing_name.error().message() == "feature name must not be empty");

  const auto missing_sketch =
      Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"), "CenterHoleCut",
                                          SketchId(), FeatureId("feature.base_extrude"));
  REQUIRE(missing_sketch.has_error());
  CHECK(missing_sketch.error().object_id() == "feature.center_hole_cut");
  CHECK(missing_sketch.error().message() == "feature input sketch id must not be empty");

  const auto missing_target = Feature::create_subtractive_extrude(
      FeatureId("feature.center_hole_cut"), "CenterHoleCut", SketchId("sketch.hole"), FeatureId());
  REQUIRE(missing_target.has_error());
  CHECK(missing_target.error().object_id() == "feature.center_hole_cut");
  CHECK(missing_target.error().message() ==
        "subtractive extrude target feature id must not be empty");
}
