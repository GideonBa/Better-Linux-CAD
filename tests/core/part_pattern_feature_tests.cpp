#include "blcad/core/part_document_json.hpp"
#include "blcad/core/part_pattern_feature.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>

using namespace blcad;

namespace {

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Parameter count_parameter(const char* id, double value) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body body(const char* id) {
  auto value = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext new_body_context(const char* id) {
  auto value =
      FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, BodyId(id));
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext join_context(const char* id) {
  auto value =
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, BodyId(id), std::nullopt);
  REQUIRE(value);
  return value.value();
}

AxisReference pattern_axis() {
  auto value =
      AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, DatumAxisId("axis.z"));
  REQUIRE(value);
  return value.value();
}

PatternSourceReference feature_source(const char* id) {
  auto value = PatternSourceReference::feature(FeatureId(id));
  REQUIRE(value);
  return value.value();
}

PatternSourceReference body_source(const char* id) {
  auto value = PatternSourceReference::body(BodyId(id));
  REQUIRE(value);
  return value.value();
}

PartDocument pattern_document(double count = 4.0) {
  auto document = PartDocument::create(DocumentId("part.pattern"), "Pattern");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("profile.width", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("profile.height", 8.0)));
  REQUIRE(document.value().add_parameter(length_parameter("feature.depth", 5.0)));
  REQUIRE(document.value().add_parameter(length_parameter("pattern.spacing", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("pattern.extent", 60.0)));
  REQUIRE(document.value().add_parameter(count_parameter("pattern.count", count)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z", {}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("profile.width"),
                                            ParameterId("profile.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  REQUIRE(document.value().add_body(body("body.base")));
  REQUIRE(document.value().add_body(body("body.linear")));
  REQUIRE(document.value().add_body(body("body.circular")));
  auto base = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.base"), ParameterId("feature.depth"));
  REQUIRE(base);
  auto with_context = base.value().with_body_result_context(new_body_context("body.base"));
  REQUIRE(with_context);
  REQUIRE(document.value().add_feature(with_context.value()));
  return document.value();
}

LinearPatternFeature
linear_pattern(FeatureBodyResultContext context = new_body_context("body.linear")) {
  auto value = LinearPatternFeature::create(
      FeatureId("pattern.linear"), "Linear",
      {feature_source("feature.base"), body_source("body.base")}, pattern_axis(),
      ParameterId("pattern.count"), LinearPatternExtentMode::TotalExtent,
      ParameterId("pattern.extent"), PatternDirectionSign::Negative, std::move(context));
  REQUIRE(value);
  return value.value();
}

CircularPatternFeature circular_pattern(const char* source = "pattern.linear") {
  auto value = CircularPatternFeature::create(
      FeatureId("pattern.circular"), "Circular", {feature_source(source)}, pattern_axis(),
      ParameterId("pattern.count"), 270.0, true, new_body_context("body.circular"));
  REQUIRE(value);
  return value.value();
}

} // namespace

TEST_CASE("Block 63 freezes ordered feature and body Pattern source identity",
          "[core][part-pattern]") {
  const auto feature = feature_source("feature.base");
  const auto source_body = body_source("body.base");
  CHECK(feature.kind() == PatternSourceKind::Feature);
  CHECK(feature.source_node_id() == "feature.base");
  CHECK(source_body.kind() == PatternSourceKind::Body);
  CHECK(source_body.source_node_id() == "body:body.base");
  CHECK(PatternSourceReference::feature(FeatureId()).has_error());
  CHECK(PatternSourceReference::body(BodyId()).has_error());

  const auto linear = linear_pattern();
  REQUIRE(linear.sources().size() == 2U);
  CHECK(linear.sources().at(0) == feature);
  CHECK(linear.sources().at(1) == source_body);
  CHECK(linear.extent_mode() == LinearPatternExtentMode::TotalExtent);
  CHECK(linear.direction_sign() == PatternDirectionSign::Negative);

  CHECK(LinearPatternFeature::create(FeatureId("pattern.duplicate"), "Duplicate",
                                     {feature, feature}, pattern_axis(),
                                     ParameterId("pattern.count"), LinearPatternExtentMode::Spacing,
                                     ParameterId("pattern.spacing"), PatternDirectionSign::Positive,
                                     new_body_context("body.linear"))
            .has_error());
  CHECK(CircularPatternFeature::create(FeatureId("pattern.angle"), "Angle", {feature},
                                       pattern_axis(), ParameterId("pattern.count"), 0.0, true,
                                       new_body_context("body.circular"))
            .has_error());
  CHECK(CircularPatternFeature::create(FeatureId("pattern.spacing"), "Spacing", {feature},
                                       pattern_axis(), ParameterId("pattern.count"), 360.0, false,
                                       new_body_context("body.circular"))
            .has_error());
}

TEST_CASE("Block 63 integrates Pattern sources parameters bodies and invalidation",
          "[core][part-pattern]") {
  auto document = pattern_document();
  REQUIRE(document.add_linear_pattern_feature(linear_pattern()));
  REQUIRE(document.add_circular_pattern_feature(circular_pattern()));
  CHECK(document.linear_pattern_feature_count() == 1U);
  CHECK(document.circular_pattern_feature_count() == 1U);
  CHECK(document.dependency_graph().has_dependency("feature.base", "pattern.linear"));
  CHECK(document.dependency_graph().has_dependency("body:body.base", "pattern.linear"));
  CHECK(document.dependency_graph().has_dependency("axis.z", "pattern.linear"));
  CHECK(document.dependency_graph().has_dependency("pattern.count", "pattern.linear"));
  CHECK(document.dependency_graph().has_dependency("pattern.extent", "pattern.linear"));
  CHECK(document.dependency_graph().has_dependency("pattern.linear", "body:body.linear"));
  CHECK(document.dependency_graph().has_dependency("pattern.linear", "pattern.circular"));
  CHECK(document.dependency_graph().has_dependency("pattern.circular", "body:body.circular"));
  CHECK(document.remove_body(BodyId("body.base")).has_error());

  document.mark_all_clean();
  auto changed = Quantity::count(6.0, "pattern.count");
  REQUIRE(changed);
  const auto affected = document.set_parameter_value(ParameterId("pattern.count"), changed.value());
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "pattern.linear") !=
        affected.value().end());
  CHECK(std::find(affected.value().begin(), affected.value().end(), "pattern.circular") !=
        affected.value().end());
  const auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("body:body.linear"));
  CHECK(plan.value().contains("body:body.circular"));
}

