#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/assembly_interference_analyzer.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/finishing_selection.hpp"
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
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
template <typename T> void unavailable(Result<T> result) {
  REQUIRE_FALSE(result);
  CHECK(result.error().category() == ErrorCategory::Dependency);
}
PartDocument explicit_history(PartDocument part, bool suppress) {
  PartFeatureHistoryIntent edit{{"base"}, {}};
  if (suppress)
    edit.suppressed = {"base"};
  REQUIRE(part.apply_history_intent(std::move(edit)));
  return part;
}
ShapeCache evaluate(const PartDocument& part) {
  auto cache = checked(ShapeCache::create(ShapeCacheId("history.physical")));
  (void)checked(GeometryRecomputeExecutor{}.execute_document(part, cache));
  return cache;
}
void same_bodies(const PartDocument& expected_document, const ShapeCache& expected,
                 const PartDocument& actual_document, const ShapeCache& actual) {
  for (const auto& body : actual_document.bodies()) {
    CAPTURE(body.id().value());
    const auto* left = expected.find_body_shape(body.id());
    const auto* right = actual.find_body_shape(body.id());
    REQUIRE(bool(left) == bool(right));
    if (!left)
      continue;
    const auto a = RectangleExtrusionAdapter{}.summarize(*left);
    const auto b = RectangleExtrusionAdapter{}.summarize(*right);
    CHECK(a.solid_count == b.solid_count);
    CHECK(a.volume_mm3 == Catch::Approx(b.volume_mm3).margin(1e-6));
    const auto bounds_a = checked(BodyTransformAdapter{}.bounds(*left));
    const auto bounds_b = checked(BodyTransformAdapter{}.bounds(*right));
    CHECK(bounds_a.minimum.x == Catch::Approx(bounds_b.minimum.x).margin(1e-6));
    CHECK(bounds_a.minimum.y == Catch::Approx(bounds_b.minimum.y).margin(1e-6));
    CHECK(bounds_a.minimum.z == Catch::Approx(bounds_b.minimum.z).margin(1e-6));
    CHECK(bounds_a.maximum.x == Catch::Approx(bounds_b.maximum.x).margin(1e-6));
    CHECK(bounds_a.maximum.y == Catch::Approx(bounds_b.maximum.y).margin(1e-6));
    CHECK(bounds_a.maximum.z == Catch::Approx(bounds_b.maximum.z).margin(1e-6));
  }
  for (const auto& body : expected_document.bodies())
    if (expected.find_body_shape(body.id()))
      REQUIRE(actual.find_body_shape(body.id()));
}
void add_fillet(PartDocument& part) {
  REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("operation"), "Fillet", BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
}
void add_transform(PartDocument& part) {
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("translate"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {10, 0, 0}, false, false))));
}
void add_box_tool(PartDocument& part) {
  REQUIRE(part.add_parameter(checked(Parameter::create_length(ParameterId("tool.size"), "Tool size",
                                                              checked(Quantity::length_mm(4))))));
  auto sketch = checked(Sketch::create(SketchId("tool"), "Tool", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("tool.profile"), ParameterId("tool.size"), ParameterId("tool.size")))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  auto feature = checked(Feature::create_additive_extrude(FeatureId("tool.feature"), "Tool",
                                                          SketchId("tool"), ParameterId("depth")));
  REQUIRE(
      part.add_feature(checked(feature.with_body_result_context(fixture::new_body(part, "tool")))));
}
void add_operation(PartDocument& part, PartFeatureFamily family) {
  const FeatureId id("operation");
  switch (family) {
  case PartFeatureFamily::Extrude: {
    auto feature = checked(
        Feature::create_additive_extrude(id, "Extrude", SketchId("profile"), ParameterId("depth")));
    REQUIRE(part.add_feature(
        checked(feature.with_body_result_context(fixture::new_body(part, "operation")))));
    break;
  }
  case PartFeatureFamily::Fillet:
    add_fillet(part);
    break;
  case PartFeatureFamily::Chamfer:
    REQUIRE(part.add_chamfer_feature(checked(ChamferFeature::create_equal_distance(
        id, "Chamfer", BodyId("body.base"), {fixture::edge(PartFeatureInputRole::ChamferEdge)},
        ParameterId("radius")))));
    break;
  case PartFeatureFamily::Shell:
    REQUIRE(part.add_shell_feature(checked(ShellFeature::create(
        id, "Shell", BodyId("body.base"), {fixture::face(PartFeatureInputRole::ShellRemovalFace)},
        ParameterId("radius"), ShellDirection::Inward))));
    break;
  case PartFeatureFamily::Draft:
    REQUIRE(part.add_draft_feature(checked(DraftFeature::create(
        id, "Draft", BodyId("body.base"),
        {checked(FaceReference::create_planar(
            PartFeatureInputRole::DraftFace,
            checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Front))))},
        fixture::axis(PartFeatureInputRole::DraftPullDirection),
        fixture::plane(PartFeatureInputRole::DraftNeutralPlane), ParameterId("angle")))));
    break;
  case PartFeatureFamily::BodyTransform:
    add_transform(part);
    break;
  case PartFeatureFamily::BodyBoolean:
    REQUIRE(part.add_body_boolean_feature(checked(BodyBooleanFeature::create(
        id, BodyBooleanOperation::Subtract, BodyId("body.base"), {BodyId("body.tool")},
        BodyBooleanResultMode::ModifyTarget, std::nullopt, false))));
    break;
  case PartFeatureFamily::LinearPattern:
    REQUIRE(part.add_linear_pattern_feature(checked(LinearPatternFeature::create(
        id, "Linear", {checked(PatternSourceReference::feature(FeatureId("base")))},
        fixture::axis(PartFeatureInputRole::PatternAxis), ParameterId("count"),
        LinearPatternExtentMode::Spacing, ParameterId("depth"), PatternDirectionSign::Positive,
        fixture::new_body(part, "operation")))));
    break;
  case PartFeatureFamily::CircularPattern:
    REQUIRE(part.add_circular_pattern_feature(checked(CircularPatternFeature::create(
        id, "Circular", {checked(PatternSourceReference::feature(FeatureId("base")))},
        fixture::axis(PartFeatureInputRole::PatternAxis), ParameterId("count"), 360, true,
        fixture::new_body(part, "operation")))));
    break;
  case PartFeatureFamily::Mirror:
    REQUIRE(part.add_mirror_feature(checked(MirrorFeature::create(
        id, "Mirror", {checked(MirrorSourceReference::feature(FeatureId("base")))},
        fixture::plane(PartFeatureInputRole::MirrorPlane), fixture::new_body(part, "operation")))));
    break;
  case PartFeatureFamily::Revolve: {
    auto sketch = checked(Sketch::create(SketchId("revolve"), "Revolve", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
        ProfileId("revolve.profile"), ParameterId("height"), ParameterId("width"), {15, 0}))));
    REQUIRE(part.add_sketch(std::move(sketch)));
    REQUIRE(part.add_datum_axis(
        checked(DatumAxis::create_explicit(DatumAxisId("axis.y"), "Y", {}, {0, 1, 0}))));
    REQUIRE(part.add_revolve_feature(checked(RevolveFeature::create_revolve(
        id, "Revolve", fixture::profile(PartFeatureInputRole::RevolveProfile, "revolve"),
        checked(AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis,
                                                 DatumAxisId("axis.y"))),
        RevolveAngleExtent::full(), fixture::new_body(part, "operation")))));
    break;
  }
  case PartFeatureFamily::Sweep: {
    REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_explicit(
        ConstructionPlaneId("path.plane"), "Path plane", {}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}))));
    auto sketch =
        checked(Sketch::create(SketchId("path.sketch"), "Path", DatumPlaneId("path.plane")));
    REQUIRE(
        sketch.add_entity(checked(LineSegment::create(SketchEntityId("line"), {0, 0}, {0, 20}))));
    REQUIRE(part.add_sketch(std::move(sketch)));
    REQUIRE(part.add_path_curve(checked(PathCurve::create(
        PathCurveId("path"), "Path",
        {checked(PathSegmentReference::create_planar(
            SketchId("path.sketch"), SketchEntityId("line"), PlanarPathCurveKind::Line))},
        PathClosure::Open, PathOrientationRule::MinimumTwist))));
    REQUIRE(part.add_sweep_feature(checked(SweepFeature::create_sweep(
        id, "Sweep",
        checked(SweepProfileReference::create_closed_region(
            fixture::profile(PartFeatureInputRole::SweepProfile, "profile"))),
        PathCurveId("path"), fixture::new_body(part, "operation")))));
    break;
  }
  case PartFeatureFamily::Loft:
    REQUIRE(part.add_construction_plane(checked(
        ConstructionPlane::create_explicit(ConstructionPlaneId("section.plane"), "Section plane",
                                           {0, 0, 20}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
    fixture::rectangle(part, "section", "section.plane");
    REQUIRE(part.add_loft_feature(checked(LoftFeature::create_loft(
        id, "Loft",
        {checked(ProfileSectionReference::create_closed_region(
             fixture::profile(PartFeatureInputRole::LoftSection, "profile"))),
         checked(ProfileSectionReference::create_closed_region(
             fixture::profile(PartFeatureInputRole::LoftSection, "section")))},
        fixture::new_body(part, "operation")))));
    break;
  case PartFeatureFamily::Surface: {
    auto sketch =
        checked(Sketch::create(SketchId("boundary"), "Boundary", DatumPlaneId("datum.xy")));
    const std::vector<Point2> points{{0, 0}, {12, 0}, {12, 8}, {0, 8}};
    std::vector<PathSegmentReference> segments;
    for (std::size_t i = 0; i < points.size(); ++i) {
      const SketchEntityId line("line." + std::to_string(i));
      REQUIRE(sketch.add_entity(
          checked(LineSegment::create(line, points[i], points[(i + 1) % points.size()]))));
      segments.push_back(checked(
          PathSegmentReference::create_planar(sketch.id(), line, PlanarPathCurveKind::Line)));
    }
    REQUIRE(part.add_sketch(std::move(sketch)));
    REQUIRE(part.add_path_curve(
        checked(PathCurve::create(PathCurveId("boundary.path"), "Boundary", std::move(segments),
                                  PathClosure::Closed, PathOrientationRule::MinimumTwist))));
    (void)fixture::new_body(part, "operation", BodyKind::Surface);
    REQUIRE(part.add_surface_feature(checked(FillSurfaceFeature::create(
        id, "Fill",
        {checked(BoundaryCurveReference::create_path_curve(PathCurveId("boundary.path")))},
        BodyId("body.operation")))));
    break;
  }
  case PartFeatureFamily::ImportedBody:
    FAIL("Imported source uses a real STEP asset fixture in a separate test");
  }
}
} // namespace

TEST_CASE(
    "Explicit history evaluates active rows and rejects stale modified or direct-operation caches",
    "[geometry][parity-b147][history-evaluation-boundary]") {
  const bool suppress = GENERATE(false, true);
  const auto original = fixture::native_base();
  auto baseline = checked(ShapeCache::create(ShapeCacheId("history.baseline")));
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(original, baseline));
  const auto original_shape = *baseline.find_body_shape(BodyId("body.base"));
  CHECK(RectangleExtrusionAdapter{}.summarize(original_shape).volume_mm3 == Catch::Approx(1600));
  auto part = explicit_history(original, suppress);
  auto cache = baseline;
  unavailable(BodyResultInspector{}.inspect(part, cache));
  const auto evaluated = checked(executor.execute_document(part, cache));
  CHECK(evaluated.executed_feature_count == (suppress ? 0 : 1));
  REQUIRE(cache.validate_complete_history_result(part));
  CHECK(cache.feature_shape_count() == (suppress ? 0 : 1));
  CHECK(cache.body_shape_count() == (suppress ? 0 : 1));
  const auto inspection = checked(BodyResultInspector{}.inspect(part, cache));
  CHECK(inspection.solid_body_count == (suppress ? 0 : 1));
  if (!suppress)
    CHECK(RectangleExtrusionAdapter{}
              .summarize(*cache.find_body_shape(BodyId("body.base")))
              .volume_mm3 == Catch::Approx(1600));
  // A completed cache cannot authorize a separate call that resurrects a skipped operation.
  unavailable(executor.execute_additive_extrude(part, FeatureId("base"), cache));
  REQUIRE(cache.validate_complete_history_result(part));
  REQUIRE(part.add_parameter(checked(
      Parameter::create_length(ParameterId("unused"), "Unused", checked(Quantity::length_mm(2))))));
  unavailable(BodyResultInspector{}.inspect(part, cache));
  const auto empty_plan =
      checked(RecomputePlan::from_graph_and_changed_nodes(part.dependency_graph(), {}));
  REQUIRE(empty_plan.empty());
  REQUIRE(executor.execute_plan(part, empty_plan, cache));
  REQUIRE(cache.validate_complete_history_result(part));
  REQUIRE(cache.store_feature_shape(FeatureId("foreign"), original_shape));
  unavailable(cache.validate_complete_history_result(part));
  unavailable(ViewportSceneBuilder{}.build_part(part, cache));
  (void)checked(executor.execute_document(part, cache));
  CHECK(cache.find_feature_shape(FeatureId("foreign")) == nullptr);
  REQUIRE(cache.validate_complete_history_result(part));
}

