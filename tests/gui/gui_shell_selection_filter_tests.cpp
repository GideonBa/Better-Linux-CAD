#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_hole_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>

#include <cmath>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
using Catch::Approx;
namespace {
template <typename T> T& control(ShellWindow& window, const char* id) {
  auto* result = window.findChild<T*>(QString::fromLatin1(id));
  REQUIRE(result);
  return *result;
}
QAction& action(ShellWindow& window, const char* id) {
  return control<QAction>(window, (std::string("blcad.action.") + id).c_str());
}
void invoke(ShellWindow& window, const char* id) {
  auto& command = action(window, id);
  REQUIRE(command.isEnabled());
  command.trigger();
  qApp->processEvents();
}
QComboBox& filter(ShellWindow& window) {
  return control<QComboBox>(window, "blcad.selection.filter");
}
std::string snapshot(ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
std::string seed(ShellWindow& window) {
  REQUIRE(window.new_part("Selection"));
  const auto empty = snapshot(window);
  REQUIRE(
      window.session().commit_part_transaction("Create selection fixture", [](PartDocument& part) {
        for (const auto& [id, value] :
             {std::pair{"width", 30.0}, {"height", 20.0}, {"depth", 10.0}})
          REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
              ParameterId(id), id, reference_checked(Quantity::length_mm(value))))));
        auto sketch = reference_checked(
            Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")));
        REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
            ProfileId("profile.base"), ParameterId("width"), ParameterId("height")))));
        REQUIRE(part.add_sketch(sketch));
        REQUIRE(part.add_body(
            reference_checked(Body::create(BodyId("body.base"), "Base", BodyKind::Solid))));
        auto feature = reference_checked(Feature::create_additive_extrude(
            FeatureId("feature.base"), "Base", sketch.id(), ParameterId("depth")));
        return part.add_feature(reference_checked(
            feature.with_body_result_context(reference_checked(FeatureBodyResultContext::create(
                FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.base"))))));
      }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  return empty;
}
void choose(ShellWindow& window, int index) {
  REQUIRE(filter(window).isEnabled());
  filter(window).setCurrentIndex(index);
  qApp->processEvents();
}
QTreeWidgetItem* find_item(QTreeWidgetItem* root, const QString& id) {
  if (root->data(0, Qt::UserRole).toString() == id)
    return root;
  for (int i = 0; i < root->childCount(); ++i)
    if (auto* found = find_item(root->child(i), id))
      return found;
  return nullptr;
}
void tree(ShellWindow& window, const char* id, bool activate = false) {
  auto& browser = window.model_browser();
  QTreeWidgetItem* found = nullptr;
  for (int i = 0; i < browser.topLevelItemCount() && !found; ++i)
    found = find_item(browser.topLevelItem(i), QString::fromLatin1(id));
  REQUIRE(found);
  browser.setCurrentItem(found);
  if (activate)
    browser.itemDoubleClicked(found, 0);
  qApp->processEvents();
}
void mask(ShellWindow& window, std::uint32_t expected) {
  CHECK(window.viewport().selection_filter_mask() == expected);
  CHECK(window.session().selection().filter_mask() == expected);
}
} // namespace

TEST_CASE("Shell selection filters reject each incompatible semantic kind and retain All vertices",
          "[gui][block136-selection-filters]") {
  ShellWindow window;
  CHECK_FALSE(filter(window).isEnabled());
  seed(window);
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("filters.blcad")));
  const auto before = snapshot(window);
  using K = geometry::ViewportSceneKind;
  const K kinds[] = {K::SolidBody, K::Face,         K::Edge,      K::Vertex,
                     K::Datum,     K::SketchEntity, K::Component, K::AssemblyTarget};
  const GuiSelectionKind selections[] = {
      GuiSelectionKind::Body,      GuiSelectionKind::Face,          GuiSelectionKind::Edge,
      GuiSelectionKind::Vertex,    GuiSelectionKind::Datum,         GuiSelectionKind::SketchEntity,
      GuiSelectionKind::Component, GuiSelectionKind::AssemblyTarget};
  const std::uint32_t expected[] = {0xFFFFFFFFU,
                                    selection_kind_bit(GuiSelectionKind::SketchEntity),
                                    selection_kind_bit(GuiSelectionKind::Datum),
                                    selection_kind_bit(GuiSelectionKind::Face),
                                    selection_kind_bit(GuiSelectionKind::Edge),
                                    selection_kind_bit(GuiSelectionKind::Body),
                                    selection_kind_bit(GuiSelectionKind::Component),
                                    selection_kind_bit(GuiSelectionKind::AssemblyTarget)};
  REQUIRE(filter(window).count() == 8);
  std::vector<geometry::ViewportSceneItem> scene;
  const auto* shape = window.session().part_shape_cache()->find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  // Exercise the shell's semantic bridge with one presentation of each kind.
  // These fixture roles are transient and do not invent Core references.
  for (int i = 0; i < 8; ++i)
    scene.push_back({kinds[i], "filter-fixture/" + std::to_string(i), *shape});
  for (int selected_filter = 0; selected_filter < 8; ++selected_filter) {
    INFO("Filter " << selected_filter);
    for (int kind = 0; kind < 8; ++kind) {
      INFO("Kind " << kind);
      choose(window, 0);
      REQUIRE(window.viewport().set_scene(scene));
      const auto id = "filter-fixture/" + std::to_string(kind);
      REQUIRE(window.viewport().select_semantic(id));
      choose(window, selected_filter);
      mask(window, expected[selected_filter]);
      const bool allowed = (expected[selected_filter] & selection_kind_bit(selections[kind])) != 0;
      if (selected_filter == 3 || selected_filter == 4) {
        // Edge/Face filters rebuild authoritative idle finishing presentations.
        // Synthetic roles are absent there and must be evicted. Re-publish the
        // fixture under the new mask to check the full kind/filter bridge too.
        CHECK_FALSE(window.viewport().selected_semantic());
        CHECK(window.session().selection().empty());
        REQUIRE(window.viewport().set_scene(scene));
        CHECK(window.viewport().select_semantic(id) == allowed);
      }
      CHECK(window.viewport().selected_semantic().has_value() == allowed);
      CHECK(window.session().selection().size() == (allowed ? 1 : 0));
      CHECK(window.viewport().select_semantic(id) == allowed);
    }
  }
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
}

