#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;

namespace {

Body body(const char* id) {
  auto value = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(value);
  return value.value();
}

PartDocument document_with_bodies() {
  auto document = PartDocument::create(DocumentId("part.boolean"), "BooleanPart");
  REQUIRE(document);
  for (const char* id : {"body.a", "body.b", "body.c", "body.result"})
    REQUIRE(document.value().add_body(body(id)));
  return document.value();
}

BodyBooleanFeature boolean_feature(const char* id, BodyBooleanOperation operation,
                                   const char* target, std::vector<BodyId> tools,
                                   BodyBooleanResultMode mode = BodyBooleanResultMode::ModifyTarget,
                                   std::optional<BodyId> produced = std::nullopt,
                                   bool keep_tools = false) {
  auto feature =
      BodyBooleanFeature::create(FeatureId(id), operation, BodyId(target), std::move(tools), mode,
                                 std::move(produced), keep_tools);
  REQUIRE(feature);
  return feature.value();
}

} // namespace

TEST_CASE("BodyBooleanFeature freezes operations result modes and deterministic tools",
          "[core][body-boolean]") {
  CHECK(to_string(BodyBooleanOperation::Add) == "add");
  CHECK(to_string(BodyBooleanOperation::Subtract) == "subtract");
  CHECK(to_string(BodyBooleanOperation::Intersect) == "intersect");
  CHECK(to_string(BodyBooleanResultMode::ModifyTarget) == "modify_target");
  CHECK(to_string(BodyBooleanResultMode::NewBody) == "new_body");

  const auto modifying =
      BodyBooleanFeature::create(FeatureId("boolean.add"), BodyBooleanOperation::Add,
                                 BodyId("body.a"), {BodyId("body.c"), BodyId("body.b")},
                                 BodyBooleanResultMode::ModifyTarget, std::nullopt, true);
  REQUIRE(modifying);
  CHECK(modifying.value().id().value() == "boolean.add");
  CHECK(modifying.value().operation() == BodyBooleanOperation::Add);
  CHECK(modifying.value().target_body().value() == "body.a");
  REQUIRE(modifying.value().tool_bodies().size() == 2);
  CHECK(modifying.value().tool_bodies()[0].value() == "body.b");
  CHECK(modifying.value().tool_bodies()[1].value() == "body.c");
  CHECK(modifying.value().effective_result_body().value() == "body.a");
  CHECK(modifying.value().keep_tool_bodies());

  const auto new_body = BodyBooleanFeature::create(
      FeatureId("boolean.intersect"), BodyBooleanOperation::Intersect, BodyId("body.a"),
      {BodyId("body.b")}, BodyBooleanResultMode::NewBody, BodyId("body.result"), false);
  REQUIRE(new_body);
  CHECK(new_body.value().produced_body()->value() == "body.result");
  CHECK(new_body.value().effective_result_body().value() == "body.result");
  CHECK_FALSE(new_body.value().keep_tool_bodies());
}

TEST_CASE("BodyBooleanFeature rejects ambiguous or incomplete intent", "[core][body-boolean]") {
  const auto create = [](FeatureId id, BodyId target, std::vector<BodyId> tools,
                         BodyBooleanResultMode mode,
                         std::optional<BodyId> produced = std::nullopt) {
    return BodyBooleanFeature::create(std::move(id), BodyBooleanOperation::Subtract,
                                      std::move(target), std::move(tools), mode,
                                      std::move(produced), false);
  };

  CHECK(
      create(FeatureId(), BodyId("body.a"), {BodyId("body.b")}, BodyBooleanResultMode::ModifyTarget)
          .has_error());
  CHECK(create(FeatureId("boolean.empty_target"), BodyId(), {BodyId("body.b")},
               BodyBooleanResultMode::ModifyTarget)
            .has_error());
  CHECK(create(FeatureId("boolean.no_tools"), BodyId("body.a"), {},
               BodyBooleanResultMode::ModifyTarget)
            .has_error());
  CHECK(create(FeatureId("boolean.target_tool"), BodyId("body.a"), {BodyId("body.a")},
               BodyBooleanResultMode::ModifyTarget)
            .has_error());
  CHECK(create(FeatureId("boolean.duplicate"), BodyId("body.a"),
               {BodyId("body.b"), BodyId("body.b")}, BodyBooleanResultMode::ModifyTarget)
            .has_error());
  CHECK(create(FeatureId("boolean.modify_result"), BodyId("body.a"), {BodyId("body.b")},
               BodyBooleanResultMode::ModifyTarget, BodyId("body.result"))
            .has_error());
  CHECK(create(FeatureId("boolean.new_missing"), BodyId("body.a"), {BodyId("body.b")},
               BodyBooleanResultMode::NewBody)
            .has_error());
  CHECK(create(FeatureId("boolean.new_alias"), BodyId("body.a"), {BodyId("body.b")},
               BodyBooleanResultMode::NewBody, BodyId("body.b"))
            .has_error());
}

