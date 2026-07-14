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

PartDocument make_json_part() {
  auto part = PartDocument::create(DocumentId("part.feature_body_json"), "FeatureBodyJson");
  REQUIRE(part);
  auto quantity = Quantity::length_mm(10.0, "part.length");
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId("part.length"), "length", quantity.value());
  REQUIRE(parameter);
  REQUIRE(part.value().add_parameter(parameter.value()));
  auto plane = DatumPlane::xy();
  REQUIRE(plane);
  REQUIRE(part.value().add_datum_plane(plane.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(part.value().add_sketch(sketch.value()));
  REQUIRE(part.value().add_body(make_body("body.base")));
  REQUIRE(part.value().add_body(make_body("body.result")));
  return part.value();
}

Feature make_feature(const char* id, FeatureBodyOperationMode mode, std::optional<BodyId> target,
                     std::optional<BodyId> produced) {
  auto feature = Feature::create_additive_extrude(FeatureId(id), id, SketchId("sketch.base"),
                                                  ParameterId("part.length"));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(mode, std::move(target), std::move(produced));
  REQUIRE(context);
  auto updated = feature.value().with_body_result_context(std::move(context.value()));
  REQUIRE(updated);
  return updated.value();
}

nlohmann::json serialized_explicit_part() {
  auto part = make_json_part();
  REQUIRE(part.add_feature(make_feature("feature.new", FeatureBodyOperationMode::NewBody,
                                        std::nullopt, BodyId("body.base"))));
  REQUIRE(part.add_feature(make_feature("feature.join", FeatureBodyOperationMode::Join,
                                        BodyId("body.base"), std::nullopt)));
  const auto serialized = serialize_part_document_to_json(part);
  REQUIRE(serialized);
  return nlohmann::json::parse(serialized.value());
}

} // namespace

TEST_CASE("Feature Body operation JSON roundtrips explicit and implicit result identity",
          "[core][feature-body-operation-json][integration][part-construction-mvp]") {
  const auto root = serialized_explicit_part();
  REQUIRE(root.at("features").size() == 2U);
  const auto& new_body = root.at("features").at(0);
  CHECK(new_body.at("operation_mode") == "new_body");
  CHECK_FALSE(new_body.contains("target_body"));
  CHECK(new_body.at("produced_body") == "body.base");
  const auto& join = root.at("features").at(1);
  CHECK(join.at("operation_mode") == "join");
  CHECK(join.at("target_body") == "body.base");
  CHECK_FALSE(join.contains("produced_body"));

  const auto restored = deserialize_part_document_from_json(root.dump(2));
  REQUIRE(restored);
  REQUIRE(restored.value().feature_count() == 2U);
  REQUIRE(restored.value().features()[0].body_result_context().has_value());
  CHECK(restored.value().features()[0].body_result_context()->operation_mode() ==
        FeatureBodyOperationMode::NewBody);
  CHECK(restored.value().features()[0].body_result_context()->effective_produced_body().value() ==
        "body.base");
  REQUIRE(restored.value().features()[1].body_result_context().has_value());
  CHECK(restored.value().features()[1].body_result_context()->operation_mode() ==
        FeatureBodyOperationMode::Join);
  CHECK(restored.value().features()[1].body_result_context()->effective_produced_body().value() ==
        "body.base");

  const auto serialized_again = serialize_part_document_to_json(restored.value());
  REQUIRE(serialized_again);
  CHECK(nlohmann::json::parse(serialized_again.value()) == root);
}

TEST_CASE("Historical Feature JSON restores the exact null Body context default",
          "[core][feature-body-operation-json]") {
  auto part = make_json_part();
  auto legacy = Feature::create_additive_extrude(
      FeatureId("feature.legacy"), "Legacy", SketchId("sketch.base"), ParameterId("part.length"));
  REQUIRE(legacy);
  REQUIRE(part.add_feature(legacy.value()));
  const auto serialized = serialize_part_document_to_json(part);
  REQUIRE(serialized);
  auto root = nlohmann::json::parse(serialized.value());
  CHECK_FALSE(root.at("features").at(0).contains("operation_mode"));

  const auto restored = deserialize_part_document_from_json(root.dump());
  REQUIRE(restored);
  REQUIRE(restored.value().feature_count() == 1U);
  CHECK_FALSE(restored.value().features().front().body_result_context().has_value());
}

TEST_CASE("Feature Body operation JSON fails closed on malformed or missing references",
          "[core][feature-body-operation-json]") {
  SECTION("orphan body reference without mode") {
    auto root = serialized_explicit_part();
    root.at("features").at(0).erase("operation_mode");
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() ==
          "feature body references require operation_mode in part document json");
  }

  SECTION("unsupported mode") {
    auto root = serialized_explicit_part();
    root.at("features").at(0).at("operation_mode") = "merge";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() ==
          "unsupported feature body operation mode in part document json");
  }

  SECTION("fields must be strings") {
    auto root = serialized_explicit_part();
    root.at("features").at(0).at("produced_body") = 42;
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "feature body operation fields must be strings");
  }

  SECTION("invalid mode combination") {
    auto root = serialized_explicit_part();
    root.at("features").at(0)["target_body"] = "body.base";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "new-body feature result must not have target body");
  }

  SECTION("missing produced Body") {
    auto root = serialized_explicit_part();
    root.at("features").at(0).at("produced_body") = "body.missing";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "feature produced body must exist in part document");
  }
}
