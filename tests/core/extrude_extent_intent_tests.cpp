#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

using namespace blcad;

namespace {

Parameter length_parameter(const char* id, double value = 10.0) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

FaceReference limit_face(SemanticFace face) {
  auto identity = SemanticFaceReference::create(FeatureId("feature.base"), face);
  REQUIRE(identity);
  auto reference =
      FaceReference::create_planar(PartFeatureInputRole::ExtrudeLimitFace, identity.value());
  REQUIRE(reference);
  return reference.value();
}

ExtrudeFeatureIntent intent(ExtrudeExtentIntent extent, std::optional<double> taper = std::nullopt,
                            std::optional<ExtrudeThinIntent> thin = std::nullopt) {
  auto result = ExtrudeFeatureIntent::create(std::move(extent), taper, std::move(thin));
  REQUIRE(result);
  return result.value();
}

PartDocument extent_document() {
  auto document = PartDocument::create(DocumentId("part.extents"), "ExtrudeExtents");
  REQUIRE(document);
  for (const char* id : {"distance", "first", "second", "thin.first", "thin.second"})
    REQUIRE(document.value().add_parameter(length_parameter(id)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("distance"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));
  return document.value();
}

const nlohmann::json& json_feature(const nlohmann::json& root, std::string_view id) {
  const auto& features = root.at("features");
  const auto found = std::find_if(features.begin(), features.end(), [&id](const auto& value) {
    return value.at("id").template get<std::string>() == id;
  });
  REQUIRE(found != features.end());
  return *found;
}

nlohmann::json& json_feature(nlohmann::json& root, std::string_view id) {
  auto& features = root.at("features");
  const auto found = std::find_if(features.begin(), features.end(), [&id](const auto& value) {
    return value.at("id").template get<std::string>() == id;
  });
  REQUIRE(found != features.end());
  return *found;
}

Feature explicit_additive(const char* id, ExtrudeExtentIntent extent,
                          std::optional<double> taper = std::nullopt,
                          std::optional<ExtrudeThinIntent> thin = std::nullopt) {
  auto feature =
      Feature::create_additive_extrude(FeatureId(id), id, SketchId("sketch.base"),
                                       intent(std::move(extent), taper, std::move(thin)));
  REQUIRE(feature);
  return feature.value();
}

} // namespace

TEST_CASE("Extrude extent and thin modes have stable persistence spellings",
          "[core][extrude-extent]") {
  CHECK(to_string(ExtrudeExtentMode::Distance) == "distance");
  CHECK(to_string(ExtrudeExtentMode::Symmetric) == "symmetric");
  CHECK(to_string(ExtrudeExtentMode::TwoSided) == "two_sided");
  CHECK(to_string(ExtrudeExtentMode::ThroughAll) == "through_all");
  CHECK(to_string(ExtrudeExtentMode::ToNext) == "to_next");
  CHECK(to_string(ExtrudeExtentMode::ToFace) == "to_face");
  CHECK(to_string(ExtrudeExtentMode::Between) == "between");
  CHECK(to_string(ExtrudeThinMode::OneSided) == "one_sided");
  CHECK(to_string(ExtrudeThinMode::TwoSided) == "two_sided");
  CHECK(to_string(ExtrudeThinMode::MidPlane) == "mid_plane");
}

TEST_CASE("Extrude intent factories enforce exact mode-specific requirements",
          "[core][extrude-extent]") {
  CHECK(ExtrudeExtentIntent::distance(ParameterId()).has_error());
  CHECK(ExtrudeExtentIntent::symmetric(ParameterId()).has_error());
  CHECK(ExtrudeExtentIntent::two_sided(ParameterId("same"), ParameterId("same")).has_error());
  CHECK(
      ExtrudeThinIntent::create(ExtrudeThinMode::TwoSided, ParameterId("thin.first")).has_error());
  CHECK(ExtrudeThinIntent::create(ExtrudeThinMode::MidPlane, ParameterId("thin.first"),
                                  ParameterId("thin.second"))
            .has_error());
  CHECK(ExtrudeFeatureIntent::create(ExtrudeExtentIntent::through_all(), 90.0).has_error());
  CHECK(ExtrudeFeatureIntent::create(ExtrudeExtentIntent::through_all(),
                                     std::numeric_limits<double>::infinity())
            .has_error());

  auto two_sided = ExtrudeExtentIntent::two_sided(ParameterId("first"), ParameterId("second"));
  REQUIRE(two_sided);
  CHECK(two_sided.value().mode() == ExtrudeExtentMode::TwoSided);
  CHECK(two_sided.value().first_distance_parameter() == ParameterId("first"));
  CHECK(two_sided.value().second_distance_parameter() == ParameterId("second"));

  auto between =
      ExtrudeExtentIntent::between(limit_face(SemanticFace::Bottom), limit_face(SemanticFace::Top));
  REQUIRE(between);
  CHECK(between.value().mode() == ExtrudeExtentMode::Between);
  REQUIRE(between.value().first_face());
  REQUIRE(between.value().second_face());
  CHECK(ExtrudeExtentIntent::between(limit_face(SemanticFace::Top), limit_face(SemanticFace::Top))
            .has_error());
}

TEST_CASE("Feature retains richer additive and subtractive Extrude intent",
          "[core][extrude-extent]") {
  auto thin = ExtrudeThinIntent::create(ExtrudeThinMode::TwoSided, ParameterId("thin.first"),
                                        ParameterId("thin.second"));
  REQUIRE(thin);
  auto symmetric = ExtrudeExtentIntent::symmetric(ParameterId("distance"));
  REQUIRE(symmetric);
  auto additive_intent = ExtrudeFeatureIntent::create(symmetric.value(), 7.5, thin.value());
  REQUIRE(additive_intent);
  auto additive = Feature::create_additive_extrude(FeatureId("feature.symmetric"), "Symmetric",
                                                   SketchId("sketch.base"), additive_intent.value(),
                                                   ExtrudeDirection::OppositeSketchNormal);
  REQUIRE(additive);
  CHECK(additive.value().extrude_intent().extent().mode() == ExtrudeExtentMode::Symmetric);
  REQUIRE(additive.value().extrude_intent().taper_angle_deg());
  CHECK(*additive.value().extrude_intent().taper_angle_deg() == Catch::Approx(7.5));
  REQUIRE(additive.value().extrude_intent().thin());
  CHECK(additive.value().extrude_intent().thin()->mode() == ExtrudeThinMode::TwoSided);

  auto subtractive = Feature::create_subtractive_extrude(
      FeatureId("feature.cut"), "Cut", SketchId("sketch.base"), FeatureId("feature.base"),
      intent(ExtrudeExtentIntent::to_next()));
  REQUIRE(subtractive);
  CHECK(subtractive.value().extrude_intent().extent().mode() == ExtrudeExtentMode::ToNext);
  CHECK_FALSE(subtractive.value().extrude_intent().is_historical_subtractive_default());

  auto legacy_additive = Feature::create_additive_extrude(
      FeatureId("feature.legacy_add"), "Legacy", SketchId("sketch.base"), ParameterId("distance"));
  auto legacy_cut =
      Feature::create_subtractive_extrude(FeatureId("feature.legacy_cut"), "LegacyCut",
                                          SketchId("sketch.base"), FeatureId("feature.base"));
  REQUIRE(legacy_additive);
  REQUIRE(legacy_cut);
  CHECK(legacy_additive.value().extrude_intent().is_historical_additive_default());
  CHECK(legacy_cut.value().extrude_intent().is_historical_subtractive_default());
}

TEST_CASE("PartDocument validates richer Extrude parameters faces and dependencies",
          "[core][extrude-extent]") {
  auto document = extent_document();
  auto thin = ExtrudeThinIntent::create(ExtrudeThinMode::TwoSided, ParameterId("thin.first"),
                                        ParameterId("thin.second"));
  REQUIRE(thin);
  auto two_sided = ExtrudeExtentIntent::two_sided(ParameterId("first"), ParameterId("second"));
  REQUIRE(two_sided);
  REQUIRE(document.add_feature(
      explicit_additive("feature.parameters", two_sided.value(), 3.0, thin.value())));
  REQUIRE(document.add_feature(explicit_additive(
      "feature.face", ExtrudeExtentIntent::to_face(limit_face(SemanticFace::Top)).value())));

  const auto parameter_dependents = document.dependency_graph().direct_dependents("first");
  CHECK(std::find(parameter_dependents.begin(), parameter_dependents.end(), "feature.parameters") !=
        parameter_dependents.end());
  const auto face_dependents = document.dependency_graph().direct_dependents("feature.base");
  CHECK(std::find(face_dependents.begin(), face_dependents.end(), "feature.face") !=
        face_dependents.end());

  auto missing = ExtrudeExtentIntent::distance(ParameterId("missing"));
  REQUIRE(missing);
  CHECK(document.add_feature(explicit_additive("feature.missing", missing.value())).has_error());
}

TEST_CASE("Richer Extrude JSON roundtrips every extent plus taper and thin intent",
          "[core][extrude-extent][integration][part-construction-mvp]") {
  auto document = extent_document();
  auto symmetric = ExtrudeExtentIntent::symmetric(ParameterId("distance"));
  auto two_sided = ExtrudeExtentIntent::two_sided(ParameterId("first"), ParameterId("second"));
  auto to_face = ExtrudeExtentIntent::to_face(limit_face(SemanticFace::Top));
  auto between =
      ExtrudeExtentIntent::between(limit_face(SemanticFace::Bottom), limit_face(SemanticFace::Top));
  auto thin = ExtrudeThinIntent::create(ExtrudeThinMode::MidPlane, ParameterId("thin.first"));
  REQUIRE(symmetric);
  REQUIRE(two_sided);
  REQUIRE(to_face);
  REQUIRE(between);
  REQUIRE(thin);
  REQUIRE(document.add_feature(
      explicit_additive("feature.symmetric", symmetric.value(), -5.0, thin.value())));
  REQUIRE(document.add_feature(explicit_additive("feature.two_sided", two_sided.value())));
  REQUIRE(document.add_feature(
      explicit_additive("feature.through_all", ExtrudeExtentIntent::through_all())));
  REQUIRE(
      document.add_feature(explicit_additive("feature.to_next", ExtrudeExtentIntent::to_next())));
  REQUIRE(document.add_feature(explicit_additive("feature.to_face", to_face.value())));
  REQUIRE(document.add_feature(explicit_additive("feature.between", between.value())));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  const auto& legacy = json_feature(root, "feature.base");
  CHECK(legacy.at("length_parameter") == "distance");
  CHECK_FALSE(legacy.contains("extrude"));
  const auto& richer = json_feature(root, "feature.symmetric").at("extrude");
  CHECK(richer.at("extent") == "symmetric");
  CHECK(richer.at("first_distance_parameter") == "distance");
  CHECK(richer.at("taper_angle_deg") == Catch::Approx(-5.0));
  CHECK(richer.at("thin").at("mode") == "mid_plane");
  CHECK(json_feature(root, "feature.between").at("extrude").at("second_face").at("face") == "top");

  auto restored = deserialize_part_document_from_json(root.dump());
  REQUIRE(restored);
  REQUIRE(restored.value().feature_count() == document.feature_count());
  const Feature* restored_between = restored.value().find_feature(FeatureId("feature.between"));
  REQUIRE(restored_between != nullptr);
  CHECK(restored_between->extrude_intent().extent().mode() == ExtrudeExtentMode::Between);
  const Feature* restored_symmetric = restored.value().find_feature(FeatureId("feature.symmetric"));
  REQUIRE(restored_symmetric != nullptr);
  REQUIRE(restored_symmetric->extrude_intent().taper_angle_deg());
  CHECK(*restored_symmetric->extrude_intent().taper_angle_deg() == Catch::Approx(-5.0));
  REQUIRE(restored_symmetric->extrude_intent().thin());
  CHECK(restored_symmetric->extrude_intent().thin()->mode() == ExtrudeThinMode::MidPlane);

  auto serialized_again = serialize_part_document_to_json(restored.value());
  REQUIRE(serialized_again);
  CHECK(nlohmann::json::parse(serialized_again.value()) == root);
}

TEST_CASE("Richer Extrude JSON fails closed on malformed conditional fields",
          "[core][extrude-extent]") {
  auto document = extent_document();
  REQUIRE(
      document.add_feature(explicit_additive("feature.to_next", ExtrudeExtentIntent::to_next())));
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto root = nlohmann::json::parse(serialized.value());

  SECTION("unsupported extent") {
    json_feature(root, "feature.to_next").at("extrude").at("extent") = "up_to_magic";
    CHECK(deserialize_part_document_from_json(root.dump()).has_error());
  }
  SECTION("missing two-sided parameter") {
    auto& value = json_feature(root, "feature.to_next").at("extrude");
    value.at("extent") = "two_sided";
    value["first_distance_parameter"] = "first";
    CHECK(deserialize_part_document_from_json(root.dump()).has_error());
  }
  SECTION("invalid taper") {
    json_feature(root, "feature.to_next").at("extrude")["taper_angle_deg"] = 90.0;
    CHECK(deserialize_part_document_from_json(root.dump()).has_error());
  }
  SECTION("extent-forbidden field") {
    json_feature(root, "feature.to_next").at("extrude")["first_distance_parameter"] = "first";
    CHECK(deserialize_part_document_from_json(root.dump()).has_error());
  }
}