TEST_CASE("Every prefix including the final row stays partial and cannot consume or overwrite a "
          "full export",
          "[geometry][parity-b147][history-evaluation-boundary]") {
  const bool explicit_intent = GENERATE(false, true);
  auto part = fixture::native_base();
  if (explicit_intent)
    part = explicit_history(std::move(part), false);
  auto cache = checked(ShapeCache::create(ShapeCacheId("history.prefix")));
  const GeometryRecomputeExecutor executor;
  (void)checked(executor.execute_document(part, cache));
  REQUIRE(executor.execute_history_prefix(part, 0, cache));
  CHECK(cache.feature_shape_count() == 0);
  CHECK(cache.body_shape_count() == 0);
  unavailable(BodyResultInspector{}.inspect(part, cache));
  unavailable(BodyResultInspector{}.resolve_compatible_final_shape(part, cache));
  unavailable(ViewportSceneBuilder{}.build_part(part, cache));
  unavailable(ViewportSceneBuilder{}.build_construction_sources(part, cache));
  unavailable(ViewportSceneBuilder{}.build_body_rotation_sources(part, cache));
  const auto edge =
      checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopFront));
  unavailable(SemanticReferenceEvaluator{}.resolve_edge(part, edge, cache));
  unavailable(ResultFaceSupportGeometry::selectable_faces(part, cache, BodyId("body.base")));
  unavailable(FinishingSelectionBuilder::build(part, cache, BodyId("body.base"),
                                               PartFeatureInputRole::FilletEdge));
  (void)checked(executor.execute_history_prefix(part, 1, cache));
  REQUIRE(cache.find_body_shape(BodyId("body.base")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(1600));
  unavailable(cache.validate_complete_history_result(part));
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
  unavailable(StepExporter{}.write_part_step(part, cache, path.string()));
  unavailable(StepExporter{}.write_document_step(part, cache, path.string()));
  std::ifstream input(path);
  CHECK(std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()) ==
        "committed export");
  CHECK_FALSE(executor.execute_history_prefix(part, 2, cache));
  unavailable(cache.validate_complete_history_result(part));
  auto cleared = cache;
  cleared.clear();
  unavailable(cleared.validate_complete_history_result(part));
  unavailable(ViewportSceneBuilder{}.build_part(part, cleared));
  (void)checked(executor.execute_document(part, cache));
  REQUIRE(cache.validate_complete_history_result(part));
  CHECK(checked(BodyResultInspector{}.inspect(part, cache)).solid_body_count == 1);
}

