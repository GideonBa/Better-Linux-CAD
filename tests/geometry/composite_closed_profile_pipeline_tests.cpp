#include "blcad/geometry/profile_region_classifier.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

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

Sketch make_composite_sketch(SketchId id) {
  auto sketch = Sketch::create(id, "Composite", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "outer.bottom", Point2{-20.0, -10.0}, Point2{20.0, -10.0});
  add_line(sketch.value(), "outer.right", Point2{20.0, -10.0}, Point2{20.0, 10.0});
  add_line(sketch.value(), "outer.top", Point2{20.0, 10.0}, Point2{-20.0, 10.0});
  add_line(sketch.value(), "outer.left", Point2{-20.0, 10.0}, Point2{-20.0, -10.0});
  add_line(sketch.value(), "inner.bottom", Point2{-5.0, -3.0}, Point2{5.0, -3.0});
  add_line(sketch.value(), "inner.right", Point2{5.0, -3.0}, Point2{5.0, 3.0});
  add_line(sketch.value(), "inner.top", Point2{5.0, 3.0}, Point2{-5.0, 3.0});
  add_line(sketch.value(), "inner.left", Point2{-5.0, 3.0}, Point2{-5.0, -3.0});
  auto profile = CompositeClosedProfile::create(
      ProfileId("profile.composite"),
      {SketchEntityId("outer.bottom"), SketchEntityId("outer.right"), SketchEntityId("outer.top"),
       SketchEntityId("outer.left")},
      {{SketchEntityId("inner.bottom"), SketchEntityId("inner.right"), SketchEntityId("inner.top"),
        SketchEntityId("inner.left")}});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

PartDocument make_additive_document() {
  auto document = PartDocument::create(DocumentId("part.composite.additive"), "CompositeAdditive");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_composite_sketch(SketchId("sketch.composite"))));
  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.composite"), "Composite",
                                       SketchId("sketch.composite"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument make_subtractive_document() {
  auto document = PartDocument::create(DocumentId("part.composite.cut"), "CompositeCut");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 80.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 8.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto base_profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                               ParameterId("part.height"));
  REQUIRE(base_profile);
  REQUIRE(base_sketch.value().add_profile(base_profile.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));
  auto base_feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(base_feature);
  REQUIRE(document.value().add_feature(base_feature.value()));

  REQUIRE(document.value().add_sketch(make_composite_sketch(SketchId("sketch.composite"))));
  auto cut_feature =
      Feature::create_subtractive_extrude(FeatureId("feature.composite_cut"), "Cut",
                                          SketchId("sketch.composite"), FeatureId("feature.base"));
  REQUIRE(cut_feature);
  REQUIRE(document.value().add_feature(cut_feature.value()));
  return document.value();
}

Sketch make_rectangle_and_circle_sketch(bool parametric_rectangle, Point2 circle_center,
                                        const char* circle_diameter_parameter = "part.diameter",
                                        bool clockwise_outer = false) {
  auto sketch = Sketch::create(SketchId("sketch.nested"), "Nested", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  if (parametric_rectangle) {
    auto rectangle = RectangleProfile::create(ProfileId("profile.outer"), ParameterId("part.width"),
                                              ParameterId("part.height"));
    REQUIRE(rectangle);
    REQUIRE(sketch.value().add_profile(rectangle.value()));
  } else {
    if (clockwise_outer) {
      add_line(sketch.value(), "outer.left", {-20.0, -10.0}, {-20.0, 10.0});
      add_line(sketch.value(), "outer.top", {-20.0, 10.0}, {20.0, 10.0});
      add_line(sketch.value(), "outer.right", {20.0, 10.0}, {20.0, -10.0});
      add_line(sketch.value(), "outer.bottom", {20.0, -10.0}, {-20.0, -10.0});
    } else {
      add_line(sketch.value(), "outer.bottom", {-20.0, -10.0}, {20.0, -10.0});
      add_line(sketch.value(), "outer.right", {20.0, -10.0}, {20.0, 10.0});
      add_line(sketch.value(), "outer.top", {20.0, 10.0}, {-20.0, 10.0});
      add_line(sketch.value(), "outer.left", {-20.0, 10.0}, {-20.0, -10.0});
    }
    auto outer = ClosedProfile::create(
        ProfileId("profile.outer"),
        clockwise_outer
            ? std::vector<SketchEntityId>{SketchEntityId("outer.left"), SketchEntityId("outer.top"),
                                          SketchEntityId("outer.right"),
                                          SketchEntityId("outer.bottom")}
            : std::vector<SketchEntityId>{
                  SketchEntityId("outer.bottom"), SketchEntityId("outer.right"),
                  SketchEntityId("outer.top"), SketchEntityId("outer.left")});
    REQUIRE(outer);
    REQUIRE(sketch.value().add_profile(outer.value()));
  }
  auto circle = CircleProfile::create(ProfileId("profile.circle"),
                                      ParameterId(circle_diameter_parameter), circle_center);
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));
  return sketch.value();
}