TEST_CASE("Block 63 supports in-place Pattern body-result chaining", "[core][part-pattern]") {
  auto document = pattern_document();
  auto joined = LinearPatternFeature::create(
      FeatureId("pattern.linear"), "Linear", {body_source("body.base")}, pattern_axis(),
      ParameterId("pattern.count"), LinearPatternExtentMode::Spacing,
      ParameterId("pattern.spacing"), PatternDirectionSign::Positive, join_context("body.base"));
  REQUIRE(joined);
  REQUIRE(document.add_linear_pattern_feature(joined.value()));
  CHECK(document.dependency_graph().has_dependency("feature.base", "pattern.linear"));
  CHECK_FALSE(document.dependency_graph().has_dependency("body:body.base", "pattern.linear"));
  CHECK(document.dependency_graph().has_dependency("pattern.linear", "body:body.base"));
}

TEST_CASE("Block 63 Pattern JSON is additive strict and roundtrips exact intent",
          "[core][part-pattern][integration][part-construction-mvp]") {
  auto document = pattern_document();
  REQUIRE(document.add_linear_pattern_feature(linear_pattern()));
  REQUIRE(document.add_circular_pattern_feature(circular_pattern()));
  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto root = nlohmann::json::parse(serialized.value());
  REQUIRE(root.at("part_patterns").size() == 2U);
  CHECK(root.at("part_patterns").at(0).at("type") == "linear_pattern");
  CHECK(root.at("part_patterns").at(0).at("sources").at(0).at("kind") == "feature");
  CHECK(root.at("part_patterns").at(0).at("sources").at(1).at("kind") == "body");
  CHECK(root.at("part_patterns").at(0).at("extent").at("mode") == "total_extent");
  CHECK(root.at("part_patterns").at(0).at("extent").at("direction") == "negative");
  CHECK(root.at("part_patterns").at(1).at("total_angle_deg") == 270.0);

  const auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const auto* linear = restored.value().find_linear_pattern_feature(FeatureId("pattern.linear"));
  const auto* circular =
      restored.value().find_circular_pattern_feature(FeatureId("pattern.circular"));
  REQUIRE(linear != nullptr);
  REQUIRE(circular != nullptr);
  CHECK(linear->sources().at(0).source_node_id() == "feature.base");
  CHECK(linear->sources().at(1).source_node_id() == "body:body.base");
  CHECK(circular->axis().source_node_id() == "axis.z");
  CHECK(circular->equal_spacing());

  root.erase("part_patterns");
  const auto historical = deserialize_part_document_from_json(root.dump());
  REQUIRE(historical);
  CHECK(historical.value().linear_pattern_feature_count() == 0U);
  CHECK(historical.value().circular_pattern_feature_count() == 0U);

  auto malformed = nlohmann::json::parse(serialized.value());
  malformed.at("part_patterns").at(0).at("sources").at(0)["kind"] = "face";
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  malformed = nlohmann::json::parse(serialized.value());
  malformed.at("part_patterns").at(1)["equal_spacing"] = false;
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
}