TEST_CASE(
    "All native families restore predecessors and outputs against independent legacy geometry",
    "[geometry][parity-b147][history-physical]") {
  const auto family = GENERATE(
      PartFeatureFamily::Extrude, PartFeatureFamily::Revolve, PartFeatureFamily::Sweep,
      PartFeatureFamily::Loft, PartFeatureFamily::Surface, PartFeatureFamily::LinearPattern,
      PartFeatureFamily::CircularPattern, PartFeatureFamily::Mirror, PartFeatureFamily::Fillet,
      PartFeatureFamily::Chamfer, PartFeatureFamily::Shell, PartFeatureFamily::Draft,
      PartFeatureFamily::BodyBoolean, PartFeatureFamily::BodyTransform);
  CAPTURE(static_cast<int>(family));
  auto before = fixture::native_base();
  if (family == PartFeatureFamily::BodyBoolean)
    add_box_tool(before);
  auto authored = before;
  add_operation(authored, family);
  const auto legacy = evaluate(authored);
  const auto expected_suppressed = evaluate(before);
  auto order = checked(authored.executable_history());
  std::vector<std::string> ids;
  for (const auto& entry : order)
    ids.push_back(entry.node_id);
  REQUIRE(authored.apply_history_intent({ids, {}}));
  auto incremental = evaluate(authored);
  same_bodies(authored, legacy, authored, incremental);
  const auto source = checked(serialize_part_document_to_json(authored));
  for (std::size_t prefix = 0; prefix <= ids.size(); ++prefix) {
    CAPTURE(prefix);
    REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(authored, prefix, incremental));
    unavailable(incremental.validate_complete_history_result(authored));
    CHECK(checked(serialize_part_document_to_json(authored)) == source);
  }
  REQUIRE(authored.apply_history_intent({ids, {ids.back()}}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(
      authored, checked(authored.create_recompute_plan()), incremental));
  REQUIRE(incremental.validate_complete_history_result(authored));
  same_bodies(before, expected_suppressed, authored, incremental);
  CHECK(incremental.find_feature_shape(FeatureId(ids.back())) == nullptr);
  const auto fresh = evaluate(authored);
  same_bodies(authored, fresh, authored, incremental);
  REQUIRE(authored.apply_history_intent({ids, {}}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(
      authored, checked(authored.create_recompute_plan()), incremental));
  same_bodies(authored, legacy, authored, incremental);
}

TEST_CASE("Reusing an identical feature ID and JSON invalidates its previous lifetime cache",
          "[geometry][parity-b147][history-physical]") {
  auto part = explicit_history(fixture::native_base(), false);
  auto cache = evaluate(part);
  const auto source = checked(serialize_part_document_to_json(part));
  const auto feature = *part.find_feature(FeatureId("base"));
  REQUIRE(part.remove_feature(feature.id()));
  REQUIRE(part.add_feature(feature));
  CHECK(checked(serialize_part_document_to_json(part)) == source);
  unavailable(cache.validate_complete_history_result(part));
  unavailable(ViewportSceneBuilder{}.build_part(part, cache));
  REQUIRE(
      GeometryRecomputeExecutor{}.execute_plan(part, checked(part.create_recompute_plan()), cache));
  REQUIRE(cache.validate_complete_history_result(part));
}

TEST_CASE("Suppressed modifiers are bypassed before transforms and failed suffixes publish nothing",
          "[geometry][parity-b147][history-physical]") {
  auto expected = fixture::native_base();
  add_transform(expected);
  const auto oracle = evaluate(expected);
  auto part = fixture::native_base();
  add_fillet(part);
  add_transform(part);
  REQUIRE(part.apply_history_intent({{"base", "operation", "translate"}, {"operation"}}));
  auto cache = evaluate(part);
  same_bodies(expected, oracle, part, cache);
  REQUIRE(part.set_parameter_value(ParameterId("radius"), checked(Quantity::length_mm(30))));
  REQUIRE(
      GeometryRecomputeExecutor{}.execute_plan(part, checked(part.create_recompute_plan()), cache));
  const auto suppressed = part;
  REQUIRE(part.apply_history_intent({{"base", "operation", "translate"}, {}}));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.validate_complete_history_result(suppressed));
  unavailable(cache.validate_complete_history_result(part));
  same_bodies(expected, oracle, suppressed, cache);
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(part, 1, cache));
  REQUIRE(cache.find_body_shape(BodyId("body.base")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(1600));
  REQUIRE(part.set_parameter_value(ParameterId("radius"), checked(Quantity::length_mm(1))));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.validate_complete_history_result(part));
}

