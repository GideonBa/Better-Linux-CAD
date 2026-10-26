#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_assembly_workbench.hpp"
#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
void write(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::WriteOnly));
  REQUIRE(file.write(bytes) == bytes.size());
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
std::string snapshot(const GuiDocumentSession& session) {
  return session.part_document()
             ? checked(serialize_part_document_to_json(*session.part_document()))
             : checked(serialize_project_to_json(*session.project()));
}
void create(GuiDocumentSession& session, bool project) {
  if (project)
    REQUIRE(session.create_project(DocumentId("project.source"), "Source project"));
  else
    REQUIRE(session.create_part(DocumentId("part.source"), "Source part"));
}
QString filename(bool project) {
  return project ? "model.blcad.project.json" : "model.blcad.json";
}
StepImportSource register_source(GuiDocumentSession& session, const QTemporaryDir& directory,
                                 const char* path = "source.step") {
  write(directory.filePath(path), "ISO-10303-21;\nsource A\nEND-ISO-10303-21;\n");
  const GuiStepImportWorkbench workbench;
  auto preview =
      checked(workbench.prepare_register(session, "Plate", path, StepImportMode::Reference));
  REQUIRE(workbench.apply(session, preview));
  return preview.source;
}
} // namespace

TEST_CASE("STEP source tasks require Save and register exact portable intent with atomic history",
          "[gui][step-import-workbench][step-import-source]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const GuiStepImportWorkbench workbench;
  CHECK_FALSE(
      workbench.prepare_register(session, "Source", "source.step", StepImportMode::Reference));
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto before = snapshot(session);
  write(directory.filePath("source.step"), "source A");
  auto preview = checked(
      workbench.prepare_register(session, "Plate", "./source.step", StepImportMode::EditableBody));
  CHECK(preview.source.project_relative_asset_path() == "source.step");
  CHECK(preview.source.mode() == StepImportMode::EditableBody);
  CHECK(preview.report.state() == StepImportFreshness::Available);
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  REQUIRE(session.save());
  REQUIRE(workbench.apply(session, preview));
  const auto after = snapshot(session);
  CHECK(after != before);
  CHECK(session.undo_label() == "Register STEP source");
  CHECK_FALSE(
      workbench.prepare_register(session, "Duplicate", "source.step", StepImportMode::Reference));
  auto statuses = checked(workbench.inspect_sources(session));
  REQUIRE(statuses.size() == 1);
  CHECK(statuses.front().report.state() == StepImportFreshness::Available);
  CHECK_FALSE(statuses.front().embedded_part);
  auto browser = GuiDocumentBrowser::build(session);
  const auto& owner_id = statuses.front().owner_document.value();
  const auto& source_id = preview.source.id().value();
  const auto id = std::string("step-source/") + (project ? "project/" : "part/") +
                  std::to_string(owner_id.size()) + ":" + owner_id + "/" +
                  std::to_string(source_id.size()) + ":" + source_id;
  const auto* node = browser.find(id);
  REQUIRE(node);
  CHECK(node->kind == GuiBrowserNodeKind::ExternalSource);
  CHECK(node->viewport_semantic_id.empty());
  for (const auto& property : node->properties)
    CHECK_FALSE(property.editable);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  CHECK(snapshot(reopened) == after);
  auto unchanged = checked(workbench.prepare_refresh(reopened, preview.source.id()));
  CHECK(checked(workbench.apply(reopened, unchanged)) == 0);
  CHECK_FALSE(reopened.can_undo());
  const auto removal = checked(workbench.prepare_remove(reopened, preview.source.id()));
  REQUIRE(workbench.apply(reopened, removal));
  REQUIRE(checked(workbench.inspect_sources(reopened)).empty());
  REQUIRE(reopened.undo());
  REQUIRE(reopened.can_redo());
  const auto before_refresh = snapshot(reopened);
  unchanged = checked(workbench.prepare_refresh(reopened, preview.source.id()));
  CHECK(checked(workbench.apply(reopened, unchanged)) == 0);
  CHECK(snapshot(reopened) == before_refresh);
  REQUIRE(reopened.can_redo());
  REQUIRE(reopened.redo());
  CHECK(checked(workbench.inspect_sources(reopened)).empty());
  CHECK(bytes(directory.filePath("source.step")) == "source A");
}

TEST_CASE(
    "STEP source refresh rereads bytes rejects stale intent and accepts only inspected digest",
    "[gui][step-import-workbench][step-import-freshness]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, directory);
  const GuiStepImportWorkbench workbench;
  const auto before = snapshot(session);
  write(directory.filePath("source.step"), "source B");
  auto preview = checked(workbench.prepare_refresh(session, source.id()));
  CHECK(preview.report.state() == StepImportFreshness::Changed);
  CHECK(snapshot(session) == before);
  write(directory.filePath("source.step"), "source C");
  CHECK_FALSE(workbench.apply(session, preview));
  CHECK(snapshot(session) == before);
  preview = checked(workbench.prepare_refresh(session, source.id()));
  REQUIRE(workbench.apply(session, preview));
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Available);
  const auto refreshed = snapshot(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Changed);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == refreshed);
  auto removal = checked(workbench.prepare_remove(session, source.id()));
  register_source(session, directory, "second.step");
  CHECK_FALSE(workbench.apply(session, removal));
  auto forged = checked(workbench.prepare_remove(session, source.id()));
  forged.source =
      checked(StepImportSource::create(source.id(), "Forged", "source.step",
                                       forged.source.sha256_digest(), StepImportMode::Reference));
  CHECK_FALSE(workbench.apply(session, forged));
  CHECK(bytes(directory.filePath("source.step")) == "source C");
}

