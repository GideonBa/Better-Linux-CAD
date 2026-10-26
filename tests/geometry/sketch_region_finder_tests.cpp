#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/geometry/sketch_region_finder.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

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

Sketch make_unordered_square_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.region"), "Region", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.right", Point2{20.0, 0.0}, Point2{20.0, 10.0});
  add_line(sketch.value(), "line.bottom", Point2{0.0, 0.0}, Point2{20.0, 0.0});
  add_line(sketch.value(), "line.left", Point2{0.0, 10.0}, Point2{0.0, 0.0});
  add_line(sketch.value(), "line.top", Point2{20.0, 10.0}, Point2{0.0, 10.0});
  return sketch.value();
}

Sketch make_branched_sketch() {
  auto sketch = make_unordered_square_sketch();
  add_line(sketch, "line.branch", Point2{20.0, 10.0}, Point2{30.0, 10.0});
  return sketch;
}

PartDocument make_additive_region_document() {
  auto document = PartDocument::create(DocumentId("part.region.additive"), "RegionAdditive");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  auto sketch = make_unordered_square_sketch();
  REQUIRE(document.value().add_sketch(sketch));
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.region"), "Region", SketchId("sketch.region"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument make_subtractive_region_document() {
  auto document = PartDocument::create(DocumentId("part.region.cut"), "RegionCut");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));
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

  auto cut_sketch =
      Sketch::create(SketchId("sketch.cut_region"), "CutRegion", DatumPlaneId("datum.xy"));
  REQUIRE(cut_sketch);
  add_line(cut_sketch.value(), "line.cut.b", Point2{-5.0, -5.0}, Point2{5.0, -5.0});
  add_line(cut_sketch.value(), "line.cut.c", Point2{-5.0, 5.0}, Point2{-5.0, -5.0});
  add_line(cut_sketch.value(), "line.cut.a", Point2{5.0, -5.0}, Point2{-5.0, 5.0});
  REQUIRE(document.value().add_sketch(cut_sketch.value()));
  auto cut_feature = Feature::create_subtractive_extrude(
      FeatureId("feature.cut"), "Cut", SketchId("sketch.cut_region"), FeatureId("feature.base"));
  REQUIRE(cut_feature);
  REQUIRE(document.value().add_feature(cut_feature.value()));
  return document.value();
}

} // namespace

TEST_CASE("SketchRegionFinder detects a deterministic single region from unordered explicit lines",
          "[geometry][sketch-region]") {
  PartDocument document = make_additive_region_document();
  const Sketch* sketch = document.find_sketch(SketchId("sketch.region"));
  REQUIRE(sketch != nullptr);

  const SketchRegionFinder finder;
  auto region = finder.find_single_region(document, *sketch);
  REQUIRE(region);
  CHECK(region.value().id == ProfileId("generated.region.sketch.region.0"));
  REQUIRE(region.value().line_segments.size() == 4U);
  REQUIRE(region.value().vertices.size() == 4U);
  CHECK(region.value().vertices[0].x == Catch::Approx(20.0));
  CHECK(region.value().vertices[0].y == Catch::Approx(0.0));

  auto profile = finder.make_closed_profile(region.value());
  REQUIRE(profile);
  CHECK(profile.value().line_segments().size() == 4U);
}

TEST_CASE("SketchRegionFinder rejects branched regions", "[geometry][sketch-region]") {
  auto document = PartDocument::create(DocumentId("part.region.branch"), "RegionBranch");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  Sketch sketch = make_branched_sketch();
  REQUIRE(document.value().add_sketch(sketch));

  const Sketch* stored = document.value().find_sketch(SketchId("sketch.region"));
  REQUIRE(stored != nullptr);
  const SketchRegionFinder finder;
  auto region = finder.find_single_region(document.value(), *stored);
  REQUIRE_FALSE(region);
  CHECK(region.error().message() == "automatic region detection rejects gaps and branched regions");
}

TEST_CASE("Geometry recomputes additive extrude from detected sketch region",
          "[geometry][sketch-region]") {
  PartDocument document = make_additive_region_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.region.additive"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.region"));
}

TEST_CASE("Geometry recomputes subtractive cut from detected sketch region",
          "[geometry][sketch-region]") {
  PartDocument document = make_subtractive_region_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.region.cut"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.cut"));
}