TEST_CASE("Independent reorder and explicit dependent suppression preserve unrelated Bodies",
          "[geometry][parity-b147][history-physical]") {
  auto part = fixture::native_base();
  add_box_tool(part);
  const auto independent = part;
  const auto oracle = evaluate(independent);
  REQUIRE(part.apply_history_intent({{"tool.feature", "base"}, {}}));
  auto cache = evaluate(part);
  same_bodies(independent, oracle, part, cache);
  const auto ordered = checked(serialize_part_document_to_json(part));
  CHECK_FALSE(part.apply_history_intent({{"base", "base"}, {}}));
  CHECK(checked(serialize_part_document_to_json(part)) == ordered);
  REQUIRE(cache.validate_complete_history_result(part));
  add_fillet(part);
  REQUIRE(part.apply_history_intent({{"tool.feature", "base", "operation"}, {"base"}}));
  const auto before = part;
  const auto rejected = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().object_id() == "operation");
  CHECK(rejected.error().message().find("base") != std::string::npos);
  same_bodies(independent, oracle, independent, cache);
  REQUIRE(
      part.apply_history_intent({{"tool.feature", "base", "operation"}, {"base", "operation"}}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.validate_complete_history_result(part));
  CHECK(cache.find_body_shape(BodyId("body.base")) == nullptr);
  REQUIRE(cache.find_body_shape(BodyId("body.tool")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.tool")))
            .volume_mm3 == Catch::Approx(128));
  CHECK(checked(BodyResultInspector{}.inspect(part, cache)).solid_body_count == 1);
  unavailable(SemanticReferenceEvaluator{}.resolve_edge(
      part, checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopFront)),
      cache));
  CHECK(checked(serialize_part_document_to_json(before)).find("suppressed_features") !=
        std::string::npos);
}

