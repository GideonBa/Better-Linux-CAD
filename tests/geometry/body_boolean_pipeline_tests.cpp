#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter length_parameter(const std::string& id, double millimeters) {
  auto quantity = Quantity::length_mm(millimeters, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body solid_body(const std::string& id) {
  auto body = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(body);
  return body.value();
}

void add_box(PartDocument& document, const std::string& suffix, double width, double height,
             double depth, Point2 center = {}) {
  REQUIRE(document.add_parameter(length_parameter(suffix + ".width", width)));
  REQUIRE(document.add_parameter(length_parameter(suffix + ".height", height)));
  REQUIRE(document.add_parameter(length_parameter(suffix + ".depth", depth)));

  auto sketch =
      Sketch::create(SketchId("sketch." + suffix), suffix, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile." + suffix),
                                             ParameterId(suffix + ".width"),
                                             ParameterId(suffix + ".height"), center);
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  REQUIRE(document.add_body(solid_body("body." + suffix)));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature." + suffix), suffix, SketchId("sketch." + suffix),
      ParameterId(suffix + ".depth"));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body." + suffix));
  REQUIRE(context);
  auto attached = feature.value().with_body_result_context(context.value());
  REQUIRE(attached);
  REQUIRE(document.add_feature(attached.value()));
}

PartDocument make_document(BodyBooleanOperation operation, BodyBooleanResultMode result_mode,
                           bool keep_tools, std::vector<BodyId> tools = {BodyId("body.b")},
                           Point2 tool_center = {}) {
  auto document = PartDocument::create(DocumentId("part.boolean_geometry"), "BooleanGeometry");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  add_box(document.value(), "a", 40.0, 30.0, 5.0);
  add_box(document.value(), "b", 20.0, 10.0, 7.0, tool_center);
  if (tools.size() > 1U)
    add_box(document.value(), "c", 10.0, 5.0, 9.0);

  std::optional<BodyId> result_body;
  if (result_mode == BodyBooleanResultMode::NewBody) {
    REQUIRE(document.value().add_body(solid_body("body.result")));
    result_body = BodyId("body.result");
  }
  auto body_boolean = BodyBooleanFeature::create(
      FeatureId("boolean.combine"), operation, BodyId("body.a"), std::move(tools), result_mode,
      result_body, keep_tools);
  REQUIRE(body_boolean);
  REQUIRE(document.value().add_body_boolean_feature(body_boolean.value()));
  return document.value();
}

ShapeCache make_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("cache.body_boolean"));
  REQUIRE(cache);
  return cache.value();
}

double body_volume(const ShapeCache& cache, const char* body_id) {
  const GeometryShape* shape = cache.find_body_shape(BodyId(body_id));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

} // namespace

TEST_CASE("Body boolean Geometry executes Add Subtract and Intersect",
          "[geometry][body-boolean]") {
  struct Case {
    BodyBooleanOperation operation;
    double expected_volume;
  };
  for (const Case test_case : {Case{BodyBooleanOperation::Add, 6400.0},
                               Case{BodyBooleanOperation::Subtract, 5000.0},
                               Case{BodyBooleanOperation::Intersect, 1000.0}}) {
    PartDocument document = make_document(test_case.operation, BodyBooleanResultMode::NewBody, true);
    ShapeCache cache = make_cache();

    const auto summary = GeometryRecomputeExecutor{}.execute_document(document, cache);

    REQUIRE(summary);
    CHECK(summary.value().executed_feature_count == 3U);
    CHECK(body_volume(cache, "body.result") == Catch::Approx(test_case.expected_volume));
    CHECK(cache.find_body_shape(BodyId("body.a")) != nullptr);
    CHECK(cache.find_body_shape(BodyId("body.b")) != nullptr);
    REQUIRE(cache.find_body_result(BodyId("body.result")) != nullptr);
    CHECK(cache.find_body_result(BodyId("body.result"))->source_feature_id.value() ==
          "boolean.combine");
  }
}

