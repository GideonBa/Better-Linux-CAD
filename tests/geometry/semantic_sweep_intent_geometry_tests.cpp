#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/derived_curve.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;

TEST_CASE("Unimplemented material-region intent preserves the certified outer-boundary result",
          "[geometry][parity-b167][semantic-sweep-intent]") {
  auto part = sf::straight_sweep();
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("material.intent")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto accepted = part;
  const auto old = *part.find_sweep_feature(FeatureId("driven.sweep"));
  const auto region = std::get<ProfileRegionReference>(old.profile().source());
  const auto profile = f::checked(
      SweepProfileReference::create_closed_region(region, SweepRegionPolicy::MaterialRegion));
  REQUIRE(part.update_sweep_feature(f::checked(SweepFeature::create_sweep(
      old.id(), old.name(), profile, old.path(), old.body_result_context()))));
  const auto source = f::checked(serialize_part_document_to_json(part));
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("multi-wire") != std::string::npos);
  CHECK(f::checked(serialize_part_document_to_json(part)) == source);
  CHECK(cache.validate_complete_geometry_result(accepted));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
  REQUIRE(cache.find_body_shape(BodyId("driven.body")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("driven.body")))
            .volume_mm3 == Catch::Approx(6.4).margin(1e-6));
}

namespace {
PathCurve edge_path(PathSegmentReference segment, PathClosure closure = PathClosure::Open) {
  return f::checked(PathCurve::create(PathCurveId("semantic.path"), "Semantic path", {segment},
                                      closure, PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                                      f::checked(PathExecutionPolicy::curve_preserving(1e-6))));
}
EdgeReference top_front(bool reverse = false) {
  std::array<Point3, ResultEdgeReference::sample_count> samples;
  for (std::size_t i = 0; i < samples.size(); ++i)
    samples[i] = {-4 + 8 * double(i) / double(samples.size() - 1), -3, 5};
  if (reverse)
    std::reverse(samples.begin(), samples.end());
  return f::checked(EdgeReference::create_result(
      PartFeatureInputRole::PathSegment,
      f::checked(ResultEdgeReference::create(FeatureId("base.feature"), 0, 8, samples))));
}
void check_edge(const PartDocument& part, ShapeCache& cache, double length, Point3 start,
                Point3 end) {
  GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(part, cache));
  const auto path =
      f::checked(executor.resolve_path_segments(part, PathCurveId("semantic.path"), cache));
  REQUIRE(path.size() == 1);
  CHECK(path[0].kind == SweepPathSegmentKind::DerivedNative);
  CHECK(distance(path[0].start, start) < 1e-7);
  CHECK(distance(path[0].end, end) < 1e-7);
  CHECK(f::checked(SweepAdapter{}.measure_path_length(PathCurveId("semantic.path"), path)) ==
        Catch::Approx(length).margin(1e-7));
}
} // namespace
TEST_CASE("Recovered native path direction uses the canonical witness before explicit reversal",
          "[geometry][parity-b167][semantic-sweep-intent][semantic-path-geometry]") {
  for (bool witness_reversed : {false, true})
    for (bool path_reversed : {false, true}) {
      auto part = f::part();
      REQUIRE(part.add_path_curve(edge_path(f::checked(PathSegmentReference::create_feature_edge(
          top_front(witness_reversed), {}, path_reversed)))));
      auto cache = f::checked(ShapeCache::create(ShapeCacheId("directions")));
      CHECK(std::get<ResultEdgeReference>(top_front(witness_reversed).source()).identity() ==
            std::get<ResultEdgeReference>(top_front(false).source()).identity());
      const bool backwards = path_reversed;
      check_edge(part, cache, 8, {backwards ? 4. : -4., -3, 5}, {backwards ? -4. : 4., -3, 5});
      const auto saved = f::checked(serialize_part_document_to_json(part));
      auto reopened = f::checked(deserialize_part_document_from_json(saved));
      auto fresh = f::checked(ShapeCache::create(ShapeCacheId("reopened")));
      check_edge(reopened, fresh, 8, {backwards ? 4. : -4., -3, 5}, {backwards ? -4. : 4., -3, 5});
      CHECK(f::checked(serialize_part_document_to_json(reopened)) == saved);
    }
}
TEST_CASE("A real upstream edge split loses its full witness and retains the accepted cache",
          "[geometry][parity-b167][semantic-sweep-intent][semantic-path-geometry]") {
  auto part = f::part();
  REQUIRE(part.add_path_curve(
      edge_path(f::checked(PathSegmentReference::create_feature_edge(top_front())))));
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("split")));
  check_edge(part, cache, 8, {-4, -3, 5}, {4, -3, 5});
  const auto accepted = part;
  const auto original = *part.find_sketch(SketchId("base.sketch"));
  auto split = f::checked(Sketch::create(original.id(), original.name(), DatumPlaneId("datum.xy")));
  const std::array<Point2, 5> points{{{-4, -3}, {0, -3}, {4, -3}, {4, 3}, {-4, 3}}};
  std::vector<SketchEntityId> edges;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto id = SketchEntityId("split." + std::to_string(i));
    REQUIRE(split.add_entity(
        f::checked(LineSegment::create(id, points[i], points[(i + 1) % points.size()]))));
    edges.push_back(id);
  }
  REQUIRE(split.add_profile(f::checked(ClosedProfile::create(ProfileId("base.profile"), edges))));
  REQUIRE(part.update_sketch(split));
  const auto source = f::checked(serialize_part_document_to_json(part));
  const auto plan = f::checked(part.create_recompute_plan());
  const auto planned = GeometryRecomputeExecutor{}.execute_plan(part, plan, cache);
  REQUIRE_FALSE(planned);
  CHECK(planned.error().message().find("no longer exists") != std::string::npos);
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  CHECK(cache.validate_complete_geometry_result(accepted));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
  CHECK(f::checked(serialize_part_document_to_json(part)) == source);
  REQUIRE(part.update_sketch(original));
  check_edge(part, cache, 8, {-4, -3, 5}, {4, -3, 5});
}
TEST_CASE("Generated linear roles and circular rims retain exact native geometry",
          "[geometry][parity-b167][semantic-sweep-intent][semantic-path-geometry]") {
  SECTION("Existing generated linear references opt in to native execution") {
    auto part = f::part();
    REQUIRE(part.add_path_curve(
        edge_path(f::checked(PathSegmentReference::create_semantic_edge(f::checked(
            SemanticEdgeReference::create(FeatureId("base.feature"), SemanticEdge::TopFront)))))));
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("linear")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    const auto path = f::checked(GeometryRecomputeExecutor{}.resolve_path_segments(
        part, PathCurveId("semantic.path"), cache));
    CHECK(f::checked(SweepAdapter{}.measure_path_length(PathCurveId("semantic.path"), path)) ==
          Catch::Approx(8));
  }
  SECTION("Circular role phase is anchored to the source sketch") {
    auto part = f::part();
    auto sketch =
        f::checked(Sketch::create(SketchId("rim.sketch"), "Rim", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(
        f::checked(CircleProfile::create(ProfileId("circle"), ParameterId("radius")))));
    REQUIRE(part.add_sketch(sketch));
    auto cut = f::checked(Feature::create_subtractive_extrude(
        FeatureId("circle.cut"), "Cut", sketch.id(), FeatureId("base.feature")));
    REQUIRE(part.add_feature(
        f::checked(cut.with_body_result_context(f::checked(FeatureBodyResultContext::create(
            FeatureBodyOperationMode::Cut, BodyId("base.body"), {}))))));
    const auto edge = f::checked(EdgeReference::create_circular(
        PartFeatureInputRole::PathSegment,
        f::checked(SemanticCircularEdgeReference::create(cut.id(), ProfileId("circle"),
                                                         SemanticCircularEdge::SourceRim))));
    REQUIRE(part.add_path_curve(edge_path(
        f::checked(PathSegmentReference::create_feature_edge(edge)), PathClosure::Closed)));
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("circular")));
    check_edge(part, cache, 4 * std::numbers::pi, {2, 0, 0}, {2, 0, 0});
  }
}
TEST_CASE("Imported native paths use the explicit normalized base Body and verify source bytes",
          "[geometry][parity-b167][semantic-sweep-intent][semantic-path-geometry]") {
  imported_body_test::Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(gp_Pnt(12, 7, 3), 8, 6, 5).Shape(), "INCH");
  auto part = fixture.editable();
  const auto& catalog = part.imported_topology_catalogs().front();
  const auto& output = part.imported_body_features().front().definition().outputs().front();
  const auto found =
      std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& e) {
        return e.descriptor.kind == ImportedTopologyKind::Edge &&
               std::abs(e.descriptor.measure - 8) < 1e-6;
      });
  REQUIRE(found != catalog.entries().end());
  const auto imported = f::checked(
      ImportedTopologyReference::create(catalog.id(), found->id, ImportedTopologyCapability::Edge));
  const auto edge =
      f::checked(EdgeReference::create_imported(PartFeatureInputRole::PathSegment, imported));
  REQUIRE(part.add_path_curve(
      edge_path(f::checked(PathSegmentReference::create_feature_edge(edge, output.body)))));
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("imported")));
  StepImportTopologyContext context(fixture.root);
  GeometryRecomputeExecutor executor(&context);
  REQUIRE(executor.execute_document(part, cache));
  const auto path =
      f::checked(executor.resolve_path_segments(part, PathCurveId("semantic.path"), cache));
  CHECK(f::checked(SweepAdapter{}.measure_path_length(PathCurveId("semantic.path"), path)) ==
        Catch::Approx(8).margin(1e-6));
  CHECK(path.front().start.x >= 12 - 1e-6);
  CHECK(path.front().start.y >= 7 - 1e-6);
  CHECK(path.front().start.z >= 3 - 1e-6);
  const auto source = f::checked(serialize_part_document_to_json(part));
  const auto bytes = fixture.source_bytes();
  fixture.write_bytes(bytes + "\n");
  CHECK_FALSE(executor.execute_document(part, cache));
  CHECK(f::checked(serialize_part_document_to_json(part)) == source);
  fixture.write_bytes(bytes);
  StepImportTopologyContext restored(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&restored}.execute_document(part, cache));
}