TEST_CASE("PartDocument connects BodyBooleanFeature dependencies and invalidation",
          "[core][body-boolean]") {
  PartDocument document = document_with_bodies();
  auto feature = boolean_feature("boolean.subtract", BodyBooleanOperation::Subtract, "body.a",
                                 {BodyId("body.c"), BodyId("body.b")},
                                 BodyBooleanResultMode::NewBody, BodyId("body.result"), true);

  REQUIRE(document.add_body_boolean_feature(feature));
  CHECK(document.body_boolean_feature_count() == 1);
  REQUIRE(document.find_body_boolean_feature(FeatureId("boolean.subtract")) != nullptr);
  const DependencyGraph& graph = document.dependency_graph();
  CHECK(graph.has_dependency("body:body.a", "boolean.subtract"));
  CHECK(graph.has_dependency("body:body.b", "boolean.subtract"));
  CHECK(graph.has_dependency("body:body.c", "boolean.subtract"));
  CHECK(graph.has_dependency("boolean.subtract", "body:body.result"));

  document.mark_all_clean();
  const auto affected = document.mark_body_changed(BodyId("body.b"));
  REQUIRE(affected);
  REQUIRE(document.invalidation_state().find("boolean.subtract") != nullptr);
  REQUIRE(document.invalidation_state().find("body:body.result") != nullptr);
  CHECK(document.invalidation_state().find("boolean.subtract")->status ==
        InvalidationStatus::Dirty);
  CHECK(document.invalidation_state().find("body:body.result")->status ==
        InvalidationStatus::Dirty);
  const auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("boolean.subtract"));
  CHECK(plan.value().contains("body:body.result"));
}

TEST_CASE("ModifyTarget advances the target producer and rejects real cycles transactionally",
          "[core][body-boolean]") {
  PartDocument document = document_with_bodies();
  REQUIRE(document.add_body_boolean_feature(
      boolean_feature("boolean.first", BodyBooleanOperation::Add, "body.a", {BodyId("body.b")},
                      BodyBooleanResultMode::NewBody, BodyId("body.c"), true)));

  const auto rejected = document.add_body_boolean_feature(boolean_feature(
      "boolean.cycle", BodyBooleanOperation::Intersect, "body.a", {BodyId("body.c")}));
  REQUIRE(rejected.has_error());
  CHECK(rejected.error().category() == ErrorCategory::Dependency);
  CHECK(document.body_boolean_feature_count() == 1);
  CHECK(document.find_body_boolean_feature(FeatureId("boolean.cycle")) == nullptr);
  CHECK_FALSE(document.dependency_graph().has_node("boolean.cycle"));

  PartDocument chain = document_with_bodies();
  REQUIRE(chain.add_body_boolean_feature(
      boolean_feature("boolean.base", BodyBooleanOperation::Add, "body.b", {BodyId("body.c")},
                      BodyBooleanResultMode::NewBody, BodyId("body.a"), true)));
  REQUIRE(chain.add_body_boolean_feature(boolean_feature(
      "boolean.modify", BodyBooleanOperation::Subtract, "body.a", {BodyId("body.b")})));
  CHECK(chain.dependency_graph().has_dependency("boolean.base", "boolean.modify"));
  CHECK(chain.dependency_graph().has_dependency("boolean.modify", "body:body.a"));
  CHECK_FALSE(chain.dependency_graph().has_dependency("body:body.a", "boolean.modify"));
}

