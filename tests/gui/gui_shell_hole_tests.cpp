#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_hole_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <catch2/catch_approx.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
using Catch::Approx;
namespace {
template <typename T> T& field(ShellWindow& window, const char* name) {
  auto* widget = window.findChild<T*>(QStringLiteral("blcad.hole.") + name);
  REQUIRE(widget);
  return *widget;
}
void input(ShellWindow& window, const char* name, const QString& text) {
  field<QLineEdit>(window, name).setText(text);
}
void choose(ShellWindow& window, const char* name, const QString& id) {
  auto& combo = field<QComboBox>(window, name);
  const int index = combo.findData(id);
  REQUIRE(index >= 0);
  combo.setCurrentIndex(index);
}
std::string snapshot(const ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
double volume(const ShellWindow& window, const char* body = "body.base") {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(BodyId(body));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
QString seed(ShellWindow& window, const QTemporaryDir& directory, bool second_body = false) {
  auto part = reference_checked(PartDocument::create(DocumentId("part.holes"), "Holes"));
  for (const auto& [name, value] : {std::pair{"width", 80.0},
                                    {"height", 60.0},
                                    {"depth", 12.0},
                                    {"diameter", 6.0},
                                    {"radius", 20.0}})
    REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
        ParameterId(name), name, reference_checked(Quantity::length_mm(value))))));
  REQUIRE(part.add_parameter(reference_checked(Parameter::create_count(
      ParameterId("copies"), "copies", reference_checked(Quantity::count(4))))));
  REQUIRE(part.add_parameter(reference_checked(Parameter::create_count(
      ParameterId("huge"), "huge", reference_checked(Quantity::count(1001))))));
  REQUIRE(part.add_expression_parameter(ParameterId("computed"), "computed", ParameterType::Length,
                                        "diameter / 2"));
  for (auto plane : {reference_checked(DatumPlane::xy()), reference_checked(DatumPlane::xz()),
                     reference_checked(DatumPlane::yz())})
    REQUIRE(part.add_datum_plane(plane));
  REQUIRE(part.add_construction_plane(reference_checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId("plane.tilt"), "Tilted", {0, 0, 6}, {1, 0, 0}, {0, std::sqrt(0.75), -0.5},
      {0, 0.5, std::sqrt(0.75)}))));
  for (const auto* id :
       second_body ? std::vector<const char*>{"base", "other"} : std::vector<const char*>{"base"}) {
    auto sketch = reference_checked(
        Sketch::create(SketchId(std::string("sketch.") + id), id, DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
        ProfileId(std::string("profile.") + id), ParameterId("width"), ParameterId("height"),
        {std::string(id) == "other" ? 120.0 : 0.0, 0.0}))));
    REQUIRE(sketch.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("line"), {0, 0}, {5, 0}))));
    REQUIRE(part.add_sketch(sketch));
    const BodyId body(std::string("body.") + id);
    REQUIRE(part.add_body(reference_checked(Body::create(body, id, BodyKind::Solid))));
    auto feature = reference_checked(Feature::create_additive_extrude(
        FeatureId(std::string("feature.") + id), id, sketch.id(), ParameterId("depth")));
    const auto context = reference_checked(
        FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, body));
    REQUIRE(part.add_feature(reference_checked(feature.with_body_result_context(context))));
  }
  REQUIRE(part.add_derived_workplane(reference_checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("plane.top"), "Top face",
      reference_checked(
          SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top))))));
  // Reserve both a graph-wide ID and a generated parameter name.
  REQUIRE(part.add_sketch(reference_checked(
      Sketch::create(SketchId("feature.hole.1"), "Reserved", DatumPlaneId("datum.xy")))));
  REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
      ParameterId("reserved"), "hole_2_diameter", reference_checked(Quantity::length_mm(2))))));
  const auto path = directory.filePath("holes.blcad");
  REQUIRE(write_part_document_json_file(part, path.toStdString()));
  REQUIRE(window.open_path(path));
  REQUIRE(window.session().commit_part_sketch_intent_transaction(
      "Accepted catalogs",
      [](PartDocument& document, std::vector<SketchConstraintCatalog>& constraints,
         std::vector<SketchDimensionCatalog>& dimensions) {
        const auto target = reference_checked(SketchConstraintIntentTarget::entity("entity/line"));
        auto constraint = reference_checked(
            SketchConstraintCatalog::create(document.id(), SketchId("sketch.base")));
        REQUIRE(constraint.add(reference_checked(
            SketchConstraintIntent::create(SketchConstraintId("accepted.horizontal"),
                                           SketchSolverConstraintKind::Horizontal, {target}))));
        auto dimension = reference_checked(
            SketchDimensionCatalog::create(document.id(), SketchId("sketch.base")));
        REQUIRE(dimension.add(reference_checked(SketchDimensionIntent::create(
            SketchDimensionId("accepted.length"), SketchDimensionKind::Length,
            SketchDimensionMode::Reference, {target}))));
        constraints.push_back(std::move(constraint));
        dimensions.push_back(std::move(dimension));
        return Result<std::size_t>::success(2);
      }));
  REQUIRE(window.save_to({}));
  REQUIRE(window.open_path(path));
  return path;
}
void preview(ShellWindow& window) {
  const auto result = window.hole_tools().preview();
  if (!result)
    FAIL(result.error().message());
  CHECK(field<QPushButton>(window, "apply").isEnabled());
  CHECK(window.viewport().feature_preview_active());
  CHECK(window.viewport().feature_preview_replaces("body/body.base"));
  CHECK_FALSE(window.viewport().feature_preview_replaces("datum-plane/datum.xy"));
  CHECK_FALSE(window.viewport().feature_preview_replaces("construction-plane/plane.tilt"));
  CHECK_FALSE(window.viewport().feature_preview_replaces("derived-workplane/plane.top"));
}
void clean(ShellWindow& window) {
  CHECK_FALSE(window.hole_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(field<QLineEdit>(window, "name").text().isEmpty());
  CHECK(field<QComboBox>(window, "target").count() == 0);
  CHECK(field<QComboBox>(window, "plane").count() == 0);
}
} // namespace

