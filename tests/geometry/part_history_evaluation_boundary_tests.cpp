#include "../support/part_history_fixture.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace blcad;
using namespace blcad::geometry;
namespace fixture = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
template <typename T> void unsupported(Result<T> result) {
  REQUIRE_FALSE(result);
  CHECK(result.error().category() == ErrorCategory::Dependency);
  CHECK(result.error().message().find("effective evaluator") != std::string::npos);
}
PartDocument explicit_history(PartDocument part, bool suppress) {
  PartFeatureHistoryIntent edit{{"base"}, {}};
  if (suppress)
    edit.suppressed = {"base"};
  REQUIRE(part.apply_history_intent(std::move(edit)));
  return part;
}
} // namespace

TEST_CASE(
    "Unimplemented effective history cannot execute a full incremental or direct feature plan",
    "[geometry][parity-b146][history-evaluation-boundary]") {
  const bool suppress = GENERATE(false, true);
  const auto original = fixture::native_base();
  auto cache = checked(ShapeCache::create(ShapeCacheId("history.cache")));
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(original, cache));
  REQUIRE(cache.find_body_shape(BodyId("body.base")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(1600));
  const auto part = explicit_history(original, suppress);
  const auto empty_plan =
      checked(RecomputePlan::from_graph_and_changed_nodes(part.dependency_graph(), {}));
  REQUIRE(empty_plan.empty());
  unsupported(executor.execute_document(part, cache));
  unsupported(executor.execute_plan(part, empty_plan, cache));
  using Command = Result<std::size_t> (GeometryRecomputeExecutor::*)(const PartDocument&, FeatureId,
                                                                     ShapeCache&) const;
  const Command commands[]{&GeometryRecomputeExecutor::execute_additive_extrude,
                           &GeometryRecomputeExecutor::execute_subtractive_extrude,
                           &GeometryRecomputeExecutor::execute_body_boolean,
                           &GeometryRecomputeExecutor::execute_imported_body_feature,
                           &GeometryRecomputeExecutor::execute_revolve,
                           &GeometryRecomputeExecutor::execute_linear_pattern,
                           &GeometryRecomputeExecutor::execute_circular_pattern,
                           &GeometryRecomputeExecutor::execute_mirror,
                           &GeometryRecomputeExecutor::execute_fillet,
                           &GeometryRecomputeExecutor::execute_chamfer,
                           &GeometryRecomputeExecutor::execute_shell,
                           &GeometryRecomputeExecutor::execute_draft,
                           &GeometryRecomputeExecutor::execute_surface_feature,
                           &GeometryRecomputeExecutor::execute_sweep,
                           &GeometryRecomputeExecutor::execute_loft};
  for (const auto command : commands)
    unsupported((executor.*command)(part, FeatureId("base"), cache));
  unsupported(executor.execute_body_transform(part, BodyTransformId("transform"), cache));
  unsupported(executor.preview_path_curve(part, PathCurveId("path"), cache));
  unsupported(executor.resolve_path_segments(part, PathCurveId("path"), cache));
  unsupported(
      executor.resolve_feature_axis(part, fixture::axis(PartFeatureInputRole::RevolveAxis), cache));
  CHECK(cache.feature_shape_count() == 1);
  CHECK(cache.body_shape_count() == 1);
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(1600));
}

TEST_CASE(
    "Unimplemented effective history cannot consume old geometry in picking inspection or export",
    "[geometry][parity-b146][history-evaluation-boundary]") {
  const bool suppress = GENERATE(false, true);
  const auto original = fixture::native_base();
  auto cache = checked(ShapeCache::create(ShapeCacheId("history.old.cache")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(original, cache));
  const auto part = explicit_history(original, suppress);
  unsupported(BodyResultInspector{}.inspect(part, cache));
  unsupported(BodyResultInspector{}.resolve_compatible_final_shape(part, cache));
  unsupported(ViewportSceneBuilder{}.build_part(part, cache));
  unsupported(ViewportSceneBuilder{}.build_construction_sources(part, cache));
  unsupported(ViewportSceneBuilder{}.build_body_rotation_sources(part, cache));
  const auto edge =
      checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopFront));
  const auto vertex =
      checked(SemanticVertexReference::create(FeatureId("base"), SemanticVertex::TopFrontLeft));
  unsupported(SemanticReferenceEvaluator{}.resolve_edge(part, edge));
  unsupported(SemanticReferenceEvaluator{}.resolve_edge(part, edge, cache));
  unsupported(SemanticReferenceEvaluator{}.resolve_vertex(part, vertex));
  unsupported(SemanticReferenceEvaluator{}.resolve_vertex(part, vertex, cache));
  unsupported(WorkplaneResolver{}.resolve(part, DatumPlaneId("datum.xy")));
  unsupported(WorkplaneResolver{}.resolve(part, DatumPlaneId("datum.xy"), cache));
  unsupported(WorkplaneResolver{}.resolve_for_sketch(part, *part.find_sketch(SketchId("profile"))));
  unsupported(
      WorkplaneResolver{}.resolve_for_sketch(part, *part.find_sketch(SketchId("profile")), cache));
  unsupported(WorkplaneResolver{}.resolve_generated_face(
      part, checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  unsupported(ResultFaceSupportGeometry::selectable_faces(part, cache, BodyId("body.base")));
  const auto face = checked(ResultFaceReference::create(FeatureId("base"), BodyId("body.base"),
                                                        {{FeatureId("base"), "cap.end"}}));
  const auto plane = checked(ResultFaceWorkplane::create(DatumPlaneId("support"), "Support", face));
  unsupported(ResultFaceSupportGeometry::resolve(part, plane, cache));
  unsupported(cache.validated_reference_result(part));
  // This API must reject new history before attempting source ownership validation.
  const auto imported = fixture::imported().imported_body_features().front();
  unsupported(cache.validated_imported_body_result(part, imported));
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("blcad-history-boundary-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  REQUIRE(std::filesystem::create_directory(directory));
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  } cleanup{directory};
  const auto path = directory / "committed.step";
  {
    std::ofstream output(path);
    output << "committed export";
  }
  unsupported(StepExporter{}.write_part_step(part, cache, path.string()));
  unsupported(StepExporter{}.write_document_step(part, cache, path.string()));
  std::ifstream input(path);
  CHECK(std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()) ==
        "committed export");
}
