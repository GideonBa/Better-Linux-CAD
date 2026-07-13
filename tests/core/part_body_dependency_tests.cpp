#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace blcad;

namespace {

Body make_body(const char* id) {
  auto body = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(body);
  return body.value();
}

PartDocument make_dependency_part() {
  auto part = PartDocument::create(DocumentId("part.body_dependency"), "BodyDependency");
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
  for (const auto* id : {"body.base", "body.result", "body.unused"})
    REQUIRE(part.value().add_body(make_body(id)));
  return part.value();
}

Feature additive(const char* id, FeatureBodyOperationMode mode, std::optional<BodyId> target,
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

bool contains(const std::vector<std::string>& values, const char* value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

} // namespace

TEST_CASE("Feature Body operations build an acyclic producer-state dependency chain",
          "[core][part-body-dependency]") {
  auto part = make_dependency_part();
  REQUIRE(part.add_feature(additive("feature.new", FeatureBodyOperationMode::NewBody, std::nullopt,
                                    BodyId("body.base"))));
  REQUIRE(part.add_feature(additive("feature.modify", FeatureBodyOperationMode::Join,
                                    BodyId("body.base"), std::nullopt)));
  REQUIRE(part.add_feature(additive("feature.consume", FeatureBodyOperationMode::Intersect,
                                    BodyId("body.base"), BodyId("body.result"))));

  const auto& graph = part.dependency_graph();
  CHECK(graph.has_node("body:body.base"));
  CHECK(graph.has_node("body:body.result"));
  CHECK(graph.has_dependency("feature.new", "feature.modify"));
  CHECK(graph.has_dependency("feature.modify", "body:body.base"));
  CHECK_FALSE(graph.has_dependency("body:body.base", "feature.modify"));
  CHECK(graph.has_dependency("body:body.base", "feature.consume"));
  CHECK(graph.has_dependency("feature.consume", "body:body.result"));
  CHECK_FALSE(graph.has_cycle());
}

TEST_CASE("Feature and Body invalidation follows the body result chain",
          "[core][part-body-dependency]") {
  auto part = make_dependency_part();
  REQUIRE(part.add_feature(additive("feature.new", FeatureBodyOperationMode::NewBody, std::nullopt,
                                    BodyId("body.base"))));
  REQUIRE(part.add_feature(additive("feature.modify", FeatureBodyOperationMode::Join,
                                    BodyId("body.base"), std::nullopt)));
  REQUIRE(part.add_feature(additive("feature.consume", FeatureBodyOperationMode::Cut,
                                    BodyId("body.base"), BodyId("body.result"))));

  const auto affected = part.mark_feature_changed(FeatureId("feature.new"));
  REQUIRE(affected);
  CHECK(contains(affected.value(), "feature.modify"));
  CHECK(contains(affected.value(), "body:body.base"));
  CHECK(contains(affected.value(), "feature.consume"));
  CHECK(contains(affected.value(), "body:body.result"));
  const auto plan = part.create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("feature.modify"));
  CHECK(plan.value().contains("body:body.base"));
  CHECK(plan.value().contains("feature.consume"));
  CHECK(plan.value().contains("body:body.result"));

  part.mark_all_clean();
  const auto body_affected = part.mark_body_changed(BodyId("body.base"));
  REQUIRE(body_affected);
  CHECK(contains(body_affected.value(), "feature.consume"));
  CHECK(contains(body_affected.value(), "body:body.result"));
  CHECK_FALSE(contains(body_affected.value(), "feature.modify"));
}

TEST_CASE("Body removal rejects feature authority and cleans unused graph state",
          "[core][part-body-dependency]") {
  auto part = make_dependency_part();
  REQUIRE(part.add_feature(additive("feature.new", FeatureBodyOperationMode::NewBody, std::nullopt,
                                    BodyId("body.base"))));

  const auto rejected = part.remove_body(BodyId("body.base"));
  REQUIRE(rejected.has_error());
  CHECK(rejected.error().message() == "body with dependent model intent cannot be removed");
  CHECK(part.find_body(BodyId("body.base")) != nullptr);

  REQUIRE(part.dependency_graph().has_node("body:body.unused"));
  REQUIRE(part.invalidation_state().has_node("body:body.unused"));
  REQUIRE(part.remove_body(BodyId("body.unused")));
  CHECK_FALSE(part.dependency_graph().has_node("body:body.unused"));
  CHECK_FALSE(part.invalidation_state().has_node("body:body.unused"));
}

TEST_CASE("Feature Body producer conflicts and cycles fail transactionally",
          "[core][part-body-dependency]") {
  SECTION("duplicate producer") {
    auto part = make_dependency_part();
    REQUIRE(part.add_feature(additive("feature.first", FeatureBodyOperationMode::NewBody,
                                      std::nullopt, BodyId("body.base"))));
    const auto duplicate = part.add_feature(additive(
        "feature.second", FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.base")));
    REQUIRE(duplicate.has_error());
    CHECK(duplicate.error().message() == "feature produced body already has a producing feature");
    CHECK(part.feature_count() == 1U);
    CHECK_FALSE(part.dependency_graph().has_cycle());
  }

  SECTION("cycle") {
    auto part = make_dependency_part();
    REQUIRE(part.add_feature(additive("feature.source", FeatureBodyOperationMode::NewBody,
                                      std::nullopt, BodyId("body.base"))));
    REQUIRE(part.add_feature(additive("feature.consumer", FeatureBodyOperationMode::Join,
                                      BodyId("body.base"), BodyId("body.result"))));
    auto cut =
        Feature::create_subtractive_extrude(FeatureId("feature.cycle"), "Cycle",
                                            SketchId("sketch.base"), FeatureId("feature.consumer"));
    REQUIRE(cut);
    auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                    BodyId("body.base"), std::nullopt);
    REQUIRE(context);
    auto with_context = cut.value().with_body_result_context(context.value());
    REQUIRE(with_context);
    const auto rejected = part.add_feature(with_context.value());
    REQUIRE(rejected.has_error());
    CHECK(rejected.error().message() ==
          "feature body operation must not create a dependency cycle");
    CHECK(part.feature_count() == 2U);
    CHECK_FALSE(part.dependency_graph().has_cycle());
  }
}
