#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body solid_body(const char* id) {
  auto body = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(body);
  return body.value();
}

Sketch rectangle_sketch(const char* id, const char* width, const char* height, Point2 center = {}) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId(std::string(id) + ".profile"),
                                            ParameterId(width), ParameterId(height), center);
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  return sketch.value();
}

ExtrudeFeatureIntent extrude_intent(ExtrudeExtentIntent extent,
                                    std::optional<double> taper = std::nullopt,
                                    std::optional<ExtrudeThinIntent> thin = std::nullopt) {
  auto value = ExtrudeFeatureIntent::create(std::move(extent), taper, std::move(thin));
  REQUIRE(value);
  return value.value();
}

FaceReference limit_face(SemanticFace face) {
  auto identity = SemanticFaceReference::create(FeatureId("feature.base"), face);
  REQUIRE(identity);
  auto reference =
      FaceReference::create_planar(PartFeatureInputRole::ExtrudeLimitFace, identity.value());
  REQUIRE(reference);
  return reference.value();
}

ShapeCache shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("cache.extrude_extent"));
  REQUIRE(cache);
  return cache.value();
}

ShapeSummary summary(const GeometryShape& shape) {
  return RectangleExtrusionAdapter{}.summarize(shape);
}

PartDocument simple_extent_document(ExtrudeFeatureIntent intent) {
  auto document = PartDocument::create(DocumentId("part.extent"), "Extent");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("width", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("height", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("first", 3.0)));
  REQUIRE(document.value().add_parameter(length_parameter("second", 5.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.value().add_sketch(rectangle_sketch("sketch.extent", "width", "height")));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.extent"), "Extent",
                                                  SketchId("sketch.extent"), std::move(intent));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument face_extent_document(ExtrudeFeatureIntent intent) {
  auto document = PartDocument::create(DocumentId("part.face_extent"), "FaceExtent");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 30.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 30.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("tool.width", 4.0)));
  REQUIRE(document.value().add_parameter(length_parameter("tool.height", 4.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(
      document.value().add_sketch(rectangle_sketch("sketch.base", "base.width", "base.height")));
  REQUIRE(
      document.value().add_sketch(rectangle_sketch("sketch.tool", "tool.width", "tool.height")));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));
  auto tool = Feature::create_additive_extrude(FeatureId("feature.tool"), "Tool",
                                               SketchId("sketch.tool"), std::move(intent));
  REQUIRE(tool);
  REQUIRE(document.value().add_feature(tool.value()));
  return document.value();
}

PartDocument operation_document(FeatureBodyOperationMode mode, ExtrudeExtentIntent extent,
                                Point2 tool_center = {8.0, 0.0}) {
  auto document = PartDocument::create(DocumentId("part.operation"), "Operation");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("depth", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("tool.width", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("tool.height", 10.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.value().add_body(solid_body("body.base")));
  if (mode == FeatureBodyOperationMode::NewBody)
    REQUIRE(document.value().add_body(solid_body("body.tool")));
  REQUIRE(
      document.value().add_sketch(rectangle_sketch("sketch.base", "base.width", "base.height")));
  REQUIRE(document.value().add_sketch(
      rectangle_sketch("sketch.tool", "tool.width", "tool.height", tool_center)));

  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("depth"));
  REQUIRE(base);
  auto base_context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                       std::nullopt, BodyId("body.base"));
  REQUIRE(base_context);
  auto base_attached = base.value().with_body_result_context(base_context.value());
  REQUIRE(base_attached);
  REQUIRE(document.value().add_feature(base_attached.value()));

  auto feature = Feature::create_additive_extrude(FeatureId("feature.operation"), "Operation",
                                                  SketchId("sketch.tool"),
                                                  extrude_intent(std::move(extent), 0.0));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(
      mode,
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{} : BodyId("body.base"),
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{BodyId("body.tool")}
                                                : std::optional<BodyId>{});
  REQUIRE(context);
  auto attached = feature.value().with_body_result_context(context.value());
  REQUIRE(attached);
  REQUIRE(document.value().add_feature(attached.value()));
  return document.value();
}

} // namespace

TEST_CASE("Block 60 executes symmetric and two-sided extrusion spans",
          "[geometry][extrude-extent]") {
  SECTION("symmetric total depth") {
    auto extent = ExtrudeExtentIntent::symmetric(ParameterId("second"));
    REQUIRE(extent);
    const PartDocument document = simple_extent_document(extrude_intent(extent.value()));
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
    REQUIRE(cache.final_shape() != nullptr);
    CHECK(summary(*cache.final_shape()).volume_mm3 == Approx(500.0).margin(1.0e-6));
  }
  SECTION("independent forward and reverse depths") {
    auto extent = ExtrudeExtentIntent::two_sided(ParameterId("first"), ParameterId("second"));
    REQUIRE(extent);
    const PartDocument document = simple_extent_document(extrude_intent(extent.value()));
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
    REQUIRE(cache.final_shape() != nullptr);
    CHECK(summary(*cache.final_shape()).volume_mm3 == Approx(800.0).margin(1.0e-6));
  }
}

TEST_CASE("Block 60 resolves ToFace and Between semantic planar limits",
          "[geometry][extrude-extent]") {
  SECTION("to face") {
    auto extent = ExtrudeExtentIntent::to_face(limit_face(SemanticFace::Top));
    REQUIRE(extent);
    const PartDocument document = face_extent_document(extrude_intent(extent.value()));
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
    REQUIRE(cache.final_shape() != nullptr);
    CHECK(summary(*cache.final_shape()).volume_mm3 == Approx(320.0).margin(1.0e-6));
  }
  SECTION("between faces") {
    auto extent = ExtrudeExtentIntent::between(limit_face(SemanticFace::Bottom),
                                               limit_face(SemanticFace::Top));
    REQUIRE(extent);
    const PartDocument document = face_extent_document(extrude_intent(extent.value()));
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
    REQUIRE(cache.final_shape() != nullptr);
    CHECK(summary(*cache.final_shape()).volume_mm3 == Approx(320.0).margin(1.0e-6));
  }
}

