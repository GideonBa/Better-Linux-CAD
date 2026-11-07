#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"

#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fstream>
#include <iterator>

using namespace blcad;
using namespace blcad::gui;
namespace fixture = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
std::string snapshot(const GuiDocumentSession& session) {
  return checked(serialize_part_document_to_json(*session.part_document()));
}
std::string read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
PartDocument machining() {
  auto part = fixture::native_base();
  REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Fillet", BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
  return part;
}
GuiDocumentSession open(const PartDocument& part, QTemporaryDir& directory) {
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("context.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(part, path));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path));
  return session;
}
Result<std::size_t> parameter(PartDocument& part, const char* id, double value) {
  auto changed = part.set_parameter_value(ParameterId(id), checked(Quantity::length_mm(value)));
  return changed ? Result<std::size_t>::success(changed.value().size())
                 : Result<std::size_t>::failure(changed.error());
}
void edit(GuiDocumentSession& session, const char* id, double value) {
  REQUIRE(session.commit_part_transaction(
      "Edit prefix dimension", [=](PartDocument& part) { return parameter(part, id, value); }));
}
double volume(const geometry::ShapeCache& cache) {
  const auto* shape = cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
geometry::GeometryShapeBounds bounds(const geometry::ShapeCache& cache) {
  const auto* shape = cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  return checked(geometry::BodyTransformAdapter{}.bounds(*shape));
}
Result<std::size_t> translate(PartDocument& part, const char* id, Vector3 delta) {
  return part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId(id), BodyId("body.base"), BodyTransformCoordinateSpace::World, std::nullopt,
      delta, false, false)));
}
void complete_matches_fresh(const GuiDocumentSession& session) {
  REQUIRE_FALSE(session.history_prefix_count());
  REQUIRE(session.derived_results_fresh());
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  auto fresh = checked(geometry::ShapeCache::create(ShapeCacheId("independent.full")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(*session.part_document(), fresh));
  CHECK(volume(*session.part_shape_cache()) == Catch::Approx(volume(fresh)).margin(1e-6));
  const auto actual = bounds(*session.part_shape_cache()), expected = bounds(fresh);
  CHECK(actual.minimum.x == Catch::Approx(expected.minimum.x).margin(1e-6));
  CHECK(actual.minimum.y == Catch::Approx(expected.minimum.y).margin(1e-6));
  CHECK(actual.minimum.z == Catch::Approx(expected.minimum.z).margin(1e-6));
  CHECK(actual.maximum.x == Catch::Approx(expected.maximum.x).margin(1e-6));
  CHECK(actual.maximum.y == Catch::Approx(expected.maximum.y).margin(1e-6));
  CHECK(actual.maximum.z == Catch::Approx(expected.maximum.z).margin(1e-6));
}
} // namespace