PartDocument make_nested_profile_document(bool parametric_rectangle, Point2 circle_center,
                                          ProfileId selected_profile, bool symmetric_extent = false,
                                          bool clockwise_outer = false) {
  auto document = PartDocument::create(DocumentId("part.nested"), "Nested");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.diameter", "diameter", 6.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto body = Body::create(BodyId("body.nested"), "Nested", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.value().add_body(body.value()));
  REQUIRE(document.value().add_sketch(make_rectangle_and_circle_sketch(
      parametric_rectangle, circle_center, "part.diameter", clockwise_outer)));

  auto feature = [&]() -> Result<Feature> {
    if (!symmetric_extent)
      return Feature::create_additive_extrude(FeatureId("feature.nested"), "Nested",
                                              SketchId("sketch.nested"), ParameterId("part.depth"));
    auto extent = ExtrudeExtentIntent::symmetric(ParameterId("part.depth"));
    if (extent.has_error())
      return Result<Feature>::failure(extent.error());
    auto intent = ExtrudeFeatureIntent::create(std::move(extent.value()));
    if (intent.has_error())
      return Result<Feature>::failure(intent.error());
    return Feature::create_additive_extrude(FeatureId("feature.nested"), "Nested",
                                            SketchId("sketch.nested"), std::move(intent.value()));
  }();
  REQUIRE(feature);
  auto profiled = feature.value().with_profile(std::move(selected_profile));
  REQUIRE(profiled);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.nested"));
  REQUIRE(context);
  auto attached = profiled.value().with_body_result_context(context.value());
  REQUIRE(attached);
  REQUIRE(document.value().add_feature(attached.value()));
  return document.value();
}

ShapeSummary recompute_nested_profile(const PartDocument& document) {
  auto cache = ShapeCache::create(ShapeCacheId("cache.nested"));
  REQUIRE(cache);
  auto summary = GeometryRecomputeExecutor{}.execute_document(document, cache.value());
  REQUIRE(summary);
  REQUIRE(summary.value().executed_feature_count == 1U);
  REQUIRE(cache.value().has_final_shape());
  return RectangleExtrusionAdapter{}.summarize(*cache.value().final_shape());
}

void add_polygon_profile(Sketch& sketch, const std::string& entity_prefix,
                         const ProfileId& profile_id, const std::vector<Point2>& vertices) {
  REQUIRE(vertices.size() >= 3U);
  std::vector<SketchEntityId> entities;
  entities.reserve(vertices.size());
  for (std::size_t index = 0U; index < vertices.size(); ++index) {
    SketchEntityId entity(entity_prefix + "." + std::to_string(index));
    auto line =
        LineSegment::create(entity, vertices[index], vertices[(index + 1U) % vertices.size()]);
    REQUIRE(line);
    REQUIRE(sketch.add_entity(line.value()));
    entities.push_back(std::move(entity));
  }
  auto profile = ClosedProfile::create(profile_id, std::move(entities));
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
}

PartDocument make_nested_polygon_document(std::vector<Point2> outer, std::vector<Point2> inner,
                                          ProfileId selected_profile,
                                          std::vector<Point2> island = {}) {
  auto document = PartDocument::create(DocumentId("part.nested.polygon"), "Nested polygon");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto body = Body::create(BodyId("body.nested.polygon"), "Nested polygon", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.value().add_body(body.value()));

  auto sketch =
      Sketch::create(SketchId("sketch.nested.polygon"), "Nested polygon", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_polygon_profile(sketch.value(), "outer", ProfileId("profile.outer"), outer);
  add_polygon_profile(sketch.value(), "inner", ProfileId("profile.inner"), inner);
  if (!island.empty())
    add_polygon_profile(sketch.value(), "island", ProfileId("profile.island"), island);
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.nested.polygon"), "Nested polygon", SketchId("sketch.nested.polygon"),
      ParameterId("part.depth"));
  REQUIRE(feature);
  auto profiled = feature.value().with_profile(std::move(selected_profile));
  REQUIRE(profiled);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.nested.polygon"));
  REQUIRE(context);
  auto attached = profiled.value().with_body_result_context(context.value());
  REQUIRE(attached);
  REQUIRE(document.value().add_feature(attached.value()));
  return document.value();
}

