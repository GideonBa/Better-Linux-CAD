#include "../support/engineering_resource_fixture.hpp"
#include "../support/missing_reference_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include "../support/repair_open_fixture.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>

using namespace blcad;
using namespace blcad::gui;

namespace {

std::filesystem::path temp_path(std::string_view name) {
  return std::filesystem::temp_directory_path() / std::string(name);
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("GUI Part lifecycle saves canonical JSON and tracks dirty state",
          "[gui][document-session]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.gui"), "GUI Part"));
  CHECK(session.dirty());
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
  CHECK(session.part_shape_cache() != nullptr);

  const auto path = temp_path("blcad_gui_block96_part.blcad.json");
  REQUIRE(session.save_as(path));
  CHECK_FALSE(session.dirty());
  CHECK(session.file_path() == path);

  REQUIRE(session.commit_part_transaction("Add XY datum", [](PartDocument& part) {
    auto plane = DatumPlane::xy();
    if (plane.has_error())
      return Result<std::size_t>::failure(plane.error());
    return part.add_datum_plane(std::move(plane.value()));
  }));
  REQUIRE(session.part_document() != nullptr);
  CHECK(session.part_document()->datum_plane_count() == 1);
  CHECK(session.dirty());
  CHECK(session.can_undo());
  CHECK(session.undo_label() == "Add XY datum");

  REQUIRE(session.undo());
  CHECK(session.part_document()->datum_plane_count() == 0);
  CHECK_FALSE(session.dirty());
  CHECK(session.can_redo());
  CHECK(session.redo_label() == "Add XY datum");

  REQUIRE(session.redo());
  CHECK(session.part_document()->datum_plane_count() == 1);
  CHECK(session.dirty());
  CHECK_FALSE(session.close());
  REQUIRE(session.save());
  CHECK_FALSE(session.dirty());
  CHECK(session.close());

  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}

TEST_CASE("GUI session opens Part and Project files through existing persistence",
          "[gui][document-session]") {
  const auto part_path = temp_path("blcad_gui_block96_open.blcad.json");
  const auto project_path = temp_path("blcad_gui_block96_open.blcad.project.json");

  GuiDocumentSession writer;
  REQUIRE(writer.create_part(DocumentId("part.open"), "Open Part"));
  REQUIRE(writer.save_as(part_path));
  REQUIRE(writer.create_project(DocumentId("project.open"), "Open Project", true));
  REQUIRE(writer.save_as(project_path));

  GuiDocumentSession reader;
  REQUIRE(reader.open_file(part_path));
  CHECK(reader.document_kind() == GuiDocumentKind::Part);
  CHECK(reader.display_name() == "Open Part");
  CHECK_FALSE(reader.dirty());
  REQUIRE(reader.open_file(project_path));
  CHECK(reader.document_kind() == GuiDocumentKind::Project);
  CHECK(reader.display_name() == "Open Project");
  CHECK_FALSE(reader.dirty());

  std::error_code ignored;
  std::filesystem::remove(part_path, ignored);
  std::filesystem::remove(project_path, ignored);
}