TEST_CASE("End of Part retains complete intent and every marker boundary remains partial",
          "[gui][parity-b148][history-prefix-session]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  const auto original = snapshot(session);
  CHECK_FALSE(session.begin_part_history_context(3));
  CHECK_FALSE(session.part_workflow_active());
  REQUIRE(session.derived_results_fresh());
  REQUIRE(session.begin_part_history_context(1));
  REQUIRE(session.history_prefix_count() == 1);
  REQUIRE(session.history_prefix_ready());
  CHECK(session.part_workflow_active());
  CHECK_FALSE(session.derived_results_fresh());
  CHECK_FALSE(session.command_context().derived_results_fresh);
  CHECK(snapshot(session) == original);
  REQUIRE(session.part_document()->find_fillet_feature(FeatureId("fillet")));
  CHECK(volume(*session.part_shape_cache()) == Catch::Approx(1600));
  for (const std::size_t count : {0U, 2U, 1U}) {
    CAPTURE(count);
    REQUIRE(session.move_part_history_marker(count));
    REQUIRE(session.history_prefix_ready());
    CHECK(session.history_prefix_count() == count);
    CHECK_FALSE(session.derived_results_fresh());
    CHECK_FALSE(
        session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
    CHECK_FALSE(geometry::BodyResultInspector{}.inspect(*session.part_document(),
                                                        *session.part_shape_cache()));
    CHECK_FALSE(geometry::ViewportSceneBuilder{}.build_part(*session.part_document(),
                                                            *session.part_shape_cache()));
    const auto step = directory.filePath("partial.step").toStdString();
    CHECK_FALSE(geometry::StepExporter{}.write_document_step(*session.part_document(),
                                                             *session.part_shape_cache(), step));
    CHECK_FALSE(std::filesystem::exists(step));
    CHECK(snapshot(session) == original);
    if (count == 0)
      CHECK(session.part_shape_cache()->body_shape_count() == 0);
  }
  session.set_derived_results_fresh(true);
  CHECK_FALSE(session.derived_results_fresh());
  REQUIRE(session.recompute());
  CHECK_FALSE(session.derived_results_fresh());
  CHECK(session.history_prefix_ready());
  CHECK_FALSE(session.begin_part_history_context(0));
  CHECK_FALSE(session.begin_part_workflow());
  session.cancel_part_workflow();
  CHECK(snapshot(session) == original);
  CHECK_FALSE(session.can_undo());
  complete_matches_fresh(session);
}

TEST_CASE(
    "Prefix edits keep an invalid suffix correctable and publish exactly one full history entry",
    "[gui][parity-b148][history-prefix-session]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  const auto before = snapshot(session);
  REQUIRE(session.begin_part_history_context(1));
  edit(session, "radius", 30);
  const auto invalid = snapshot(session);
  CHECK(invalid != before);
  CHECK(session.history_prefix_ready());
  CHECK(volume(*session.part_shape_cache()) == Catch::Approx(1600));
  CHECK_FALSE(session.move_part_history_marker(2));
  CHECK(session.history_prefix_count() == 1);
  CHECK(session.history_prefix_ready());
  CHECK_FALSE(session.publish_part_workflow("Invalid suffix"));
  CHECK(session.history_prefix_ready());
  CHECK(snapshot(session) == invalid);
  REQUIRE(session.save());
  CHECK(read(session.file_path()) == before);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(session.history_prefix_count() == 1);
  CHECK(session.history_prefix_ready());
  REQUIRE(session.redo());
  CHECK(snapshot(session) == invalid);
  edit(session, "radius", 0.5);
  const auto after = snapshot(session);
  REQUIRE(session.publish_part_workflow("Edit before finishing"));
  CHECK_FALSE(session.part_workflow_active());
  CHECK(session.undo_label() == "Edit before finishing");
  CHECK(snapshot(session) == after);
  complete_matches_fresh(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  complete_matches_fresh(session);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  complete_matches_fresh(session);
}

TEST_CASE("Insertion at the marker replays exact Body predecessors before the retained suffix",
          "[gui][parity-b148][history-prefix-session]") {
  auto part = fixture::native_base();
  REQUIRE(translate(part, "suffix", {0, 20, 0}));
  QTemporaryDir directory;
  auto session = open(part, directory);
  const auto before = snapshot(session);
  const auto source_bounds = bounds(*session.part_shape_cache());
  REQUIRE(session.begin_part_history_context(1));
  auto candidate = *session.part_document();
  REQUIRE(translate(candidate, "inserted", {5, 0, 0}));
  const auto preview = checked(session.preview_part_history_candidate(candidate));
  CHECK((candidate.authored_history_intent()->order ==
         std::vector<std::string>{"base", "inserted", "suffix"}));
  CHECK(bounds(preview).minimum.x == Catch::Approx(source_bounds.minimum.x + 5));
  CHECK(bounds(preview).minimum.y == Catch::Approx(source_bounds.minimum.y - 20));
  CHECK(snapshot(session) == before);
  CHECK(session.history_prefix_count() == 1);
  REQUIRE(session.commit_part_transaction("Insert at marker", [](PartDocument& draft) {
    return translate(draft, "inserted", {5, 0, 0});
  }));
  CHECK(session.history_prefix_count() == 2);
  CHECK(session.history_prefix_ready());
  CHECK(snapshot(session) == checked(serialize_part_document_to_json(candidate)));
  const auto plan = checked(session.part_document()->create_history_evaluation_plan());
  REQUIRE(plan.entries.back().body_inputs.size() == 1);
  CHECK(plan.entries.back().body_inputs.front().producer_id == "inserted");
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(session.history_prefix_count() == 1);
  REQUIRE(session.redo());
  CHECK(session.history_prefix_count() == 2);
  REQUIRE(session.publish_part_workflow("Insert before suffix"));
  CHECK(bounds(*session.part_shape_cache()).minimum.x ==
        Catch::Approx(source_bounds.minimum.x + 5));
  CHECK(bounds(*session.part_shape_cache()).minimum.y == Catch::Approx(source_bounds.minimum.y));
  complete_matches_fresh(session);
  const auto after = snapshot(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
}

TEST_CASE("Prefix insertion cannot depend on a future creator or reorder the existing source",
          "[gui][parity-b148][history-prefix-session]") {
  auto part = fixture::native_base();
  auto tool = checked(Feature::create_additive_extrude(FeatureId("tool"), "Tool",
                                                       SketchId("profile"), ParameterId("depth")));
  REQUIRE(
      part.add_feature(checked(tool.with_body_result_context(fixture::new_body(part, "tool")))));
  QTemporaryDir directory;
  auto session = open(part, directory);
  const auto before = snapshot(session);
  REQUIRE(session.begin_part_history_context(1));
  const auto add_future_consumer = [](PartDocument& candidate) {
    auto pattern = checked(LinearPatternFeature::create(
        FeatureId("pattern"), "Pattern",
        {checked(PatternSourceReference::feature(FeatureId("tool")))},
        fixture::axis(PartFeatureInputRole::PatternAxis), ParameterId("count"),
        LinearPatternExtentMode::Spacing, ParameterId("depth"), PatternDirectionSign::Positive,
        fixture::new_body(candidate, "pattern")));
    return candidate.add_linear_pattern_feature(std::move(pattern));
  };
  auto candidate = *session.part_document();
  REQUIRE(add_future_consumer(candidate));
  CHECK_FALSE(session.preview_part_history_candidate(candidate));
  CHECK_FALSE(session.commit_part_transaction("Illegal future input", add_future_consumer));
  CHECK(snapshot(session) == before);
  CHECK(session.history_prefix_ready());
  CHECK_FALSE(session.can_undo());
  candidate = *session.part_document();
  REQUIRE(candidate.apply_history_intent({{"tool", "base"}, {}}));
  CHECK_FALSE(session.prepare_part_history_candidate(candidate));
  CHECK_FALSE(session.commit_part_transaction("Reorder during prefix", [](PartDocument& draft) {
    return draft.apply_history_intent({{"tool", "base"}, {}});
  }));
  CHECK(snapshot(session) == before);
  CHECK(session.history_prefix_ready());
}

TEST_CASE("Cancel preserves committed Save As metadata and the original redo branch",
          "[gui][parity-b148][history-prefix-session]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  edit(session, "width", 22);
  const auto redo = snapshot(session);
  REQUIRE(session.undo());
  const auto before = snapshot(session);
  REQUIRE(session.set_presentation_visible("sketch/profile", false));
  const auto hidden = session.hidden_presentations();
  session.selection().set_filter_mask(selection_kind_bit(GuiSelectionKind::Body));
  REQUIRE(session.selection().add({GuiSelectionKind::Body, "body/body.base"}));
  const auto selection = session.selection().entries();
  REQUIRE(session.begin_part_history_context(1));
  edit(session, "radius", 30);
  const auto target =
      std::filesystem::path(directory.filePath("saved-context.blcad.json").toStdString());
  REQUIRE(session.save_as(target));
  CHECK(read(target) == before);
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(target));
  CHECK(snapshot(reopened) == before);
  CHECK(reopened.derived_results_fresh());
  REQUIRE(session.show_all_presentations() == 1);
  REQUIRE(session.task().begin("prefix.child"));
  CHECK_FALSE(session.publish_part_workflow("Unfinished child"));
  CHECK_FALSE(session.move_part_history_marker(0));
  session.cancel_part_workflow();
  CHECK_FALSE(session.task().active());
  CHECK_FALSE(session.history_prefix_count());
  CHECK(snapshot(session) == before);
  CHECK(session.file_path() == target);
  CHECK_FALSE(session.dirty());
  CHECK(session.hidden_presentations() == hidden);
  CHECK(session.selection().entries() == selection);
  REQUIRE(session.can_redo());
  REQUIRE(session.redo());
  CHECK(snapshot(session) == redo);
  complete_matches_fresh(session);
}

TEST_CASE("Copied prefix sessions share only the immutable committed checkpoint",
          "[gui][parity-b148][history-prefix-session]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  const auto before = snapshot(session);
  REQUIRE(session.begin_part_history_context(1));
  auto copy = session;
  edit(copy, "radius", 0.5);
  REQUIRE(copy.move_part_history_marker(2));
  CHECK(session.history_prefix_count() == 1);
  CHECK(snapshot(session) == before);
  CHECK(session.history_prefix_ready());
  REQUIRE(copy.publish_part_workflow("Copied edit"));
  CHECK(session.part_workflow_active());
  CHECK(session.history_prefix_ready());
  CHECK(snapshot(session) == before);
  complete_matches_fresh(copy);
  session.cancel_part_workflow();
  CHECK(snapshot(session) == before);
}

TEST_CASE("A marker-only visit stays transient and rejects source lifetime or cache loss",
          "[gui][parity-b148][history-prefix-session]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  const auto before = snapshot(session);
  REQUIRE(session.begin_part_history_context(2));
  CHECK(checked(session.publish_part_workflow("Marker only")) == 0);
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  complete_matches_fresh(session);
  REQUIRE(session.begin_part_history_context(1));
  const auto feature = *session.part_document()->find_fillet_feature(FeatureId("fillet"));
  REQUIRE(session.part_document()->remove_fillet_feature(feature.id()));
  REQUIRE(session.part_document()->add_fillet_feature(feature));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.history_prefix_ready());
  CHECK_FALSE(session.publish_part_workflow("Reused lifetime"));
  auto candidate = *session.part_document();
  CHECK_FALSE(session.preview_part_history_candidate(candidate));
  session.cancel_part_workflow();
  REQUIRE(session.begin_part_history_context(1));
  const_cast<geometry::ShapeCache*>(session.part_shape_cache())->clear();
  CHECK_FALSE(session.history_prefix_ready());
  CHECK_FALSE(session.publish_part_workflow("Cleared partial cache"));
  REQUIRE(session.recompute());
  CHECK(session.history_prefix_ready());
  CHECK_FALSE(session.derived_results_fresh());
  session.cancel_part_workflow();
  complete_matches_fresh(session);
}

TEST_CASE("Prefix Save preserves committed Sketch sidecars and publishes corrected intent once",
          "[gui][parity-b148][history-prefix-session]") {
  auto part = machining();
  auto notes = checked(Sketch::create(SketchId("notes"), "Notes", DatumPlaneId("datum.xy")));
  REQUIRE(notes.add_entity(
      checked(LineSegment::create(SketchEntityId("line"), {100, 100}, {110, 100}))));
  REQUIRE(part.add_sketch(std::move(notes)));
  QTemporaryDir directory;
  auto session = open(part, directory);
  const auto set_intent = [](PartDocument& doc, std::vector<SketchConstraintCatalog>& constraints,
                             std::vector<SketchDimensionCatalog>& dimensions, bool draft) {
    const auto start =
        checked(SketchConstraintIntentTarget::point(SketchPointId("entity/line/start")));
    const auto end = checked(SketchConstraintIntentTarget::point(SketchPointId("entity/line/end")));
    constraints = {checked(SketchConstraintCatalog::create(
        doc.id(), SketchId("notes"),
        {checked(SketchConstraintIntent::create(
            SketchConstraintId(draft ? "fixed.draft" : "fixed.entry"),
            SketchSolverConstraintKind::Fixed, {draft ? end : start}))}))};
    dimensions = {checked(SketchDimensionCatalog::create(
        doc.id(), SketchId("notes"),
        {checked(SketchDimensionIntent::create(
            SketchDimensionId("length"), SketchDimensionKind::PointToPointDistance,
            SketchDimensionMode::Reference, {start, end}, std::nullopt,
            draft ? Point2{8, 9} : Point2{2, 3}))}))};
    return Result<std::size_t>::success(1);
  };
  REQUIRE(session.commit_part_sketch_intent_transaction(
      "Seed notes intent", [&](PartDocument& doc, auto& constraints, auto& dimensions) {
        return set_intent(doc, constraints, dimensions, false);
      }));
  REQUIRE(session.save());
  const auto before = snapshot(session);
  const auto original = session.file_path();
  const auto constraints = session.sketch_constraint_catalogs();
  const auto dimensions = session.sketch_dimension_catalogs();
  const auto constraint_bytes = read(original.string() + ".sketch-constraints.json");
  const auto dimension_bytes = read(original.string() + ".sketch-dimensions.json");
  REQUIRE(session.begin_part_history_context(1));
  REQUIRE(session.commit_part_sketch_intent_transaction(
      "Stage notes and invalid radius",
      [&](PartDocument& doc, auto& current_constraints, auto& current_dimensions) {
        REQUIRE(parameter(doc, "radius", 30));
        return set_intent(doc, current_constraints, current_dimensions, true);
      }));
  const auto final_constraints = session.sketch_constraint_catalogs();
  const auto final_dimensions = session.sketch_dimension_catalogs();
  CHECK(final_constraints != constraints);
  CHECK(final_dimensions != dimensions);
  REQUIRE(session.save());
  CHECK(read(original) == before);
  CHECK(read(original.string() + ".sketch-constraints.json") == constraint_bytes);
  CHECK(read(original.string() + ".sketch-dimensions.json") == dimension_bytes);
  const auto target = std::filesystem::path(directory.filePath("notes.blcad.json").toStdString());
  REQUIRE(session.save_as(target));
  CHECK(read(target) == before);
  CHECK(read(target.string() + ".sketch-constraints.json") == constraint_bytes);
  CHECK(read(target.string() + ".sketch-dimensions.json") == dimension_bytes);
  CHECK_FALSE(session.publish_part_workflow("Invalid notes draft"));
  CHECK(read(target) == before);
  edit(session, "radius", 0.5);
  const auto after = snapshot(session);
  REQUIRE(session.publish_part_workflow("Edit prefix notes and finishing"));
  CHECK(session.undo_label() == "Edit prefix notes and finishing");
  CHECK(session.sketch_constraint_catalogs() == final_constraints);
  CHECK(session.sketch_dimension_catalogs() == final_dimensions);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(session.sketch_constraint_catalogs() == constraints);
  CHECK(session.sketch_dimension_catalogs() == dimensions);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(target));
  CHECK(snapshot(reopened) == after);
  CHECK(reopened.sketch_constraint_catalogs() == final_constraints);
  CHECK(reopened.sketch_dimension_catalogs() == final_dimensions);
  complete_matches_fresh(reopened);
}

