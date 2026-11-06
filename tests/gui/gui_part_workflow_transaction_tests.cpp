#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"

#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>

using namespace blcad;
using namespace blcad::gui;

namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result.has_error() ? result.error().message() : "success"));
  REQUIRE(result);
  return std::move(result.value());
}

std::string json(const GuiDocumentSession& session) {
  return checked(serialize_part_document_to_json(*session.part_document()));
}

std::string read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

SketchConstraintIntentTarget point(const char* name) {
  return checked(SketchConstraintIntentTarget::point(SketchPointId(name)));
}

Result<std::size_t> intent(PartDocument& part, std::vector<SketchConstraintCatalog>& constraints,
                           std::vector<SketchDimensionCatalog>& dimensions, bool draft = false) {
  constraints = {checked(SketchConstraintCatalog::create(
      part.id(), SketchId("sketch"),
      {checked(SketchConstraintIntent::create(
          SketchConstraintId(draft ? "fixed.draft" : "fixed.entry"),
          SketchSolverConstraintKind::Fixed,
          {point(draft ? "entity/line/end" : "entity/line/start")}))}))};
  dimensions = {checked(SketchDimensionCatalog::create(
      part.id(), SketchId("sketch"),
      {checked(SketchDimensionIntent::create(
          SketchDimensionId("length"), SketchDimensionKind::PointToPointDistance,
          SketchDimensionMode::Reference, {point("entity/line/start"), point("entity/line/end")},
          std::nullopt, draft ? Point2{8, 9} : Point2{2, 3}))}))};
  return Result<std::size_t>::success(1U);
}

GuiDocumentSession fixture() {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.workflow"), "Part workflow"));
  REQUIRE(session.commit_part_sketch_intent_transaction(
      "Seed source and intent", [](PartDocument& part, auto& constraints, auto& dimensions) {
        REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
        for (const auto& [name, value] : {std::pair{"width", 20.0}, {"height", 10.0}})
          REQUIRE(part.add_parameter(checked(Parameter::create_length(
              ParameterId(name), name, checked(Quantity::length_mm(value))))));
        auto sketch =
            checked(Sketch::create(SketchId("sketch"), "Source", DatumPlaneId("datum.xy")));
        REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
            ProfileId("profile"), ParameterId("width"), ParameterId("height")))));
        REQUIRE(sketch.add_entity(
            checked(LineSegment::create(SketchEntityId("line"), {100, 100}, {110, 100}))));
        REQUIRE(part.add_sketch(std::move(sketch)));
        return intent(part, constraints, dimensions);
      }));
  return session;
}

void edit_source(GuiDocumentSession& session, double width = 30) {
  REQUIRE(session.commit_part_sketch_intent_transaction(
      "Edit source Sketch", [width](PartDocument& part, auto& constraints, auto& dimensions) {
        REQUIRE(
            part.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(width))));
        return intent(part, constraints, dimensions, true);
      }));
}

