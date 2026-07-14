#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/main_window.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/body.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/parameter.hpp"

#include <QApplication>
#include <QTableWidget>
#include <QTreeWidget>

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;

namespace {

const GuiPropertyValue* find_property(const GuiBrowserNode& node, std::string_view key) {
  for (const auto& property : node.properties)
    if (property.key == key)
      return &property;
  return nullptr;
}

QTreeWidgetItem* find_item(QTreeWidgetItem* item, QStringView id) {
  if (item->data(0, Qt::UserRole).toString() == id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_item(item->child(index), id))
      return found;
  return nullptr;
}

} // namespace

TEST_CASE("Block 98 Part browser exposes deterministic groups and typed editable properties",
          "[gui][model-browser][gui][property-editor]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.browser"), "Browser Part"));
  REQUIRE(session.commit_part_transaction("Seed browser", [](PartDocument& part) {
    auto parameter = Parameter::create_length(ParameterId("parameter.width"), "Width",
                                               Quantity::length_mm(20.0).value());
    if (parameter.has_error()) return Result<std::size_t>::failure(parameter.error());
    if (auto added = part.add_parameter(std::move(parameter.value())); added.has_error()) return added;
    auto datum = DatumPlane::xy();
    if (datum.has_error()) return Result<std::size_t>::failure(datum.error());
    if (auto added = part.add_datum_plane(std::move(datum.value())); added.has_error()) return added;
    auto body = Body::create(BodyId("body.main"), "Main Body", BodyKind::Solid);
    if (body.has_error()) return Result<std::size_t>::failure(body.error());
    return part.add_body(std::move(body.value()));
  }));

  const auto browser = GuiDocumentBrowser::build(session);
  REQUIRE(browser.roots().size() == 1);
  const auto& root = browser.roots().front();
  REQUIRE(root.children.size() == 9);
  CHECK(root.children[0].label == "Parameters");
  CHECK(root.children[1].label == "Datums / Workplanes");
  CHECK(root.children[6].label == "Bodies");
  CHECK(root.children[8].label == "Session diagnostics");

  const auto* parameter = browser.find("parameter.width");
  REQUIRE(parameter != nullptr);
  REQUIRE(find_property(*parameter, "value") != nullptr);
  CHECK(find_property(*parameter, "value")->kind == GuiPropertyKind::Quantity);
  CHECK(find_property(*parameter, "value")->editable);
  CHECK(find_property(*parameter, "id")->editable == false);

  const auto* body = browser.find("body.main");
  REQUIRE(body != nullptr);
  CHECK(body->selection_kind == GuiSelectionKind::Body);
  CHECK(find_property(*body, "active")->editable == false);
  CHECK_FALSE(edit_browser_property(session, *body, "visibility", "maybe", false));
  REQUIRE(edit_browser_property(session, *body, "visibility", "false", false));
  CHECK(session.part_document()->find_body(BodyId("body.main"))->visibility() == BodyVisibility::Visible);
  REQUIRE(edit_browser_property(session, *body, "visibility", "false", true));
  CHECK(session.part_document()->find_body(BodyId("body.main"))->visibility() == BodyVisibility::Hidden);
  CHECK(session.can_undo());

  const auto updated_browser = GuiDocumentBrowser::build(session);
  const auto* updated_parameter = updated_browser.find("parameter.width");
  REQUIRE(updated_parameter != nullptr);
  REQUIRE(edit_browser_property(session, *updated_parameter, "value", "32.5", true));
  CHECK(session.part_document()->find_parameter(ParameterId("parameter.width"))->value().millimeters() == 32.5);
}

TEST_CASE("Block 98 Assembly browser edits occurrence state through Project transactions",
          "[gui][model-browser][gui][property-editor]") {
  GuiDocumentSession session;
  REQUIRE(session.create_project(DocumentId("project.browser"), "Assembly Browser"));
  REQUIRE(session.commit_project_transaction("Seed occurrence", [](Project& project) {
    auto part = PartDocument::create(DocumentId("part.member"), "Member");
    if (part.has_error()) return Result<std::size_t>::failure(part.error());
    if (auto added = project.add_part_document(std::move(part.value())); added.has_error()) return added;
    if (auto added = project.assembly().add_member_part(DocumentId("part.member")); added.has_error()) return added;
    auto component = ComponentInstance::create(ComponentInstanceId("component.member"), "Member Occurrence",
                                               DocumentId("part.member"));
    if (component.has_error()) return Result<std::size_t>::failure(component.error());
    return project.assembly().add_component_instance(std::move(component.value()));
  }));

  const auto browser = GuiDocumentBrowser::build(session);
  const auto* component = browser.find("component.member");
  REQUIRE(component != nullptr);
  CHECK(component->selection_kind == GuiSelectionKind::Component);
  REQUIRE(edit_browser_property(session, *component, "suppression", "suppressed", true));
  CHECK(session.project()->assembly()
            .find_component_instance(ComponentInstanceId("component.member"))
            ->suppression_state() == ComponentSuppressionState::Suppressed);
  REQUIRE(edit_browser_property(session, *component, "grounding", "grounded", true));
  CHECK(session.project()->assembly()
            .find_component_instance(ComponentInstanceId("component.member"))
            ->grounding_state() == ComponentGroundingState::Grounded);
}

TEST_CASE("Block 98 tree selection updates semantic session selection and property table",
          "[gui][selection-sync][gui][model-browser]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  REQUIRE(window.session().create_part(DocumentId("part.sync"), "Selection Sync"));
  REQUIRE(window.session().commit_part_transaction("Add selectable body", [](PartDocument& part) {
    auto body = Body::create(BodyId("body.sync"), "Sync Body", BodyKind::Solid);
    if (body.has_error()) return Result<std::size_t>::failure(body.error());
    if (auto added = part.add_body(std::move(body.value())); added.has_error()) return added;
    auto datum = DatumPlane::xy();
    if (datum.has_error()) return Result<std::size_t>::failure(datum.error());
    return part.add_datum_plane(std::move(datum.value()));
  }));
  window.refresh_command_state();

  auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("blcad.model_browser"));
  auto* table = window.findChild<QTableWidget*>(QStringLiteral("blcad.property_table"));
  REQUIRE(tree != nullptr);
  REQUIRE(table != nullptr);
  QTreeWidgetItem* body_item = nullptr;
  for (int index = 0; index < tree->topLevelItemCount() && !body_item; ++index)
    body_item = find_item(tree->topLevelItem(index), u"body.sync");
  REQUIRE(body_item != nullptr);
  tree->setCurrentItem(body_item);
  qApp->processEvents();

  CHECK(window.session().selection().contains(GuiSelectionKind::Body, "body/body.sync"));
  CHECK(table->rowCount() >= 8);

  auto* viewport = window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"));
  REQUIRE(viewport != nullptr);
  REQUIRE(viewport->select_semantic("datum-plane/datum.xy"));
  qApp->processEvents();
  REQUIRE(tree->currentItem() != nullptr);
  CHECK(tree->currentItem()->data(0, Qt::UserRole).toString() == QStringLiteral("datum.xy"));
  CHECK(window.session().selection().contains(GuiSelectionKind::Datum, "datum-plane/datum.xy"));
}