TEST_CASE(
    "Shell filters evict browser profile fallbacks and blocked datums without disabling inspection",
    "[gui][block136-selection-filters]") {
  ShellWindow window;
  seed(window);
  tree(window, "sketch.base");
  REQUIRE(action(window, "extrude").isEnabled());
  choose(window, 5);
  mask(window, selection_kind_bit(GuiSelectionKind::Body));
  CHECK_FALSE(action(window, "extrude").isEnabled());
  CHECK_FALSE(
      window.feature_tools().select_profile(SketchId("sketch.base"), ProfileId("profile.base")));
  tree(window, "sketch.base");
  CHECK_FALSE(action(window, "extrude").isEnabled());
  CHECK(window.session().selection().empty());
  CHECK(window.property_inspector().rowCount() > 0);
  tree(window, "datum.xy");
  CHECK_FALSE(action(window, "create_sketch").isEnabled());
  CHECK(window.session().selection().empty());
  choose(window, 2);
  tree(window, "body.base");
  tree(window, "datum.xy");
  CHECK(action(window, "create_sketch").isEnabled());
  choose(window, 1);
  CHECK_FALSE(action(window, "create_sketch").isEnabled());
  tree(window, "sketch.base");
  CHECK(action(window, "extrude").isEnabled());
  choose(window, 0);
  tree(window, "body.base");
  CHECK_FALSE(action(window, "extrude").isEnabled());
}