TEST_CASE("Body boolean Geometry applies result and tool retention modes",
          "[geometry][body-boolean]") {
  SECTION("ModifyTarget consumes tools") {
    PartDocument document = make_document(BodyBooleanOperation::Subtract,
                                          BodyBooleanResultMode::ModifyTarget, false);
    const BodyBooleanFeature intent = document.body_boolean_features().front();
    ShapeCache cache = make_cache();

    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));

    CHECK(body_volume(cache, "body.a") == Catch::Approx(5000.0));
    CHECK(cache.find_body_shape(BodyId("body.b")) == nullptr);
    CHECK(cache.find_feature_shape(FeatureId("feature.b")) != nullptr);
    CHECK(document.body_count() == 2U);
    CHECK(document.body_boolean_features().front() == intent);
  }

  SECTION("NewBody retains the target and consumes tools") {
    PartDocument document = make_document(BodyBooleanOperation::Add,
                                          BodyBooleanResultMode::NewBody, false);
    ShapeCache cache = make_cache();
    const GeometryRecomputeExecutor executor;
    REQUIRE(executor.execute_additive_extrude(document, FeatureId("feature.a"), cache));
    REQUIRE(executor.execute_additive_extrude(document, FeatureId("feature.b"), cache));

    const auto result_index =
        executor.execute_body_boolean(document, FeatureId("boolean.combine"), cache);

    REQUIRE(result_index);
    REQUIRE(result_index.value() < cache.body_shapes().size());
    CHECK(cache.body_shapes()[result_index.value()].body_id.value() == "body.result");
    CHECK(body_volume(cache, "body.result") == Catch::Approx(6400.0));
    CHECK(body_volume(cache, "body.a") == Catch::Approx(6000.0));
    CHECK(cache.find_body_shape(BodyId("body.b")) == nullptr);
  }
}

TEST_CASE("Body boolean Geometry uses deterministic multi-tool order",
          "[geometry][body-boolean]") {
  PartDocument document = make_document(
      BodyBooleanOperation::Add, BodyBooleanResultMode::NewBody, true,
      {BodyId("body.c"), BodyId("body.b")});
  REQUIRE(document.body_boolean_features().front().tool_bodies().size() == 2U);
  CHECK(document.body_boolean_features().front().tool_bodies()[0].value() == "body.b");
  CHECK(document.body_boolean_features().front().tool_bodies()[1].value() == "body.c");
  ShapeCache cache = make_cache();

  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));

  CHECK(body_volume(cache, "body.result") == Catch::Approx(6500.0));
}

TEST_CASE("Body boolean incremental recompute restores consumed tool backing",
          "[geometry][body-boolean]") {
  PartDocument document = make_document(BodyBooleanOperation::Subtract,
                                        BodyBooleanResultMode::ModifyTarget, false);
  ShapeCache cache = make_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  CHECK(cache.find_body_shape(BodyId("body.b")) == nullptr);

  auto wider = Quantity::length_mm(50.0, "a.width");
  REQUIRE(wider);
  REQUIRE(document.set_parameter_value(ParameterId("a.width"), wider.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("feature.a"));
  CHECK(plan.value().contains("boolean.combine"));
  CHECK_FALSE(plan.value().contains("feature.b"));

  const auto summary = executor.execute_plan(document, plan.value(), cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  CHECK(body_volume(cache, "body.a") == Catch::Approx(6500.0));
  CHECK(cache.find_body_shape(BodyId("body.b")) == nullptr);
}

TEST_CASE("Body boolean failures identify feature or body and preserve the cache",
          "[geometry][body-boolean]") {
  SECTION("missing tool backing identifies the Body") {
    PartDocument document = make_document(BodyBooleanOperation::Add,
                                          BodyBooleanResultMode::NewBody, true);
    ShapeCache cache = make_cache();
    const GeometryRecomputeExecutor executor;
    REQUIRE(executor.execute_additive_extrude(document, FeatureId("feature.a"), cache));
    const double before = body_volume(cache, "body.a");

    const auto failed = executor.execute_body_boolean(document, FeatureId("boolean.combine"), cache);

    REQUIRE(failed.has_error());
    CHECK(failed.error().object_id() == "body.b");
    CHECK(body_volume(cache, "body.a") == Catch::Approx(before));
    CHECK(cache.find_body_shape(BodyId("body.result")) == nullptr);
  }

  SECTION("empty OCCT result identifies the Boolean feature") {
    PartDocument document = make_document(BodyBooleanOperation::Intersect,
                                          BodyBooleanResultMode::NewBody, true,
                                          {BodyId("body.b")}, Point2{100.0, 0.0});
    ShapeCache cache = make_cache();

    const auto failed = GeometryRecomputeExecutor{}.execute_document(document, cache);

    REQUIRE(failed.has_error());
    CHECK(failed.error().category() == ErrorCategory::Geometry);
    CHECK(failed.error().object_id() == "boolean.combine");
    CHECK(cache.body_shape_count() == 0U);
    CHECK(cache.feature_shape_count() == 0U);
  }
}
