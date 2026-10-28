#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include <TopExp_Explorer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <gp_Pnt.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <optional>
#include <cmath>
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

PartDocument cut_probe(double scale, Point2 tool_center, double tool_width = 2.0,
                       double tool_height = 2.0) {
  auto created = PartDocument::create(DocumentId("part.cut_probe"), "Cut diagnostics");
  REQUIRE(created);
  auto document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  add_box(document, "a", 10 * scale, 10 * scale, 10 * scale);
  add_box(document, "b", tool_width * scale, tool_height * scale, 20 * scale,
          {tool_center.x * scale, tool_center.y * scale});
  return document;
}

} // namespace

TEST_CASE("Cut distinguishes real removal from disjoint and tangent tools at different scales",
          "[geometry][body-boolean][cut-material-diagnostics]") {
  const double scale = GENERATE(0.001, 1.0, 1000.0);
  const double offset = GENERATE(0.0, 6.0, 100.0);
  CAPTURE(scale, offset);
  const auto document = cut_probe(scale, {offset, 0});
  auto cache = make_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  const auto target = *cache.find_body_shape(BodyId("body.a"));
  const auto tool = *cache.find_body_shape(BodyId("body.b"));
  const auto before = body_volume(cache, "body.a");
  const auto cut = BodyBooleanAdapter{}.combine(FeatureId("cut.probe"),
                                                BodyBooleanOperation::Subtract, target, {tool});
  if (offset == 0) {
    REQUIRE(cut);
    CHECK(RectangleExtrusionAdapter{}.summarize(cut.value()).volume_mm3 ==
          Catch::Approx(960 * scale * scale * scale).epsilon(1e-8));
    const auto* result = OcctShapeView::get(cut.value());
    REQUIRE(result);
    BRepClass3d_SolidClassifier center(*result, gp_Pnt(0, 0, 5 * scale), 1e-7);
    CHECK(center.State() == TopAbs_OUT);
    BRepClass3d_SolidClassifier material(*result, gp_Pnt(4 * scale, 0, 5 * scale), 1e-7);
    CHECK(material.State() == TopAbs_IN);
  } else {
    REQUIRE(cut.has_error());
    CHECK(cut.error().category() == ErrorCategory::Geometry);
    CHECK(cut.error().object_id() == "cut.probe");
    CHECK(cut.error().message().find("no measurable material") != std::string::npos);
  }
  CHECK(body_volume(cache, "body.a") == before);
  CHECK(RectangleExtrusionAdapter{}.summarize(target).volume_mm3 == before);
}

TEST_CASE("Cut reports full target removal separately from invalid solid inputs",
          "[geometry][body-boolean][cut-material-diagnostics]") {
  const auto document = cut_probe(1, {}, 20, 20);
  auto cache = make_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  const auto target = *cache.find_body_shape(BodyId("body.a"));
  const auto tool = *cache.find_body_shape(BodyId("body.b"));
  const auto removed = BodyBooleanAdapter{}.combine(FeatureId("cut.full"),
                                                    BodyBooleanOperation::Subtract, target, {tool});
  REQUIRE(removed.has_error());
  CHECK(removed.error().object_id() == "cut.full");
  CHECK(removed.error().message().find("entire target") != std::string::npos);
  auto face_ref = SemanticFaceReference::create(FeatureId("feature.a"), SemanticFace::Top);
  REQUIRE(face_ref);
  auto face = SurfaceAdapter{}.extract_semantic_face(FeatureId("cut.face"), document, cache,
                                                     face_ref.value());
  REQUIRE(face);
  const auto invalid = BodyBooleanAdapter{}.combine(
      FeatureId("cut.invalid"), BodyBooleanOperation::Subtract, target, {face.value()});
  REQUIRE(invalid.has_error());
  CHECK(invalid.error().message().find("solid") != std::string::npos);
  CHECK(invalid.error().message().find("no measurable material") == std::string::npos);
  CHECK(body_volume(cache, "body.a") == Catch::Approx(1000));
}

