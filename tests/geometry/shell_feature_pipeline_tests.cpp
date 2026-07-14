#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shell_adapter.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using namespace blcad;
using namespace blcad::geometry;

namespace {
Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument box_document(double thickness = 1.0) {
  auto document = PartDocument::create(DocumentId("part.shell-geometry"), "Shell geometry");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 12.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 8.0)));
  REQUIRE(document.value().add_parameter(length_parameter("shell.thickness", thickness)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                            ParameterId("base.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto body = Body::create(BodyId("body.base"), "Base", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.value().add_body(body.value()));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.base"));
  REQUIRE(context);
  auto contextual = base.value().with_body_result_context(context.value());
  REQUIRE(contextual);
  REQUIRE(document.value().add_feature(contextual.value()));
  return document.value();
}

FaceReference face(SemanticFace value, FeatureId source = FeatureId("feature.base")) {
  auto semantic = SemanticFaceReference::create(std::move(source), value);
  REQUIRE(semantic);
  auto reference =
      FaceReference::create_planar(PartFeatureInputRole::ShellRemovalFace, semantic.value());
  REQUIRE(reference);
  return reference.value();
}

FaceReference cylindrical_face() {
  auto semantic = SemanticCylindricalFaceReference::create(FeatureId("feature.hole"),
                                                           ProfileId("profile.hole"));
  REQUIRE(semantic);
  auto reference =
      FaceReference::create_cylindrical(PartFeatureInputRole::ShellRemovalFace, semantic.value());
  REQUIRE(reference);
  return reference.value();
}

void add_hole(PartDocument& document) {
  REQUIRE(document.add_parameter(length_parameter("hole.diameter", 4.0)));
  auto sketch = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto circle = CircleProfile::create(ProfileId("profile.hole"), ParameterId("hole.diameter"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  auto hole = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"), FeatureId("feature.base"));
  REQUIRE(hole);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                  BodyId("body.base"), std::nullopt);
  REQUIRE(context);
  auto contextual = hole.value().with_body_result_context(context.value());
  REQUIRE(contextual);
  REQUIRE(document.add_feature(contextual.value()));
}

void add_shell(PartDocument& document, std::vector<FaceReference> faces,
               ShellDirection direction = ShellDirection::Inward) {
  auto shell = ShellFeature::create(FeatureId("feature.shell"), "Shell", BodyId("body.base"),
                                    std::move(faces), ParameterId("shell.thickness"), direction);
  REQUIRE(shell);
  REQUIRE(document.add_shell_feature(shell.value()));
}

ShapeCache cache() {
  auto value = ShapeCache::create(ShapeCacheId("cache.shell"));
  REQUIRE(value);
  return value.value();
}

double volume(const ShapeCache& shape_cache) {
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

GeometryShapeBounds bounds(const ShapeCache& shape_cache) {
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape != nullptr);
  auto result = BodyTransformAdapter{}.bounds(*shape);
  REQUIRE(result);
  return result.value();
}
} // namespace

TEST_CASE("Block 72 shells a closed box into an open-top solid",
          "[geometry][shell-feature][integration][part-construction-mvp]") {
  auto document = box_document();
  add_shell(document, {face(SemanticFace::Top)});
  ShapeCache shape_cache = cache();
  const auto result = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  INFO((result.has_error() ? result.error().object_id() + ": " + result.error().message()
                           : "success"));
  REQUIRE(result);
  CHECK(result.value().executed_feature_count == 2U);
  CHECK(volume(shape_cache) == Catch::Approx(660.0).margin(1.0e-5));
  CHECK(bounds(shape_cache).maximum.z == Catch::Approx(8.0).margin(2.0e-3));
  CHECK(shape_cache.find_feature_shape(FeatureId("feature.shell")) != nullptr);
}

TEST_CASE("Block 72 supports multiple semantic removal faces", "[geometry][shell-feature]") {
  auto single = box_document();
  add_shell(single, {face(SemanticFace::Top)});
  ShapeCache single_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(single, single_cache));

  auto multiple = box_document();
  add_shell(multiple, {face(SemanticFace::Top), face(SemanticFace::Front)});
  ShapeCache multiple_cache = cache();
  const auto result = GeometryRecomputeExecutor{}.execute_document(multiple, multiple_cache);
  INFO((result.has_error() ? result.error().object_id() + ": " + result.error().message()
                           : "success"));
  REQUIRE(result);
  CHECK(volume(multiple_cache) < volume(single_cache));
  CHECK(bounds(multiple_cache).maximum.z == Catch::Approx(8.0).margin(2.0e-3));
}