TEST_CASE(
    "Marker-only publication reobserves imported suppliers and Reference bases remain immutable",
    "[gui][parity-b148][history-prefix-session][history-prefix-import]") {
  const bool reference = GENERATE(false, true);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto root = std::filesystem::path(directory.path().toStdString());
  const auto path = root / "supplier.step";
  const auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(20)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(8))));
  REQUIRE(geometry::StepExporter{}.write_step(shape, path.string()));
  const auto asset = checked(inspect_step_import_asset(root, "supplier.step"));
  REQUIRE(asset.observed_sha256_digest());
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("supplier"), "Supplier", "supplier.step", *asset.observed_sha256_digest(),
      reference ? StepImportMode::Reference : StepImportMode::EditableBody));
  const auto imported = checked(geometry::StepImportReader{}.read(source, root));
  const auto catalog = checked(geometry::StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("catalog"), imported, 0));
  auto part =
      reference
          ? checked(geometry::StepReferenceCommands{}.create_definition(
                source, catalog, DocumentId("reference"), "Reference", root))
          : checked(geometry::StepEditableCommands{}.create_definition(
                source, catalog, DocumentId("editable"), "Editable", FeatureId("base"), root));
  auto session = open(part, directory);
  const auto before = snapshot(session);
  if (reference) {
    CHECK_FALSE(session.begin_part_history_context(0));
    CHECK_FALSE(session.part_workflow_active());
    CHECK(session.derived_results_fresh());
    CHECK(snapshot(session) == before);
    return;
  }
  REQUIRE(session.begin_part_history_context(1));
  const auto bytes = read(path);
  {
    std::ofstream changed(path, std::ios::binary | std::ios::app);
    REQUIRE(changed);
    changed << '\n';
  }
  CHECK_FALSE(session.publish_part_workflow("Marker only with changed source"));
  CHECK(session.part_workflow_active());
  CHECK(session.history_prefix_count() == 1);
  CHECK_FALSE(session.derived_results_fresh());
  CHECK(snapshot(session) == before);
  {
    std::ofstream restored(path, std::ios::binary | std::ios::trunc);
    REQUIRE(restored);
    restored << bytes;
  }
  CHECK(checked(session.publish_part_workflow("Marker only with restored source")) == 0);
  CHECK_FALSE(session.part_workflow_active());
  CHECK_FALSE(session.history_prefix_count());
  CHECK(session.derived_results_fresh());
  CHECK_FALSE(session.can_undo());
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
}