TEST_CASE("Cut evaluates all tools together and accepts small real material removal",
          "[geometry][body-boolean][cut-material-diagnostics]") {
  auto document = cut_probe(1, {100, 0});
  add_box(document, "c", 2, 2, 20);
  add_box(document, "tiny", 0.001, 0.001, 20);
  auto cache = make_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  const auto target = *cache.find_body_shape(BodyId("body.a"));
  const auto disjoint = *cache.find_body_shape(BodyId("body.b"));
  const auto overlapping = *cache.find_body_shape(BodyId("body.c"));
  const auto tiny = *cache.find_body_shape(BodyId("body.tiny"));
  for (const auto& tools :
       {std::vector{disjoint, overlapping}, std::vector{overlapping, disjoint}}) {
    auto result = BodyBooleanAdapter{}.combine(FeatureId("cut.multi"),
                                               BodyBooleanOperation::Subtract, target, tools);
    REQUIRE(result);
    CHECK(RectangleExtrusionAdapter{}.summarize(result.value()).volume_mm3 == Catch::Approx(960));
  }
  auto result = BodyBooleanAdapter{}.combine(FeatureId("cut.tiny"), BodyBooleanOperation::Subtract,
                                             target, {tiny});
  REQUIRE(result);
  const double volume = RectangleExtrusionAdapter{}.summarize(result.value()).volume_mm3;
  CHECK(volume < 1000);
  CHECK(1000 - volume == Catch::Approx(0.00001).margin(1e-10));
}

TEST_CASE("Legacy Through All validates aggregate holes and replays from the pre-cut producer",
          "[geometry][body-boolean][cut-material-diagnostics][boolean-cache-isolation]") {
  const bool pattern = GENERATE(false, true);
  const bool misses = GENERATE(false, true);
  auto document = cut_probe(1, {100, 100});
  REQUIRE(document.add_parameter(length_parameter("hole.diameter", 1)));
  auto sketch = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  if (pattern) {
    REQUIRE(document.add_parameter(length_parameter("hole.radius", 3)));
    REQUIRE(document.add_parameter(Parameter::create_count(ParameterId("hole.count"), "Count",
                                                           Quantity::count(2, "Count").value())
                                       .value()));
    auto holes = CircularHolePattern::create(
        ProfileId("holes"), ParameterId("hole.radius"), ParameterId("hole.count"),
        ParameterId("hole.diameter"), {misses ? 100.0 : 6.0, 0});
    REQUIRE(holes);
    REQUIRE(sketch.value().add_profile(holes.value()));
  } else {
    auto circle = CircleProfile::create(ProfileId("circle"), ParameterId("hole.diameter"),
                                        {misses ? 100.0 : 0.0, 0});
    REQUIRE(circle);
    REQUIRE(sketch.value().add_profile(circle.value()));
  }
  REQUIRE(document.add_sketch(sketch.value()));
  auto cut = Feature::create_subtractive_extrude(FeatureId("cut.legacy"), "Through All",
                                                 sketch.value().id(), FeatureId("feature.a"));
  REQUIRE(cut);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("body.a"),
                                                  std::nullopt);
  REQUIRE(context);
  REQUIRE(document.add_feature(cut.value().with_body_result_context(context.value()).value()));
  auto cache = make_cache();
  auto result = GeometryRecomputeExecutor{}.execute_document(document, cache);
  if (misses) {
    REQUIRE(result.has_error());
    CHECK(result.error().object_id() == "cut.legacy");
    CHECK(result.error().message().find("no measurable material") != std::string::npos);
    CHECK(cache.body_shape_count() == 0);
  } else {
    REQUIRE(result);
    constexpr double pi = 3.14159265358979323846;
    const double expected = 1000 - pi * 0.25 * 10;
    CHECK(body_volume(cache, "body.a") == Catch::Approx(expected));
    const auto* source = cache.find_feature_shape(FeatureId("feature.a"));
    REQUIRE(source);
    CHECK(RectangleExtrusionAdapter{}.summarize(*source).volume_mm3 == Catch::Approx(1000));
    // A second execution must use the source before this feature, not cut the
    // already-cut body again and misdiagnose a valid feature as ineffective.
    REQUIRE(GeometryRecomputeExecutor{}.execute_subtractive_extrude(
        document, FeatureId("cut.legacy"), cache));
    CHECK(body_volume(cache, "body.a") == Catch::Approx(expected));
    const auto json = serialize_part_document_to_json(document).value();
    auto restored = deserialize_part_document_from_json(json);
    REQUIRE(restored);
    auto reopened = make_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored.value(), reopened));
    CHECK(body_volume(reopened, "body.a") == Catch::Approx(expected));
  }
}

