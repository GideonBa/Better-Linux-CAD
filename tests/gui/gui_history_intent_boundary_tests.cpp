#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_measure.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <QApplication>

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
    "Effective history transactions update native scenes Measure persistence and Undo together",
    "[gui][parity-b147][history-evaluation-boundary][document-session]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("native.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(fixture::native_base(), path));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path));
  const auto before = snapshot(session);
  GuiMeasureController measure;
  REQUIRE(measure.begin(session, GuiMeasureKind::BodySummary));
  REQUIRE(measure.select_body(BodyId("body.base")));
  CHECK(checked(measure.preview(session)).volume_mm3 == Catch::Approx(1600));
  OcctViewport viewport;
  viewport.resize(800, 600);
  viewport.show();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(viewport.native_viewer_available());
  const auto update_scene = [&] {
    REQUIRE(session.part_shape_cache());
    REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
    REQUIRE(viewport.set_scene(checked(geometry::ViewportSceneBuilder{}.build_part(
        *session.part_document(), *session.part_shape_cache()))));
    qApp->processEvents();
  };
  update_scene();
  REQUIRE(viewport.select_semantic("body/body.base"));
  REQUIRE(session.commit_part_transaction("Suppress base", [](PartDocument& part) {
    return part.apply_history_intent({{"base"}, {"base"}});
  }));
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
  CHECK(session.undo_label() == "Suppress base");
  CHECK(session.dirty());
  REQUIRE(session.part_shape_cache());
  CHECK(session.part_shape_cache()->body_shape_count() == 0);
  CHECK(session.part_shape_cache()->feature_shape_count() == 0);
  CHECK_FALSE(measure.preview(session));
  update_scene();
  CHECK_FALSE(viewport.select_semantic("body/body.base"));
  CHECK_FALSE(viewport.selected_semantic());
  const GuiStepExportRequest export_request{GuiStepExportMode::PartMultiBody,
                                            directory.filePath("empty.step").toStdString()};
  CHECK_FALSE(GuiAnalysisExportWorkbench{}.preflight_step_export(session, export_request));
  REQUIRE(session.save());
  const auto suppressed = snapshot(session);
  CHECK(checked(serialize_part_document_to_json(checked(read_part_document_json_file(path)))) ==
        suppressed);
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  REQUIRE(reopened.part_shape_cache());
  REQUIRE(reopened.part_shape_cache()->validate_complete_history_result(*reopened.part_document()));
  CHECK(reopened.part_shape_cache()->body_shape_count() == 0);
  CHECK(reopened.recompute_status() == GuiRecomputeStatus::Fresh);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  CHECK(checked(measure.preview(session)).volume_mm3 == Catch::Approx(1600));
  update_scene();
  REQUIRE(viewport.select_semantic("body/body.base"));
  REQUIRE(session.redo());
  CHECK(snapshot(session) == suppressed);
  CHECK_FALSE(session.dirty());
  CHECK_FALSE(measure.preview(session));
  update_scene();
  CHECK_FALSE(viewport.select_semantic("body/body.base"));
}

TEST_CASE("Invalid restored suffixes preserve committed history geometry file and undo state",
          "[gui][parity-b147][history-evaluation-boundary][document-session]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("history.blcad.json").toStdString();
  auto part = fixture::native_base();
  REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Fillet", BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
  REQUIRE(part.set_parameter_value(ParameterId("radius"), checked(Quantity::length_mm(30))));
  REQUIRE(part.apply_history_intent({{"base", "fillet"}, {"fillet"}}));
  REQUIRE(write_part_document_json_file(part, path));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path));
  const auto before = snapshot(session);
  const std::string undo(session.undo_label());
  const std::string redo(session.redo_label());
  CHECK_FALSE(
      session.commit_part_transaction("Restore invalid fillet", [](PartDocument& candidate) {
        return candidate.apply_history_intent({{"base", "fillet"}, {}});
      }));
  CHECK(snapshot(session) == before);
  CHECK(session.file_path() == std::filesystem::path(path));
  CHECK(session.undo_label() == undo);
  CHECK(session.redo_label() == redo);
  CHECK_FALSE(session.dirty());
  REQUIRE(session.part_shape_cache());
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  CHECK(session.part_shape_cache()->find_feature_shape(FeatureId("fillet")) == nullptr);
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*session.part_shape_cache()->find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(1600));
  CHECK(checked(serialize_part_document_to_json(checked(read_part_document_json_file(path)))) ==
        before);
  REQUIRE(
      session.commit_part_transaction("Correct and restore fillet", [](PartDocument& candidate) {
        const auto changed =
            candidate.set_parameter_value(ParameterId("radius"), checked(Quantity::length_mm(1)));
        if (!changed)
          return Result<std::size_t>::failure(changed.error());
        return candidate.apply_history_intent({{"base", "fillet"}, {}});
      }));
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  REQUIRE(session.part_shape_cache()->find_feature_shape(FeatureId("fillet")));
  CHECK(session.undo_label() == "Correct and restore fillet");
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  CHECK_FALSE(session.can_undo());
  REQUIRE(session.redo());
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  REQUIRE(session.part_shape_cache()->find_feature_shape(FeatureId("fillet")));
}

TEST_CASE("Repair opening retains failed active intent until explicit suppression succeeds",
          "[gui][parity-b147][history-evaluation-boundary][document-session]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto native_path = directory.filePath("native.blcad.json").toStdString();
  const auto invalid_path = directory.filePath("invalid.blcad.json").toStdString();
  auto part = fixture::native_base();
  REQUIRE(write_part_document_json_file(part, native_path));
  REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Fillet", BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
  REQUIRE(part.set_parameter_value(ParameterId("radius"), checked(Quantity::length_mm(30))));
  REQUIRE(part.apply_history_intent({{"base", "fillet"}, {}}));
  REQUIRE(write_part_document_json_file(part, invalid_path));
  const auto persisted = checked(serialize_part_document_to_json(part));
  GuiDocumentSession session;
  REQUIRE(session.open_file(native_path));
  const auto before = snapshot(session);
  CHECK_FALSE(session.open_file(invalid_path));
  CHECK(snapshot(session) == before);
  CHECK(session.file_path() == std::filesystem::path(native_path));
  REQUIRE(session.open_file(invalid_path, true, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(snapshot(session) == persisted);
  CHECK(session.reference_repair_mode());
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK(session.part_shape_cache() == nullptr);
  REQUIRE(session.commit_part_repair_transaction(
      "Suppress invalid fillet", [](PartDocument& candidate) {
        return candidate.apply_history_intent({{"base", "fillet"}, {"fillet"}});
      }));
  CHECK_FALSE(session.reference_repair_mode());
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
  REQUIRE(session.part_shape_cache());
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  CHECK(session.part_shape_cache()->find_feature_shape(FeatureId("fillet")) == nullptr);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == persisted);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK(session.part_shape_cache() == nullptr);
  REQUIRE(session.redo());
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
  REQUIRE(session.part_shape_cache());
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
}