TEST_CASE("Missing and unreadable STEP metadata stays openable for explicit recovery",
          "[gui][step-import-workbench][step-import-freshness]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, directory);
  REQUIRE(session.save());
  REQUIRE(QFile::remove(directory.filePath("source.step")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  const auto before = snapshot(reopened);
  const GuiStepImportWorkbench workbench;
  CHECK(reopened.derived_results_fresh());
  CHECK(checked(workbench.inspect(reopened, source.id())).state() == StepImportFreshness::Missing);
  REQUIRE_FALSE(reopened.diagnostics().empty());
  CHECK(reopened.diagnostics().back().severity == GuiDiagnosticSeverity::Warning);
  CHECK_FALSE(workbench.prepare_refresh(reopened, source.id()));
  REQUIRE(reopened.save());
  auto missing = checked(workbench.prepare_remove(reopened, source.id()));
  REQUIRE(QDir().mkdir(directory.filePath("source.step")));
  CHECK_FALSE(workbench.apply(reopened, missing));
  CHECK(snapshot(reopened) == before);
  CHECK(checked(workbench.inspect(reopened, source.id())).state() ==
        StepImportFreshness::Unreadable);
  auto unreadable = checked(workbench.prepare_remove(reopened, source.id()));
  REQUIRE(workbench.apply(reopened, unreadable));
  CHECK(checked(workbench.inspect_sources(reopened)).empty());
  REQUIRE(reopened.undo());
  CHECK(snapshot(reopened) == before);
  REQUIRE(QDir().rmdir(directory.filePath("source.step")));
  write(directory.filePath("source.step"), "Recovered source");
  auto refresh = checked(workbench.prepare_refresh(reopened, source.id()));
  REQUIRE(workbench.apply(reopened, refresh));
  CHECK(checked(workbench.inspect(reopened, source.id())).state() ==
        StepImportFreshness::Available);
}

TEST_CASE("Save As requires matching relocated assets and invalidates old source previews",
          "[gui][step-import-workbench][step-import-json]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir original, moved;
  REQUIRE(session.save_as(original.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, original);
  REQUIRE(session.save());
  const auto old_path = session.file_path();
  const auto before = snapshot(session);
  const GuiStepImportWorkbench workbench;
  const auto preview = checked(workbench.prepare_remove(session, source.id()));
  const auto target = moved.filePath(filename(project));
  CHECK_FALSE(session.save_as(target.toStdString()));
  CHECK_FALSE(QFile::exists(target));
  CHECK(session.file_path() == old_path);
  write(moved.filePath("source.step"), "Wrong source");
  CHECK_FALSE(session.save_as(target.toStdString()));
  CHECK(snapshot(session) == before);
  write(moved.filePath("source.step"), bytes(original.filePath("source.step")));
  REQUIRE(session.save_as(target.toStdString()));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(workbench.apply(session, preview));
  CHECK_FALSE(session.dirty());
  REQUIRE(QFile::remove(original.filePath("source.step")));
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Available);
}

TEST_CASE("Document and STEP export writers preserve source assets and aliases",
          "[gui][step-import-workbench][step-import-source]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  const auto document = directory.filePath("model.blcad.json");
  REQUIRE(session.save_as(document.toStdString()));
  const GuiStepImportWorkbench workbench;
  CHECK_FALSE(
      workbench.prepare_register(session, "Self", "model.blcad.json", StepImportMode::Reference));
  register_source(session, directory);
  const auto content = bytes(directory.filePath("source.step"));
  std::filesystem::create_hard_link(directory.filePath("source.step").toStdString(),
                                    directory.filePath("alias.step").toStdString());
  for (const auto* name : {"source.step", "alias.step"}) {
    const auto path = directory.filePath(name).toStdString();
    CHECK_FALSE(session.save_as(path));
    CHECK_FALSE(GuiAnalysisExportWorkbench{}.preflight_step_export(
        session, {GuiStepExportMode::PartMultiBody, path}));
    CHECK(bytes(directory.filePath(name)) == content);
  }
  REQUIRE(QFile::remove(directory.filePath("source.step")));
  CHECK_FALSE(session.save_as(directory.filePath("source.step").toStdString()));
  CHECK_FALSE(QFile::exists(directory.filePath("source.step")));
  write(directory.filePath("other.blcad.json.sketch-constraints.json"), "Sidecar source");
  auto sidecar = checked(workbench.prepare_register(
      session, "Sidecar", "other.blcad.json.sketch-constraints.json", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, sidecar));
  CHECK_FALSE(session.save_as(directory.filePath("other.blcad.json").toStdString()));
  CHECK(bytes(directory.filePath("other.blcad.json.sketch-constraints.json")) == "Sidecar source");
}

TEST_CASE("Assembly definition import verifies embedded source roots and protects their assets",
          "[gui][step-import-workbench][step-import-source]") {
  GuiDocumentSession part;
  create(part, false);
  QTemporaryDir original, target;
  REQUIRE(part.save_as(original.filePath("part.blcad.json").toStdString()));
  register_source(part, original);
  GuiDocumentSession project;
  create(project, true);
  const GuiAssemblyWorkbench assembly;
  CHECK_FALSE(assembly.add_part(project, *part.part_document()));
  REQUIRE(project.save_as(target.filePath("model.blcad.project.json").toStdString()));
  CHECK_FALSE(assembly.add_part(project, *part.part_document()));
  write(target.filePath("source.step"), bytes(original.filePath("source.step")));
  REQUIRE(assembly.add_part(project, *part.part_document()));
  const auto inspected = checked(GuiStepImportWorkbench{}.inspect_sources(project));
  REQUIRE(inspected.size() == 1);
  CHECK(inspected.front().owner_document == part.part_document()->id());
  CHECK(inspected.front().embedded_part);
  CHECK_FALSE(
      validate_step_import_output_path(project, target.filePath("source.step").toStdString()));
  const auto browser = GuiDocumentBrowser::build(project);
  const auto& id = inspected.front().report.source().id().value();
  REQUIRE(browser.find("step-source/part/11:part.source/" + std::to_string(id.size()) + ":" + id));
}

TEST_CASE("STEP source presentation distinguishes registry kinds and delimited persistent ids",
          "[gui][step-import-workbench][step-import-source]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  auto project =
      checked(Project::create(DocumentId("a"), "Root project",
                              checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  const auto add_source = [&](auto& owner, const char* id, const char* path, const char* name) {
    write(directory.filePath(path), name);
    REQUIRE(owner.add_step_import_source(checked(
        StepImportSource::create(StepImportSourceId(id), name, path,
                                 checked(step_import_sha256(name)), StepImportMode::Reference))));
  };
  add_source(project, "b/c", "root.step", "Project source");
  auto same_id_part = checked(PartDocument::create(DocumentId("a"), "Same-id Part"));
  add_source(same_id_part, "b/c", "part.step", "Part source");
  REQUIRE(project.add_part_document(std::move(same_id_part)));
  auto slashed_part = checked(PartDocument::create(DocumentId("a/b"), "Slashed Part"));
  add_source(slashed_part, "c", "slashed.step", "Slashed source");
  REQUIRE(project.add_part_document(std::move(slashed_part)));
  const auto serialized = checked(serialize_project_to_json(project));
  const auto path = directory.filePath("model.blcad.project.json");
  write(path, QByteArray::fromStdString(serialized));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path.toStdString()));
  const auto before = snapshot(session);
  const auto statuses = checked(GuiStepImportWorkbench{}.inspect_sources(session));
  REQUIRE(statuses.size() == 3);
  CHECK(statuses[0].owner_document == DocumentId("a"));
  CHECK_FALSE(statuses[0].embedded_part);
  CHECK(statuses[1].owner_document == DocumentId("a"));
  CHECK(statuses[1].embedded_part);
  CHECK(statuses[2].owner_document == DocumentId("a/b"));
  CHECK(statuses[2].embedded_part);
  for (const auto& status : statuses)
    CHECK(status.report.state() == StepImportFreshness::Available);
  const auto browser = GuiDocumentBrowser::build(session);
  const auto check_node = [&](const char* semantic_id, const char* owner, const char* source_id,
                              const char* name, const char* asset_path) {
    const auto* node = browser.find(semantic_id);
    REQUIRE(node);
    CHECK(node->kind == GuiBrowserNodeKind::ExternalSource);
    CHECK(node->owner_document_id == owner);
    CHECK(node->label == name);
    CHECK_FALSE(node->selection_kind);
    CHECK(node->viewport_semantic_id.empty());
    std::size_t matched = 0;
    for (const auto& property : node->properties) {
      CHECK_FALSE(property.editable);
      if (property.key == "id") {
        CHECK(property.value == source_id);
        ++matched;
      }
      if (property.key == "path") {
        CHECK(property.value == asset_path);
        ++matched;
      }
    }
    CHECK(matched == 2);
  };
  check_node("step-source/project/1:a/3:b/c", "a", "b/c", "Project source", "root.step");
  check_node("step-source/part/1:a/3:b/c", "a", "b/c", "Part source", "part.step");
  check_node("step-source/part/3:a/b/1:c", "a/b", "c", "Slashed source", "slashed.step");
  CHECK(snapshot(session) == before);
  CHECK(before == serialized);
  CHECK_FALSE(session.dirty());
  CHECK_FALSE(session.can_undo());
}