TEST_CASE("Curved Cut spans preserve exact circles arcs and nested material across extents",
          "[geometry][cut-material-diagnostics][extrude-extent][nested-spline-region]") {
  const bool arcs = GENERATE(false, true);
  const bool ring = GENERATE(false, true);
  const auto mode = GENERATE(ExtrudeExtentMode::Distance, ExtrudeExtentMode::Symmetric,
                             ExtrudeExtentMode::TwoSided, ExtrudeExtentMode::ThroughAll);
  CAPTURE(arcs, ring, mode);
  auto document = cut_probe(1, {100, 100});
  REQUIRE(document.add_parameter(length_parameter("cut.depth", 8)));
  REQUIRE(document.add_parameter(length_parameter("cut.second", 7)));
  REQUIRE(document.add_parameter(length_parameter("outer.diameter", 4)));
  REQUIRE(document.add_parameter(length_parameter("inner.diameter", 2)));
  auto sketch = Sketch::create(SketchId("sketch.curved"), "Curved Cut", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  const ProfileId outer("profile.outer");
  if (arcs) {
    auto upper =
        ArcSegment::create_three_point(SketchEntityId("arc.upper"), {-2, 0}, {0, 2}, {2, 0});
    auto lower =
        ArcSegment::create_three_point(SketchEntityId("arc.lower"), {2, 0}, {0, -2}, {-2, 0});
    REQUIRE(upper);
    REQUIRE(lower);
    REQUIRE(sketch.value().add_entity(upper.value()));
    REQUIRE(sketch.value().add_entity(lower.value()));
    auto profile = ArcClosedProfile::create(outer, {upper.value().id(), lower.value().id()});
    REQUIRE(profile);
    REQUIRE(sketch.value().add_profile(profile.value()));
  } else {
    auto circle = CircleProfile::create(outer, ParameterId("outer.diameter"));
    REQUIRE(circle);
    REQUIRE(sketch.value().add_profile(circle.value()));
  }
  if (ring) {
    auto inner = CircleProfile::create(ProfileId("profile.inner"), ParameterId("inner.diameter"));
    REQUIRE(inner);
    REQUIRE(sketch.value().add_profile(inner.value()));
  }
  REQUIRE(document.add_sketch(sketch.value()));
  auto extent = ExtrudeExtentIntent::distance(ParameterId("cut.depth"));
  if (mode == ExtrudeExtentMode::Symmetric)
    extent = ExtrudeExtentIntent::symmetric(ParameterId("cut.depth"));
  else if (mode == ExtrudeExtentMode::TwoSided)
    extent = ExtrudeExtentIntent::two_sided(ParameterId("cut.depth"), ParameterId("cut.second"));
  else if (mode == ExtrudeExtentMode::ThroughAll)
    extent = Result<ExtrudeExtentIntent>::success(ExtrudeExtentIntent::through_all());
  REQUIRE(extent);
  auto intent = ExtrudeFeatureIntent::create(extent.value());
  REQUIRE(intent);
  auto feature = Feature::create_additive_extrude(FeatureId("cut.curved"), "Curved Cut",
                                                  sketch.value().id(), intent.value());
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("body.a"),
                                                  std::nullopt);
  REQUIRE(context);
  auto profiled = feature.value().with_profile(outer);
  REQUIRE(profiled);
  REQUIRE(document.add_feature(profiled.value().with_body_result_context(context.value()).value()));
  auto cache = make_cache();
  const auto result = GeometryRecomputeExecutor{}.execute_document(document, cache);
  INFO((result ? "accepted" : result.error().message()));
  REQUIRE(result);
  const double depth = mode == ExtrudeExtentMode::Symmetric    ? 4
                       : mode == ExtrudeExtentMode::ThroughAll ? 10
                                                               : 8;
  constexpr double pi = 3.14159265358979323846;
  CHECK(body_volume(cache, "body.a") ==
        Catch::Approx(1000 - pi * (ring ? 3 : 4) * depth).margin(1e-7));
  const auto* raw = OcctShapeView::get(*cache.find_body_shape(BodyId("body.a")));
  REQUIRE(raw);
  CHECK(BRepClass3d_SolidClassifier(*raw, gp_Pnt(0, 0, 2), 1e-7).State() ==
        (ring ? TopAbs_IN : TopAbs_OUT));
  CHECK(BRepClass3d_SolidClassifier(*raw, gp_Pnt(1.5, 0, 2), 1e-7).State() == TopAbs_OUT);
  CHECK(BRepClass3d_SolidClassifier(*raw, gp_Pnt(4, 0, 2), 1e-7).State() == TopAbs_IN);
  REQUIRE(GeometryRecomputeExecutor{}.execute_additive_extrude(document, FeatureId("cut.curved"),
                                                               cache));
  CHECK(body_volume(cache, "body.a") ==
        Catch::Approx(1000 - pi * (ring ? 3 : 4) * depth).margin(1e-7));
}

