#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;

namespace {

Body make_body(const char* id) {
  auto body = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(body);
  return body.value();
}

Parameter make_length_parameter(const char* id) {
  auto value = Quantity::length_mm(10.0, id);
  REQUIRE(value);
  auto parameter = Parameter::create_length(ParameterId(id), id, value.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_feature_part(bool with_bodies = true) {
  auto part = PartDocument::create(DocumentId("part.feature_body"), "FeatureBody");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.length")));
  auto plane = DatumPlane::xy();
  REQUIRE(plane);
  REQUIRE(part.value().add_datum_plane(plane.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(part.value().add_sketch(sketch.value()));
  if (with_bodies) {
    REQUIRE(part.value().add_body(make_body("body.base")));
    REQUIRE(part.value().add_body(make_body("body.result")));
  }
  return part.value();
}

Feature make_additive_feature(const char* id = "feature.base") {
  auto feature = Feature::create_additive_extrude(FeatureId(id), id, SketchId("sketch.base"),
                                                  ParameterId("part.length"));
  REQUIRE(feature);
  return feature.value();
}

Feature with_context(Feature feature, FeatureBodyOperationMode mode, std::optional<BodyId> target,
                     std::optional<BodyId> produced) {
  auto context = FeatureBodyResultContext::create(mode, std::move(target), std::move(produced));
  REQUIRE(context);
  auto updated = feature.with_body_result_context(std::move(context.value()));
  REQUIRE(updated);
  return updated.value();
}

} // namespace

TEST_CASE("Feature body result context freezes operation modes and result identity",
          "[core][feature-body-operation]") {
  const auto new_body = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                         std::nullopt, BodyId("body.new"));
  REQUIRE(new_body);
  CHECK(to_string(new_body.value().operation_mode()) == "new_body");
  CHECK_FALSE(new_body.value().target_body().has_value());
  REQUIRE(new_body.value().produced_body().has_value());
  CHECK(new_body.value().effective_produced_body().value() == "body.new");

  const auto join = FeatureBodyResultContext::create(FeatureBodyOperationMode::Join,
                                                     BodyId("body.base"), std::nullopt);
  REQUIRE(join);
  CHECK(to_string(join.value().operation_mode()) == "join");
  CHECK(join.value().effective_produced_body().value() == "body.base");

  const auto cut = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                    BodyId("body.base"), BodyId("body.cut_result"));
  REQUIRE(cut);
  CHECK(to_string(cut.value().operation_mode()) == "cut");
  CHECK(cut.value().effective_produced_body().value() == "body.cut_result");

  const auto intersect = FeatureBodyResultContext::create(FeatureBodyOperationMode::Intersect,
                                                          BodyId("body.base"), std::nullopt);
  REQUIRE(intersect);
  CHECK(to_string(intersect.value().operation_mode()) == "intersect");
  CHECK(intersect.value().effective_produced_body().value() == "body.base");
}

TEST_CASE("Feature body result context rejects invalid mode combinations",
          "[core][feature-body-operation]") {
  CHECK(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, BodyId("body.base"),
                                         BodyId("body.result"))
            .has_error());
  CHECK(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                         std::nullopt)
            .has_error());
  CHECK(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, BodyId())
            .has_error());
  CHECK(FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, std::nullopt, std::nullopt)
            .has_error());
  CHECK(FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId(), std::nullopt)
            .has_error());
  CHECK(FeatureBodyResultContext::create(FeatureBodyOperationMode::Intersect, BodyId("body.base"),
                                         BodyId())
            .has_error());

  const auto unsupported = FeatureBodyResultContext::create(
      static_cast<FeatureBodyOperationMode>(999), std::nullopt, BodyId("body.result"));
  REQUIRE(unsupported.has_error());
  CHECK(unsupported.error().message() == "unsupported feature body operation mode");
}

TEST_CASE("Feature preserves historical null context and supports immutable explicit context",
          "[core][feature-body-operation]") {
  const auto legacy = make_additive_feature();
  CHECK_FALSE(legacy.body_result_context().has_value());

  const auto explicit_feature =
      with_context(legacy, FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.result"));
  CHECK_FALSE(legacy.body_result_context().has_value());
  REQUIRE(explicit_feature.body_result_context().has_value());
  CHECK(explicit_feature.body_result_context()->operation_mode() ==
        FeatureBodyOperationMode::NewBody);
  CHECK(explicit_feature.body_result_context()->effective_produced_body().value() == "body.result");
  CHECK(explicit_feature.id() == legacy.id());
  CHECK(explicit_feature.input_sketch() == legacy.input_sketch());
}

TEST_CASE("PartDocument validates explicit feature Body references",
          "[core][feature-body-operation]") {
  SECTION("new body result must exist") {
    auto part = make_feature_part();
    auto feature = with_context(make_additive_feature(), FeatureBodyOperationMode::NewBody,
                                std::nullopt, BodyId("body.result"));
    REQUIRE(part.add_feature(std::move(feature)));
    REQUIRE(part.feature_count() == 1U);
    REQUIRE(part.features().front().body_result_context().has_value());
    CHECK(part.features().front().body_result_context()->effective_produced_body().value() ==
          "body.result");
  }

  SECTION("missing produced body fails closed") {
    auto part = make_feature_part();
    auto feature = with_context(make_additive_feature(), FeatureBodyOperationMode::NewBody,
                                std::nullopt, BodyId("body.missing"));
    const auto added = part.add_feature(std::move(feature));
    REQUIRE(added.has_error());
    CHECK(added.error().message() == "feature produced body must exist in part document");
  }

  SECTION("missing target body fails closed") {
    auto part = make_feature_part();
    auto feature = with_context(make_additive_feature(), FeatureBodyOperationMode::Join,
                                BodyId("body.missing"), std::nullopt);
    const auto added = part.add_feature(std::move(feature));
    REQUIRE(added.has_error());
    CHECK(added.error().message() == "feature target body must exist in part document");
  }

  SECTION("implicit target result is valid") {
    auto part = make_feature_part();
    auto feature = with_context(make_additive_feature(), FeatureBodyOperationMode::Join,
                                BodyId("body.base"), std::nullopt);
    REQUIRE(part.add_feature(std::move(feature)));
    CHECK(part.dependency_graph().has_node("body:body.base"));
  }

  SECTION("explicit result identity must exist") {
    auto part = make_feature_part();
    auto feature = with_context(make_additive_feature(), FeatureBodyOperationMode::Intersect,
                                BodyId("body.base"), BodyId("body.missing"));
    const auto added = part.add_feature(std::move(feature));
    REQUIRE(added.has_error());
    CHECK(added.error().message() == "feature produced body must exist in part document");
  }
}

TEST_CASE("Historical feature JSON remains unchanged and restores null Body context",
          "[core][feature-body-operation]") {
  auto part = make_feature_part(false);
  REQUIRE(part.add_feature(make_additive_feature()));

  const auto serialized = serialize_part_document_to_json(part);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  REQUIRE(root.at("features").size() == 1U);
  CHECK_FALSE(root.at("features").at(0).contains("operation_mode"));
  CHECK_FALSE(root.at("features").at(0).contains("target_body"));
  CHECK_FALSE(root.at("features").at(0).contains("produced_body"));

  const auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().feature_count() == 1U);
  CHECK_FALSE(restored.value().features().front().body_result_context().has_value());
  CHECK(restored.value().body_count() == 0U);
}