TEST_CASE("PartDocument validates Boolean body references producers ids and removal",
          "[core][body-boolean]") {
  PartDocument document = document_with_bodies();
  auto missing_tool = boolean_feature("boolean.missing", BodyBooleanOperation::Add, "body.a",
                                      {BodyId("body.missing")});
  CHECK(document.add_body_boolean_feature(missing_tool).has_error());

  REQUIRE(document.add_body_boolean_feature(
      boolean_feature("boolean.result", BodyBooleanOperation::Add, "body.a", {BodyId("body.b")},
                      BodyBooleanResultMode::NewBody, BodyId("body.result"), false)));
  const auto duplicate_result = document.add_body_boolean_feature(boolean_feature(
      "boolean.duplicate_result", BodyBooleanOperation::Subtract, "body.c", {BodyId("body.b")},
      BodyBooleanResultMode::NewBody, BodyId("body.result"), true));
  REQUIRE(duplicate_result.has_error());
  CHECK(duplicate_result.error().category() == ErrorCategory::Dependency);

  CHECK(document.remove_body(BodyId("body.a")).has_error());
  CHECK(document.remove_body(BodyId("body.b")).has_error());
  CHECK(document.remove_body(BodyId("body.result")).has_error());
  CHECK(document.remove_body(BodyId("body.c")));
}

TEST_CASE("BodyBooleanFeature JSON is canonical additive and backward compatible",
          "[core][body-boolean][integration][part-construction-mvp]") {
  PartDocument document = document_with_bodies();
  REQUIRE(document.add_body_boolean_feature(
      boolean_feature("boolean.subtract", BodyBooleanOperation::Subtract, "body.a",
                      {BodyId("body.c"), BodyId("body.b")}, BodyBooleanResultMode::NewBody,
                      BodyId("body.result"), false)));

  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  REQUIRE(root.at("body_booleans").size() == 1);
  const auto& value = root.at("body_booleans").at(0);
  CHECK(value.at("id") == "boolean.subtract");
  CHECK(value.at("operation") == "subtract");
  CHECK(value.at("target_body") == "body.a");
  CHECK(value.at("tool_bodies") == nlohmann::json::array({"body.b", "body.c"}));
  CHECK(value.at("result_mode") == "new_body");
  CHECK(value.at("produced_body") == "body.result");
  CHECK(value.at("keep_tool_bodies") == false);

  const auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().body_boolean_feature_count() == 1);
  CHECK(restored.value().body_boolean_features().front() ==
        document.body_boolean_features().front());
  const auto serialized_again = serialize_part_document_to_json(restored.value());
  REQUIRE(serialized_again);
  CHECK(serialized_again.value() == serialized.value());

  auto historical = root;
  historical.erase("body_booleans");
  const auto historical_result = deserialize_part_document_from_json(historical.dump());
  REQUIRE(historical_result);
  CHECK(historical_result.value().body_boolean_feature_count() == 0);
}

TEST_CASE("BodyBooleanFeature JSON rejects malformed intent", "[core][body-boolean]") {
  PartDocument document = document_with_bodies();
  REQUIRE(document.add_body_boolean_feature(
      boolean_feature("boolean.add", BodyBooleanOperation::Add, "body.a", {BodyId("body.b")})));
  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto canonical = nlohmann::json::parse(serialized.value());

  auto malformed = canonical;
  malformed["body_booleans"] = "invalid";
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());

  malformed = canonical;
  malformed["body_booleans"][0]["operation"] = "merge";
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());

  malformed = canonical;
  malformed["body_booleans"][0]["tool_bodies"] = nlohmann::json::array();
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());

  malformed = canonical;
  malformed["body_booleans"][0]["keep_tool_bodies"] = "false";
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());

  malformed = canonical;
  malformed["body_booleans"][0]["target_body"] = "body.missing";
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
}