void extrude(GuiDocumentSession& session) {
  GuiInteractiveExtrudeController controller;
  REQUIRE(controller.begin_with_new_inputs(
      session, SketchId("sketch"), ProfileId("profile"), FeatureId("extrusion"), "Extrusion",
      checked(
          Parameter::create_length(ParameterId("depth"), "Depth", checked(Quantity::length_mm(8)))),
      checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  REQUIRE(controller.preview(session));
  REQUIRE(controller.apply(session));
}

double volume(const GuiDocumentSession& session) {
  REQUIRE(session.part_shape_cache());
  const auto* shape = session.part_shape_cache()->find_body_shape(BodyId("body"));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
} // namespace

TEST_CASE("Part workflow publishes source Sketch intent and feature geometry as one history entry",
          "[gui][document-session][parity-b145][part-workflow]") {
  auto session = fixture();
  const auto before = json(session);
  const auto constraints = session.sketch_constraint_catalogs();
  const auto dimensions = session.sketch_dimension_catalogs();
  REQUIRE(session.begin_part_workflow());
  edit_source(session);
  extrude(session);
  const auto after = json(session);
  const auto final_constraints = session.sketch_constraint_catalogs();
  const auto final_dimensions = session.sketch_dimension_catalogs();
  CHECK(volume(session) == Catch::Approx(2400).margin(1e-6));
  const auto revision = session.model_revision();
  REQUIRE(session.publish_part_workflow("Create Extrude and edit source"));
  CHECK_FALSE(session.part_workflow_active());
  CHECK(session.model_revision() > revision);
  CHECK(session.undo_label() == "Create Extrude and edit source");
  CHECK(json(session) == after);
  REQUIRE(session.undo());
  CHECK(json(session) == before);
  CHECK(session.sketch_constraint_catalogs() == constraints);
  CHECK(session.sketch_dimension_catalogs() == dimensions);
  CHECK(session.undo_label() == "Seed source and intent");
  REQUIRE(session.redo());
  CHECK(json(session) == after);
  CHECK(session.sketch_constraint_catalogs() == final_constraints);
  CHECK(session.sketch_dimension_catalogs() == final_dimensions);
  CHECK(volume(session) == Catch::Approx(2400).margin(1e-6));
}

TEST_CASE("Part workflow cancel restores the full entry state and pre-existing redo branch",
          "[gui][document-session][parity-b145][part-workflow]") {
  auto session = fixture();
  edit_source(session, 25);
  const auto old_redo = json(session);
  REQUIRE(session.undo());
  const auto before = json(session);
  const auto constraints = session.sketch_constraint_catalogs();
  const auto dimensions = session.sketch_dimension_catalogs();
  REQUIRE(session.set_presentation_visible("sketch/sketch", false));
  const auto hidden = session.hidden_presentations();
  const auto mask = selection_kind_bit(GuiSelectionKind::SketchEntity);
  session.selection().set_filter_mask(mask);
  REQUIRE(session.selection().add({GuiSelectionKind::SketchEntity, "source.selection"}));
  const auto selection = session.selection().entries();
  REQUIRE(session.begin_part_workflow());
  CHECK_FALSE(session.can_redo());
  CHECK_FALSE(session.redo());
  CHECK_FALSE(session.can_undo());
  CHECK_FALSE(session.undo());
  edit_source(session);
  extrude(session);
  REQUIRE(session.undo());
  REQUIRE(session.undo());
  CHECK_FALSE(session.can_undo());
  CHECK_FALSE(session.undo());
  REQUIRE(session.redo());
  REQUIRE(session.redo());
  session.selection().clear();
  session.selection().set_filter_mask(selection_kind_bit(GuiSelectionKind::Body));
  REQUIRE(session.show_all_presentations() == 1);
  REQUIRE(session.set_workspace(GuiWorkspace::Sketch));
  REQUIRE(session.task().begin("sketch.local"));
  const auto model_revision = session.model_revision();
  const auto presentation_revision = session.presentation_revision();
  session.cancel_part_workflow();
  CHECK_FALSE(session.part_workflow_active());
  CHECK_FALSE(session.task().active());
  CHECK(session.workspace() == GuiWorkspace::Part);
  CHECK(json(session) == before);
  CHECK(session.sketch_constraint_catalogs() == constraints);
  CHECK(session.sketch_dimension_catalogs() == dimensions);
  CHECK(session.selection().entries() == selection);
  CHECK(session.selection().filter_mask() == mask);
  CHECK(session.hidden_presentations() == hidden);
  CHECK(session.model_revision() > model_revision);
  CHECK(session.presentation_revision() > presentation_revision);
  REQUIRE(session.can_redo());
  CHECK(session.redo_label() == "Edit source Sketch");
  REQUIRE(session.redo());
  CHECK(json(session) == old_redo);
  CHECK(session.part_document()->features().empty());
}

TEST_CASE("Part workflow Save and Save As publish committed document and both sidecars only",
          "[gui][document-session][parity-b145][part-workflow]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto original = std::filesystem::path(directory.filePath("original.json").toStdString());
  const auto target = std::filesystem::path(directory.filePath("renamed.json").toStdString());
  auto session = fixture();
  REQUIRE(session.save_as(original));
  const auto source_json = json(session);
  const auto constraints = session.sketch_constraint_catalogs();
  const auto dimensions = session.sketch_dimension_catalogs();
  const auto constraint_bytes = read(original.string() + ".sketch-constraints.json");
  const auto dimension_bytes = read(original.string() + ".sketch-dimensions.json");
  REQUIRE(session.begin_part_workflow());
  edit_source(session);
  extrude(session);
  CHECK_FALSE(session.dirty());
  REQUIRE(session.save());
  CHECK(read(original) == source_json);
  CHECK(read(original.string() + ".sketch-constraints.json") == constraint_bytes);
  CHECK(read(original.string() + ".sketch-dimensions.json") == dimension_bytes);
  REQUIRE(session.save_as(target));
  CHECK(session.file_path() == target);
  CHECK_FALSE(session.dirty());
  CHECK(read(target) == source_json);
  CHECK(read(target.string() + ".sketch-constraints.json") == constraint_bytes);
  CHECK(read(target.string() + ".sketch-dimensions.json") == dimension_bytes);
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(target));
  CHECK(json(reopened) == source_json);
  CHECK(reopened.sketch_constraint_catalogs() == constraints);
  CHECK(reopened.sketch_dimension_catalogs() == dimensions);
  session.cancel_part_workflow();
  CHECK(session.file_path() == target);
  CHECK_FALSE(session.dirty());
  CHECK(json(session) == source_json);
  CHECK(session.sketch_constraint_catalogs() == constraints);
  CHECK(session.sketch_dimension_catalogs() == dimensions);
}

TEST_CASE("Part workflow tracks committed dirty state and preserves failed-save path and bytes",
          "[gui][document-session][parity-b145][part-workflow]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = std::filesystem::path(directory.filePath("saved.json").toStdString());
  auto session = fixture();
  CHECK(session.dirty());
  REQUIRE(session.begin_part_workflow());
  edit_source(session);
  CHECK(session.dirty());
  CHECK_FALSE(session.save());
  CHECK(session.file_path().empty());
  REQUIRE(session.save_as(path));
  CHECK_FALSE(session.dirty());
  const auto before_bytes = read(path);
  CHECK_FALSE(session.save_as(std::filesystem::path(directory.path().toStdString())));
  CHECK(session.file_path() == path);
  CHECK(read(path) == before_bytes);
  CHECK(session.part_workflow_active());
  REQUIRE(session.publish_part_workflow("Publish corrected source"));
  CHECK(session.dirty());
  REQUIRE(session.undo());
  CHECK_FALSE(session.dirty());
  CHECK(json(session) == before_bytes);
  REQUIRE(session.redo());
  REQUIRE(session.save());
  CHECK_FALSE(session.dirty());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(json(reopened) == json(session));
  CHECK(reopened.sketch_constraint_catalogs() == session.sketch_constraint_catalogs());
  CHECK(reopened.sketch_dimension_catalogs() == session.sketch_dimension_catalogs());
}

TEST_CASE(
    "Failed Part workflow publication keeps draft correctable and files and history committed",
    "[gui][document-session][parity-b145][part-workflow]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = std::filesystem::path(directory.filePath("committed.json").toStdString());
  auto session = fixture();
  REQUIRE(session.save_as(path));
  const auto before = json(session);
  REQUIRE(session.begin_part_workflow());
  edit_source(session);
  extrude(session);
  REQUIRE(session.part_document()->set_parameter_value(ParameterId("height"),
                                                       checked(Quantity::length_mm(1e-12))));
  const auto invalid = json(session);
  CHECK_FALSE(session.publish_part_workflow("Create Extrude"));
  CHECK(session.part_workflow_active());
  CHECK(json(session) == invalid);
  CHECK_FALSE(session.dirty());
  CHECK(read(path) == before);
  REQUIRE(session.save());
  CHECK(read(path) == before);
  REQUIRE(session.part_document()->set_parameter_value(ParameterId("height"),
                                                       checked(Quantity::length_mm(10))));
  REQUIRE(session.publish_part_workflow("Create Extrude"));
  CHECK(volume(session) == Catch::Approx(2400).margin(1e-6));
  REQUIRE(session.undo());
  CHECK(json(session) == before);
}

TEST_CASE("Part workflow rejects nested scopes and replacement until publication or cancellation",
          "[gui][document-session][parity-b145][part-workflow]") {
  GuiDocumentSession empty;
  CHECK_FALSE(empty.begin_part_workflow());
  REQUIRE(empty.create_project(DocumentId("project"), "Project"));
  CHECK_FALSE(empty.begin_part_workflow());
  auto session = fixture();
  session.set_derived_results_fresh(false);
  CHECK_FALSE(session.begin_part_workflow());
  REQUIRE(session.recompute());
  REQUIRE(session.set_workspace(GuiWorkspace::Sketch));
  CHECK_FALSE(session.begin_part_workflow());
  REQUIRE(session.set_workspace(GuiWorkspace::Part));
  REQUIRE(session.task().begin("unrelated"));
  CHECK_FALSE(session.begin_part_workflow());
  REQUIRE(session.task().cancel());
  REQUIRE(session.begin_part_workflow());
  const auto before = json(session);
  CHECK_FALSE(session.begin_part_workflow());
  CHECK_FALSE(session.close(true));
  CHECK_FALSE(session.create_part(DocumentId("other"), "Other", true));
  CHECK_FALSE(session.create_project(DocumentId("other"), "Other", true));
  CHECK_FALSE(session.open_file("/tmp/nonexistent-workflow.json", true));
  CHECK_FALSE(session.publish_part_workflow(""));
  REQUIRE(session.set_workspace(GuiWorkspace::Sketch));
  CHECK_FALSE(session.publish_part_workflow("Still editing source"));
  CHECK(session.part_workflow_active());
  CHECK(json(session) == before);
  session.cancel_part_workflow();
  CHECK(session.workspace() == GuiWorkspace::Part);
  CHECK(session.close(true));
}

TEST_CASE(
    "Copied Part workflows share immutable entry state and retain independent draft histories",
    "[gui][document-session][parity-b145][part-workflow]") {
  auto session = fixture();
  const auto before = json(session);
  REQUIRE(session.begin_part_workflow());
  edit_source(session);
  auto copied = session;
  extrude(copied);
  REQUIRE(copied.publish_part_workflow("Copied extrusion"));
  CHECK(session.part_workflow_active());
  CHECK(session.part_document()->features().empty());
  CHECK(copied.part_document()->features().size() == 1);
  session.cancel_part_workflow();
  CHECK(json(session) == before);
  REQUIRE(copied.undo());
  CHECK(json(copied) == before);
  REQUIRE(copied.redo());
  CHECK(volume(copied) == Catch::Approx(2400).margin(1e-6));
}

TEST_CASE("Unchanged Part workflow preserves history and redo without a spurious publication",
          "[gui][document-session][parity-b145][part-workflow]") {
  auto session = fixture();
  edit_source(session);
  REQUIRE(session.undo());
  const auto before = json(session);
  const auto label = std::string(session.undo_label());
  REQUIRE(session.begin_part_workflow());
  edit_source(session);
  REQUIRE(session.undo());
  CHECK(checked(session.publish_part_workflow("Unchanged")) == 0);
  CHECK_FALSE(session.part_workflow_active());
  CHECK(json(session) == before);
  CHECK(session.undo_label() == label);
  REQUIRE(session.redo());
  CHECK(session.part_document()->find_parameter(ParameterId("width"))->value().millimeters() == 30);
  CHECK_FALSE(session.publish_part_workflow("No workflow"));
  session.cancel_part_workflow();
  CHECK(session.undo_label() == "Edit source Sketch");
}

TEST_CASE("Part workflow rejects unrecorded source state and failed sidecar edits are atomic",
          "[gui][document-session][parity-b145][part-workflow]") {
  auto session = fixture();
  auto unrecorded = session;
  REQUIRE(unrecorded.part_document()->set_parameter_value(ParameterId("width"),
                                                          checked(Quantity::length_mm(40))));
  REQUIRE(unrecorded.recompute());
  CHECK_FALSE(unrecorded.begin_part_workflow());
  const auto before = json(session);
  const auto constraints = session.sketch_constraint_catalogs();
  const auto dimensions = session.sketch_dimension_catalogs();
  REQUIRE(session.begin_part_workflow());
  CHECK_FALSE(session.commit_part_sketch_intent_transaction(
      "Wrong catalog owner", [](PartDocument& part, auto& constraints, auto& dimensions) {
        REQUIRE(part.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(99))));
        constraints = {
            checked(SketchConstraintCatalog::create(DocumentId("other"), SketchId("sketch"), {}))};
        dimensions.clear();
        return Result<std::size_t>::success(1U);
      }));
  CHECK(json(session) == before);
  CHECK(session.sketch_constraint_catalogs() == constraints);
  CHECK(session.sketch_dimension_catalogs() == dimensions);
  session.cancel_part_workflow();
  CHECK(json(session) == before);
}