TEST_CASE(
    "Marker insertion preserves explicit suppression flags and counts authored suppressed rows",
    "[gui][parity-b148][history-prefix-session]") {
  auto part = machining();
  REQUIRE(part.apply_history_intent({{"base", "fillet"}, {"base", "fillet"}}));
  QTemporaryDir directory;
  auto session = open(part, directory);
  const auto before = snapshot(session);
  REQUIRE(session.begin_part_history_context(1));
  CHECK(session.part_shape_cache()->body_shape_count() == 0);
  REQUIRE(session.commit_part_transaction("Insert independent Body", [](PartDocument& candidate) {
    auto feature = checked(Feature::create_additive_extrude(
        FeatureId("inserted"), "Independent", SketchId("profile"), ParameterId("depth")));
    return candidate.add_feature(
        checked(feature.with_body_result_context(fixture::new_body(candidate, "inserted"))));
  }));
  CHECK(session.history_prefix_count() == 2);
  REQUIRE(session.history_prefix_ready());
  const auto& intent = session.part_document()->authored_history_intent();
  REQUIRE(intent);
  CHECK((intent->order == std::vector<std::string>{"base", "inserted", "fillet"}));
  CHECK((intent->suppressed == std::vector<std::string>{"base", "fillet"}));
  const auto browser = GuiDocumentBrowser::build(session);
  const auto* suffix = browser.find("fillet");
  REQUIRE(suffix);
  CHECK(suffix->label.find("suppressed") != std::string::npos);
  CHECK(suffix->label.find("after End of Part") != std::string::npos);
  CHECK_FALSE(session.part_shape_cache()->find_body_shape(BodyId("body.base")));
  REQUIRE(session.part_shape_cache()->find_body_shape(BodyId("body.inserted")));
  REQUIRE(session.publish_part_workflow("Insert before suppressed finishing"));
  CHECK(session.derived_results_fresh());
  CHECK(session.part_shape_cache()->body_shape_count() == 1);
  auto fresh = checked(geometry::ShapeCache::create(ShapeCacheId("independent.suppressed.full")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(*session.part_document(), fresh));
  REQUIRE(fresh.find_body_shape(BodyId("body.inserted")));
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*fresh.find_body_shape(BodyId("body.inserted")))
            .volume_mm3 == Catch::Approx(1600));
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.history_prefix_count());
  CHECK(session.part_shape_cache()->body_shape_count() == 0);
  REQUIRE(session.redo());
  CHECK(session.part_shape_cache()->body_shape_count() == 1);
}