TEST_CASE("Assembly analysis and posed export rebuild effective Part history",
          "[geometry][parity-b147][history-physical]") {
  auto part = fixture::native_base();
  add_fillet(part);
  REQUIRE(part.apply_history_intent({{"base", "operation"}, {"operation"}}));
  auto assembly = checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
  REQUIRE(assembly.add_member_part(part.id()));
  REQUIRE(assembly.add_component_instance(
      checked(ComponentInstance::create(ComponentInstanceId("first"), "First", part.id()))));
  REQUIRE(assembly.add_component_instance(checked(
      ComponentInstance::create(ComponentInstanceId("second"), "Second", part.id(),
                                ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                ComponentGroundingState::Free, RigidTransform{{0, 0, 4}, {}}))));
  auto project = checked(Project::create(DocumentId("project"), "Project", std::move(assembly)));
  REQUIRE(project.add_part_document(std::move(part)));
  const auto analysis = checked(AssemblyInterferenceAnalyzer{}.analyze(project));
  REQUIRE(analysis.interferences.size() == 1);
  CHECK(analysis.interferences.front().overlap_volume_mm3 == Catch::Approx(800).margin(1e-6));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(checked(AssemblyStepExporter{}.build_posed_shape(project)))
            .volume_mm3 == Catch::Approx(3200));
  REQUIRE(project.part_documents().front().apply_history_intent(
      {{"base", "operation"}, {"base", "operation"}}));
  CHECK_FALSE(AssemblyInterferenceAnalyzer{}.analyze(project));
  CHECK_FALSE(AssemblyStepExporter{}.build_posed_shape(project));
}

