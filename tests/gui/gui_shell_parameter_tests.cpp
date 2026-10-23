#include "../support/reference_repair_fixture.hpp"
#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_parameter_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <catch2/catch_approx.hpp>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
using Catch::Approx;
namespace {
template <typename T> T& field(ShellWindow& window, const char* name) {
  auto* result = window.findChild<T*>(QStringLiteral("blcad.parameters.") + name);
  REQUIRE(result);
  return *result;
}
std::string snapshot(const ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
void choose(ShellWindow& window, const QString& id = {}) {
  auto& combo = field<QComboBox>(window, "parameter");
  const int index = combo.findData(id);
  REQUIRE(index >= 0);
  combo.setCurrentIndex(index);
}
void input(ShellWindow& window, const QString& value) {
  field<QLineEdit>(window, "input").setText(value);
}
QString add(ShellWindow& window, const QString& name, const QString& value,
            ParameterType type = ParameterType::Length, bool formula = false) {
  choose(window);
  field<QLineEdit>(window, "name").setText(name);
  auto& types = field<QComboBox>(window, "type");
  types.setCurrentIndex(types.findData(static_cast<int>(type)));
  field<QComboBox>(window, "mode").setCurrentIndex(formula ? 1 : 0);
  input(window, value);
  REQUIRE(window.parameter_tools().stage());
  return field<QComboBox>(window, "parameter").currentData().toString();
}
const Parameter& parameter(ShellWindow& window, const QString& id) {
  const auto* result =
      window.session().part_document()->find_parameter(ParameterId(id.toStdString()));
  REQUIRE(result);
  return *result;
}
QString seed(ShellWindow& window, const QTemporaryDir& directory) {
  auto part = blcad::test::reference_repair_fixture(false);
  REQUIRE(part.add_expression_parameter(ParameterId("computed"), "computed", ParameterType::Length,
                                        "width / 2"));
  REQUIRE(part.add_expression_parameter(ParameterId("chain"), "chain", ParameterType::Length,
                                        "computed + 1 mm"));
  // Global graph IDs, not only parameter IDs, must be reserved by the allocator.
  REQUIRE(part.add_sketch(reference_checked(
      Sketch::create(SketchId("parameter.user.1"), "ID collision", DatumPlaneId("datum.xy")))));
  const auto path = directory.filePath("parameters.blcad.json");
  REQUIRE(write_part_document_json_file(part, path.toStdString()));
  REQUIRE(window.open_path(path));
  REQUIRE(window.session().commit_part_sketch_intent_transaction(
      "Seed accepted catalogs",
      [](PartDocument& document, std::vector<SketchConstraintCatalog>& constraints,
         std::vector<SketchDimensionCatalog>& dimensions) {
        const auto target = reference_checked(SketchConstraintIntentTarget::entity("entity/line"));
        auto constraint_catalog = reference_checked(
            SketchConstraintCatalog::create(document.id(), SketchId("sketch.refs")));
        REQUIRE(constraint_catalog.add(reference_checked(
            SketchConstraintIntent::create(SketchConstraintId("accepted.horizontal"),
                                           SketchSolverConstraintKind::Horizontal, {target}))));
        auto dimension_catalog = reference_checked(
            SketchDimensionCatalog::create(document.id(), SketchId("sketch.refs")));
        REQUIRE(dimension_catalog.add(reference_checked(SketchDimensionIntent::create(
            SketchDimensionId("accepted.length"), SketchDimensionKind::Length,
            SketchDimensionMode::Reference, {target}))));
        constraints.push_back(std::move(constraint_catalog));
        dimensions.push_back(std::move(dimension_catalog));
        return Result<std::size_t>::success(2);
      }));
  REQUIRE(window.save_to({}));
  REQUIRE(window.open_path(path));
  return path;
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
void preview(ShellWindow& window) {
  REQUIRE(window.parameter_tools().preview());
  REQUIRE(field<QPushButton>(window, "apply").isEnabled());
}
void clean(ShellWindow& window) {
  CHECK_FALSE(window.parameter_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(field<QTableWidget>(window, "catalog").rowCount() == 0);
  CHECK(field<QLineEdit>(window, "name").text().isEmpty());
  CHECK(field<QLineEdit>(window, "input").text().isEmpty());
}
} // namespace

TEST_CASE("Parameter batches create all literal types and formula chains with one history entry",
          "[gui][block136-parameters]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = seed(window, directory);
  const auto before = snapshot(window);
  const auto original = bytes(path);
  const auto revision = window.session().presentation_revision();
  REQUIRE(window.parameter_tools().begin());
  CHECK_FALSE(window.parameter_tools().apply());
  const auto length = add(window, "precise", "12.123456789012345");
  CHECK(length == "parameter.user.2");
  const auto count = add(window, "copies", "3", ParameterType::Count);
  const auto angle = add(window, "inclination", "-23.123456789012345", ParameterType::Angle);
  const auto expression =
      add(window, "derived", "precise * copies + 1 mm", ParameterType::Length, true);
  const auto chained = add(window, "chained", "derived / 2", ParameterType::Length, true);
  const auto counted = add(window, "total", "copies * 2", ParameterType::Count, true);
  preview(window);
  CHECK(window.viewport().feature_preview_replaces("feature/feature.new"));
  CHECK_FALSE(window.viewport().feature_preview_replaces("datum-plane/datum.xy"));
  CHECK(snapshot(window) == before);
  CHECK(bytes(path) == original);
  CHECK_FALSE(window.session().dirty());
  CHECK_FALSE(window.session().can_undo());
  CHECK(window.session().presentation_revision() == revision);
  REQUIRE(window.parameter_tools().apply());
  clean(window);
  CHECK(parameter(window, length).value().millimeters() == 12.123456789012345);
  CHECK(parameter(window, angle).value().degrees() == -23.123456789012345);
  CHECK(parameter(window, count).value().count_value() == 3);
  CHECK(parameter(window, counted).value().count_value() == 6);
  CHECK(parameter(window, expression).value().millimeters() == Approx(37.370370367037035));
  CHECK(parameter(window, chained).value().millimeters() == Approx(18.685185183518518));
  REQUIRE(window.session().sketch_constraint_catalogs().size() == 1);
  REQUIRE(window.session().sketch_dimension_catalogs().size() == 1);
  CHECK(window.session().sketch_constraint_catalogs().front().sketch() == SketchId("sketch.refs"));
  CHECK(window.session().sketch_dimension_catalogs().front().sketch() == SketchId("sketch.refs"));
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().can_undo());
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  REQUIRE(window.save_to({}));
  const auto saved = bytes(path);
  REQUIRE(window.open_path(path));
  REQUIRE(window.save_to({}));
  CHECK(bytes(path) == saved);
  CHECK(snapshot(window) == after);
}

TEST_CASE(
    "Parameter input rejects invalid numbers names formulas units and cycles without losing drafts",
    "[gui][block136-parameters]") {
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory);
  const auto before = snapshot(window);
  REQUIRE(window.parameter_tools().begin());
  const auto staged = add(window, "retained", "5");
  choose(window);
  field<QLineEdit>(window, "name").setText("new_parameter");
  for (const auto& invalid : {"", "0", "-2", "nan", "inf", "1e999", "1 mm", "1,5", "1 2"}) {
    INFO(invalid);
    input(window, invalid);
    CHECK_FALSE(window.parameter_tools().stage());
    CHECK(field<QLineEdit>(window, "input").text() == invalid);
    CHECK_FALSE(window.parameter_tools().preview());
  }
  input(window, "4");
  for (const auto& invalid : {"", "1abc", "bad name", "ä", "a-b", "a\n", "width", "retained"}) {
    INFO(invalid);
    field<QLineEdit>(window, "name").setText(QString::fromUtf8(invalid));
    CHECK_FALSE(window.parameter_tools().stage());
    CHECK(field<QLineEdit>(window, "name").text() == QString::fromUtf8(invalid));
  }
  field<QLineEdit>(window, "name").setText("new_parameter");
  field<QComboBox>(window, "type").setCurrentIndex(1);
  input(window, "1.5");
  CHECK_FALSE(window.parameter_tools().stage());
  field<QComboBox>(window, "mode").setCurrentIndex(1);
  for (const auto& invalid : {"unknown + 1", "copies / 0", "width", "1.5", "1 deg", "(2 +"}) {
    input(window, invalid);
    CHECK_FALSE(window.parameter_tools().stage());
  }
  field<QComboBox>(window, "type").setCurrentIndex(2);
  input(window, "30");
  CHECK_FALSE(window.parameter_tools().stage());
  choose(window, "computed");
  CHECK(field<QLineEdit>(window, "name").isReadOnly());
  CHECK_FALSE(field<QComboBox>(window, "type").isEnabled());
  CHECK_FALSE(field<QComboBox>(window, "mode").isEnabled());
  for (const auto& invalid : {"chain / 2", "computed + 1 mm", "2", "width * width", "width / 0"}) {
    input(window, invalid);
    CHECK_FALSE(window.parameter_tools().stage());
  }
  input(window, "retained * 3");
  REQUIRE(window.parameter_tools().stage());
  preview(window);
  CHECK(snapshot(window) == before);
  REQUIRE(window.parameter_tools().apply());
  CHECK(parameter(window, "computed").value().millimeters() == 15);
  CHECK(parameter(window, "chain").value().millimeters() == 16);
  CHECK(parameter(window, staged).value().millimeters() == 5);
  REQUIRE(window.save_to({}));
  const auto accepted = snapshot(window);
  REQUIRE(window.open_path(QString::fromStdString(window.session().file_path().string())));
  CHECK(snapshot(window) == accepted);
}

TEST_CASE("Parameter selection reset and pending input revoke full-model previews",
          "[gui][block136-parameters]") {
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory);
  const auto before = snapshot(window);
  REQUIRE(window.parameter_tools().begin());
  const auto id = add(window, "draft", "4");
  preview(window);
  input(window, "6");
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(field<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(field<QPushButton>(window, "preview").isEnabled());
  CHECK_FALSE(window.parameter_tools().apply());
  choose(window, "width");
  choose(window, id);
  CHECK(field<QLineEdit>(window, "input").text() == "4");
  input(window, "4");
  CHECK_FALSE(window.parameter_tools().stage());
  field<QPushButton>(window, "reset").click();
  CHECK(field<QComboBox>(window, "parameter").findData(id) == -1);
  CHECK_FALSE(window.parameter_tools().preview());
  CHECK_FALSE(window.viewport().feature_preview_active());
  window.parameter_tools().cancel();
  clean(window);
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
}

TEST_CASE(
    "Parameter edits update usages and downstream geometry while invalid geometry stays local",
    "[gui][block136-parameters]") {
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory);
  const auto before = snapshot(window);
  REQUIRE(window.parameter_tools().begin());
  choose(window, "width");
  CHECK(field<QLabel>(window, "usage").text().contains("computed"));
  input(window, "30");
  REQUIRE(window.parameter_tools().stage());
  const auto& table = field<QTableWidget>(window, "catalog");
  CHECK(table.item(3, 2)->text() == "15 mm");
  CHECK(table.item(4, 2)->text() == "16 mm");
  preview(window);
  CHECK(snapshot(window) == before);
  REQUIRE(window.parameter_tools().apply());
  CHECK(parameter(window, "chain").value().millimeters() == 16);
  const auto* shape =
      window.session().part_shape_cache()->find_feature_shape(FeatureId("feature.old"));
  REQUIRE(shape);
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 == Approx(2400));
  const auto accepted = snapshot(window);
  REQUIRE(window.parameter_tools().begin());
  choose(window, "width");
  // Valid positive Length; too small to produce valid downstream OCCT geometry.
  input(window, "1e-12");
  REQUIRE(window.parameter_tools().stage());
  CHECK_FALSE(window.parameter_tools().preview());
  CHECK_FALSE(window.parameter_tools().apply());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == accepted);
  CHECK(field<QLineEdit>(window, "input").text() == "9.9999999999999998e-13");
  input(window, "24");
  REQUIRE(window.parameter_tools().stage());
  preview(window);
  REQUIRE(window.parameter_tools().apply());
  CHECK(parameter(window, "chain").value().millimeters() == 13);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == accepted);
}