TEST_CASE("Prefix child commands bound local Undo and collapse into one parent draft operation",
          "[gui][parity-b148][history-prefix-session][history-prefix-child]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  const auto original = snapshot(session);
  REQUIRE(session.begin_part_history_context(1));
  edit(session, "width", 22);
  const auto parent = snapshot(session);
  REQUIRE(session.begin_part_history_command("part.extrude"));
  CHECK_FALSE(session.begin_part_history_command("nested"));
  CHECK_FALSE(session.can_undo());
  edit(session, "radius", 30);
  CHECK_FALSE(session.publish_part_workflow("Premature root Apply"));
  CHECK_FALSE(session.move_part_history_marker(2));
  REQUIRE(session.undo());
  CHECK(snapshot(session) == parent);
  CHECK_FALSE(session.can_undo());
  REQUIRE(session.redo());
  REQUIRE(session.save());
  CHECK(read(session.file_path()) == original);
  session.cancel_part_history_command();
  CHECK_FALSE(session.part_history_command_active());
  CHECK(session.history_prefix_count() == 1);
  CHECK(snapshot(session) == parent);
  REQUIRE(session.can_undo());
  REQUIRE(session.begin_part_history_command("part.extrude"));
  edit(session, "width", 24);
  edit(session, "radius", 0.5);
  const auto after = snapshot(session);
  REQUIRE(session.publish_part_history_command("Child edits"));
  CHECK_FALSE(session.part_history_command_active());
  CHECK(session.undo_label() == "Child edits");
  CHECK(session.history_prefix_ready());
  CHECK_FALSE(session.derived_results_fresh());
  REQUIRE(session.undo());
  CHECK(snapshot(session) == parent);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  REQUIRE(session.publish_part_workflow("Complete prefix edits"));
  complete_matches_fresh(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == original);
  CHECK_FALSE(session.can_undo());
}