TEST_CASE("Hole authoring publishes complete single and circular cuts as one history entry",
          "[gui][block136-holes][presentation-visibility]") {
  for (const bool circular : {false, true}) {
    INFO(circular);
    ShellWindow window;
    QTemporaryDir directory;
    const auto path = seed(window, directory);
    const auto before = snapshot(window);
    const auto saved = bytes(path);
    const auto parameters = window.session().part_document()->parameter_count();
    const auto revision = window.session().presentation_revision();
    REQUIRE(window.hole_tools().begin());
    CHECK_FALSE(window.hole_tools().apply());
    CHECK(field<QLineEdit>(window, "name").text() == "Holes 3");
    field<QComboBox>(window, "placement").setCurrentIndex(circular ? 1 : 0);
    preview(window);
    CHECK(snapshot(window) == before);
    CHECK(bytes(path) == saved);
    CHECK(window.session().presentation_revision() == revision);
    CHECK_FALSE(window.session().dirty());
    CHECK_FALSE(window.session().can_undo());
    const auto model_revision = window.session().model_revision();
    const auto diameter = field<QLineEdit>(window, "diameter").text();
    REQUIRE(window.session().set_presentation_visible("datum-plane/datum.xy", false));
    window.refresh_feature_selection_scene();
    CHECK(window.hole_tools().active());
    CHECK(window.session().task().active());
    CHECK(field<QLineEdit>(window, "diameter").text() == diameter);
    preview(window);
    REQUIRE(window.session().set_presentation_visible("datum-plane/datum.xy", true));
    window.refresh_feature_selection_scene();
    CHECK(window.hole_tools().active());
    CHECK(window.session().model_revision() == model_revision);
    CHECK(window.session().presentation_revision() == revision + 2U);
    CHECK(snapshot(window) == before);
    REQUIRE(window.hole_tools().apply());
    clean(window);
    const auto& part = *window.session().part_document();
    REQUIRE(part.find_feature(FeatureId("feature.hole.3")));
    const auto* sketch = part.find_sketch(SketchId("sketch.hole.3"));
    REQUIRE(sketch);
    CHECK(sketch->circle_profiles().size() == (circular ? 0 : 1));
    CHECK(sketch->circular_hole_patterns().size() == (circular ? 1 : 0));
    CHECK(part.parameter_count() == parameters + (circular ? 3 : 1));
    CHECK(volume(window) ==
          Approx(57600 - (circular ? 6 : 1) * std::numbers::pi * 9 * 12).margin(1e-5));
    REQUIRE(window.session().sketch_constraint_catalogs().size() == 1);
    REQUIRE(window.session().sketch_dimension_catalogs().size() == 1);
    const auto after = snapshot(window);
    REQUIRE(window.undo());
    CHECK(snapshot(window) == before);
    CHECK_FALSE(window.session().can_undo());
    REQUIRE(window.redo());
    CHECK(snapshot(window) == after);
    REQUIRE(window.save_to({}));
    const auto canonical = bytes(path);
    REQUIRE(window.open_path(path));
    REQUIRE(window.save_to({}));
    CHECK(bytes(path) == canonical);
    CHECK(snapshot(window) == after);
  }
}