TEST_CASE("Body boolean Geometry executes Add Subtract and Intersect",
          "[geometry][body-boolean][integration][part-construction-mvp]") {
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

TEST_CASE("Interleaved translated cut rotate and scale retain geometry across JSON and incremental "
          "replay",
          "[geometry][body-boolean][body-transform][feature-order]") {
  auto created = PartDocument::create(DocumentId("part.interleaved"), "Interleaved body history");
  REQUIRE(created);
  auto document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  add_box(document, "a", 20, 10, 5, {25, 0});
  add_box(document, "b", 20, 10, 5, {5, 0});
  auto translated = BodyTransform::create_translate(BodyTransformId("translate"), BodyId("body.a"),
                                                    BodyTransformCoordinateSpace::World,
                                                    std::nullopt, {-15, 0, 0}, false, false);
  REQUIRE(translated);
  REQUIRE(document.add_body_transform(translated.value()));
  auto cut = BodyBooleanFeature::create(FeatureId("cut"), BodyBooleanOperation::Subtract,
                                        BodyId("body.a"), {BodyId("body.b")},
                                        BodyBooleanResultMode::ModifyTarget, std::nullopt, true);
  REQUIRE(cut);
  REQUIRE(document.add_body_boolean_feature(cut.value()));
  auto axis = BodyTransformRotationAxis::create_explicit({}, {0, 0, 1});
  REQUIRE(axis);
  auto rotated = BodyTransform::create_rotate(BodyTransformId("rotate"), BodyId("body.a"),
                                              BodyTransformCoordinateSpace::World, std::nullopt,
                                              axis.value(), 90, false, false);
  REQUIRE(rotated);
  REQUIRE(document.add_body_transform(rotated.value()));
  auto scaled = BodyTransform::create_uniform_scale(BodyTransformId("scale"), BodyId("body.a"),
                                                    BodyTransformCoordinateSpace::World,
                                                    std::nullopt, 2, {}, false, false);
  REQUIRE(scaled);
  REQUIRE(document.add_body_transform(scaled.value()));
  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  for (auto* state : {&document, &restored.value()}) {
    auto cache = make_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(*state, cache));
    const auto check = [&] {
      CHECK(body_volume(cache, "body.a") == Catch::Approx(2000));
      CHECK(body_volume(cache, "body.b") == Catch::Approx(1000));
      auto bounds = BodyTransformAdapter{}.bounds(*cache.find_body_shape(BodyId("body.a")));
      REQUIRE(bounds);
      CHECK(bounds.value().minimum.y == Catch::Approx(30).margin(1e-5));
      CHECK(bounds.value().maximum.y == Catch::Approx(40).margin(1e-5));
    };
    check();
    state->mark_all_clean();
    REQUIRE(state->mark_body_transform_changed(BodyTransformId("translate")));
    const auto plan = state->create_recompute_plan();
    REQUIRE(plan);
    REQUIRE(GeometryRecomputeExecutor{}.execute_plan(*state, plan.value(), cache));
    check();
    CHECK(serialize_part_document_to_json(*state).value() == serialized.value());
  }
}