TEST_CASE("Cancelling a prefix child restores its parent redo branch and committed Save As path",
          "[gui][parity-b148][history-prefix-session][history-prefix-child]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  const auto original = snapshot(session);
  REQUIRE(session.begin_part_history_context(1));
  edit(session, "width", 22);
  const auto redo = snapshot(session);
  REQUIRE(session.undo());
  REQUIRE(session.begin_part_history_command("part.extrude"));
  CHECK_FALSE(session.can_redo());
  edit(session, "width", 24);
  const auto target =
      std::filesystem::path(directory.filePath("child-save.blcad.json").toStdString());
  REQUIRE(session.save_as(target));
  CHECK(read(target) == original);
  session.cancel_part_history_command();
  CHECK(session.file_path() == target);
  CHECK(snapshot(session) == original);
  REQUIRE(session.can_redo());
  REQUIRE(session.redo());
  CHECK(snapshot(session) == redo);
  CHECK(session.history_prefix_ready());
  session.cancel_part_workflow();
  CHECK(session.file_path() == target);
  CHECK(snapshot(session) == original);
  complete_matches_fresh(session);
}

TEST_CASE("Foreign tasks cannot publish prefix children or their parent",
          "[gui][parity-b148][history-prefix-session][history-prefix-child]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  const auto before = snapshot(session);
  REQUIRE(session.begin_part_history_context(1));
  REQUIRE(session.begin_part_history_command("part.extrude"));
  REQUIRE(session.task().begin("foreign"));
  edit(session, "radius", 0.5);
  CHECK(session.part_modeling_ready());
  CHECK_FALSE(session.derived_results_fresh());
  CHECK_FALSE(session.publish_part_history_command("Foreign child"));
  CHECK_FALSE(session.publish_part_workflow("Foreign root"));
  session.cancel_part_history_command();
  CHECK_FALSE(session.task().active());
  CHECK_FALSE(session.part_history_command_active());
  CHECK(session.part_workflow_active());
  CHECK(session.history_prefix_ready());
  CHECK(snapshot(session) == before);
}