TEST_CASE("Block 119 recognizes and selects multiple disjoint bounded regions",
          "[geometry][sketch-regions][gui][sketch-profile-selection]") {
  auto document = PartDocument::create(DocumentId("part.multi-region"), "MultiRegion");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.multi"), "Multi", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "a.bottom", {0.0, 0.0}, {4.0, 0.0});
  add_line(sketch.value(), "a.right", {4.0, 0.0}, {4.0, 4.0});
  add_line(sketch.value(), "a.top", {4.0, 4.0}, {0.0, 4.0});
  add_line(sketch.value(), "a.left", {0.0, 4.0}, {0.0, 0.0});
  add_line(sketch.value(), "b.bottom", {10.0, 0.0}, {14.0, 0.0});
  add_line(sketch.value(), "b.right", {14.0, 0.0}, {14.0, 4.0});
  add_line(sketch.value(), "b.top", {14.0, 4.0}, {10.0, 4.0});
  add_line(sketch.value(), "b.left", {10.0, 4.0}, {10.0, 0.0});
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto analysis = SketchFinishService::analyze(document.value(), sketch.value());
  REQUIRE(analysis);
  CHECK(analysis.value().regions.size() == 2U);
  CHECK(analysis.value().diagnostics.empty());
  auto selected = SketchFinishService::select_region_at(analysis.value(), {12.0, 2.0});
  REQUIRE(selected.has_value());
  CHECK(selected->value() == "generated.region.sketch.multi.1");
}

TEST_CASE("Block 119 emits open-contour diagnostics and fails Finish Sketch atomically",
          "[geometry][sketch-regions][integration][sketch-finish]") {
  auto document = PartDocument::create(DocumentId("part.open"), "Open");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.open"), "Open", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.a", {0.0, 0.0}, {5.0, 0.0});
  add_line(sketch.value(), "line.b", {5.0, 0.0}, {5.0, 5.0});
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto analysis = SketchFinishService::analyze(document.value(), sketch.value());
  REQUIRE(analysis);
  REQUIRE(analysis.value().diagnostics.size() == 1U);
  CHECK(analysis.value().diagnostics.front().kind == SketchRegionDiagnosticKind::OpenContour);
  auto finished = SketchFinishService::finish(document.value(), SketchId("sketch.open"));
  REQUIRE_FALSE(finished);
  CHECK(document.value().find_sketch(SketchId("sketch.open"))->profile_count() == 0U);
}

TEST_CASE("Block 119 Finish Sketch materializes the selected region in one document candidate",
          "[integration][sketch-finish]") {
  auto document = PartDocument::create(DocumentId("part.finish"), "Finish");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  Sketch sketch = make_unordered_square_sketch();
  REQUIRE(document.value().add_sketch(sketch));

  auto finished = SketchFinishService::finish(document.value(), SketchId("sketch.region"));
  REQUIRE(finished);
  CHECK(finished.value().selected_profile == ProfileId("generated.region.sketch.region.0"));
  const auto* completed = finished.value().document.find_sketch(SketchId("sketch.region"));
  REQUIRE(completed != nullptr);
  CHECK(completed->closed_profiles().size() == 1U);
  CHECK(document.value().find_sketch(SketchId("sketch.region"))->closed_profiles().empty());
}