TEST_CASE(
    "Shell task reference masks temporarily override a Body filter and restore it on every exit",
    "[gui][block136-selection-filters]") {
  ShellWindow window;
  seed(window);
  choose(window, 5);
  REQUIRE(window.viewport().select_semantic("body/body.base"));
  const auto before = snapshot(window);
  invoke(window, "fillet");
  CHECK_FALSE(filter(window).isEnabled());
  auto& targets = control<QListWidget>(window, "blcad.finishing.targets");
  REQUIRE(targets.count() > 0);
  const auto items = window.feature_tools().finishing_selection_items();
  REQUIRE_FALSE(items.empty());
  REQUIRE(window.viewport().select_semantic(items.front().semantic_id));
  REQUIRE_FALSE(window.session().selection().empty());
  CHECK(window.session().selection().entries().front().kind == GuiSelectionKind::Edge);
  // Even programmatic activation cannot change a disabled task-owned filter.
  filter(window).setCurrentIndex(0);
  CHECK(filter(window).currentIndex() == 5);
  CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Edge));
  int outcome = 0;
  SECTION("Cancel") {
    window.feature_tools().cancel();
  }
  SECTION("Escape") {
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escape);
  }
  SECTION("Apply") {
    outcome = 1;
    REQUIRE(window.feature_tools().apply());
  }
  SECTION("Undo") {
    outcome = 2;
    REQUIRE(window.undo());
  }
  CHECK_FALSE(window.session().task().active());
  CHECK(filter(window).isEnabled());
  CHECK(filter(window).currentIndex() == 5);
  mask(window, selection_kind_bit(GuiSelectionKind::Body));
  for (const auto& selection : window.session().selection().entries())
    CHECK(selection.kind == GuiSelectionKind::Body);
  CHECK_FALSE(window.viewport().feature_preview_active());
  if (outcome == 0)
    CHECK(snapshot(window) == before);
  else
    CHECK(snapshot(window) != before);
}

TEST_CASE("Shell Sketch focus and interaction capture protect the chosen model filter",
          "[gui][block136-selection-filters]") {
  ShellWindow window;
  seed(window);
  choose(window, 5);
  const auto before = snapshot(window);
  tree(window, "sketch.base", true);
  REQUIRE(window.active_sketch());
  CHECK_FALSE(filter(window).isEnabled());
  CHECK(window.session().selection().allows(GuiSelectionKind::SketchEntity));
  REQUIRE(window.finish_active_sketch());
  mask(window, selection_kind_bit(GuiSelectionKind::Body));
  CHECK(filter(window).isEnabled());
  CHECK_FALSE(action(window, "extrude").isEnabled());
  CHECK(snapshot(window) == before);
  window.viewport().grab_interaction_mouse();
  CHECK_FALSE(filter(window).isEnabled());
  filter(window).setCurrentIndex(1);
  CHECK(filter(window).currentIndex() == 5);
  window.viewport().release_interaction_mouse();
  CHECK(filter(window).isEnabled());
  mask(window, selection_kind_bit(GuiSelectionKind::Body));
}

