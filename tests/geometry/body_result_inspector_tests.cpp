#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter make_length_parameter(const std::string& id, double millimeters) {
  auto quantity = Quantity::length_mm(millimeters, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body make_body(const std::string& id, BodyKind kind,
               BodyVisibility visibility = BodyVisibility::Visible) {
  auto body = Body::create(BodyId(id), id, kind, visibility);
  REQUIRE(body);
  return body.value();
}

Feature make_new_body_feature(const std::string& suffix) {
  auto feature = Feature::create_additive_extrude(FeatureId("feature." + suffix),
                                                  "Extrude" + suffix, SketchId("sketch." + suffix),
                                                  ParameterId(suffix + ".depth"));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body." + suffix));
  REQUIRE(context);
  auto attached = feature.value().with_body_result_context(context.value());
  REQUIRE(attached);
  return attached.value();
}

PartDocument make_two_body_document() {
  auto document = PartDocument::create(DocumentId("part.inspection"), "InspectionPart");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  for (const std::string suffix : {"a", "b"}) {
    REQUIRE(document.value().add_parameter(
        make_length_parameter(suffix + ".width", suffix == "a" ? 40.0 : 20.0)));
    REQUIRE(document.value().add_parameter(
        make_length_parameter(suffix + ".height", suffix == "a" ? 30.0 : 10.0)));
    REQUIRE(document.value().add_parameter(
        make_length_parameter(suffix + ".depth", suffix == "a" ? 5.0 : 7.0)));

    auto sketch =
        Sketch::create(SketchId("sketch." + suffix), "Sketch" + suffix, DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    auto profile =
        RectangleProfile::create(ProfileId("profile." + suffix), ParameterId(suffix + ".width"),
                                 ParameterId(suffix + ".height"));
    REQUIRE(profile);
    REQUIRE(sketch.value().add_profile(profile.value()));
    REQUIRE(document.value().add_sketch(sketch.value()));
    REQUIRE(document.value().add_body(
        make_body("body." + suffix, BodyKind::Solid,
                  suffix == "b" ? BodyVisibility::Hidden : BodyVisibility::Visible)));
    REQUIRE(document.value().add_feature(make_new_body_feature(suffix)));
  }
  return document.value();
}

ShapeCache make_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.inspection"));
  REQUIRE(cache);
  return cache.value();
}

GeometryShape make_shape() {
  auto width = Quantity::length_mm(10.0, "shape.width");
  auto height = Quantity::length_mm(10.0, "shape.height");
  auto depth = Quantity::length_mm(10.0, "shape.depth");
  REQUIRE(width);
  REQUIRE(height);
  REQUIRE(depth);
  auto shape = RectangleExtrusionAdapter{}.make_extruded_rectangle(width.value(), height.value(),
                                                                   depth.value());
  REQUIRE(shape);
  return shape.value();
}

} // namespace

TEST_CASE("BodyResultInspector reports stable metadata counts and shape summaries",
          "[geometry][multi-body-inspection]") {
  PartDocument document = make_two_body_document();
  ShapeCache cache = make_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));

  const auto inspected = BodyResultInspector{}.inspect(document, cache);

  REQUIRE(inspected);
  CHECK(inspected.value().solid_body_count == 2);
  CHECK(inspected.value().surface_body_count == 0);
  REQUIRE(inspected.value().bodies.size() == 2);
  CHECK(inspected.value().bodies[0].body_id.value() == "body.a");
  CHECK(inspected.value().bodies[0].kind == BodyKind::Solid);
  CHECK(inspected.value().bodies[0].visibility == BodyVisibility::Visible);
  CHECK(inspected.value().bodies[1].body_id.value() == "body.b");
  CHECK(inspected.value().bodies[1].visibility == BodyVisibility::Hidden);
  REQUIRE(inspected.value().bodies[0].shape_summary.has_value());
  REQUIRE(inspected.value().bodies[1].shape_summary.has_value());
  CHECK(inspected.value().bodies[0].shape_summary->solid_count == 1);
  CHECK(inspected.value().bodies[1].shape_summary->solid_count == 1);
  CHECK(inspected.value().bodies[0].shape_summary->volume_mm3 == Catch::Approx(6000.0));
  CHECK(inspected.value().bodies[1].shape_summary->volume_mm3 == Catch::Approx(1400.0));
}