ShapeSummary recompute_nested_polygon(const PartDocument& document) {
  auto cache = ShapeCache::create(ShapeCacheId("cache.nested.polygon"));
  REQUIRE(cache);
  auto summary = GeometryRecomputeExecutor{}.execute_document(document, cache.value());
  REQUIRE(summary);
  REQUIRE(summary.value().executed_feature_count == 1U);
  REQUIRE(cache.value().has_final_shape());
  return RectangleExtrusionAdapter{}.summarize(*cache.value().final_shape());
}

} // namespace

TEST_CASE("Geometry recomputes additive extrude from composite closed profile with a hole",
          "[geometry][composite-profile]") {
  PartDocument document = make_additive_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.composite.additive"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.composite"));

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.value().final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  CHECK(shape.volume_mm3 < 4000.0);
}

TEST_CASE("Geometry recomputes subtractive cut from composite closed profile with a hole",
          "[geometry][composite-profile]") {
  PartDocument document = make_subtractive_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.composite.cut"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.composite_cut"));
}

TEST_CASE("Selected line rectangle extrudes contained circle as an exact hole",
          "[geometry][composite-profile][nested-profile]") {
  const ShapeSummary shape = recompute_nested_profile(
      make_nested_profile_document(false, Point2{}, ProfileId("profile.outer")));
  REQUIRE_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  constexpr double pi = 3.14159265358979323846;
  CHECK(shape.volume_mm3 == Approx((40.0 * 20.0 - pi * 3.0 * 3.0) * 5.0).margin(1.0e-6));
}

TEST_CASE("Selected parametric rectangle extrudes contained circle as an exact hole",
          "[geometry][composite-profile][nested-profile]") {
  const ShapeSummary shape = recompute_nested_profile(
      make_nested_profile_document(true, Point2{}, ProfileId("profile.outer")));
  REQUIRE_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  constexpr double pi = 3.14159265358979323846;
  CHECK(shape.volume_mm3 == Approx((40.0 * 20.0 - pi * 3.0 * 3.0) * 5.0).margin(1.0e-6));
}

TEST_CASE("Circle nested in a rectangle remains separately extrudable",
          "[geometry][composite-profile][nested-profile]") {
  const ShapeSummary shape = recompute_nested_profile(
      make_nested_profile_document(false, Point2{}, ProfileId("profile.circle")));
  REQUIRE_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  constexpr double pi = 3.14159265358979323846;
  CHECK(shape.volume_mm3 == Approx(pi * 3.0 * 3.0 * 5.0).margin(1.0e-6));
}

TEST_CASE("Circle crossing an outer contour is not classified as its hole",
          "[geometry][composite-profile][nested-profile]") {
  const ShapeSummary shape = recompute_nested_profile(
      make_nested_profile_document(false, Point2{19.0, 0.0}, ProfileId("profile.outer")));
  REQUIRE_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  CHECK(shape.volume_mm3 == Approx(40.0 * 20.0 * 5.0).margin(1.0e-6));
}

TEST_CASE("Richer symmetric extrusion preserves an inferred exact circular hole",
          "[geometry][composite-profile][nested-profile][extrude-extent]") {
  const ShapeSummary shape = recompute_nested_profile(
      make_nested_profile_document(false, Point2{}, ProfileId("profile.outer"), true));
  REQUIRE_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  constexpr double pi = 3.14159265358979323846;
  CHECK(shape.volume_mm3 == Approx((40.0 * 20.0 - pi * 3.0 * 3.0) * 5.0).margin(1.0e-6));
}

TEST_CASE("Clockwise line rectangle extrudes its contained circle as a hole",
          "[geometry][composite-profile][nested-profile]") {
  const ShapeSummary shape = recompute_nested_profile(
      make_nested_profile_document(false, Point2{}, ProfileId("profile.outer"), false, true));
  REQUIRE_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  constexpr double pi = 3.14159265358979323846;
  CHECK(shape.volume_mm3 == Approx((40.0 * 20.0 - pi * 3.0 * 3.0) * 5.0).margin(1.0e-6));
}