TEST_CASE("Moving a retained tool out of an associative Cut rejects replay atomically",
          "[geometry][body-boolean][cut-material-removal][body-transform]") {
  auto document =
      make_document(BodyBooleanOperation::Subtract, BodyBooleanResultMode::ModifyTarget, true);
  auto cache = make_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  const auto target_volume = body_volume(cache, "body.a");
  const auto tool_volume = body_volume(cache, "body.b");
  auto moved = BodyTransform::create_translate(BodyTransformId("move.tool"), BodyId("body.b"),
                                               BodyTransformCoordinateSpace::World, std::nullopt,
                                               {100, 0, 0}, false, false);
  REQUIRE(moved);
  REQUIRE(document.add_body_transform(moved.value()));
  const auto serialized = serialize_part_document_to_json(document).value();
  auto restored = deserialize_part_document_from_json(serialized);
  REQUIRE(restored);
  for (auto* state : {&document, &restored.value()}) {
    const auto rejected = GeometryRecomputeExecutor{}.execute_document(*state, cache);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().object_id() == "boolean.combine");
    CHECK(rejected.error().message().find("no measurable material") != std::string::npos);
    CHECK(body_volume(cache, "body.a") == Catch::Approx(target_volume));
    CHECK(body_volume(cache, "body.b") == Catch::Approx(tool_volume));
    const auto bounds = BodyTransformAdapter{}.bounds(*cache.find_body_shape(BodyId("body.b")));
    REQUIRE(bounds);
    CHECK(bounds.value().maximum.x < 100);
    CHECK(serialize_part_document_to_json(*state).value() == serialized);
  }
}

TEST_CASE("Overlapping Union removes shared coplanar seams and replays one solid boundary",
          "[geometry][body-boolean][union-boundary]") {
  auto part = PartDocument::create(DocumentId("union.boundary"), "Union boundary").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  add_box(part, "a", 20, 10, 5);
  add_box(part, "b", 20, 10, 5, {10, 0});
  REQUIRE(part.add_body_boolean_feature(
      BodyBooleanFeature::create(FeatureId("union"), BodyBooleanOperation::Add, BodyId("body.a"),
                                 {BodyId("body.b")}, BodyBooleanResultMode::ModifyTarget,
                                 std::nullopt, false)
          .value()));
  const auto serialized = serialize_part_document_to_json(part).value();
  auto restored = deserialize_part_document_from_json(serialized);
  REQUIRE(restored);
  auto cache = make_cache();
  for (auto* state : {&part, &restored.value()}) {
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(*state, cache));
    CHECK(body_volume(cache, "body.a") == Catch::Approx(1500));
    CHECK_FALSE(cache.find_body_shape(BodyId("body.b")));
    const auto* shape = OcctShapeView::get(*cache.find_body_shape(BodyId("body.a")));
    REQUIRE(shape);
    std::size_t faces = 0;
    for (TopExp_Explorer face(*shape, TopAbs_FACE); face.More(); face.Next())
      ++faces;
    CHECK(faces == 6);
    CHECK(RectangleExtrusionAdapter{}
              .summarize(*cache.find_body_shape(BodyId("body.a")))
              .solid_count == 1);
    CHECK(serialize_part_document_to_json(*state).value() == serialized);
  }
}