TEST_CASE("BodyResultInspector proves independent body recompute through public inspection",
          "[geometry][multi-body-inspection]") {
  PartDocument document = make_two_body_document();
  ShapeCache cache = make_cache();
  const GeometryRecomputeExecutor executor;
  const BodyResultInspector inspector;
  REQUIRE(executor.execute_document(document, cache));
  const auto before = inspector.inspect(document, cache);
  REQUIRE(before);

  auto new_width = Quantity::length_mm(50.0, "a.width");
  REQUIRE(new_width);
  REQUIRE(document.set_parameter_value(ParameterId("a.width"), new_width.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("feature.a"));
  CHECK_FALSE(plan.value().contains("feature.b"));
  REQUIRE(executor.execute_plan(document, plan.value(), cache));
  const auto after = inspector.inspect(document, cache);
  REQUIRE(after);

  CHECK(after.value().bodies[0].shape_summary->volume_mm3 == Catch::Approx(7500.0));
  CHECK(after.value().bodies[1].shape_summary->volume_mm3 ==
        Catch::Approx(before.value().bodies[1].shape_summary->volume_mm3));
}

TEST_CASE("BodyResultInspector exposes Bodies without computed geometry",
          "[geometry][multi-body-inspection]") {
  auto document = PartDocument::create(DocumentId("part.surface"), "SurfacePart");
  REQUIRE(document);
  REQUIRE(document.value().add_body(make_body("body.surface", BodyKind::Surface)));
  ShapeCache cache = make_cache();

  const auto inspected = BodyResultInspector{}.inspect(document.value(), cache);

  REQUIRE(inspected);
  CHECK(inspected.value().solid_body_count == 0);
  CHECK(inspected.value().surface_body_count == 1);
  REQUIRE(inspected.value().bodies.size() == 1);
  CHECK_FALSE(inspected.value().bodies[0].source_feature_id.has_value());
  CHECK_FALSE(inspected.value().bodies[0].shape_summary.has_value());
}

TEST_CASE("BodyResultInspector defines historical final-shape compatibility",
          "[geometry][multi-body-inspection]") {
  const BodyResultInspector inspector;
  ShapeCache legacy_cache = make_cache();
  REQUIRE(legacy_cache.set_final_shape(FeatureId("feature.legacy"), make_shape()));
  auto legacy = PartDocument::create(DocumentId("part.legacy"), "LegacyPart");
  REQUIRE(legacy);
  CHECK(inspector.resolve_compatible_final_shape(legacy.value(), legacy_cache));

  auto solid = PartDocument::create(DocumentId("part.solid"), "SolidPart");
  REQUIRE(solid);
  REQUIRE(solid.value().add_body(make_body("body.solid", BodyKind::Solid)));
  ShapeCache solid_cache = make_cache();
  REQUIRE(
      solid_cache.store_body_shape(BodyId("body.solid"), FeatureId("feature.solid"), make_shape()));
  CHECK(inspector.resolve_compatible_final_shape(solid.value(), solid_cache));

  auto surface = PartDocument::create(DocumentId("part.surface"), "SurfacePart");
  REQUIRE(surface);
  REQUIRE(surface.value().add_body(make_body("body.surface", BodyKind::Surface)));
  ShapeCache surface_cache = make_cache();
  REQUIRE(surface_cache.store_body_shape(BodyId("body.surface"), FeatureId("feature.surface"),
                                         make_shape()));
  const auto rejected_surface =
      inspector.resolve_compatible_final_shape(surface.value(), surface_cache);
  REQUIRE(rejected_surface.has_error());
  CHECK(rejected_surface.error().object_id() == "body.surface");

  PartDocument multi = make_two_body_document();
  ShapeCache multi_cache = make_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(multi, multi_cache));
  const auto ambiguous = inspector.resolve_compatible_final_shape(multi, multi_cache);
  REQUIRE(ambiguous.has_error());
  CHECK(ambiguous.error().category() == ErrorCategory::Geometry);
  CHECK(ambiguous.error().object_id() == "part.inspection");
  CHECK(ambiguous.error().message() == "historical final shape is ambiguous for a multi-body part");
}
