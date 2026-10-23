#include "blcad/gui/gui_document_session.hpp"

#include "../support/repair_open_fixture.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/part_document_json.hpp"
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

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
} // namespace
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
  corrupt.replace(at, source.size(), "\"source_feature\": \"feature.absent\"");
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