TEST_CASE("Block 63 JSON preserves cross-kind Pattern dependency order", "[core][part-pattern]") {
  auto document = pattern_document();
  auto circular = CircularPatternFeature::create(
      FeatureId("pattern.circular"), "Circular", {feature_source("feature.base")}, pattern_axis(),
      ParameterId("pattern.count"), 360.0, true, new_body_context("body.circular"));
  REQUIRE(circular);
  REQUIRE(document.add_circular_pattern_feature(circular.value()));
  auto linear = LinearPatternFeature::create(
      FeatureId("pattern.linear"), "Linear", {feature_source("pattern.circular")}, pattern_axis(),
      ParameterId("pattern.count"), LinearPatternExtentMode::Spacing,
      ParameterId("pattern.spacing"), PatternDirectionSign::Positive,
      new_body_context("body.linear"));
  REQUIRE(linear);
  REQUIRE(document.add_linear_pattern_feature(linear.value()));

  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  CHECK(root.at("part_patterns").at(0).at("type") == "circular_pattern");
  CHECK(root.at("part_patterns").at(1).at("type") == "linear_pattern");
  const auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().dependency_graph().has_dependency("pattern.circular", "pattern.linear"));
}

TEST_CASE("Block 63 rejects missing sources and non-pattern parameter types transactionally",
          "[core][part-pattern]") {
  auto document = pattern_document(1.0);
  auto missing = LinearPatternFeature::create(
      FeatureId("pattern.missing"), "Missing", {feature_source("feature.missing")}, pattern_axis(),
      ParameterId("pattern.count"), LinearPatternExtentMode::Spacing,
      ParameterId("pattern.spacing"), PatternDirectionSign::Positive,
      new_body_context("body.linear"));
  REQUIRE(missing);
  CHECK(document.add_linear_pattern_feature(missing.value()).has_error());
  CHECK(document.linear_pattern_feature_count() == 0U);
  CHECK_FALSE(document.dependency_graph().has_dependency("feature.missing", "pattern.missing"));

  auto bad_extent = LinearPatternFeature::create(
      FeatureId("pattern.bad_extent"), "BadExtent", {feature_source("feature.base")},
      pattern_axis(), ParameterId("pattern.count"), LinearPatternExtentMode::Spacing,
      ParameterId("pattern.count"), PatternDirectionSign::Positive,
      new_body_context("body.linear"));
  REQUIRE(bad_extent);
  CHECK(document.add_linear_pattern_feature(bad_extent.value()).has_error());
}