TEST_CASE("Hole axes follow oriented workplanes and generated-face bounds fail closed",
          "[gui][block136-holes]") {
  for (const auto& [plane, depth] : {std::pair{"datum.xy", 12.0},
                                     {"datum.xz", 60.0},
                                     {"datum.yz", 80.0},
                                     {"plane.tilt", 12.0 / std::sqrt(0.75)}}) {
    INFO(plane);
    ShellWindow window;
    QTemporaryDir directory;
    seed(window, directory);
    REQUIRE(window.hole_tools().begin());
    choose(window, "plane", plane);
    input(window, "center_y",
          std::string(plane) == "datum.xz" || std::string(plane) == "datum.yz" ? "6" : "0");
    preview(window);
    REQUIRE(window.hole_tools().apply());
    CHECK(volume(window) == Approx(57600 - std::numbers::pi * 9 * depth).margin(1e-5));
  }
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory);
  const auto before = snapshot(window);
  REQUIRE(window.hole_tools().begin());
  choose(window, "plane", "plane.top");
  input(window, "center_x", "39");
  CHECK_FALSE(window.hole_tools().preview());
  CHECK_FALSE(window.hole_tools().apply());
  CHECK(snapshot(window) == before);
  input(window, "center_x", "0");
  preview(window);
  REQUIRE(window.hole_tools().apply());
  CHECK(volume(window) == Approx(57600 - std::numbers::pi * 9 * 12).margin(1e-5));
}

TEST_CASE("Hole fields bind existing literals and computed typed parameters without writing them",
          "[gui][block136-holes]") {
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory);
  const auto count = window.session().part_document()->parameter_count();
  REQUIRE(window.hole_tools().begin());
  field<QComboBox>(window, "placement").setCurrentIndex(1);
  input(window, "diameter", "7.123456789012345");
  choose(window, "diameter_binding", "computed");
  CHECK(field<QLineEdit>(window, "diameter").isReadOnly());
  CHECK(field<QLineEdit>(window, "diameter").text() == "3");
  choose(window, "diameter_binding", {});
  CHECK(field<QLineEdit>(window, "diameter").text() == "7.123456789012345");
  choose(window, "diameter_binding", "computed");
  CHECK(field<QComboBox>(window, "count_binding").findData("diameter") == -1);
  choose(window, "radius_binding", "radius");
  choose(window, "count_binding", "huge");
  CHECK_FALSE(window.hole_tools().preview());
  choose(window, "count_binding", "copies");
  preview(window);
  REQUIRE(window.hole_tools().apply());
  CHECK(window.session().part_document()->parameter_count() == count);
  CHECK(volume(window) == Approx(57600 - 4 * std::numbers::pi * 2.25 * 12).margin(1e-5));
  REQUIRE(window.session().commit_part_transaction("Change hole inputs", [](PartDocument& part) {
    const auto changed = part.set_parameter_value(ParameterId("diameter"),
                                                  reference_checked(Quantity::length_mm(8)));
    return changed ? Result<std::size_t>::success(changed.value().size())
                   : Result<std::size_t>::failure(changed.error());
  }));
  CHECK(volume(window) == Approx(57600 - 4 * std::numbers::pi * 4 * 12).margin(1e-5));
  REQUIRE(window.undo());
  CHECK(volume(window) == Approx(57600 - 4 * std::numbers::pi * 2.25 * 12).margin(1e-5));
}

TEST_CASE("Invalid hole fields no-op cuts and erased solids retain correctable local input",
          "[gui][block136-holes]") {
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory);
  const auto before = snapshot(window);
  REQUIRE(window.hole_tools().begin());
  for (const auto* invalid : {"", "0", "-2", "nan", "inf", "1e999", "1,5", "6 mm", "1000"}) {
    INFO(invalid);
    input(window, "diameter", invalid);
    CHECK_FALSE(window.hole_tools().preview());
    CHECK_FALSE(window.hole_tools().apply());
    CHECK(field<QLineEdit>(window, "diameter").text() == invalid);
    CHECK(snapshot(window) == before);
  }
  input(window, "diameter", "6");
  input(window, "center_x", "1000");
  CHECK_FALSE(window.hole_tools().preview());
  input(window, "center_x", "0");
  field<QComboBox>(window, "placement").setCurrentIndex(1);
  for (const auto* invalid : {"0", "1.5", "1001", "1e12", "nan"}) {
    input(window, "count", invalid);
    CHECK_FALSE(window.hole_tools().preview());
    CHECK(field<QLineEdit>(window, "count").text() == invalid);
  }
  input(window, "count", "4");
  input(window, "angle", "nan");
  CHECK_FALSE(window.hole_tools().preview());
  input(window, "angle", "-22.123456789012345");
  preview(window);
  input(window, "name", "");
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.hole_tools().apply());
  CHECK_FALSE(window.hole_tools().preview());
  input(window, "name", "Recovered holes");
  preview(window);
  REQUIRE(window.hole_tools().apply());
  REQUIRE(window.session().part_document()->find_sketch(SketchId("sketch.hole.3")));
  CHECK(window.session()
            .part_document()
            ->find_sketch(SketchId("sketch.hole.3"))
            ->circular_hole_patterns()
            .front()
            .angle_offset_deg() == -22.123456789012345);
}