namespace {

template <typename T> T migration_checked(Result<T> result) {
  REQUIRE(result);
  return std::move(result.value());
}

struct LegacySplineSidecars {
  PartDocument part;
  SketchConstraintCatalog constraints;
  SketchDimensionCatalog dimensions;
};

LegacySplineSidecars legacy_spline_sidecars() {
  const DocumentId document("part.legacy.spline");
  const SketchId sketch_id("sketch.legacy.spline");
  auto part = migration_checked(PartDocument::create(document, "Legacy spline intent"));
  REQUIRE(part.add_datum_plane(migration_checked(DatumPlane::xy())));
  REQUIRE(part.add_parameter(migration_checked(Parameter::create_length(
      ParameterId("height"), "Height", migration_checked(Quantity::length_mm(10.0, "height"))))));
  auto sketch =
      migration_checked(Sketch::create(sketch_id, "Closed fit", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_entity(migration_checked(SplineSegment::create_cubic_bezier(
      SketchEntityId("curve"), {-10, 0}, {-10, 5}, {-5, 10}, {0, 10}))));
  REQUIRE(sketch.add_entity(migration_checked(SplineSegment::create_cubic_bezier(
      SketchEntityId("curve.fit.2"), {0, 10}, {5, 10}, {10, 5}, {10, 0}))));
  REQUIRE(sketch.add_entity(migration_checked(SplineSegment::create_cubic_bezier(
      SketchEntityId("curve.fit.3"), {10, 0}, {5, -10}, {-5, -10}, {-10, 0}))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  const auto point = [](std::string id) {
    return migration_checked(SketchConstraintIntentTarget::point(SketchPointId(std::move(id))));
  };
  auto constraints = migration_checked(SketchConstraintCatalog::create(
      document, sketch_id,
      {migration_checked(SketchConstraintIntent::create(
           SketchConstraintId("fixed.interior"), SketchSolverConstraintKind::Fixed,
           {point("entity/curve/end")}, SketchConstraintIntentSource::Manual)),
       migration_checked(SketchConstraintIntent::create(
           SketchConstraintId("fixed.seam"), SketchSolverConstraintKind::Fixed,
           {point("entity/curve/start")}, SketchConstraintIntentSource::Automatic)),
       migration_checked(SketchConstraintIntent::create(
           SketchConstraintId("join.interior"), SketchSolverConstraintKind::Coincident,
           {point("entity/curve/end"), point("entity/curve.fit.2/start")},
           SketchConstraintIntentSource::Automatic)),
       migration_checked(SketchConstraintIntent::create(
           SketchConstraintId("join.seam"), SketchSolverConstraintKind::Coincident,
           {point("entity/curve/start"), point("entity/curve.fit.3/end")}))}));
  auto dimensions = migration_checked(SketchDimensionCatalog::create(
      document, sketch_id,
      {migration_checked(SketchDimensionIntent::create(
           SketchDimensionId("height.driving"), SketchDimensionKind::VerticalDistance,
           SketchDimensionMode::Driving, {point("entity/curve/start"), point("entity/curve/end")},
           ParameterId("height"), {2, 7})),
       migration_checked(SketchDimensionIntent::create(
           SketchDimensionId("width.reference"), SketchDimensionKind::PointToPointDistance,
           SketchDimensionMode::Reference,
           {point("entity/curve/start"), point("entity/curve.fit.2/end")}, std::nullopt,
           {-3, 4}))}));
  return {std::move(part), std::move(constraints), std::move(dimensions)};
}

void write_legacy_spline_sidecars(const std::filesystem::path& path,
                                  const LegacySplineSidecars& fixture) {
  REQUIRE(write_part_document_json_file(fixture.part, path));
  const auto write = [](const std::filesystem::path& target, const std::string& content) {
    std::ofstream stream(target, std::ios::binary);
    REQUIRE(stream);
    stream << content;
    REQUIRE(stream);
  };
  write(path.string() + ".sketch-constraints.json",
        migration_checked(serialize_sketch_constraint_catalogs_to_json(fixture.part.id(),
                                                                       {fixture.constraints})));
  write(path.string() + ".sketch-dimensions.json",
        migration_checked(
            serialize_sketch_dimension_catalogs_to_json(fixture.part.id(), {fixture.dimensions})));
}

} // namespace

TEST_CASE("GUI open migrates legacy fitted spline sidecar points and saves canonical intent",
          "[gui][document-session][spline-sidecar-migration][persistence]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = std::filesystem::path(directory.filePath("legacy.blcad.json").toStdString());
  const auto saved =
      std::filesystem::path(directory.filePath("canonical.blcad.json").toStdString());
  const auto fixture = legacy_spline_sidecars();
  write_legacy_spline_sidecars(path, fixture);

  GuiDocumentSession session;
  REQUIRE(session.open_file(path));
  CHECK_FALSE(session.dirty());
  const auto constraints =
      migration_checked(session.sketch_constraint_catalog(fixture.constraints.sketch()));
  REQUIRE(constraints.constraints().size() == 2U);
  CHECK(constraints.document() == fixture.part.id());
  for (const auto& intent : constraints.constraints()) {
    const auto* previous = fixture.constraints.find(intent.id());
    REQUIRE(previous != nullptr);
    CHECK(intent.kind() == previous->kind());
    CHECK(intent.source() == previous->source());
    CHECK(intent.targets()[0].kind() == SketchConstraintIntentTargetKind::Point);
    CHECK(intent.targets()[0].id() == (intent.id() == SketchConstraintId("fixed.interior")
                                           ? "entity/curve.fit.2/start"
                                           : "entity/curve.fit.3/end"));
  }
  const auto dimensions =
      migration_checked(session.sketch_dimension_catalog(fixture.dimensions.sketch()));
  REQUIRE(dimensions.dimensions().size() == 2U);
  for (const auto& intent : dimensions.dimensions()) {
    const auto* previous = fixture.dimensions.find(intent.id());
    REQUIRE(previous != nullptr);
    CHECK(intent.kind() == previous->kind());
    CHECK(intent.mode() == previous->mode());
    CHECK(intent.parameter() == previous->parameter());
    CHECK(intent.label_offset() == previous->label_offset());
    CHECK(intent.targets()[0].id() == "entity/curve.fit.3/end");
    CHECK(intent.targets()[1].id() ==
          (intent.driving() ? "entity/curve.fit.2/start" : "entity/curve.fit.2/end"));
  }
  CHECK(migration_checked(serialize_part_document_to_json(*session.part_document())) ==
        migration_checked(serialize_part_document_to_json(fixture.part)));
  REQUIRE(session.save_as(saved));
  CHECK(read_file(saved.string() + ".sketch-constraints.json") ==
        migration_checked(
            serialize_sketch_constraint_catalogs_to_json(fixture.part.id(), {constraints})));
  CHECK(read_file(saved.string() + ".sketch-dimensions.json") ==
        migration_checked(
            serialize_sketch_dimension_catalogs_to_json(fixture.part.id(), {dimensions})));

  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(saved));
  CHECK(migration_checked(reopened.sketch_constraint_catalog(fixture.constraints.sketch())) ==
        constraints);
  CHECK(migration_checked(reopened.sketch_dimension_catalog(fixture.dimensions.sketch())) ==
        dimensions);
}

TEST_CASE("GUI sidecar topology migration rejects unrelated or collapsed intent atomically",
          "[gui][document-session][spline-sidecar-migration][persistence][atomic]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = std::filesystem::path(directory.filePath("invalid.blcad.json").toStdString());
  auto fixture = legacy_spline_sidecars();
  bool malformed_target = false;
  const auto point = [](std::string id) {
    return migration_checked(SketchConstraintIntentTarget::point(SketchPointId(std::move(id))));
  };
  SECTION("unknown point cannot be inferred from an endpoint-like spelling") {
    REQUIRE(fixture.constraints.add(migration_checked(SketchConstraintIntent::create(
        SketchConstraintId("unknown"), SketchSolverConstraintKind::Fixed,
        {point("entity/curve.fit.99/end")}))));
  }
  SECTION("unknown entities remain invalid") {
    REQUIRE(fixture.constraints.add(migration_checked(SketchConstraintIntent::create(
        SketchConstraintId("unknown"), SketchSolverConstraintKind::Fixed,
        {migration_checked(SketchConstraintIntentTarget::entity("entity/curve.fit.99"))}))));
  }
  SECTION("unknown dimension point cannot be silently discarded") {
    REQUIRE(fixture.dimensions.add(migration_checked(SketchDimensionIntent::create(
        SketchDimensionId("unknown"), SketchDimensionKind::AlignedDistance,
        SketchDimensionMode::Reference,
        {point("entity/curve/start"), point("entity/curve.fit.99/end")}))));
  }
  SECTION("a dimension collapsed onto a shared junction is rejected") {
    REQUIRE(fixture.dimensions.add(migration_checked(SketchDimensionIntent::create(
        SketchDimensionId("collapsed"), SketchDimensionKind::AlignedDistance,
        SketchDimensionMode::Reference,
        {point("entity/curve/end"), point("entity/curve.fit.2/start")}))));
  }
  SECTION("a collapsed noncoincident constraint is rejected") {
    REQUIRE(fixture.constraints.add(migration_checked(SketchConstraintIntent::create(
        SketchConstraintId("collapsed"), SketchSolverConstraintKind::Collinear,
        {point("entity/curve/end"), point("entity/curve.fit.2/start"),
         point("entity/curve.fit.2/end")}))));
  }
  SECTION("malformed target kinds are rejected before migration") {
    malformed_target = true;
  }
  write_legacy_spline_sidecars(path, fixture);
  if (malformed_target) {
    const auto sidecar = path.string() + ".sketch-constraints.json";
    auto json = nlohmann::json::parse(read_file(sidecar));
    json["catalogs"][0]["constraints"][0]["targets"][0]["kind"] = "spline_endpoint";
    std::ofstream stream(sidecar, std::ios::binary);
    REQUIRE(stream);
    stream << json.dump(2);
    REQUIRE(stream);
  }
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("active.part"), "Keep active Part"));
  REQUIRE(session.commit_part_transaction("Keep existing history", [](PartDocument& part) {
    return part.add_datum_plane(DatumPlane::xy().value());
  }));
  const auto before = migration_checked(serialize_part_document_to_json(*session.part_document()));
  const auto* cache = session.part_shape_cache();
  const auto history_label = session.undo_label();
  REQUIRE_FALSE(session.open_file(path, true));
  CHECK(migration_checked(serialize_part_document_to_json(*session.part_document())) == before);
  CHECK(session.part_shape_cache() == cache);
  CHECK(session.undo_label() == history_label);
  CHECK(session.file_path().empty());
  CHECK(session.dirty());
  CHECK(session.can_undo());
  CHECK(session.sketch_constraint_catalogs().empty());
  CHECK(session.sketch_dimension_catalogs().empty());
}