TEST_CASE(
    "Parameter tasks expire on escape dock close history save revisions and document replacement",
    "[gui][block136-parameters]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = seed(window, directory);
  const auto before = snapshot(window);
  for (int boundary = 0; boundary < 7; ++boundary) {
    INFO(boundary);
    REQUIRE(window.parameter_tools().begin());
    add(window, "temporary", "9");
    preview(window);
    if (boundary == 0) {
      QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
      QApplication::sendEvent(&field<QLineEdit>(window, "input"), &escape);
    } else if (boundary == 1) {
      auto* dock = window.findChild<QDockWidget*>("blcad.shell.parameters_task");
      REQUIRE(dock);
      dock->close();
    } else if (boundary == 2) {
      CHECK_FALSE(window.undo());
    } else if (boundary == 3) {
      CHECK_FALSE(window.redo());
    } else if (boundary == 4) {
      REQUIRE(window.save_to({}));
    } else if (boundary == 5) {
      window.session().report_error(Error::validation("test", "Revision changed"));
      CHECK_FALSE(window.parameter_tools().apply());
    } else {
      REQUIRE(window.open_path(path));
    }
    clean(window);
    CHECK(snapshot(window) == before);
    CHECK_FALSE(window.session().dirty());
    CHECK_FALSE(window.session().can_undo());
  }
  REQUIRE(window.parameter_tools().begin());
  add(window, "temporary", "9");
  REQUIRE(window.new_part("Replacement"));
  clean(window);
  CHECK(window.session().part_document()->parameters().empty());
}