TEST_CASE("Block 72 resolves a semantic cylindrical removal face", "[geometry][shell-feature]") {
  auto document = box_document(0.5);
  add_hole(document);
  add_shell(document, {cylindrical_face()});
  ShapeCache shape_cache = cache();
  const auto result = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  INFO((result.has_error() ? result.error().object_id() + ": " + result.error().message()
                           : "success"));
  REQUIRE(result);
  CHECK(shape_cache.find_feature_shape(FeatureId("feature.shell")) != nullptr);
}

TEST_CASE("Block 72 applies explicit inward and outward thickness directions",
          "[geometry][shell-feature]") {
  auto inward = box_document();
  add_shell(inward, {face(SemanticFace::Top)}, ShellDirection::Inward);
  ShapeCache inward_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(inward, inward_cache));

  auto outward = box_document();
  add_shell(outward, {face(SemanticFace::Top)}, ShellDirection::Outward);
  ShapeCache outward_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(outward, outward_cache));
  const auto outward_bounds = bounds(outward_cache);
  CHECK(outward_bounds.minimum.x == Catch::Approx(-11.0).margin(2.0e-3));
  CHECK(outward_bounds.maximum.x == Catch::Approx(11.0).margin(2.0e-3));
  CHECK(outward_bounds.minimum.z == Catch::Approx(-1.0).margin(2.0e-3));
  CHECK(volume(outward_cache) > volume(inward_cache));
}

TEST_CASE("Block 72 recomputes thickness and recovers a face after upstream edits",
          "[geometry][shell-feature]") {
  auto document = box_document(0.5);
  add_shell(document, {face(SemanticFace::Top)});
  ShapeCache shape_cache = cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, shape_cache));
  const double initial_volume = volume(shape_cache);

  document.mark_all_clean();
  auto thickness = Quantity::length_mm(1.0, "shell.thickness");
  REQUIRE(thickness);
  REQUIRE(document.set_parameter_value(ParameterId("shell.thickness"), thickness.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  auto thickness_result = executor.execute_plan(document, plan.value(), shape_cache);
  REQUIRE(thickness_result);
  CHECK(thickness_result.value().executed_feature_count == 1U);
  CHECK(volume(shape_cache) > initial_volume);

  document.mark_all_clean();
  auto width = Quantity::length_mm(30.0, "base.width");
  REQUIRE(width);
  REQUIRE(document.set_parameter_value(ParameterId("base.width"), width.value()));
  plan = document.create_recompute_plan();
  REQUIRE(plan);
  auto upstream_result = executor.execute_plan(document, plan.value(), shape_cache);
  REQUIRE(upstream_result);
  CHECK(upstream_result.value().executed_feature_count == 2U);
  CHECK(bounds(shape_cache).minimum.x == Catch::Approx(-15.0).margin(2.0e-3));
  CHECK(bounds(shape_cache).maximum.x == Catch::Approx(15.0).margin(2.0e-3));
}

TEST_CASE("Block 72 rejects broken faces and excessive thickness transactionally",
          "[geometry][shell-feature]") {
  auto direct_document = box_document();
  ShapeCache direct_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(direct_document, direct_cache));
  const double original_volume = volume(direct_cache);
  auto invalid =
      ShellFeature::create(FeatureId("feature.invalid-shell"), "Invalid shell", BodyId("body.base"),
                           {face(SemanticFace::Top, FeatureId("feature.missing"))},
                           ParameterId("shell.thickness"), ShellDirection::Inward);
  REQUIRE(invalid);
  const GeometryShape* target = direct_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(target != nullptr);
  const auto broken =
      ShellAdapter{}.apply(direct_document, invalid.value(), *target, direct_cache, 1.0);
  REQUIRE(broken.has_error());
  CHECK(broken.error().object_id() == "feature.missing");
  CHECK(volume(direct_cache) == Catch::Approx(original_volume));

  auto document = box_document();
  add_shell(document, {face(SemanticFace::Top)});
  ShapeCache shape_cache = cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, shape_cache));
  const double valid_volume = volume(shape_cache);
  document.mark_all_clean();
  auto excessive = Quantity::length_mm(20.0, "shell.thickness");
  REQUIRE(excessive);
  REQUIRE(document.set_parameter_value(ParameterId("shell.thickness"), excessive.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  const auto result = executor.execute_plan(document, plan.value(), shape_cache);
  REQUIRE(result.has_error());
  CHECK(volume(shape_cache) == Catch::Approx(valid_volume));
}