TEST_CASE("Concave line profile extrudes a contained line triangle as a hole in either winding",
          "[geometry][composite-profile][nested-profile][nested-polygon]") {
  std::vector<Point2> outer{{-10.0, -10.0}, {10.0, -10.0}, {10.0, -2.0},
                            {0.0, -2.0},    {0.0, 10.0},   {-10.0, 10.0}};
  // The first vertex lies on the extension of the outer vertical notch edge.
  // The finite edges do not touch; this remains a valid strict containment.
  std::vector<Point2> inner{{0.0, -6.0}, {-4.0, -8.0}, {-4.0, -4.0}};

  SECTION("counter-clockwise outer and triangle") {
    const ShapeSummary shape = recompute_nested_polygon(
        make_nested_polygon_document(outer, inner, ProfileId("profile.outer")));
    REQUIRE_FALSE(shape.is_null);
    CHECK(shape.solid_count == 1);
    CHECK(shape.volume_mm3 == Approx((280.0 - 8.0) * 5.0).margin(1.0e-6));
  }

  SECTION("clockwise outer and triangle") {
    std::reverse(outer.begin(), outer.end());
    std::reverse(inner.begin(), inner.end());
    const ShapeSummary shape = recompute_nested_polygon(
        make_nested_polygon_document(outer, inner, ProfileId("profile.outer")));
    REQUIRE_FALSE(shape.is_null);
    CHECK(shape.solid_count == 1);
    CHECK(shape.volume_mm3 == Approx((280.0 - 8.0) * 5.0).margin(1.0e-6));
  }
}

TEST_CASE("Line profile nested inside another line profile remains separately extrudable",
          "[geometry][composite-profile][nested-profile][nested-polygon]") {
  const std::vector<Point2> outer{{-10.0, -10.0}, {10.0, -10.0}, {10.0, -2.0},
                                  {0.0, -2.0},    {0.0, 10.0},   {-10.0, 10.0}};
  const std::vector<Point2> inner{{0.0, -6.0}, {-4.0, -8.0}, {-4.0, -4.0}};
  const ShapeSummary shape = recompute_nested_polygon(
      make_nested_polygon_document(outer, inner, ProfileId("profile.inner")));
  REQUIRE_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  CHECK(shape.volume_mm3 == Approx(8.0 * 5.0).margin(1.0e-6));
}

TEST_CASE("Nested profile classifier returns only immediate child contours",
          "[geometry][composite-profile][nested-profile][nested-polygon]") {
  const std::vector<Point2> outer{{-10.0, -10.0}, {10.0, -10.0}, {10.0, 10.0}, {-10.0, 10.0}};
  const std::vector<Point2> inner{{-6.0, -6.0}, {6.0, -6.0}, {6.0, 6.0}, {-6.0, 6.0}};
  const std::vector<Point2> island{{-2.0, -2.0}, {2.0, -2.0}, {0.0, 2.0}};
  PartDocument document =
      make_nested_polygon_document(outer, inner, ProfileId("profile.outer"), island);
  const Sketch* sketch = document.find_sketch(SketchId("sketch.nested.polygon"));
  REQUIRE(sketch != nullptr);

  auto holes =
      classify_contained_profile_holes(document, *sketch, ProfileId("profile.outer"), outer);
  REQUIRE(holes);
  REQUIRE(holes.value().size() == 1U);
  CHECK(holes.value().front().profile == ProfileId("profile.inner"));
  CHECK(holes.value().front().kind == ContainedProfileHoleKind::Polygon);

  const ShapeSummary shape = recompute_nested_polygon(document);
  REQUIRE_FALSE(shape.is_null);
  CHECK(shape.volume_mm3 == Approx((400.0 - 144.0) * 5.0).margin(1.0e-6));

  const ShapeSummary inner_shape = recompute_nested_polygon(
      make_nested_polygon_document(outer, inner, ProfileId("profile.inner"), island));
  REQUIRE_FALSE(inner_shape.is_null);
  CHECK(inner_shape.volume_mm3 == Approx((144.0 - 8.0) * 5.0).margin(1.0e-6));
}

TEST_CASE("Touching and crossing line contours are not inferred as profile holes",
          "[geometry][composite-profile][nested-profile][nested-polygon]") {
  const std::vector<Point2> outer{{-10.0, -10.0}, {10.0, -10.0}, {10.0, 10.0}, {-10.0, 10.0}};

  SECTION("touching") {
    const std::vector<Point2> inner{{-2.0, -10.0}, {2.0, -10.0}, {0.0, -5.0}};
    const ShapeSummary shape = recompute_nested_polygon(
        make_nested_polygon_document(outer, inner, ProfileId("profile.outer")));
    REQUIRE_FALSE(shape.is_null);
    CHECK(shape.volume_mm3 == Approx(400.0 * 5.0).margin(1.0e-6));
  }

  SECTION("crossing") {
    const std::vector<Point2> inner{{-2.0, -5.0}, {12.0, 0.0}, {-2.0, 5.0}};
    const ShapeSummary shape = recompute_nested_polygon(
        make_nested_polygon_document(outer, inner, ProfileId("profile.outer")));
    REQUIRE_FALSE(shape.is_null);
    CHECK(shape.volume_mm3 == Approx(400.0 * 5.0).margin(1.0e-6));
  }
}