TEST_CASE("Standalone parameter availability preserves foreign tasks and rejects project contexts",
          "[gui][block136-parameters]") {
  ShellWindow window;
  CHECK_FALSE(window.parameter_tools().begin());
  REQUIRE(window.new_part("Empty"));
  REQUIRE(window.parameter_tools().begin());
  add(window, "first", "2");
  preview(window);
  REQUIRE(window.parameter_tools().apply());
  REQUIRE(window.undo());
  REQUIRE(window.session().task().begin("foreign.command"));
  window.refresh();
  auto* action = window.findChild<QAction*>("blcad.action.parameters");
  REQUIRE(action);
  CHECK_FALSE(action->isEnabled());
  CHECK_FALSE(window.parameter_tools().begin());
  CHECK(window.session().task().command_id() == "foreign.command");
  REQUIRE(window.session().task().cancel());
  window.refresh();
  REQUIRE(window.parameter_tools().begin());
  add(window, "pending", "3");
  REQUIRE(window.session().task().cancel());
  REQUIRE(window.session().task().begin("foreign.command"));
  window.refresh();
  clean(window);
  CHECK(window.session().task().command_id() == "foreign.command");
  REQUIRE(window.session().task().cancel());
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("empty.blcad")));
  REQUIRE(window.new_project("Project"));
  CHECK_FALSE(window.parameter_tools().begin());
}