TEST_CASE("Curved region extraction materializes exact two-edge circle and semicircle profiles",
          "[geometry][sketch-regions][sketch-curved-regions][integration][sketch-finish]") {
  auto part = PartDocument::create(DocumentId("curved.regions"), "Curved regions").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(part.add_parameter(make_length_parameter("depth", "Depth", 4)));
  auto sketch = Sketch::create(SketchId("curved"), "Curved", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_entity(ArcSegment::create_three_point(SketchEntityId("top"), {-10, 0}, {0, 10}, {10, 0}).value()));
  double area = std::acos(-1.0) * 50;
  SECTION("semicircle") {
    add_line(sketch, "base", {-10, 0}, {10, 0});
  }
  SECTION("circle") {
    REQUIRE(sketch.add_entity(ArcSegment::create_three_point(SketchEntityId("bottom"), {-10, 0}, {0, -10}, {10, 0}).value()));
    area *= 2;
  }
  REQUIRE(part.add_sketch(sketch));
  const auto before = serialize_part_document_to_json(part).value();
  auto analysis = SketchFinishService::analyze(part, sketch);
  REQUIRE(analysis);
  INFO((analysis.value().diagnostics.empty() ? "No diagnostics" : analysis.value().diagnostics.front().message));
  REQUIRE(analysis.value().diagnostics.empty());
  REQUIRE(analysis.value().regions.size() == 1);
  const auto& region = analysis.value().regions.front();
  CHECK(region.curve_segments.size() == 2);
  CHECK(region.vertices.size() > 10);
  CHECK(SketchFinishService::select_region_at(analysis.value(), {0, 5}) == region.id);
  CHECK_FALSE(SketchFinishService::select_region_at(analysis.value(), {20, 0}));
  CHECK_FALSE(SketchRegionFinder{}.make_closed_profile(region));
  auto finished = SketchFinishService::finish(part, sketch.id());
  REQUIRE(finished);
  CHECK(serialize_part_document_to_json(part).value() == before);
  const auto* completed = finished.value().document.find_sketch(sketch.id());
  REQUIRE(completed);
  CHECK(completed->closed_profiles().empty());
  REQUIRE(completed->arc_closed_profiles().size() == 1);
  CHECK(completed->arc_closed_profiles().front().curve_segments().size() == 2);
  auto serialized = serialize_part_document_to_json(finished.value().document).value();
  auto restored = deserialize_part_document_from_json(serialized);
  REQUIRE(restored);
  CHECK(serialize_part_document_to_json(restored.value()).value() == serialized);
  REQUIRE(restored.value().add_feature(Feature::create_additive_extrude(
      FeatureId("extrude"), "Extrude", sketch.id(), ParameterId("depth")).value()));
  auto cache = ShapeCache::create(ShapeCacheId("curved.cache")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored.value(), cache));
  REQUIRE(cache.final_shape());
  CHECK(RectangleExtrusionAdapter{}.summarize(*cache.final_shape()).volume_mm3 ==
        Catch::Approx(area * 4).epsilon(1e-8));
}

TEST_CASE("Raw rational ellipse regions present exact selectable faces and preserve source weights",
          "[geometry][sketch-regions][sketch-curved-regions][viewport-scene]") {
  auto part = PartDocument::create(DocumentId("ellipse.regions"), "Ellipse").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("ellipse"), "Ellipse", DatumPlaneId("datum.xy")).value();
  for (int i = 3; i >= 0; --i) {
    auto span = SplineSegment::create_elliptical_arc(SketchEntityId("span." + std::to_string(i)),
        {20, 0}, {10, 0}, 5, i * std::acos(-1.0) / 2, std::acos(-1.0) / 2).value();
    if (i % 2) {
      auto weights = span.weights();
      std::reverse(weights.begin(), weights.end());
      span = SplineSegment::create_cubic_bezier(span.id(), span.end(), span.control2(),
                                               span.control1(), span.start(), weights).value();
    }
    REQUIRE(sketch.add_entity(span));
  }
  REQUIRE(part.add_sketch(sketch));
  auto analysis = SketchFinishService::analyze_unmaterialized(part, sketch);
  REQUIRE(analysis);
  INFO((analysis.value().diagnostics.empty() ? "No diagnostics" : analysis.value().diagnostics.front().message));
  REQUIRE(analysis.value().diagnostics.empty());
  REQUIRE(analysis.value().regions.size() == 1);
  CHECK(SketchFinishService::select_region_at(analysis.value(), {20, 0}));
  auto cache = ShapeCache::create(ShapeCacheId("cache")).value();
  auto scene = ViewportSceneBuilder{}.build_part(part, cache);
  REQUIRE(scene);
  const auto semantic = "sketch/ellipse/profile/" + analysis.value().regions.front().id.value();
  auto found = std::find_if(scene.value().begin(), scene.value().end(), [&](const auto& item) {
    return item.semantic_id == semantic;
  });
  REQUIRE(found != scene.value().end());
  const auto* shape = OcctShapeView::get(found->shape);
  REQUIRE(shape);
  GProp_GProps properties;
  BRepGProp::SurfaceProperties(*shape, properties);
  CHECK(properties.Mass() == Catch::Approx(std::acos(-1.0) * 10 * 5).epsilon(1e-8));
  auto finished = SketchFinishService::finish(part, sketch.id());
  REQUIRE(finished);
  const auto* completed = finished.value().document.find_sketch(sketch.id());
  for (const auto& source : sketch.spline_segments()) {
    const auto* span = completed->find_spline_segment(source.id());
    REQUIRE(span);
    CHECK(span->weights() == source.weights());
    CHECK(span->start().x == source.start().x);
    CHECK(span->end().x == source.end().x);
  }
  auto repeated = SketchFinishService::analyze_unmaterialized(finished.value().document, *completed);
  REQUIRE(repeated);
  CHECK(repeated.value().regions.empty());
  CHECK(repeated.value().diagnostics.empty());
}

