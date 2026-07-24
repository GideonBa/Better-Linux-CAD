#include "blcad/core/parameter.hpp"
#include "blcad/gui/shell/shell_origin.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <algorithm>

using namespace blcad;
using namespace blcad::gui;

namespace {

const GuiBrowserNode* find_group(const GuiDocumentBrowser& browser, std::string_view label) {
  for (const auto& root : browser.roots())
    for (const auto& child : root.children)
      if (child.kind == GuiBrowserNodeKind::Group && child.label == label)
        return &child;
  return nullptr;
}

QTreeWidgetItem* find_tree_item(QTreeWidgetItem* item, const QString& semantic_id) {
  if (item->data(0, Qt::UserRole).toString() == semantic_id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_tree_item(item->child(index), semantic_id))
      return found;
  return nullptr;
}

QTreeWidgetItem* find_tree_item(QTreeWidget& tree, const QString& semantic_id) {
  for (int index = 0; index < tree.topLevelItemCount(); ++index)
    if (auto* found = find_tree_item(tree.topLevelItem(index), semantic_id))
      return found;
  return nullptr;
}

} // namespace

TEST_CASE("Block 133 shell window builds ribbon browser and diagnostics",
          "[gui][shell][gui][ribbon]") {
  ShellWindow window;
  CHECK(window.ribbon().tab_count() == 3);
  CHECK(window.ribbon().tab_title(0) == QStringLiteral("3D-Modell"));
  CHECK(window.ribbon().tab_title(1) == QStringLiteral("Skizze"));
  CHECK(window.ribbon().tab_title(2) == QStringLiteral("Ansicht"));
  CHECK(window.ribbon().current_tab() == 0);
  CHECK_FALSE(window.ribbon().tab_enabled(1));
  CHECK_FALSE(window.session().has_document());
  CHECK(window.model_browser().topLevelItemCount() == 0);
}

TEST_CASE("Block 133 new part seeds origin planes axes and Ursprung folder",
          "[gui][shell][gui][origin]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Testteil")));
  const auto* part = window.session().part_document();
  REQUIRE(part != nullptr);
  CHECK(part->datum_plane_count() == 3);
  CHECK(part->datum_axis_count() == 3);
  CHECK(part->find_datum_plane(DatumPlaneId("datum.xz")) != nullptr);
  CHECK(part->find_datum_plane(DatumPlaneId("datum.yz")) != nullptr);
  CHECK(part->find_datum_axis(DatumAxisId("datum.axis.z")) != nullptr);

  const auto* origin = find_group(window.browser_model(), "Ursprung");
  REQUIRE(origin != nullptr);
  CHECK(origin->children.size() == 6U);

  // The viewport scene publishes all six origin presentations.
  CHECK(window.viewport().presentation_count() == 6U);

  // Seeding twice must fail closed (duplicate origin ids).
  CHECK(seed_origin_geometry(window.session()).has_error());
}

TEST_CASE("Block 133 selection flows viewport to session to browser and back",
          "[gui][shell][gui][selection-sync]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Auswahl")));

  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.session().selection().entries().size() == 1U);
  CHECK(window.session().selection().entries()[0].semantic_id == "datum-plane/datum.xy");
  auto* current = window.model_browser().currentItem();
  REQUIRE(current != nullptr);
  CHECK(current->data(0, Qt::UserRole + 1).toString() ==
        QStringLiteral("datum-plane/datum.xy"));

  window.viewport().clear_selection();
  CHECK(window.session().selection().entries().empty());
}