TEST_CASE("Part workflow committed-only save removes draft sidecars absent from the entry state",
          "[gui][document-session][parity-b145][part-workflow]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto target = std::filesystem::path(directory.filePath("entry.json").toStdString());
  auto session = fixture();
  REQUIRE(session.save_as(target));
  REQUIRE(std::filesystem::exists(target.string() + ".sketch-constraints.json"));
  REQUIRE(std::filesystem::exists(target.string() + ".sketch-dimensions.json"));
  REQUIRE(session.commit_part_sketch_intent_transaction(
      "Clear entry catalogs", [](PartDocument&, auto& constraints, auto& dimensions) {
        constraints.clear();
        dimensions.clear();
        return Result<std::size_t>::success(1U);
      }));
  const auto before = json(session);
  REQUIRE(session.begin_part_workflow());
  edit_source(session);
  REQUIRE(session.save_as(target));
  CHECK(read(target) == before);
  CHECK_FALSE(std::filesystem::exists(target.string() + ".sketch-constraints.json"));
  CHECK_FALSE(std::filesystem::exists(target.string() + ".sketch-dimensions.json"));
  CHECK_FALSE(session.sketch_constraint_catalogs().empty());
  CHECK_FALSE(session.sketch_dimension_catalogs().empty());
  session.cancel_part_workflow();
  CHECK(session.sketch_constraint_catalogs().empty());
  CHECK(session.sketch_dimension_catalogs().empty());
  CHECK_FALSE(session.dirty());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(target));
  CHECK(json(reopened) == before);
  CHECK(reopened.sketch_constraint_catalogs().empty());
  CHECK(reopened.sketch_dimension_catalogs().empty());
}

