#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/gui_part_history.hpp"

#include <QTemporaryDir>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace fixture = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
void add_fillet(PartDocument& part) {
  REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Fillet", BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
}
void add_tool(PartDocument& part) {
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("tool"), "Tool", SketchId("profile"), ParameterId("radius")));
  REQUIRE(
      part.add_feature(checked(feature.with_body_result_context(fixture::new_body(part, "tool")))));
}
GuiDocumentSession open(const PartDocument& part, QTemporaryDir& directory) {
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("history.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(part, path));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path));
  return session;
}
std::string snapshot(const GuiDocumentSession& session) {
  return checked(serialize_part_document_to_json(*session.part_document()));
}
const GuiPropertyValue& property(const GuiBrowserNode& node, std::string_view key) {
  const auto found = std::find_if(node.properties.begin(), node.properties.end(),
                                  [&](const auto& value) { return value.key == key; });
  REQUIRE(found != node.properties.end());
  return *found;
}
} // namespace

TEST_CASE("History suppression requires an explicit reversible dependent closure before one commit",
          "[gui][parity-b148][history-controller]") {
  auto part = fixture::native_base();
  add_fillet(part);
  QTemporaryDir directory;
  auto session = open(part, directory);
  const auto before = snapshot(session);
  GuiPartHistoryController edit;
  REQUIRE(edit.begin(session));
  CHECK_FALSE(edit.apply(session, "Unchanged history"));
  CHECK_FALSE(session.can_undo());
  REQUIRE(edit.stage_suppression(session, "base", true));
  CHECK_FALSE(edit.preview(session));
  CHECK((checked(edit.blocked_dependents(session)) == std::vector<std::string>{"fillet"}));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  REQUIRE(edit.stage_suppression(session, "base", true, true));
  auto empty = checked(edit.preview(session));
  REQUIRE(empty.shapes.validate_complete_history_result(empty.document));
  CHECK(empty.shapes.body_shape_count() == 0);
  CHECK(snapshot(session) == before);
  REQUIRE(edit.stage_suppression(session, "base", true, false));
  CHECK_FALSE(edit.preview(session));
  CHECK(edit.intent().suppressed == std::vector<std::string>{"base"});
  REQUIRE(edit.stage_suppression(session, "base", true, true));
  REQUIRE(edit.apply(session, "Suppress base and dependents"));
  CHECK_FALSE(edit.active());
  CHECK(session.undo_label() == "Suppress base and dependents");
  CHECK(session.part_shape_cache()->body_shape_count() == 0);
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(session.file_path().string())))) == before);
  const auto after = snapshot(session);
  REQUIRE(session.save());
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  REQUIRE(edit.begin(session));
  REQUIRE(edit.stage_suppression(session, "base", false));
  auto restored = checked(edit.preview(session));
  CHECK(restored.document.authored_history_intent()->suppressed ==
        std::vector<std::string>{"fillet"});
  REQUIRE(restored.shapes.find_body_shape(BodyId("body.base")));
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*restored.shapes.find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(1600));
  edit.cancel();
  CHECK(snapshot(session) == after);
}

TEST_CASE("History reorder preserves independent geometry and rejects dependent or malformed order",
          "[gui][parity-b148][history-controller]") {
  auto part = fixture::native_base();
  add_fillet(part);
  add_tool(part);
  QTemporaryDir directory;
  auto session = open(part, directory);
  const auto before = snapshot(session);
  const auto volume =
      geometry::RectangleExtrusionAdapter{}
          .summarize(*session.part_shape_cache()->find_body_shape(BodyId("body.base")))
          .volume_mm3;
  GuiPartHistoryController edit;
  REQUIRE(edit.begin(session));
  REQUIRE(edit.stage_order(session, {"fillet", "base", "tool"}));
  CHECK_FALSE(edit.preview(session));
  CHECK_FALSE(edit.apply(session, "Illegal order"));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  REQUIRE(edit.stage_order(session, {"base", "base", "tool"}));
  CHECK_FALSE(edit.preview(session));
  REQUIRE(edit.stage_order(session, {"tool", "base", "fillet"}));
  auto preview = checked(edit.preview(session));
  CHECK(preview.shapes.body_shape_count() == 2);
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*preview.shapes.find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(volume));
  CHECK(snapshot(session) == before);
  REQUIRE(edit.apply(session, "Reorder features"));
  CHECK((session.part_document()->authored_history_intent()->order ==
         std::vector<std::string>{"tool", "base", "fillet"}));
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  const auto browser = GuiDocumentBrowser::build(session);
  const auto& root = browser.roots().front();
  const auto features = std::find_if(root.children.begin(), root.children.end(),
                                     [](const auto& node) { return node.label == "Features"; });
  REQUIRE(features != root.children.end());
  REQUIRE(features->children.size() == 3);
  CHECK(features->children[0].semantic_id == "tool");
  CHECK(features->children[1].semantic_id == "base");
  CHECK(property(features->children[2], "history_position").value == "3");
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
}