TEST_CASE("Block 133 sketch entry creates a sketch on the selected origin plane",
          "[gui][shell][gui][sketch-entry]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Skizzenteil")));

  // Without a datum-plane selection the command fails closed.
  CHECK_FALSE(window.create_sketch_on_selection());

  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xz"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.active_sketch().has_value());
  CHECK(window.active_sketch()->value() == "sketch.skizze1");
  const auto* part = window.session().part_document();
  REQUIRE(part != nullptr);
  REQUIRE(part->find_sketch(SketchId("sketch.skizze1")) != nullptr);
  CHECK(part->find_sketch(SketchId("sketch.skizze1"))->workplane() ==
        DatumPlaneId("datum.xz"));
  CHECK(window.ribbon().tab_enabled(1));
  CHECK(window.ribbon().current_tab() == 1);
  CHECK(window.viewport().sketch_focus_active());
  CHECK(window.viewport().sketch_interaction_active());
  CHECK(window.viewport().projection() == GuiViewportProjection::Orthographic);

  REQUIRE(window.finish_active_sketch());
  CHECK_FALSE(window.active_sketch().has_value());
  CHECK_FALSE(window.ribbon().tab_enabled(1));
  CHECK(window.ribbon().current_tab() == 0);
  CHECK_FALSE(window.viewport().sketch_focus_active());
  CHECK_FALSE(window.viewport().sketch_interaction_active());
}

TEST_CASE("Block 133 save and open round-trip preserves the seeded origin",
          "[gui][shell][gui][document-lifecycle]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("teil.blcad.json"));
  {
    ShellWindow window;
    REQUIRE(window.new_part(QStringLiteral("Speicherteil")));
    REQUIRE(window.save_to(path));
    CHECK_FALSE(window.session().dirty());
  }
  {
    ShellWindow window;
    REQUIRE(window.open_path(path));
    const auto* part = window.session().part_document();
    REQUIRE(part != nullptr);
    CHECK(part->datum_plane_count() == 3);
    CHECK(part->datum_axis_count() == 3);
    const auto* origin = find_group(window.browser_model(), "Ursprung");
    REQUIRE(origin != nullptr);
    CHECK(origin->children.size() == 6U);
    CHECK(window.viewport().presentation_count() == 6U);
  }
}

TEST_CASE("Double-clicking a Sketch node re-enters it for editing",
          "[gui][shell][gui][sketch-reentry]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Reentry")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.active_sketch().has_value());
  const std::string sketch_id = window.active_sketch()->value();
  REQUIRE(window.finish_active_sketch());
  CHECK_FALSE(window.active_sketch().has_value());

  auto& tree = window.model_browser();
  QTreeWidgetItem* item = find_tree_item(tree, QString::fromStdString(sketch_id));
  REQUIRE(item != nullptr);

  // Emitting the browser's double-click signal drives handle_tree_activation.
  tree.itemDoubleClicked(item, 0);
  REQUIRE(window.active_sketch().has_value());
  CHECK(window.active_sketch()->value() == sketch_id);
}

TEST_CASE("Property inspector shows and edits a parameter value",
          "[gui][shell][gui][property-editor]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Params")));
  REQUIRE(window.session().commit_part_transaction("Seed parameter", [](PartDocument& part) {
    auto parameter = Parameter::create_length(ParameterId("parameter.width"), "Width",
                                              Quantity::length_mm(20.0).value());
    if (parameter.has_error())
      return Result<std::size_t>::failure(parameter.error());
    return part.add_parameter(std::move(parameter.value()));
  }));
  window.refresh();

  auto& tree = window.model_browser();
  QTreeWidgetItem* item = find_tree_item(tree, QStringLiteral("parameter.width"));
  REQUIRE(item != nullptr);
  tree.setCurrentItem(item); // populates the inspector for the selected node

  auto& table = window.property_inspector();
  int value_row = -1;
  for (int row = 0; row < table.rowCount(); ++row) {
    const auto* cell = table.item(row, 1);
    if (cell != nullptr && cell->data(Qt::UserRole).toString() == QStringLiteral("value"))
      value_row = row;
  }
  REQUIRE(value_row >= 0);
  CHECK(table.item(value_row, 1)->text() == QStringLiteral("20"));

  // Editing the value cell commits through edit_browser_property.
  table.item(value_row, 1)->setText(QStringLiteral("35"));
  const auto* parameter =
      window.session().part_document()->find_parameter(ParameterId("parameter.width"));
  REQUIRE(parameter != nullptr);
  CHECK(parameter->value().millimeters() == 35.0);
}