TEST_CASE("Mixed curve analysis distinguishes disjoint regions from open and branched components",
          "[geometry][sketch-regions][sketch-curved-regions]") {
  auto part = PartDocument::create(DocumentId("mixed.regions"), "Mixed").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("mixed"), "Mixed", DatumPlaneId("datum.xy")).value();
  for (int i = 0; i < 2; ++i) {
    const double x = i * 30;
    REQUIRE(sketch.add_entity(ArcSegment::create_three_point(SketchEntityId("top." + std::to_string(i)),
        {x - 5, 0}, {x, 5}, {x + 5, 0}).value()));
    REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId("base." + std::to_string(i)),
        {x - 5, 0}, {x + 5, 0}).value()));
  }
  SECTION("two regions require explicit selection") {
    REQUIRE(part.add_sketch(sketch));
    auto analysis = SketchFinishService::analyze(part, sketch).value();
    REQUIRE(analysis.regions.size() == 2);
    CHECK(analysis.diagnostics.empty());
    CHECK_FALSE(SketchFinishService::finish(part, sketch.id()));
    auto selected = SketchFinishService::select_region_at(analysis, {30, 2});
    REQUIRE(selected);
    auto finished = SketchFinishService::finish(part, sketch.id(), selected);
    REQUIRE(finished);
    CHECK(finished.value().document.find_sketch(sketch.id())->arc_closed_profiles().size() == 1);
  }
  SECTION("open arc remains an open diagnostic") {
    REQUIRE(sketch.add_entity(ArcSegment::create_three_point(SketchEntityId("open"), {50, 0}, {55, 5}, {60, 0}).value()));
    auto analysis = SketchFinishService::analyze(part, sketch).value();
    CHECK(analysis.regions.size() == 2);
    REQUIRE(analysis.diagnostics.size() == 1);
    CHECK(analysis.diagnostics.front().kind == SketchRegionDiagnosticKind::OpenContour);
  }
  SECTION("branch cannot masquerade as an open end") {
    add_line(sketch, "branch", {-5, 0}, {-10, 10});
    auto analysis = SketchFinishService::analyze(part, sketch).value();
    CHECK(analysis.regions.size() == 1);
    REQUIRE(analysis.diagnostics.size() == 1);
    CHECK(analysis.diagnostics.front().kind == SketchRegionDiagnosticKind::AmbiguousTopology);
  }
}

TEST_CASE("Curved region validation rejects crossings inside splines and overlapping arcs",
          "[geometry][sketch-regions][sketch-curved-regions]") {
  auto part = PartDocument::create(DocumentId("invalid.curves"), "Invalid").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("invalid"), "Invalid", DatumPlaneId("datum.xy")).value();
  SECTION("spline crosses another edge away from its chord") {
    add_line(sketch, "base", {0, 0}, {10, 0});
    add_line(sketch, "right", {10, 0}, {10, 10});
    add_line(sketch, "left", {0, 10}, {0, 0});
    REQUIRE(sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("spline"),
        {10, 10}, {10, -20}, {0, -20}, {0, 10}).value()));
  }
  SECTION("a spline intersects itself") {
    add_line(sketch, "base", {1, 0}, {0, 0});
    REQUIRE(sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("spline"),
        {0, 0}, {3, 3}, {-3, 3}, {1, 0}).value()));
  }
  SECTION("arcs overlap on the same supporting circle") {
    REQUIRE(sketch.add_entity(ArcSegment::create_three_point(SketchEntityId("a"), {-10, 0}, {0, 10}, {10, 0}).value()));
    REQUIRE(sketch.add_entity(ArcSegment::create_three_point(SketchEntityId("b"), {10, 0}, {0, 10}, {-10, 0}).value()));
  }
  REQUIRE(part.add_sketch(sketch));
  const auto before = serialize_part_document_to_json(part).value();
  auto analysis = SketchFinishService::analyze(part, sketch).value();
  CHECK(analysis.regions.empty());
  REQUIRE(analysis.diagnostics.size() == 1);
  INFO(analysis.diagnostics.front().message);
  CHECK(analysis.diagnostics.front().kind == SketchRegionDiagnosticKind::SelfIntersection);
  CHECK_FALSE(SketchFinishService::finish(part, sketch.id()));
  CHECK(serialize_part_document_to_json(part).value() == before);
}