TEST_CASE("GUI transaction failure retains the last valid document and records diagnostics",
          "[gui][document-transaction][gui][diagnostics]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.atomic"), "Atomic Part"));
  const auto before = session.part_document()->datum_plane_count();
  const auto* cache_before = session.part_shape_cache();
  const auto presentation_before = session.presentation_revision();

  const auto failed = session.commit_part_transaction("Rejected edit", [](PartDocument&) {
    return Result<std::size_t>::failure(
        Error::validation("feature.invalid", "representative command validation failure"));
  });
  REQUIRE(failed.has_error());
  CHECK(session.part_document()->datum_plane_count() == before);
  CHECK(session.part_shape_cache() == cache_before);
  CHECK_FALSE(session.can_undo());
  REQUIRE_FALSE(session.diagnostics().empty());
  CHECK(session.diagnostics().back().object_id == "feature.invalid");
  CHECK(session.diagnostics().back().severity == GuiDiagnosticSeverity::Error);
  CHECK(session.presentation_revision() == presentation_before + 1U);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
}

TEST_CASE("GUI session reports external errors through revisioned diagnostics",
          "[gui][document-session][gui][diagnostics]") {
  GuiDocumentSession session;
  const auto revision_before = session.presentation_revision();

  session.report_error(Error::validation("shell.command", "command failed"));

  REQUIRE(session.diagnostics().size() == 1U);
  CHECK(session.diagnostics().front().object_id == "shell.command");
  CHECK(session.presentation_revision() == revision_before + 1U);

  session.clear_diagnostics();
  CHECK(session.diagnostics().empty());
  CHECK(session.presentation_revision() == revision_before + 2U);
}

TEST_CASE("GUI save transaction preserves every existing file when a sidecar target is invalid",
          "[gui][document-session][persistence][atomic]") {
  const auto directory = temp_path("blcad_gui_save_transaction");
  const auto document_path = directory / "atomic.blcad.json";
  const auto constraint_path =
      std::filesystem::path(document_path.string() + ".sketch-constraints.json");
  const auto dimension_path =
      std::filesystem::path(document_path.string() + ".sketch-dimensions.json");
  const auto marker_path = dimension_path / "marker.txt";

  std::error_code ignored;
  std::filesystem::remove_all(directory, ignored);
  REQUIRE(std::filesystem::create_directories(dimension_path));
  {
    std::ofstream document(document_path, std::ios::binary);
    REQUIRE(document);
    document << "original document";
  }
  {
    std::ofstream constraints(constraint_path, std::ios::binary);
    REQUIRE(constraints);
    constraints << "original constraints";
  }
  {
    std::ofstream marker(marker_path, std::ios::binary);
    REQUIRE(marker);
    marker << "keep";
  }

  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.atomic_save"), "Atomic Save"));
  const auto revision_before = session.presentation_revision();
  const auto saved = session.save_as(document_path);

  REQUIRE(saved.has_error());
  CHECK(read_file(document_path) == "original document");
  CHECK(read_file(constraint_path) == "original constraints");
  CHECK(read_file(marker_path) == "keep");
  CHECK(session.file_path().empty());
  CHECK(session.dirty());
  CHECK(session.presentation_revision() == revision_before + 1U);
  REQUIRE_FALSE(session.diagnostics().empty());
  CHECK(session.diagnostics().back().object_id == dimension_path.string());

  std::filesystem::remove_all(directory, ignored);
}