TEST_CASE("Parameter manager refuses active planar editing and failed-recompute repair sessions",
          "[gui][block136-parameters]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = seed(window, directory);
  for (QTreeWidgetItemIterator it(&window.model_browser()); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString() == "sketch.refs") {
      window.model_browser().itemDoubleClicked(*it, 0);
      break;
    }
  REQUIRE(window.active_sketch() == SketchId("sketch.refs"));
  auto* action = window.findChild<QAction*>("blcad.action.parameters");
  REQUIRE(action);
  CHECK_FALSE(action->isEnabled());
  CHECK_FALSE(window.parameter_tools().begin());
  // Opening the committed file discards editor interaction without committing local authoring.
  REQUIRE(window.open_path(path));
  REQUIRE(window.parameter_tools().begin());
  add(window, "temporary", "2");
  preview(window);
  const auto broken = directory.filePath("failed.blcad");
  REQUIRE(write_part_document_json_file(test::repair_open_fixture(), broken.toStdString()));
  REQUIRE(window.open_path(broken, GuiDocumentOpenMode::ReferenceRepair));
  clean(window);
  CHECK_FALSE(window.session().derived_results_fresh());
  CHECK_FALSE(action->isEnabled());
  CHECK_FALSE(window.parameter_tools().begin());
  CHECK(window.session().task().command_id() == "reference.repair");
}
