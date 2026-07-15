#include "blcad/gui/gui_command_registry.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad::gui;

TEST_CASE("GUI document session derives command context from transient state",
          "[gui][command-state]") {
  GuiDocumentSession session;

  CHECK_FALSE(session.has_document());
  CHECK(session.workspace() == GuiWorkspace::Part);
  CHECK(session.command_context().selection_count == 0);

  REQUIRE(session.create_project(blcad::DocumentId("project.gearbox"), "gearbox"));
  CHECK(session.has_document());
  CHECK(session.document_kind() == GuiDocumentKind::Project);
  CHECK(session.display_name() == "gearbox");
  CHECK(session.workspace() == GuiWorkspace::Assembly);

  CHECK(session.selection().add({GuiSelectionKind::Component, "root/component:motor"}));
  CHECK_FALSE(session.selection().add({GuiSelectionKind::Component, "root/component:motor"}));
  CHECK_FALSE(session.selection().add({GuiSelectionKind::Face, ""}));
  CHECK(session.command_context().selection_count == 1);
  CHECK(session.command_context().has_selection_kind(GuiSelectionKind::Component));
  CHECK_FALSE(session.command_context().has_selection_kind(GuiSelectionKind::Face));

  session.set_derived_results_fresh(true);
  CHECK(session.command_context().derived_results_fresh);

  REQUIRE(session.task().begin("assembly.mate"));
  CHECK(session.command_context().task_active());
  CHECK_FALSE(session.set_workspace(GuiWorkspace::Inspect));
  CHECK_FALSE(session.close());

  REQUIRE(session.task().cancel());
  REQUIRE(session.set_workspace(GuiWorkspace::Inspect));
  REQUIRE(session.close(true));
  CHECK_FALSE(session.has_document());
  CHECK(session.selection().empty());
  CHECK_FALSE(session.derived_results_fresh());
}

TEST_CASE("GUI task lifecycle requires preview before apply", "[gui][command-state]") {
  GuiTaskState task;

  CHECK_FALSE(task.apply());
  CHECK_FALSE(task.cancel());
  CHECK_FALSE(task.begin(""));
  REQUIRE(task.begin("part.extrude"));
  CHECK(task.stage() == GuiTaskStage::CollectingSelection);
  CHECK(task.command_id() == "part.extrude");
  CHECK_FALSE(task.begin("part.revolve"));
  CHECK_FALSE(task.apply());

  REQUIRE(task.begin_parameter_editing());
  CHECK(task.stage() == GuiTaskStage::EditingParameters);
  REQUIRE(task.return_to_selection());
  CHECK(task.stage() == GuiTaskStage::CollectingSelection);
  REQUIRE(task.begin_parameter_editing());
  REQUIRE(task.show_preview());
  CHECK(task.stage() == GuiTaskStage::Preview);
  REQUIRE(task.return_to_editing());
  REQUIRE(task.show_preview());
  REQUIRE(task.apply());
  CHECK_FALSE(task.active());
  CHECK(task.command_id().empty());
}

TEST_CASE("GUI command registry rejects ambiguous definitions and derives enablement",
          "[gui][command-state]") {
  GuiCommandRegistry registry;
  const auto requires_part_and_idle = [](const GuiCommandContext& context) {
    return context.document_kind == GuiDocumentKind::Part && !context.task_active();
  };

  REQUIRE(registry.register_command({"part.extrude", "Extrude", requires_part_and_idle}));
  CHECK_FALSE(registry.register_command({"part.extrude", "Duplicate", requires_part_and_idle}));
  CHECK_FALSE(registry.register_command({"", "Missing id", requires_part_and_idle}));
  CHECK_FALSE(registry.register_command({"part.cut", "", requires_part_and_idle}));
  CHECK_FALSE(registry.register_command({"part.cut", "Cut", {}}));

  GuiCommandContext context;
  CHECK_FALSE(registry.is_enabled("part.extrude", context));
  CHECK_FALSE(registry.is_enabled("missing", context));

  context.document_kind = GuiDocumentKind::Part;
  CHECK(registry.is_enabled("part.extrude", context));
  context.task_stage = GuiTaskStage::Preview;
  CHECK_FALSE(registry.is_enabled("part.extrude", context));
  CHECK(registry.commands().size() == 1);
}