TEST_CASE("GUI Project transaction recomputes owned Parts and supports undo redo",
          "[gui][document-transaction][gui][document-session]") {
  GuiDocumentSession session;
  REQUIRE(session.create_project(DocumentId("project.transaction"), "Transaction Project"));

  REQUIRE(session.commit_project_transaction("Add Part", [](Project& project) {
    auto part = PartDocument::create(DocumentId("part.project_member"), "Project Member");
    if (part.has_error())
      return Result<std::size_t>::failure(part.error());
    return project.add_part_document(std::move(part.value()));
  }));
  REQUIRE(session.project() != nullptr);
  CHECK(session.project()->part_document_count() == 1);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
  REQUIRE(session.undo());
  CHECK(session.project()->part_document_count() == 0);
  REQUIRE(session.redo());
  CHECK(session.project()->part_document_count() == 1);
}

namespace {
std::filesystem::path failed_part_file(const QTemporaryDir& directory) {
  const auto path = std::filesystem::path(directory.filePath("failed.blcad.json").toStdString());
  std::ofstream file(path);
  file << test::reference_checked(serialize_part_document_to_json(test::repair_open_fixture()));
  file.close();
  REQUIRE(file.good());
  return path;
}
std::string part_snapshot(const GuiDocumentSession& session) {
  return test::reference_checked(serialize_part_document_to_json(*session.part_document()));
}
std::filesystem::path missing_part_file(const QTemporaryDir& directory) {
  const auto path = std::filesystem::path(directory.filePath("missing.blcad.json").toStdString());
  std::ofstream(path) << test::missing_reference_fixture_json();
  return path;
}
} // namespace
TEST_CASE("Missing-source repair opening saving atomic publication and history preserve intent",
          "[gui][document-session][missing-reference-session]") {
  QTemporaryDir directory;
  const auto path = missing_part_file(directory);
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("previous"), "Previous"));
  const auto previous = part_snapshot(session);
  CHECK_FALSE(session.open_file(path, true));
  CHECK(part_snapshot(session) == previous);
  REQUIRE(session.open_file(path, true, GuiDocumentOpenMode::ReferenceRepair));
  const auto before = part_snapshot(session);
  CHECK(session.reference_repair_mode());
  CHECK_FALSE(session.part_shape_cache());
  CHECK_FALSE(session.dirty());
  CHECK_FALSE(session.recompute());
  CHECK(session.reference_repair_mode());
  CHECK_FALSE(session.part_shape_cache());
  CHECK_FALSE(session.commit_part_repair_transaction(
      "Partial", [](PartDocument& part) { return test::repair_open_planes(part); }));
  CHECK(part_snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  const auto saved_failed = directory.filePath("failed-copy.blcad.json").toStdString();
  REQUIRE(session.save_as(saved_failed));
  CHECK(read_file(saved_failed) == before);
  CHECK_FALSE(read_part_document_json_file(saved_failed));
  REQUIRE(session.commit_part_repair_transaction("Repair missing references",
                                                 test::repair_all_missing_references));
  REQUIRE(session.part_shape_cache());
  CHECK_FALSE(session.reference_repair_mode());
  const auto after = part_snapshot(session);
  CHECK(session.dirty());
  REQUIRE(session.save());
  REQUIRE(session.undo());
  CHECK(part_snapshot(session) == before);
  CHECK(session.reference_repair_mode());
  CHECK_FALSE(session.part_shape_cache());
  CHECK(session.dirty());
  REQUIRE(session.redo());
  CHECK(part_snapshot(session) == after);
  CHECK_FALSE(session.reference_repair_mode());
  CHECK_FALSE(session.dirty());
  REQUIRE(read_part_document_json_file(saved_failed));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(saved_failed));
  CHECK(part_snapshot(reopened) == after);
}

