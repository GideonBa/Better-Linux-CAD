#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/geometry/sketch_region_finder.hpp"

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
