#include "blcad/gui/gui_document_browser.hpp"

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/body.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/parameter.hpp"


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
  // datum.xy is an origin id and lands in the MVP-9R "Ursprung" folder that
  // precedes "Datums / Workplanes".
  REQUIRE(root.children.size() == 10);
  CHECK(root.children[0].label == "Parameters");
  CHECK(root.children[1].label == "Ursprung");
  CHECK(root.children[2].label == "Datums / Workplanes");
  CHECK(root.children[7].label == "Bodies");
  CHECK(root.children[9].label == "Session diagnostics");

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

