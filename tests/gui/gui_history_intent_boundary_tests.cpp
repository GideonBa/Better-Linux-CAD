#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace fixture = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
std::string snapshot(const GuiDocumentSession& session) {
  REQUIRE(session.part_document());
  return checked(serialize_part_document_to_json(*session.part_document()));
}
} // namespace

TEST_CASE(
    "Unsupported history intent cannot publish a GUI transaction or retain a false fresh cache",
    "[gui][parity-b146][history-evaluation-boundary][document-session]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("native.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(fixture::native_base(), path));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path));
  const auto before = snapshot(session);
  const std::string undo(session.undo_label());
  const std::string redo(session.redo_label());
  const auto dirty = session.dirty();
  CHECK_FALSE(session.commit_part_transaction("Suppress base", [](PartDocument& part) {
    return part.apply_history_intent({{"base"}, {"base"}});
  }));
  CHECK(snapshot(session) == before);
  CHECK(session.file_path() == std::filesystem::path(path));
  CHECK(session.undo_label() == undo);
  CHECK(session.redo_label() == redo);
  CHECK(session.dirty() == dirty);
  REQUIRE(session.part_shape_cache());
  REQUIRE(session.part_shape_cache()->find_body_shape(BodyId("body.base")));
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*session.part_shape_cache()->find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(1600));
  CHECK(checked(read_part_document_json_file(path)).authored_history_intent() == std::nullopt);
}

TEST_CASE("Opening unsupported versioned history never silently activates its stored features",
          "[gui][parity-b146][history-evaluation-boundary][document-session]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto native = fixture::native_base();
  const auto old_path = directory.filePath("native.blcad.json").toStdString();
  const auto new_path = directory.filePath("history.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(native, old_path));
  const auto candidate = checked(native.with_history_intent({{"base"}, {"base"}}));
  REQUIRE(write_part_document_json_file(candidate, new_path));
  const auto persisted = checked(serialize_part_document_to_json(candidate));
  GuiDocumentSession session;
  REQUIRE(session.open_file(old_path));
  const auto before = snapshot(session);
  CHECK_FALSE(session.open_file(new_path));
  CHECK(snapshot(session) == before);
  CHECK(session.file_path() == std::filesystem::path(old_path));
  CHECK(checked(serialize_part_document_to_json(checked(read_part_document_json_file(new_path)))) ==
        persisted);
  // Repair mode may retain explicit failed intent, but it cannot return fresh Geometry.
  REQUIRE(session.open_file(new_path, true, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(snapshot(session) == persisted);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK(session.part_shape_cache() == nullptr);
}