TEST_CASE(
    "History candidates reject stale source and retain failed suffix intent without publication",
    "[gui][parity-b148][history-controller]") {
  auto part = fixture::native_base();
  add_fillet(part);
  REQUIRE(part.set_parameter_value(ParameterId("radius"), checked(Quantity::length_mm(30))));
  REQUIRE(part.apply_history_intent({{"base", "fillet"}, {"fillet"}}));
  QTemporaryDir directory;
  auto session = open(part, directory);
  const auto before = snapshot(session);
  GuiPartHistoryController edit;
  REQUIRE(edit.begin(session));
  REQUIRE(edit.stage_suppression(session, "fillet", false));
  CHECK_FALSE(edit.preview(session));
  CHECK_FALSE(edit.apply(session, "Restore invalid fillet"));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  REQUIRE(session.part_shape_cache()->validate_complete_history_result(*session.part_document()));
  REQUIRE(session.commit_part_transaction("Change upstream value", [](PartDocument& candidate) {
    const auto changed =
        candidate.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(24)));
    return changed ? Result<std::size_t>::success(changed.value().size())
                   : Result<std::size_t>::failure(changed.error());
  }));
  const auto changed = snapshot(session);
  CHECK_FALSE(edit.current(session));
  CHECK_FALSE(edit.preview(session));
  CHECK_FALSE(edit.apply(session, "Stale restore"));
  CHECK(snapshot(session) == changed);
  CHECK(session.undo_label() == "Change upstream value");
}

TEST_CASE("History source ownership detects identical ID reuse and browser exposes absent creators",
          "[gui][parity-b148][history-controller]") {
  auto part = fixture::native_base();
  REQUIRE(part.apply_history_intent({{"base"}, {}}));
  QTemporaryDir directory;
  auto session = open(part, directory);
  GuiPartHistoryController edit;
  REQUIRE(edit.begin(session));
  const auto before = snapshot(session);
  const auto feature = *session.part_document()->find_feature(FeatureId("base"));
  REQUIRE(session.part_document()->remove_feature(feature.id()));
  REQUIRE(session.part_document()->add_feature(feature));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(edit.current(session));
  CHECK_FALSE(edit.apply(session, "Reused identity"));
  edit.cancel();
  REQUIRE(session.recompute());
  REQUIRE(edit.begin(session));
  REQUIRE(edit.stage_suppression(session, "base", true));
  REQUIRE(edit.apply(session, "Suppress base"));
  const auto browser = GuiDocumentBrowser::build(session);
  REQUIRE(browser.find("base"));
  CHECK(property(*browser.find("base"), "feature_state").value == "suppressed");
  CHECK(browser.find("base")->label.find("suppressed") != std::string::npos);
  REQUIRE(browser.find("body.base"));
  CHECK(property(*browser.find("body.base"), "history_availability").value == "unavailable");
  CHECK_FALSE(browser.find("body.base")->selection_kind);
  CHECK(browser.find("body.base")->viewport_semantic_id.empty());
}

TEST_CASE("The browser projects all typed history rows without claiming fresh Geometry",
          "[gui][parity-b148][history-controller][model-browser]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("placeholder"), "Placeholder"));
  auto part = fixture::all_families(true);
  const auto inventory = checked(part.executable_history());
  PartFeatureHistoryIntent intent;
  for (const auto& entry : inventory) {
    intent.order.push_back(entry.node_id);
    if (entry.family != PartFeatureFamily::ImportedBody)
      intent.suppressed.push_back(entry.node_id);
  }
  REQUIRE(part.apply_history_intent(intent));
  *session.part_document() = std::move(part);
  session.set_derived_results_fresh(false);
  const auto before = snapshot(session);
  const auto browser = GuiDocumentBrowser::build(session);
  const auto& groups = browser.roots().front().children;
  const auto features = std::find_if(groups.begin(), groups.end(),
                                     [](const auto& node) { return node.label == "Features"; });
  REQUIRE(features != groups.end());
  REQUIRE(features->children.size() == 15);
  for (std::size_t i = 0; i < inventory.size(); ++i) {
    CHECK(features->children[i].semantic_id == inventory[i].node_id);
    CHECK(property(features->children[i], "feature_state").value ==
          (inventory[i].family == PartFeatureFamily::ImportedBody ? "active" : "suppressed"));
  }
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.derived_results_fresh());
  GuiPartHistoryController edit;
  CHECK_FALSE(edit.begin(session));
}