TEST_CASE("Missing-source admission still validates sidecars and preserves the previous session",
          "[gui][document-session][missing-reference-session]") {
  QTemporaryDir directory;
  const auto path = missing_part_file(directory);
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("previous"), "Previous"));
  const auto before = part_snapshot(session);
  for (const std::string suffix : {".sketch-constraints.json", ".sketch-dimensions.json"}) {
    const auto sidecar = path.string() + suffix;
    std::ofstream(sidecar) << "{}";
    CHECK_FALSE(session.open_file(path, true, GuiDocumentOpenMode::ReferenceRepair));
    CHECK(part_snapshot(session) == before);
    CHECK_FALSE(session.reference_repair_mode());
    REQUIRE(std::filesystem::remove(sidecar));
  }
  REQUIRE(session.open_file(path, true, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(session.reference_repair_mode());
}
TEST_CASE("Repair opening retains Core-valid failed intent without exposing partial Geometry",
          "[gui][document-session][repair-open-session]") {
  QTemporaryDir directory;
  const auto path = failed_part_file(directory);
  const auto original_file = read_file(path);
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("previous"), "Previous"));
  REQUIRE(session.save_as(directory.filePath("previous.blcad.json").toStdString()));
  const auto before = part_snapshot(session);
  CHECK_FALSE(session.open_file(path));
  CHECK(part_snapshot(session) == before);
  CHECK(session.derived_results_fresh());
  CHECK_FALSE(session.reference_repair_mode());
  REQUIRE(session.open_file(path, false, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(session.reference_repair_mode());
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK_FALSE(session.derived_results_fresh());
  CHECK(session.part_shape_cache() == nullptr);
  CHECK_FALSE(session.dirty());
  CHECK_FALSE(session.can_undo());
  CHECK(session.file_path() == path);
  CHECK(session.diagnostics().back().object_id == "feature.old");
  CHECK(part_snapshot(session) == original_file);
  REQUIRE(session.save_as(directory.filePath("copy.blcad.json").toStdString()));
  CHECK(read_file(session.file_path()) == original_file);
  CHECK(read_file(path) == original_file);
  CHECK(session.reference_repair_mode());
  CHECK_FALSE(session.dirty());
  REQUIRE(session.close(true));
  REQUIRE(session.create_part(DocumentId("new"), "New"));
  CHECK_FALSE(session.reference_repair_mode());
  CHECK(session.derived_results_fresh());
  REQUIRE(session.save_as(directory.filePath("fresh.blcad.json").toStdString()));
  REQUIRE(session.open_file(session.file_path(), false, GuiDocumentOpenMode::ReferenceRepair));
  CHECK_FALSE(session.reference_repair_mode());
  CHECK(session.derived_results_fresh());
  CHECK_FALSE(session.dirty());
}
TEST_CASE("Repair transactions restore failed snapshots and fresh repaired history atomically",
          "[gui][document-session][repair-open-session]") {
  QTemporaryDir directory;
  const auto path = failed_part_file(directory);
  GuiDocumentSession session;
  REQUIRE(session.open_file(path, false, GuiDocumentOpenMode::ReferenceRepair));
  const auto before = part_snapshot(session);
  CHECK_FALSE(session.commit_part_repair_transaction(
      "Partial repair", [](PartDocument& part) { return test::repair_open_planes(part, false); }));
  CHECK(part_snapshot(session) == before);
  CHECK(session.reference_repair_mode());
  CHECK(session.part_shape_cache() == nullptr);
  CHECK_FALSE(session.can_undo());
  CHECK_FALSE(session.dirty());
  REQUIRE(session.commit_part_repair_transaction(
      "Repair references", [](PartDocument& part) { return test::repair_open_planes(part); }));
  const auto after = part_snapshot(session);
  CHECK(after != before);
  CHECK_FALSE(session.reference_repair_mode());
  CHECK(session.derived_results_fresh());
  REQUIRE(session.part_shape_cache());
  CHECK(session.part_shape_cache()->feature_shape_count() == 4);
  const auto* valid_cache = session.part_shape_cache();
  CHECK_FALSE(session.commit_part_repair_transaction("Invalid replacement", [](PartDocument& part) {
    return part.update_derived_workplane(
        test::reference_checked(DerivedWorkplane::create_on_feature_face(
            DatumPlaneId("plane"), "Face plane",
            test::reference_checked(
                SemanticFaceReference::create(FeatureId("feature.old"), SemanticFace::Top)))));
  }));
  CHECK(part_snapshot(session) == after);
  CHECK(session.part_shape_cache() == valid_cache);
  CHECK(session.derived_results_fresh());
  CHECK_FALSE(session.reference_repair_mode());
  CHECK(session.undo_label() == "Repair references");
  CHECK(session.dirty());
  REQUIRE(session.save());
  CHECK_FALSE(session.dirty());
  REQUIRE(session.undo());
  CHECK(part_snapshot(session) == before);
  CHECK(session.reference_repair_mode());
  CHECK_FALSE(session.derived_results_fresh());
  CHECK(session.part_shape_cache() == nullptr);
  CHECK(session.dirty());
  CHECK(session.can_redo());
  REQUIRE(session.redo());
  CHECK(part_snapshot(session) == after);
  CHECK_FALSE(session.reference_repair_mode());
  CHECK(session.derived_results_fresh());
  CHECK_FALSE(session.dirty());
  REQUIRE(session.undo());
  REQUIRE(session.save_as(directory.filePath("original-copy.blcad.json").toStdString()));
  CHECK_FALSE(session.dirty());
  REQUIRE(session.redo());
  CHECK(session.dirty());
  CHECK(part_snapshot(session) == after);
}
TEST_CASE("Repair opening preserves strict validation and blocks ordinary transaction bypasses",
          "[gui][document-session][repair-open-session]") {
  QTemporaryDir directory;
  const auto path = failed_part_file(directory);
  GuiDocumentSession session;
  REQUIRE(session.open_file(path, false, GuiDocumentOpenMode::ReferenceRepair));
  const auto before = part_snapshot(session);
  bool called = false;
  const auto forbidden = [&](auto&...) {
    called = true;
    return Result<std::size_t>::success(1);
  };
  CHECK_FALSE(session.commit_part_transaction("Ordinary", forbidden));
  CHECK_FALSE(session.commit_part_constraint_transaction("Constraints", forbidden));
  CHECK_FALSE(session.commit_part_dimension_transaction("Dimensions", forbidden));
  CHECK_FALSE(session.commit_part_sketch_intent_transaction("Intent", forbidden));
  CHECK_FALSE(called);
  session.set_derived_results_fresh(true);
  CHECK_FALSE(session.derived_results_fresh());
  session.set_derived_results_fresh(false);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK(part_snapshot(session) == before);
  CHECK_FALSE(session.dirty());
  const auto corrupt_path = directory.filePath("invalid.blcad.json").toStdString();
  auto corrupt = before;
  const std::string source = "\"source_feature\": \"feature.old\"";
  const auto at = corrupt.find(source);
  REQUIRE(at != std::string::npos);
  corrupt.replace(at, source.size(), "\"source_feature\": \"width\"");
  std::ofstream(corrupt_path) << corrupt;
  CHECK_FALSE(session.open_file(corrupt_path, false, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(part_snapshot(session) == before);
  CHECK(session.reference_repair_mode());
  const auto sidecar = path.string() + ".sketch-constraints.json";
  std::ofstream(sidecar) << "{}";
  CHECK_FALSE(session.open_file(path, false, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(part_snapshot(session) == before);
  CHECK(session.reference_repair_mode());
  CHECK_FALSE(session.open_file(directory.filePath("missing.blcad.project.json").toStdString(),
                                false, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(part_snapshot(session) == before);
}

namespace {
namespace resources = blcad::test::engineering_fixture;
PartDocument resource_member(std::string id) {
  auto data = nlohmann::json::parse(migration_checked(
      serialize_part_document_to_json(blcad::test::history_fixture::native_base())));
  data["document"]["id"] = id;
  auto part = migration_checked(deserialize_part_document_from_json(data.dump()));
  resources::annotate(part);
  return part;
}
std::string project_snapshot(const GuiDocumentSession& session) {
  REQUIRE(session.project());
  return migration_checked(serialize_project_to_json(*session.project()));
}
} // namespace

TEST_CASE("Resource-bearing Projects preserve shared ownership history and offline Part extraction",
          "[gui][document-session][parity-b149][project-resource-integration]") {
  QTemporaryDir project_directory;
  QTemporaryDir standalone_directory;
  GuiDocumentSession session;
  REQUIRE(session.create_project(DocumentId("project.resources"), "Resources"));
  const auto empty = project_snapshot(session);
  REQUIRE(session.commit_project_transaction("Insert portable Parts", [](Project& project) {
    auto added = project.add_part_document(resource_member("first"));
    if (!added)
      return added;
    auto second = resource_member("second");
    const auto assigned = second.set_default_material(resources::key("synthetic.material", "r2"));
    if (!assigned)
      return assigned;
    return project.add_part_document(std::move(second));
  }));
  const auto populated = project_snapshot(session);
  CHECK(session.project()->engineering_resources().entries().size() == 3);
  CHECK(session.derived_results_fresh());
  REQUIRE(session.undo());
  CHECK(project_snapshot(session) == empty);
  REQUIRE(session.redo());
  CHECK(project_snapshot(session) == populated);
  const auto path = project_directory.filePath("project.blcad.project.json").toStdString();
  REQUIRE(session.save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(project_snapshot(reopened) == populated);
  CHECK(reopened.derived_results_fresh());
  const auto* member = reopened.project()->find_part_document(DocumentId("second"));
  REQUIRE(member);
  const auto portable = standalone_directory.filePath("member.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(*member, portable));
  const auto member_json = migration_checked(serialize_part_document_to_json(*member));
  REQUIRE(reopened.close());
  REQUIRE(std::filesystem::remove(path));
  GuiDocumentSession extracted;
  REQUIRE(extracted.open_file(portable));
  CHECK(migration_checked(serialize_part_document_to_json(*extracted.part_document())) ==
        member_json);
  CHECK(extracted.part_document()->default_material() ==
        resources::key("synthetic.material", "r2"));
  CHECK(extracted.derived_results_fresh());
  REQUIRE(extracted.part_shape_cache());
  CHECK(extracted.part_shape_cache()->body_shape_count() == 1);
}

TEST_CASE("Project candidate synchronization and malformed resource Open preserve committed state",
          "[gui][document-session][parity-b149][project-resource-integration]") {
  QTemporaryDir directory;
  GuiDocumentSession session;
  REQUIRE(session.create_project(DocumentId("project.resources"), "Resources"));
  REQUIRE(session.commit_project_transaction("Seed resources", [](Project& project) {
    auto added = project.add_part_document(resource_member("first"));
    if (!added)
      return added;
    return project.add_part_document(
        resources::checked(PartDocument::create(DocumentId("last"), "Last")));
  }));
  REQUIRE(session.save_as(directory.filePath("project.blcad.project.json").toStdString()));
  const auto before = project_snapshot(session);
  REQUIRE(session.commit_project_transaction("Add member revision", [](Project& project) {
    return project.find_part_document(DocumentId("first"))
        ->add_engineering_resource(resources::material("r3", 7000));
  }));
  const auto after = project_snapshot(session);
  CHECK(session.project()->engineering_resources().entries().size() == 4);
  REQUIRE(session.undo());
  CHECK(project_snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(project_snapshot(session) == after);
  const auto path = session.file_path();
  CHECK_FALSE(session.commit_project_transaction("Conflicting batch", [](Project& project) {
    auto added = project.find_part_document(DocumentId("first"))
                     ->add_engineering_resource(resources::material("r4", 8000));
    if (!added)
      return added;
    return project.find_part_document(DocumentId("last"))
        ->add_engineering_resource(resources::material("r1", 9000));
  }));
  CHECK(project_snapshot(session) == after);
  CHECK(session.file_path() == path);
  CHECK(session.derived_results_fresh());
  CHECK(session.undo_label() == "Add member revision");
  auto malformed = nlohmann::json::parse(after);
  malformed["parts"][0]["engineering_resource_refs"][0]["revision"] = "missing";
  const auto bad = directory.filePath("bad.blcad.project.json").toStdString();
  {
    std::ofstream output(bad);
    output << malformed.dump();
  }
  CHECK_FALSE(session.open_file(bad, true));
  CHECK(project_snapshot(session) == after);
  CHECK(session.file_path() == path);
  CHECK(session.derived_results_fresh());
  REQUIRE(session.undo());
  CHECK(project_snapshot(session) == before);
}

TEST_CASE("One Project transaction adopts revisions across selected Parts with exact Undo and Save",
          "[gui][document-session][parity-b149][resource-revision-integration]") {
  QTemporaryDir directory;
  GuiDocumentSession session;
  REQUIRE(session.create_project(DocumentId("project.revisions"), "Revisions"));
  REQUIRE(session.commit_project_transaction("Seed members", [](Project& project) {
    for (const auto& id : {"first", "second", "unselected"}) {
      auto part = resource_member(id);
      auto authored = part.apply_history_intent({{"base"}, {}});
      if (!authored)
        return authored;
      auto added = project.add_part_document(std::move(part));
      if (!added)
        return added;
    }
    return Result<std::size_t>::success(3);
  }));
  REQUIRE(session.save_as(directory.filePath("revisions.blcad.project.json").toStdString()));
  const auto before = project_snapshot(session);
  REQUIRE(session.commit_project_transaction("Adopt selected revision", [](Project& project) {
    return project.adopt_engineering_resource_revision(resources::key(),
                                                       resources::material("r3", 7000),
                                                       {DocumentId("first"), DocumentId("second")});
  }));
  const auto after = project_snapshot(session);
  CHECK(session.derived_results_fresh());
  CHECK(session.project()->find_part_document(DocumentId("first"))->default_material() ==
        resources::key("synthetic.material", "r3"));
  CHECK(session.project()->find_part_document(DocumentId("second"))->default_material() ==
        resources::key("synthetic.material", "r3"));
  CHECK(session.project()->find_part_document(DocumentId("unselected"))->default_material() ==
        resources::key());
  CHECK(session.undo_label() == "Adopt selected revision");
  REQUIRE(session.undo());
  CHECK(project_snapshot(session) == before);
  CHECK_FALSE(session.dirty());
  REQUIRE(session.redo());
  CHECK(project_snapshot(session) == after);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  CHECK(project_snapshot(reopened) == after);
  CHECK(reopened.derived_results_fresh());
  CHECK_FALSE(session.commit_project_transaction("Late failed adoption", [](Project& project) {
    return project.adopt_engineering_resource_revision(
        resources::key("synthetic.material", "r3"), resources::material("r4", 8000),
        {DocumentId("first"), DocumentId("unselected")});
  }));
  CHECK(project_snapshot(session) == after);
  CHECK(session.derived_results_fresh());
  CHECK_FALSE(session.dirty());
  CHECK(session.undo_label() == "Adopt selected revision");
  REQUIRE(session.undo());
  CHECK(project_snapshot(session) == before);
}

TEST_CASE("Session resource removal guards rollback and restore cleared snapshots through history",
          "[gui][document-session][parity-b149][resource-revision-integration]") {
  QTemporaryDir directory;
  GuiDocumentSession session;
  REQUIRE(session.create_project(DocumentId("project.remove"), "Removal"));
  REQUIRE(session.commit_project_transaction("Seed member", [](Project& project) {
    return project.add_part_document(resource_member("first"));
  }));
  REQUIRE(session.save_as(directory.filePath("remove.blcad.project.json").toStdString()));
  const auto before = project_snapshot(session);
  CHECK_FALSE(
      session.commit_project_transaction("Remove referenced revision", [](Project& project) {
        return project.remove_engineering_resource(resources::key());
      }));
  CHECK(project_snapshot(session) == before);
  CHECK(session.derived_results_fresh());
  REQUIRE(session.commit_project_transaction("Clear unused resources", [](Project& project) {
    auto* part = project.find_part_document(DocumentId("first"));
    auto cleared = part->set_default_material(std::nullopt);
    if (!cleared)
      return cleared;
    for (const auto& key : {resources::key(), resources::key("synthetic.material", "r2"),
                            resources::key("synthetic.table")}) {
      auto removed = part->remove_engineering_resource(key);
      if (!removed)
        return removed;
      removed = project.remove_engineering_resource(key);
      if (!removed)
        return removed;
    }
    return Result<std::size_t>::success(3);
  }));
  const auto cleared = project_snapshot(session);
  CHECK(nlohmann::json::parse(cleared).at("version") == 1);
  REQUIRE(session.undo());
  CHECK(project_snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(project_snapshot(session) == cleared);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  CHECK(project_snapshot(reopened) == cleared);
  CHECK(reopened.derived_results_fresh());
}

TEST_CASE(
    "Material-only Part transactions retain BRep ownership and exact history while updating mass",
    "[gui][document-session][parity-b150][physical-reuse-session]") {
  for (bool authored : {false, true}) {
    DYNAMIC_SECTION("authored history " << authored) {
      QTemporaryDir directory;
      auto part = resource_member("physical.part");
      if (authored)
        REQUIRE(part.apply_history_intent({{"base"}, {}}));
      const auto path = directory.filePath("physical.blcad.json").toStdString();
      GuiDocumentSession session;
      REQUIRE(session.create_part(part.id(), part.name()));
      REQUIRE(session.commit_part_transaction("Seed physical input", [&](PartDocument& candidate) {
        candidate = part;
        return Result<std::size_t>::success(1);
      }));
      REQUIRE(session.save_as(path));
      REQUIRE(session.part_shape_cache());
      REQUIRE(
          session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
      const auto shape = *session.part_shape_cache()->find_body_shape(BodyId("body.base"));
      const auto integrated = migration_checked(
          geometry::BodyResultInspector{}.inspect_physical_properties(*session.part_document()));
      const auto before = part_snapshot(session);
      REQUIRE(session.commit_part_transaction("Assign denser material", [](PartDocument& p) {
        return p.set_default_material(resources::key("synthetic.material", "r2"));
      }));
      const auto after = part_snapshot(session);
      CHECK(session.derived_results_fresh());
      CHECK(session.dirty());
      CHECK(session.undo_label() == "Assign denser material");
      REQUIRE(
          session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
      CHECK(geometry::OcctShapeView::get(*session.part_shape_cache()->find_body_shape(
                BodyId("body.base"))) == geometry::OcctShapeView::get(shape));
      const auto reweighted =
          migration_checked(geometry::BodyResultInspector{}.inspect_physical_properties(
              *session.part_document(), integrated));
      CHECK(reweighted.data().geometry_reused);
      CHECK(reweighted.data().complete_mass()->mass_kg == Catch::Approx(.0096));
      REQUIRE(session.undo());
      CHECK(part_snapshot(session) == before);
      CHECK_FALSE(session.dirty());
      REQUIRE(session.redo());
      CHECK(part_snapshot(session) == after);
      REQUIRE(session.save());
      GuiDocumentSession reopened;
      REQUIRE(reopened.open_file(path));
      CHECK(part_snapshot(reopened) == after);
      CHECK(reopened.derived_results_fresh());
      REQUIRE(session.commit_part_transaction("Change depth", [](PartDocument& p) {
        const auto changed = p.set_parameter_value(ParameterId("depth"),
                                                   resources::checked(Quantity::length_mm(10)));
        return changed ? Result<std::size_t>::success(changed.value().size())
                       : Result<std::size_t>::failure(changed.error());
      }));
      CHECK_FALSE(geometry::OcctShapeView::get(*session.part_shape_cache()->find_body_shape(
                      BodyId("body.base"))) == geometry::OcctShapeView::get(shape));
      CHECK_FALSE(reweighted.validate_current(*session.part_document()));
    }
  }
}

TEST_CASE("Material-only Project transactions reuse selected and untouched Part caches atomically",
          "[gui][document-session][parity-b150][physical-reuse-session]") {
  QTemporaryDir directory;
  GuiDocumentSession session;
  REQUIRE(session.create_project(DocumentId("physical.project"), "Physical"));
  REQUIRE(session.commit_project_transaction("Seed members", [](Project& p) {
    for (const auto& id : {"first", "second"}) {
      auto member = resource_member(id);
      auto authored = member.apply_history_intent({{"base"}, {}});
      if (!authored)
        return authored;
      auto added = p.add_part_document(std::move(member));
      if (!added)
        return added;
    }
    return Result<std::size_t>::success(2);
  }));
  REQUIRE(session.save_as(directory.filePath("physical.blcad.project.json").toStdString()));
  const auto before = project_snapshot(session);
  REQUIRE(session.project_part_shape_cache(DocumentId("first")));
  REQUIRE(session.project_part_shape_cache(DocumentId("second")));
  REQUIRE(session.derived_results_fresh());
  for (const auto& id : {"first", "second"}) {
    const auto* old = session.project()->find_part_document(DocumentId(id));
    REQUIRE(
        session.project_part_shape_cache(DocumentId(id))->validate_complete_geometry_result(*old));
  }
  const auto generation =
      session.project()->find_part_document(DocumentId("first"))->executable_generation();
  const auto first =
      *session.project_part_shape_cache(DocumentId("first"))->find_body_shape(BodyId("body.base"));
  const auto second =
      *session.project_part_shape_cache(DocumentId("second"))->find_body_shape(BodyId("body.base"));
  REQUIRE(session.commit_project_transaction("Adopt material", [](Project& p) {
    return p.adopt_engineering_resource_revision(resources::key(), resources::material("r3", 7000),
                                                 {DocumentId("first")});
  }));
  const auto after = project_snapshot(session);
  CHECK(session.derived_results_fresh());
  CHECK(session.project()->find_part_document(DocumentId("first"))->executable_generation() ==
        generation);
  CHECK(session.project()->find_part_document(DocumentId("second"))->executable_generation() ==
        generation);
  CHECK(geometry::OcctShapeView::get(*session.project_part_shape_cache(DocumentId("first"))
                                          ->find_body_shape(BodyId("body.base"))) ==
        geometry::OcctShapeView::get(first));
  CHECK(geometry::OcctShapeView::get(*session.project_part_shape_cache(DocumentId("second"))
                                          ->find_body_shape(BodyId("body.base"))) ==
        geometry::OcctShapeView::get(second));
  for (const auto& id : {"first", "second"})
    REQUIRE(session.project_part_shape_cache(DocumentId(id))
                ->validate_complete_geometry_result(
                    *session.project()->find_part_document(DocumentId(id))));
  CHECK_FALSE(session.commit_project_transaction("Late failed adoption", [](Project& p) {
    return p.adopt_engineering_resource_revision(resources::key("synthetic.material", "r3"),
                                                 resources::material("r4", 8000),
                                                 {DocumentId("first"), DocumentId("missing")});
  }));
  CHECK(project_snapshot(session) == after);
  CHECK(session.derived_results_fresh());
  CHECK(session.undo_label() == "Adopt material");
  REQUIRE(session.undo());
  CHECK(project_snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(project_snapshot(session) == after);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  CHECK(project_snapshot(reopened) == after);
  CHECK(reopened.derived_results_fresh());
}

TEST_CASE(
    "Material changes inside End of Part retain partial scope outer Save and one accepted Undo",
    "[gui][document-session][parity-b150][physical-reuse-session]") {
  QTemporaryDir directory;
  auto part = resource_member("physical.prefix");
  REQUIRE(part.apply_history_intent({{"base"}, {}}));
  const auto path = directory.filePath("physical.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(part, path));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path));
  const auto original = part_snapshot(session);
  const auto saved = read_file(path);
  const auto assign = [](PartDocument& p) {
    return p.set_default_material(resources::key("synthetic.material", "r2"));
  };
  REQUIRE(session.begin_part_history_context(1));
  REQUIRE(session.commit_part_transaction("Local material", assign));
  CHECK_FALSE(session.derived_results_fresh());
  CHECK(session.history_prefix_ready());
  CHECK_FALSE(
      session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  REQUIRE(session.save());
  CHECK(read_file(path) == saved);
  session.cancel_part_workflow();
  CHECK(part_snapshot(session) == original);
  REQUIRE(session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  REQUIRE(session.begin_part_history_context(1));
  REQUIRE(session.commit_part_transaction("Local material", assign));
  REQUIRE(session.publish_part_workflow("Apply material at End of Part"));
  CHECK(session.derived_results_fresh());
  REQUIRE(session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  CHECK(session.undo_label() == "Apply material at End of Part");
  const auto accepted = part_snapshot(session);
  const auto physical = migration_checked(
      geometry::BodyResultInspector{}.inspect_physical_properties(*session.part_document()));
  CHECK(physical.data().complete_mass()->mass_kg == Catch::Approx(.0096));
  REQUIRE(session.undo());
  CHECK(part_snapshot(session) == original);
  REQUIRE(session.redo());
  CHECK(part_snapshot(session) == accepted);
}