TEST_CASE("A valid spline region is not rejected by intersections of its endpoint chord",
          "[geometry][sketch-regions][sketch-curved-regions]") {
  auto part = PartDocument::create(DocumentId("spline.notch"), "Spline notch").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("notch"), "Notch", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("top"),
      {-10, 0}, {-10, 20}, {10, 20}, {10, 0}).value()));
  add_line(sketch, "right", {10, 0}, {10, -10});
  add_line(sketch, "notch.right", {10, -10}, {0, 5});
  add_line(sketch, "notch.left", {0, 5}, {-10, -10});
  add_line(sketch, "left", {-10, -10}, {-10, 0});
  REQUIRE(part.add_sketch(sketch));
  auto analysis = SketchFinishService::analyze(part, sketch).value();
  REQUIRE(analysis.diagnostics.empty());
  REQUIRE(analysis.regions.size() == 1);
  auto finished = SketchFinishService::finish(part, sketch.id());
  INFO((finished ? std::string("Profile materialized") : finished.error().message()));
  REQUIRE(finished);
  CHECK(finished.value().document.find_sketch(sketch.id())->arc_closed_profiles().size() == 1);
}

TEST_CASE("Nested rational spline regions expose the outer material face with an exact hole",
          "[geometry][sketch-regions][sketch-curved-regions][nested-spline-region]") {
  auto part = PartDocument::create(DocumentId("nested.spline"), "Nested spline").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(part.add_parameter(make_length_parameter("depth", "Depth", 6)));
  auto sketch = Sketch::create(SketchId("nested"), "Nested", DatumPlaneId("datum.xy")).value();
  add_line(sketch, "a.bottom", {-20, -15}, {20, -15});
  add_line(sketch, "a.right", {20, -15}, {20, 15});
  add_line(sketch, "a.top", {20, 15}, {-20, 15});
  add_line(sketch, "a.left", {-20, 15}, {-20, -15});
  for (int i = 0; i < 4; ++i)
    REQUIRE(sketch.add_entity(SplineSegment::create_elliptical_arc(
                                  SketchEntityId("b.spline." + std::to_string(i)), {}, {8, 0}, 4,
                                  i * std::acos(-1.0) / 2, std::acos(-1.0) / 2)
                                  .value()));
  REQUIRE(part.add_sketch(sketch));
  auto analysis = SketchFinishService::analyze(part, sketch).value();
  REQUIRE(analysis.regions.size() == 2);
  const auto outer = SketchFinishService::select_region_at(analysis, {15, 0});
  const auto inner = SketchFinishService::select_region_at(analysis, {0, 0});
  REQUIRE(outer);
  REQUIRE(inner);
  CHECK(outer != inner);
  const auto material_area = 40 * 30 - std::acos(-1.0) * 8 * 4;
  const auto check_face = [&](const PartDocument& state) {
    auto cache = ShapeCache::create(ShapeCacheId("scene")).value();
    auto scene = ViewportSceneBuilder{}.build_part(state, cache);
    REQUIRE(scene);
    auto face = std::find_if(scene.value().begin(), scene.value().end(), [&](const auto& item) {
      return item.semantic_id == "sketch/nested/profile/" + outer->value();
    });
    REQUIRE(face != scene.value().end());
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(*OcctShapeView::get(face->shape), properties);
    CHECK(properties.Mass() == Catch::Approx(material_area).epsilon(1e-8));
  };
  check_face(part);
  auto finished = SketchFinishService::finish(part, sketch.id(), outer);
  REQUIRE(finished);
  part = finished.value().document;
  // The inner spline remains unmaterialized and still participates in the
  // selected outer region; source curves/weights never become a polygon.
  REQUIRE(part.find_sketch(sketch.id())->arc_closed_profiles().empty());
  check_face(part);
  REQUIRE(part.add_feature(Feature::create_additive_extrude(FeatureId("extrude"), "Material",
                                                            sketch.id(), ParameterId("depth"))
                               .value()
                               .with_profile(*outer)
                               .value()));
  auto cache = ShapeCache::create(ShapeCacheId("solid")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  CHECK(RectangleExtrusionAdapter{}.summarize(*cache.final_shape()).volume_mm3 ==
        Catch::Approx(material_area * 6).epsilon(1e-8));
  auto restored =
      deserialize_part_document_from_json(serialize_part_document_to_json(part).value());
  REQUIRE(restored);
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored.value(), cache));
  CHECK(RectangleExtrusionAdapter{}.summarize(*cache.final_shape()).volume_mm3 ==
        Catch::Approx(material_area * 6).epsilon(1e-8));
}