TEST_CASE("Block 60 builds tapered and open-line thin extrusions", "[geometry][extrude-extent]") {
  SECTION("tapered rectangle") {
    auto extent = ExtrudeExtentIntent::distance(ParameterId("second"));
    REQUIRE(extent);
    const PartDocument document = simple_extent_document(extrude_intent(extent.value(), 8.0));
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
    REQUIRE(cache.final_shape() != nullptr);
    const auto result = summary(*cache.final_shape());
    CHECK(result.solid_count == 1U);
    CHECK(result.volume_mm3 > 500.0);
  }
  SECTION("mid-plane thin line") {
    auto document = PartDocument::create(DocumentId("part.thin"), "Thin");
    REQUIRE(document);
    REQUIRE(document.value().add_parameter(length_parameter("depth", 5.0)));
    REQUIRE(document.value().add_parameter(length_parameter("wall", 2.0)));
    REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
    auto sketch = Sketch::create(SketchId("sketch.thin"), "Thin", DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    auto line = LineSegment::create(SketchEntityId("line.open"), {-5.0, 0.0}, {5.0, 0.0});
    REQUIRE(line);
    REQUIRE(sketch.value().add_entity(line.value()));
    REQUIRE(document.value().add_sketch(sketch.value()));
    auto extent = ExtrudeExtentIntent::distance(ParameterId("depth"));
    auto thin = ExtrudeThinIntent::create(ExtrudeThinMode::MidPlane, ParameterId("wall"));
    REQUIRE(extent);
    REQUIRE(thin);
    auto feature = Feature::create_additive_extrude(
        FeatureId("feature.thin"), "Thin", SketchId("sketch.thin"),
        extrude_intent(extent.value(), std::nullopt, thin.value()));
    REQUIRE(feature);
    REQUIRE(document.value().add_feature(feature.value()));
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document.value(), cache));
    REQUIRE(cache.final_shape() != nullptr);
    CHECK(summary(*cache.final_shape()).volume_mm3 == Approx(100.0).margin(1.0e-6));
  }
}

TEST_CASE("Block 60 executes NewBody Join Cut and Intersect operation modes",
          "[geometry][extrude-extent]") {
  const auto run = [](FeatureBodyOperationMode mode, double expected_volume) {
    auto extent = ExtrudeExtentIntent::distance(ParameterId("depth"));
    REQUIRE(extent);
    const PartDocument document = operation_document(mode, extent.value());
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
    const BodyId result_body =
        mode == FeatureBodyOperationMode::NewBody ? BodyId("body.tool") : BodyId("body.base");
    const GeometryShape* result = cache.find_body_shape(result_body);
    REQUIRE(result != nullptr);
    CHECK(summary(*result).volume_mm3 == Approx(expected_volume).margin(1.0e-5));
    REQUIRE(GeometryRecomputeExecutor{}.execute_additive_extrude(
        document, FeatureId("feature.operation"), cache));
    result = cache.find_body_shape(result_body);
    REQUIRE(result != nullptr);
    CHECK(summary(*result).volume_mm3 == Approx(expected_volume).margin(1.0e-5));
  };

  SECTION("new body") {
    run(FeatureBodyOperationMode::NewBody, 1000.0);
  }
  SECTION("join") {
    run(FeatureBodyOperationMode::Join, 1800.0);
  }
  SECTION("cut") {
    run(FeatureBodyOperationMode::Cut, 800.0);
  }
  SECTION("intersect") {
    run(FeatureBodyOperationMode::Intersect, 200.0);
  }
}

TEST_CASE("Block 60 executes richer SubtractiveExtrude intent", "[geometry][extrude-extent]") {
  auto document = PartDocument::create(DocumentId("part.richer_cut"), "RicherCut");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("width", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("height", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("depth", 10.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.value().add_sketch(rectangle_sketch("sketch.base", "width", "height")));
  REQUIRE(
      document.value().add_sketch(rectangle_sketch("sketch.cut", "width", "height", {8.0, 0.0})));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("depth"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));
  auto extent = ExtrudeExtentIntent::distance(ParameterId("depth"));
  REQUIRE(extent);
  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.cut"), "Cut",
                                                 SketchId("sketch.cut"), FeatureId("feature.base"),
                                                 extrude_intent(extent.value(), 0.0));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));
  auto cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document.value(), cache));
  REQUIRE(cache.final_shape() != nullptr);
  CHECK(summary(*cache.final_shape()).volume_mm3 == Approx(800.0).margin(1.0e-5));
}

TEST_CASE("Block 60 executes ThroughAll and ToNext against target projection",
          "[geometry][extrude-extent]") {
  SECTION("through all") {
    const PartDocument document =
        operation_document(FeatureBodyOperationMode::Intersect, ExtrudeExtentIntent::through_all());
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
    const GeometryShape* result = cache.find_body_shape(BodyId("body.base"));
    REQUIRE(result != nullptr);
    CHECK(summary(*result).volume_mm3 == Approx(200.0).margin(1.0e-5));
  }
  SECTION("to next") {
    const PartDocument document = operation_document(FeatureBodyOperationMode::Intersect,
                                                     ExtrudeExtentIntent::to_next(), {0.0, 0.0});
    auto cache = shape_cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
    const GeometryShape* result = cache.find_body_shape(BodyId("body.base"));
    REQUIRE(result != nullptr);
    CHECK(summary(*result).volume_mm3 == Approx(1000.0).margin(1.0e-5));
  }
}