TEST_CASE("Part workflow publishes catalog-only edits even when model JSON is unchanged",
          "[gui][document-session][parity-b145][part-workflow]") {
  auto session = fixture();
  const auto before = json(session);
  const auto constraints = session.sketch_constraint_catalogs();
  const auto dimensions = session.sketch_dimension_catalogs();
  REQUIRE(session.begin_part_workflow());
  REQUIRE(session.commit_part_sketch_intent_transaction(
      "Edit only catalogs", [](PartDocument& part, auto& constraints, auto& dimensions) {
        return intent(part, constraints, dimensions, true);
      }));
  CHECK(json(session) == before);
  const auto after_constraints = session.sketch_constraint_catalogs();
  const auto after_dimensions = session.sketch_dimension_catalogs();
  CHECK(after_constraints != constraints);
  CHECK(after_dimensions != dimensions);
  CHECK(checked(session.publish_part_workflow("Edit Sketch intent")) == 1U);
  CHECK(session.undo_label() == "Edit Sketch intent");
  REQUIRE(session.undo());
  CHECK(json(session) == before);
  CHECK(session.sketch_constraint_catalogs() == constraints);
  CHECK(session.sketch_dimension_catalogs() == dimensions);
  REQUIRE(session.redo());
  CHECK(json(session) == before);
  CHECK(session.sketch_constraint_catalogs() == after_constraints);
  CHECK(session.sketch_dimension_catalogs() == after_dimensions);
}