TEST_CASE("Shell filters survive history and failed replacement but reset after successful "
          "document changes",
          "[gui][block136-selection-filters]") {
  ShellWindow window;
  const auto empty = seed(window);
  QTemporaryDir directory;
  const auto path = directory.filePath("filter-lifecycle.blcad");
  REQUIRE(window.save_to(path));
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  file.close();
  choose(window, 4);
  REQUIRE(window.save_to(path));
  REQUIRE(file.open(QIODevice::ReadOnly));
  CHECK(file.readAll() == bytes);
  file.close();
  REQUIRE(window.undo());
  CHECK(snapshot(window) == empty);
  REQUIRE(window.redo());
  REQUIRE(window.save_to(path));
  CHECK_FALSE(window.open_path(directory.filePath("missing.blcad")));
  CHECK(filter(window).currentIndex() == 4);
  mask(window, selection_kind_bit(GuiSelectionKind::Edge));
  bool cancelled = false;
  SECTION("Cancelled replacement") {
    REQUIRE(window.session().commit_part_transaction("Unsaved parameter", [](PartDocument& part) {
      return part.add_parameter(reference_checked(Parameter::create_length(
          ParameterId("unsaved"), "Unsaved", reference_checked(Quantity::length_mm(7)))));
    }));
    const auto dirty = snapshot(window);
    bool answered = false;
    QTimer::singleShot(0, [&] {
      if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        answered = true;
        dialog->button(QMessageBox::Cancel)->click();
      }
    });
    cancelled = true;
    CHECK_FALSE(window.new_project("Cancelled"));
    REQUIRE(answered);
    CHECK(snapshot(window) == dirty);
    CHECK(filter(window).currentIndex() == 4);
    mask(window, selection_kind_bit(GuiSelectionKind::Edge));
  }
  SECTION("Open") {
    REQUIRE(window.open_path(path));
  }
  SECTION("New Part") {
    REQUIRE(window.new_part("Next Part"));
  }
  SECTION("New Project") {
    REQUIRE(window.new_project("Next Project"));
  }
  CHECK(filter(window).currentIndex() == (cancelled ? 4 : 0));
  mask(window, cancelled ? selection_kind_bit(GuiSelectionKind::Edge) : 0xFFFFFFFFU);
}

TEST_CASE(
    "Hole tasks collect workplanes beyond the model mask and restore the exact original scope",
    "[gui][block136-selection-filters]") {
  ShellWindow window;
  seed(window);
  choose(window, 5);
  const auto scope = std::vector<std::string>{"body/body.base"};
  window.viewport().set_selection_semantic_scope(scope);
  REQUIRE(window.viewport().select_semantic("body/body.base"));
  const auto before = snapshot(window);
  invoke(window, "hole");
  CHECK_FALSE(filter(window).isEnabled());
  CHECK_FALSE(window.viewport().selection_semantic_scope());
  CHECK(window.viewport().selection_filter_mask() ==
        (selection_kind_bit(GuiSelectionKind::Body) | selection_kind_bit(GuiSelectionKind::Datum)));
  tree(window, "datum.xz");
  CHECK(control<QComboBox>(window, "blcad.hole.plane").currentData() == "datum.xz");
  REQUIRE_FALSE(window.session().selection().empty());
  CHECK(window.session().selection().entries().front().kind == GuiSelectionKind::Datum);
  tree(window, "datum.xy");
  control<QLineEdit>(window, "blcad.hole.diameter").setText("3");
  REQUIRE(window.hole_tools().preview());
  bool applied = false;
  bool foreign = false;
  SECTION("A subsequent task owns its own reference restrictions") {
    REQUIRE(window.session().task().cancel());
    REQUIRE(window.session().task().begin("foreign"));
    window.viewport().set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Face));
    const auto next_scope = std::vector<std::string>{"foreign/face"};
    window.viewport().set_selection_semantic_scope(next_scope);
    window.refresh();
    CHECK_FALSE(window.hole_tools().active());
    CHECK(window.session().task().command_id() == "foreign");
    CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Face));
    REQUIRE(window.viewport().selection_semantic_scope());
    CHECK(*window.viewport().selection_semantic_scope() == next_scope);
    CHECK(snapshot(window) == before);
    REQUIRE(window.session().task().cancel());
    foreign = true;
  }
  SECTION("Cancel") {
    window.hole_tools().cancel();
  }
  SECTION("Escape") {
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escape);
  }
  SECTION("Apply") {
    REQUIRE(window.hole_tools().apply());
    applied = true;
  }
  if (!foreign) {
    CHECK_FALSE(window.session().task().active());
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK(filter(window).isEnabled());
    CHECK(filter(window).currentIndex() == 5);
    mask(window, selection_kind_bit(GuiSelectionKind::Body));
    REQUIRE(window.viewport().selection_semantic_scope());
    CHECK(*window.viewport().selection_semantic_scope() == scope);
    if (applied) {
      CHECK(snapshot(window) != before);
      REQUIRE(window.undo());
    }
    CHECK(snapshot(window) == before);
  }
}