TEST_CASE("Hole target collection preserves other bodies and rejects foreign semantic selections",
          "[gui][block136-holes]") {
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory, true);
  REQUIRE(window.session().selection().add({GuiSelectionKind::Body, "body/body.other"}));
  REQUIRE(window.hole_tools().begin());
  CHECK(field<QComboBox>(window, "target").currentData() == "body.other");
  CHECK_FALSE(window.hole_tools().accept_selection({GuiSelectionKind::Body, "body/missing"}));
  CHECK_FALSE(window.hole_tools().accept_selection({GuiSelectionKind::Face, "body/body.base"}));
  CHECK_FALSE(
      window.hole_tools().accept_selection({GuiSelectionKind::Datum, "derived-workplane/missing"}));
  REQUIRE(window.hole_tools().accept_selection({GuiSelectionKind::Datum, "datum-plane/datum.xy"}));
  input(window, "center_x", "120");
  REQUIRE(window.hole_tools().preview());
  CHECK(volume(window) == Approx(57600));
  REQUIRE(window.hole_tools().apply());
  CHECK(volume(window) == Approx(57600));
  CHECK(volume(window, "body.other") == Approx(57600 - std::numbers::pi * 9 * 12).margin(1e-5));
}

TEST_CASE("Hole previews expire at escape close save history revision and replacement boundaries",
          "[gui][block136-holes]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = seed(window, directory);
  const auto before = snapshot(window);
  for (int boundary = 0; boundary < 8; ++boundary) {
    INFO(boundary);
    REQUIRE(window.hole_tools().begin());
    preview(window);
    switch (boundary) {
    case 0: {
      QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
      QApplication::sendEvent(&field<QLineEdit>(window, "diameter"), &escape);
      break;
    }
    case 1: {
      auto* dock = window.findChild<QDockWidget*>("blcad.shell.hole_task");
      REQUIRE(dock);
      dock->close();
      break;
    }
    case 2:
      REQUIRE(window.save_to({}));
      break;
    case 3:
      CHECK_FALSE(window.undo());
      break;
    case 4:
      CHECK_FALSE(window.redo());
      break;
    case 5:
      window.session().report_error(Error::validation("test", "Revision"));
      CHECK_FALSE(window.hole_tools().apply());
      break;
    case 6:
      REQUIRE(window.open_path(path));
      break;
    case 7:
      REQUIRE(window.session().task().cancel());
      REQUIRE(window.session().task().begin("foreign"));
      window.refresh();
      CHECK(window.session().task().command_id() == "foreign");
      REQUIRE(window.session().task().cancel());
      break;
    }
    clean(window);
    CHECK(snapshot(window) == before);
    CHECK_FALSE(window.session().dirty());
    CHECK_FALSE(window.session().can_undo());
  }
  REQUIRE(window.hole_tools().begin());
  preview(window);
  REQUIRE(window.new_part("Replacement"));
  clean(window);
}

TEST_CASE(
    "Hole command availability rejects missing bodies planar editing foreign tasks and repair",
    "[gui][block136-holes]") {
  ShellWindow window;
  CHECK_FALSE(window.hole_tools().begin());
  REQUIRE(window.new_part("Empty"));
  CHECK_FALSE(window.hole_tools().begin());
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("empty.blcad")));
  const auto path = seed(window, directory);
  REQUIRE(window.session().task().begin("foreign"));
  window.refresh();
  CHECK_FALSE(window.hole_tools().begin());
  REQUIRE(window.session().task().cancel());
  window.refresh();
  REQUIRE(window.hole_tools().begin());
  CHECK_FALSE(window.session().close(true));
  REQUIRE(window.session().task().cancel());
  REQUIRE(window.session().close(true));
  // A queued field event may arrive before ShellWindow's next refresh after a
  // direct session replacement. Never read a parameter from the expired Part.
  choose(window, "diameter_binding", "computed");
  clean(window);
  REQUIRE(window.open_path(path));
  for (QTreeWidgetItemIterator it(&window.model_browser()); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString() == "sketch.base") {
      window.model_browser().itemDoubleClicked(*it, 0);
      break;
    }
  REQUIRE(window.active_sketch());
  CHECK_FALSE(window.hole_tools().begin());
  REQUIRE(window.open_path(path));
  const auto broken = directory.filePath("failed.blcad");
  REQUIRE(write_part_document_json_file(test::repair_open_fixture(), broken.toStdString()));
  REQUIRE(window.open_path(broken, GuiDocumentOpenMode::ReferenceRepair));
  CHECK_FALSE(window.hole_tools().begin());
  CHECK(window.session().task().command_id() == "reference.repair");
}
