#include "blcad/gui/gui_document_session.hpp"

#include "blcad/core/datum_plane.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace blcad;
using namespace blcad::gui;

namespace {

std::filesystem::path temp_path(std::string_view name) {
  return std::filesystem::temp_directory_path() / std::string(name);
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
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
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
