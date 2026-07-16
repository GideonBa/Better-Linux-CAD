#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sketch_spline_geometry.hpp"
#include "blcad/geometry/sketch_text_layout.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

Sketch make_spline_profile_sketch(SketchId sketch_id, DatumPlaneId workplane_id) {
  auto sketch = Sketch::create(sketch_id, "SplineProfile", workplane_id);
  REQUIRE(sketch);

  add_line(sketch.value(), "line.bottom", Point2{-10.0, -8.0}, Point2{10.0, -8.0});
  auto spline =
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.right"), Point2{10.0, -8.0},
                                         Point2{18.0, -4.0}, Point2{18.0, 4.0}, Point2{10.0, 8.0});
  REQUIRE(spline);
  REQUIRE(sketch.value().add_entity(spline.value()));
  add_line(sketch.value(), "line.top", Point2{10.0, 8.0}, Point2{-10.0, 8.0});
  add_line(sketch.value(), "line.left", Point2{-10.0, 8.0}, Point2{-10.0, -8.0});

  auto tangent = SketchTangentContinuity::create_tangent(
      SketchConstraintId("tangent.bottom_spline"), SketchEntityId("line.bottom"),
      SketchEntityId("spline.right"));
  REQUIRE(tangent);
  REQUIRE(sketch.value().add_tangent_continuity(tangent.value()));

  auto profile = ArcClosedProfile::create(
      ProfileId("profile.spline"), {SketchEntityId("line.bottom"), SketchEntityId("spline.right"),
                                    SketchEntityId("line.top"), SketchEntityId("line.left")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

PartDocument make_additive_spline_document() {
  auto document = PartDocument::create(DocumentId("part.spline_additive"), "SplineAdditive");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 6.0)));
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  REQUIRE(document.value().add_sketch(
      make_spline_profile_sketch(SketchId("sketch.spline"), DatumPlaneId("datum.xy"))));
  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.spline"), "SplineExtrude",
                                       SketchId("sketch.spline"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument make_subtractive_spline_document() {
  auto document = PartDocument::create(DocumentId("part.spline_cut"), "SplineCut");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 80.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 12.0)));
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  REQUIRE(document.value().add_sketch(
      make_spline_profile_sketch(SketchId("sketch.cut"), DatumPlaneId("datum.xy"))));
  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.cut"), "SplineCut",
                                                 SketchId("sketch.cut"), FeatureId("feature.base"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));
  return document.value();
}

} // namespace

TEST_CASE("Geometry recomputes additive extrude from spline closed profile", "[geometry][spline]") {
  PartDocument document = make_additive_spline_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.spline_additive"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;

  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  REQUIRE(cache.value().final_shape() != nullptr);

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.value().final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
}

TEST_CASE("Geometry recomputes subtractive cut from spline closed profile", "[geometry][spline]") {
  PartDocument document = make_subtractive_spline_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.spline_cut"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;

  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  REQUIRE(cache.value().final_shape() != nullptr);

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.value().final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
}

TEST_CASE("Block 113 spline evaluator publishes deterministic samples curvature and continuity",
          "[geometry][sketch-spline-edit]") {
  auto first = SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.a"), {0.0, 0.0}, {3.0, 0.0}, {7.0, 5.0}, {10.0, 5.0});
  auto second = SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.b"), {10.0, 5.0}, {13.0, 5.0}, {17.0, 0.0}, {20.0, 0.0});
  REQUIRE(first);
  REQUIRE(second);

  auto result = SketchSplineGeometryEvaluator{}.evaluate({first.value(), second.value()}, 16U);
  REQUIRE(result);
  REQUIRE(result.value().sampled_segments.size() == 2U);
  CHECK(result.value().sampled_segments.front().size() == 17U);
  CHECK(result.value().sampled_segments.front().front().position == Point2{0.0, 0.0});
  CHECK(result.value().sampled_segments.back().back().position == Point2{20.0, 0.0});
  REQUIRE(result.value().continuity.size() == 1U);
  CHECK(result.value().continuity.front().position_continuous);
  CHECK(result.value().continuity.front().tangent_continuous);
  CHECK(std::isfinite(result.value().sampled_segments.front()[8].curvature));

  auto broken = SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.b"), {10.0, 5.0}, {10.0, 8.0}, {17.0, 0.0}, {20.0, 0.0});
  REQUIRE(broken);
  auto diagnostic = SketchSplineGeometryEvaluator{}.evaluate({first.value(), broken.value()});
  REQUIRE(diagnostic);
  CHECK(diagnostic.value().continuity.front().position_continuous);
  CHECK_FALSE(diagnostic.value().continuity.front().tangent_continuous);
}

TEST_CASE("Block 113 text layout resolves explicit system and built-in font fallbacks",
          "[geometry][sketch-text]") {
  auto document = PartDocument::create(DocumentId("part.text_layout"), "Text Layout");
  REQUIRE(document);
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  REQUIRE(document.value().add_sketch(
      Sketch::create(SketchId("sketch.text"), "Text", DatumPlaneId("datum.xy")).value()));
  REQUIRE(document.value().add_parameter(make_length_parameter("text.height", "text_height", 7.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 40.0)));
  REQUIRE(document.value().add_expression_parameter(ParameterId("text.value"), "text_value",
                                                    ParameterType::Length, "width / 2"));

  auto text = SketchText::create(
      SketchTextId("text.width"), SketchId("sketch.text"), "WIDTH ${value}", "DIN 1451",
      ParameterId("text.height"), Point2{3.0, 4.0}, std::nullopt,
      {SketchTextBinding{"value", ParameterId("text.value"), 1U}});
  REQUIRE(text);

  auto system_fallback = SketchTextLayoutResolver{}.resolve(
      document.value(), text.value(), {"DejaVu Sans", "Liberation Sans"});
  REQUIRE(system_fallback);
  CHECK(system_fallback.value().resolved_font == "Liberation Sans");
  CHECK(system_fallback.value().used_fallback);
  CHECK(system_fallback.value().resolved_text == "WIDTH 20 mm");
  CHECK(system_fallback.value().width_mm > 0.0);
  CHECK_FALSE(system_fallback.value().glyphs.empty());
  CHECK_FALSE(system_fallback.value().diagnostics.empty());

  auto built_in = SketchTextLayoutResolver{}.resolve(document.value(), text.value(), {});
  REQUIRE(built_in);
  CHECK(built_in.value().resolved_font == "BLCAD Simplex Stroke");
  CHECK(std::any_of(built_in.value().diagnostics.begin(), built_in.value().diagnostics.end(),
                    [](const auto& diagnostic) {
                      return diagnostic.code == "font_builtin_fallback";
                    }));
  CHECK(std::any_of(built_in.value().glyphs.begin(), built_in.value().glyphs.end(),
                    [](const auto& glyph) { return !glyph.strokes.empty(); }));
}