TEST_CASE("The existing Extrude controller previews and stages a child before an invalid suffix",
          "[gui][parity-b148][history-prefix-session][history-prefix-child]") {
  QTemporaryDir directory;
  auto session = open(machining(), directory);
  const auto before = snapshot(session);
  REQUIRE(session.begin_part_history_context(1));
  edit(session, "radius", 30);
  REQUIRE(session.begin_part_history_command("part.extrude"));
  REQUIRE(session.task().begin("part.extrude"));
  REQUIRE(session.task().begin_parameter_editing());
  REQUIRE(session.task().show_preview());
  GuiInteractiveExtrudeController controller;
  REQUIRE(controller.begin_with_new_inputs(
      session, SketchId("profile"), ProfileId("profile.profile"), FeatureId("child"), "Child",
      checked(Parameter::create_length(ParameterId("child.depth"), "Child depth",
                                       checked(Quantity::length_mm(3)))),
      checked(Body::create(BodyId("body.child"), "Child", BodyKind::Solid))));
  const auto preview = checked(controller.preview_shape(session));
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(preview).volume_mm3 == Catch::Approx(600));
  REQUIRE(controller.apply(session));
  CHECK(session.history_prefix_count() == 2);
  REQUIRE(session.publish_part_history_command("Extrude child"));
  CHECK_FALSE(session.part_history_command_active());
  CHECK_FALSE(session.publish_part_workflow("Still active child task"));
  REQUIRE(session.task().apply());
  CHECK_FALSE(session.publish_part_workflow("Invalid full suffix"));
  CHECK(session.history_prefix_ready());
  CHECK((session.part_document()->authored_history_intent()->order ==
         std::vector<std::string>{"base", "child", "fillet"}));
  edit(session, "radius", 0.5);
  REQUIRE(session.publish_part_workflow("Extrude before finishing"));
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  CHECK(session.part_shape_cache()->body_shape_count() == 2);
  REQUIRE(session.part_shape_cache()->find_body_shape(BodyId("body.child")));
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*session.part_shape_cache()->find_body_shape(BodyId("body.child")))
            .volume_mm3 == Catch::Approx(600));
  complete_matches_fresh(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
}