TEST_CASE(
    "Suppressed producers hide unused derived support Sketches and paths without stale scene items",
    "[geometry][parity-b148][history-physical][history-resource-scene]") {
  auto part = fixture::native_base();
  REQUIRE(part.add_derived_workplane(checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("support"), "Support",
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))))));
  auto sketch =
      checked(Sketch::create(SketchId("dependent.sketch"), "Dependent", DatumPlaneId("support")));
  REQUIRE(sketch.add_entity(checked(LineSegment::create(SketchEntityId("line"), {0, 0}, {5, 0}))));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_path_curve(checked(
      PathCurve::create(PathCurveId("dependent.path"), "Dependent path",
                        {checked(PathSegmentReference::create_planar(
                            sketch.id(), SketchEntityId("line"), PlanarPathCurveKind::Line))},
                        PathClosure::Open, PathOrientationRule::MinimumTwist))));
  auto shapes = evaluate(part);
  const auto has = [](const auto& scene, const std::string& id) {
    return std::any_of(scene.begin(), scene.end(),
                       [&](const auto& item) { return item.semantic_id == id; });
  };
  auto scene = checked(ViewportSceneBuilder{}.build_part(part, shapes));
  CHECK(has(scene, "derived-workplane/support"));
  CHECK(has(scene, "sketch/dependent.sketch/entity/line"));
  CHECK(has(scene, "path/dependent.path"));
  REQUIRE(part.apply_history_intent({{"base"}, {"base"}}));
  shapes = evaluate(part);
  const auto source = checked(serialize_part_document_to_json(part));
  scene = checked(ViewportSceneBuilder{}.build_part(part, shapes));
  CHECK_FALSE(has(scene, "body/body.base"));
  CHECK_FALSE(has(scene, "derived-workplane/support"));
  CHECK_FALSE(has(scene, "sketch/dependent.sketch/entity/line"));
  CHECK_FALSE(has(scene, "path/dependent.path"));
  CHECK(has(scene, "datum-plane/datum.xy"));
  CHECK(checked(serialize_part_document_to_json(part)) == source);
  REQUIRE(part.apply_history_intent({{"base"}, {}}));
  shapes = evaluate(part);
  scene = checked(ViewportSceneBuilder{}.build_part(part, shapes));
  CHECK(has(scene, "derived-workplane/support"));
  CHECK(has(scene, "sketch/dependent.sketch/entity/line"));
  CHECK(has(scene, "path/dependent.path"));
}
