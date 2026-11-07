#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QToolButton>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sys/resource.h>

using namespace blcad;
using namespace blcad::gui;

namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result.has_error() ? result.error().message() : "success"));
  REQUIRE(result);
  return std::move(result.value());
}
template <typename T> T& field(ShellWindow& window, const char* name) {
  auto* found = window.findChild<T*>(QString::fromLatin1(name));
  REQUIRE(found);
  return *found;
}
void click(QWidget& widget) {
  INFO(widget.objectName().toStdString());
  QApplication::processEvents();
  for (auto* parent = widget.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* scroll = qobject_cast<QScrollArea*>(parent)) {
      const auto center = widget.mapTo(scroll->widget(), widget.rect().center());
      scroll->ensureVisible(center.x(), center.y(),
                            std::min(widget.width() / 2 + 8, scroll->viewport()->width() / 2),
                            std::min(widget.height() / 2 + 8, scroll->viewport()->height() / 2));
    }
  QApplication::processEvents();
  REQUIRE(widget.isVisible());
  REQUIRE(widget.isEnabled());
  REQUIRE(widget.window()->childAt(widget.mapTo(widget.window(), widget.rect().center())) ==
          &widget);
  const QPointF local = widget.rect().center();
  const QPointF global = widget.mapToGlobal(local.toPoint());
  QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
  QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
  QApplication::sendEvent(&widget, &press);
  QApplication::sendEvent(&widget, &release);
  QApplication::processEvents();
}
void key(QWidget& widget, int code, const QString& text = {},
         Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent press(QEvent::KeyPress, code, modifiers, text);
  QKeyEvent release(QEvent::KeyRelease, code, modifiers, text);
  QApplication::sendEvent(&widget, &press);
  QApplication::sendEvent(&widget, &release);
  QApplication::processEvents();
}
void input(QLineEdit& editor, const QString& value) {
  click(editor);
  key(editor, Qt::Key_A, {}, Qt::ControlModifier);
  key(editor, 0, value);
  key(editor, Qt::Key_Return);
}
void source(ShellWindow& window, const char* name, bool open_line = false) {
  auto& combo = field<QComboBox>(window, "blcad.extrude.profile");
  const auto row = combo.findData(QStringList{
      QString::fromLatin1(name), open_line ? QString{} : QString::fromLatin1(name) + ".profile"});
  REQUIRE(row >= 0);
  click(combo);
  REQUIRE(combo.view()->isVisible());
  key(*combo.view(), Qt::Key_Home);
  for (int index = 0; index < row; ++index)
    key(*combo.view(), Qt::Key_Down);
  key(*combo.view(), Qt::Key_Return);
  REQUIRE(combo.currentIndex() == row);
}
std::string json(ShellWindow& window) {
  return checked(serialize_part_document_to_json(*window.session().part_document()));
}
void seed(ShellWindow& window, bool base = false) {
  REQUIRE(window.new_part(QStringLiteral("Create Another")));
  REQUIRE(window.session().commit_part_transaction("Seed sources", [&](PartDocument& part) {
    for (const auto& [id, value] : {std::pair{"first.width", base ? 4.0 : 20.0},
                                    {"second.width", base ? 4.0 : 12.0},
                                    {"height", base ? 6.0 : 10.0},
                                    {"base.width", 30.0},
                                    {"base.height", 30.0},
                                    {"base.depth", 20.0}})
      REQUIRE(part.add_parameter(checked(
          Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
    for (const auto name : {std::string("first"), std::string("second")}) {
      auto sketch = checked(Sketch::create(SketchId(name), name, DatumPlaneId("datum.xy")));
      REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
          ProfileId(name + ".profile"), ParameterId(name + ".width"), ParameterId("height"),
          name == "first" ? Point2{0, 0} : Point2{base ? 8.0 : 40.0, 0}))));
      REQUIRE(part.add_sketch(std::move(sketch)));
    }
    if (base) {
      auto sketch = checked(Sketch::create(SketchId("base"), "Base", DatumPlaneId("datum.xy")));
      REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
          ProfileId("base.profile"), ParameterId("base.width"), ParameterId("base.height")))));
      REQUIRE(part.add_sketch(std::move(sketch)));
      REQUIRE(part.add_body(checked(Body::create(BodyId("base.body"), "Base", BodyKind::Solid))));
      auto feature = checked(Feature::create_additive_extrude(
          FeatureId("base.feature"), "Base", SketchId("base"), ParameterId("base.depth")));
      REQUIRE(part.add_feature(
          checked(feature.with_body_result_context(checked(FeatureBodyResultContext::create(
              FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("base.body")))))));
    }
    return Result<std::size_t>::success(1U);
  }));
  window.resize(
      QSize(1280, 800).boundedTo(QGuiApplication::primaryScreen()->availableGeometry().size()));
  window.show();
  window.refresh();
  QApplication::processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
}
double volume(ShellWindow& window, BodyId body) {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(body);
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void capture(ShellWindow& window, const char* name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty())
    REQUIRE(window.grab().save(directory + '/' + name + ".png"));
}
} // namespace

TEST_CASE(
    "Command-first Extrude requires an explicit profile and Clear cannot commit an old candidate",
    "[gui][parity-b145][create-another][extrude-profile-selector]") {
  ShellWindow window;
  seed(window);
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  const auto before = json(window);
  const auto selected = window.session().selection().entries();
  REQUIRE(window.feature_tools().extrude_action().isEnabled());
  REQUIRE(window.feature_tools().begin_extrude());
  CHECK(window.feature_tools().active());
  CHECK_FALSE(window.feature_tools().begin_extrude());
  CHECK_FALSE(window.feature_tools().begin_revolve());
  CHECK(window.session().task().stage() == GuiTaskStage::CollectingSelection);
  CHECK(field<QComboBox>(window, "blcad.extrude.profile").currentIndex() == -1);
  CHECK(field<QLabel>(window, "blcad.extrude.profile.selector.count").text() == "0 selected");
  CHECK(field<QLabel>(window, "blcad.extrude.profile_summary").text() ==
        "Select a profile or single open line.");
  CHECK(field<QToolButton>(window, "blcad.extrude.profile.selector.pick").isChecked());
  CHECK_FALSE(window.feature_tools().numeric_editor().isEnabled());
  CHECK_FALSE(window.feature_tools().apply());
  CHECK(json(window) == before);
  source(window, "first");
  REQUIRE(window.feature_tools().coordinator().extrude().active());
  input(window.feature_tools().numeric_editor(), "8 mm");
  REQUIRE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  click(field<QToolButton>(window, "blcad.extrude.profile.selector.clear"));
  CHECK(field<QLabel>(window, "blcad.extrude.profile_summary").text() ==
        "Select a profile or single open line.");
  CHECK(window.session().task().stage() == GuiTaskStage::CollectingSelection);
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(window.feature_tools().apply_create_another());
  CHECK(json(window) == before);
  source(window, "second");
  input(window.feature_tools().numeric_editor(), "8 mm");
  click(field<QPushButton>(window, "blcad.feature.apply"));
  REQUIRE(window.session().part_document()->features().size() == 1);
  CHECK(volume(window, window.session().part_document()->bodies().front().id()) ==
        Catch::Approx(960).margin(1e-6));
  CHECK(window.session().selection().entries() == selected);
  REQUIRE(window.undo());
  CHECK(json(window) == before);
  REQUIRE(window.redo());
  const auto next_selection = window.session().selection().entries();
  REQUIRE(window.feature_tools().begin_extrude());
  click(field<QPushButton>(window, "blcad.feature.cancel"));
  CHECK(window.session().selection().entries() == next_selection);
}

TEST_CASE(
    "Create Another commits once and retains scalar settings while collecting fresh references",
    "[gui][parity-b145][create-another][extrude-create-another]") {
  ShellWindow window;
  seed(window);
  const auto before = json(window);
  REQUIRE(window.feature_tools().begin_extrude());
  source(window, "first");
  input(window.feature_tools().numeric_editor(), "8 mm");
  auto& extent = field<QComboBox>(window, "blcad.extrude.extent");
  extent.setCurrentIndex(extent.findData(static_cast<int>(ExtrudeExtentMode::TwoSided)));
  auto& second = field<QLineEdit>(window, "blcad.extrude.second_distance");
  input(second, "3 mm");
  REQUIRE(field<QPushButton>(window, "blcad.feature.create_another").isEnabled());
  click(field<QPushButton>(window, "blcad.feature.create_another"));
  const auto first = json(window);
  REQUIRE(window.session().part_document()->features().size() == 1);
  const auto body = window.session().part_document()->bodies().front().id();
  CHECK(volume(window, body) == Catch::Approx(2200).margin(1e-6));
  CHECK(window.session().task().stage() == GuiTaskStage::CollectingSelection);
  CHECK(field<QComboBox>(window, "blcad.extrude.profile").currentIndex() == -1);
  for (const auto id : {"blcad.extrude.target", "blcad.extrude.path", "blcad.extrude.first_face",
                        "blcad.extrude.second_face"})
    CHECK(field<QComboBox>(window, id).currentIndex() == -1);
  CHECK(window.feature_tools().numeric_editor().text() == "8");
  CHECK(second.text() == "3 mm");
  CHECK(extent.currentData().toInt() == static_cast<int>(ExtrudeExtentMode::TwoSided));
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  for (auto* control :
       {static_cast<QWidget*>(&field<QLabel>(window, "blcad.extrude.profile.selector.count")),
        static_cast<QWidget*>(&field<QToolButton>(window, "blcad.extrude.profile.selector.pick"))})
    CHECK(control->visibleRegion().boundingRect() == control->rect());
  capture(window, "create-another-empty");
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(window.save_to(directory.filePath("first.blcad")));
  GuiDocumentSession saved;
  REQUIRE(saved.open_file(directory.filePath("first.blcad").toStdString()));
  CHECK(checked(serialize_part_document_to_json(*saved.part_document())) == first);
  source(window, "second");
  REQUIRE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  window.resize(
      QSize(1024, 640).boundedTo(QGuiApplication::primaryScreen()->availableGeometry().size()));
  QApplication::processEvents();
  for (const auto id :
       {"blcad.feature.apply", "blcad.feature.create_another", "blcad.feature.cancel"}) {
    auto& button = field<QPushButton>(window, id);
    CHECK(button.isVisible());
    CHECK(window.rect().contains(button.mapTo(&window, button.rect().center())));
  }
  capture(window, "create-another-compact");
  click(field<QPushButton>(window, "blcad.feature.apply"));
  const auto complete = json(window);
  REQUIRE(window.session().part_document()->features().size() == 2);
  CHECK(volume(window, window.session().part_document()->bodies().back().id()) ==
        Catch::Approx(1320).margin(1e-6));
  REQUIRE(window.undo());
  CHECK(json(window) == first);
  CHECK_FALSE(window.session().dirty());
  REQUIRE(window.undo());
  CHECK(json(window) == before);
  REQUIRE(window.redo());
  CHECK(json(window) == first);
  REQUIRE(window.redo());
  CHECK(json(window) == complete);
  REQUIRE(window.save_to(directory.filePath("complete.blcad")));
  REQUIRE(saved.open_file(directory.filePath("complete.blcad").toStdString()));
  CHECK(checked(serialize_part_document_to_json(*saved.part_document())) == complete);
  window.viewport().clear_selection();
  window.session().selection().clear();
  window.refresh();
  auto& repeat = field<QAction>(window, "blcad.action.repeat_last_command");
  REQUIRE(repeat.isEnabled());
  repeat.trigger();
  CHECK(window.session().task().stage() == GuiTaskStage::CollectingSelection);
  CHECK(field<QComboBox>(window, "blcad.extrude.profile").currentIndex() == -1);
  key(window, Qt::Key_Escape);
  CHECK_FALSE(window.feature_tools().active());
  CHECK(json(window) == complete);
}

TEST_CASE("Create Another leaves required Cut targets and limit faces unselected and keeps Pick "
          "roles exclusive",
          "[gui][parity-b145][create-another][extrude-create-another-targets]") {
  ShellWindow window;
  seed(window, true);
  REQUIRE(window.feature_tools().begin_extrude());
  source(window, "first");
  auto& operation = field<QComboBox>(window, "blcad.extrude.operation");
  operation.setCurrentIndex(operation.findData(static_cast<int>(FeatureBodyOperationMode::Cut)));
  auto& target = field<QComboBox>(window, "blcad.extrude.target");
  target.setCurrentIndex(target.findData("base.body"));
  auto& extent = field<QComboBox>(window, "blcad.extrude.extent");
  extent.setCurrentIndex(extent.findData(static_cast<int>(ExtrudeExtentMode::ToFace)));
  auto& face = field<QComboBox>(window, "blcad.extrude.first_face");
  face.setCurrentIndex(face.findData("base.feature/top"));
  REQUIRE(field<QPushButton>(window, "blcad.feature.create_another").isEnabled());
  click(field<QPushButton>(window, "blcad.feature.create_another"));
  const auto first = json(window);
  CHECK(volume(window, BodyId("base.body")) == Catch::Approx(17520).margin(1e-5));
  CHECK(target.currentIndex() == -1);
  CHECK(face.currentIndex() == -1);
  source(window, "second");
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  target.setCurrentIndex(target.findData("base.body"));
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  click(field<QToolButton>(window, "blcad.extrude.first_face.selector.pick"));
  CHECK_FALSE(field<QToolButton>(window, "blcad.extrude.profile.selector.pick").isChecked());
  CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Face));
  click(field<QToolButton>(window, "blcad.extrude.profile.selector.pick"));
  CHECK_FALSE(field<QToolButton>(window, "blcad.extrude.first_face.selector.pick").isChecked());
  CHECK(window.viewport().selection_filter_mask() ==
        selection_kind_bit(GuiSelectionKind::SketchEntity));
  face.setCurrentIndex(face.findData("base.feature/top"));
  REQUIRE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  click(field<QPushButton>(window, "blcad.feature.cancel"));
  CHECK(json(window) == first);
  CHECK(volume(window, BodyId("base.body")) == Catch::Approx(17520).margin(1e-5));
  CHECK_FALSE(window.session().task().active());
  CHECK(window.session().undo_label() == "Apply Extrude");
}

TEST_CASE("Switching Extrude sources retains invalid authored fields and rejects incompatible path "
          "direction",
          "[gui][parity-b145][create-another][extrude-profile-correction]") {
  ShellWindow window;
  seed(window);
  REQUIRE(window.session().commit_part_transaction("Open line", [&](PartDocument& part) {
    auto sketch = checked(Sketch::create(SketchId("line"), "Line", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_entity(
        checked(LineSegment::create(SketchEntityId("line.edge"), {0, 30}, {10, 30}))));
    return part.add_sketch(std::move(sketch));
  }));
  window.refresh();
  const auto before = json(window);
  REQUIRE(window.feature_tools().begin_extrude());
  source(window, "first");
  auto& taper = field<QLineEdit>(window, "blcad.extrude.taper");
  input(taper, "invalid");
  source(window, "second");
  CHECK(taper.text() == "invalid");
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.create_another").isEnabled());
  CHECK_FALSE(window.feature_tools().apply_create_another());
  CHECK(json(window) == before);
  input(taper, "0 deg");
  auto& direction = field<QComboBox>(window, "blcad.extrude.direction");
  direction.setCurrentIndex(2);
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  source(window, "line", true);
  CHECK(direction.currentIndex() == -1);
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  direction.setCurrentIndex(0);
  REQUIRE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  click(field<QPushButton>(window, "blcad.feature.cancel"));
  CHECK(json(window) == before);
}

TEST_CASE("Extrude primary choices exclude construction lines and ambiguous raw linework",
          "[gui][parity-b145][create-another][extrude-profile-exclusions]") {
  ShellWindow window;
  REQUIRE(window.new_part("Unsupported sources"));
  REQUIRE(window.session().commit_part_transaction("Unsupported sources", [&](PartDocument& part) {
    for (const auto role :
         {SketchLineRole::Construction, SketchLineRole::Centerline, SketchLineRole::Geometry}) {
      const auto name = "source." + std::to_string(static_cast<int>(role));
      auto sketch = checked(Sketch::create(SketchId(name), name, DatumPlaneId("datum.xy")));
      REQUIRE(sketch.add_entity(
          checked(LineSegment::create(SketchEntityId(name + ".line"), {0, 0}, {20, 0}, role))));
      if (role == SketchLineRole::Geometry)
        REQUIRE(sketch.add_entity(
            checked(LineSegment::create(SketchEntityId(name + ".other"), {10, -10}, {10, 10}))));
      REQUIRE(part.add_sketch(std::move(sketch)));
    }
    return Result<std::size_t>::success(1);
  }));
  window.refresh();
  CHECK_FALSE(window.feature_tools().extrude_action().isEnabled());
  CHECK_FALSE(window.feature_tools().begin_extrude());
  CHECK_FALSE(window.session().part_workflow_active());
}

TEST_CASE(
    "Shared Extrude workflow records checked UI transition timings without a portable latency gate",
    "[gui][parity-b145][workflow-interaction-baseline]") {
  ShellWindow window;
  seed(window);
  const auto before = json(window);
  QJsonArray samples;
  for (int trial = 0; trial < 11; ++trial) {
    window.viewport().clear_selection();
    window.session().selection().clear();
    window.feature_tools().clear_profile_selection();
    const auto measured = [](auto operation) {
      QElapsedTimer clock;
      clock.start();
      operation();
      return clock.nsecsElapsed() / 1e6;
    };
    const auto begin = measured([&] { window.feature_tools().extrude_action().trigger(); });
    REQUIRE(window.session().task().stage() == GuiTaskStage::CollectingSelection);
    const auto pick = measured([&] { source(window, "first"); });
    const auto numeric = measured([&] { input(window.feature_tools().numeric_editor(), "8 mm"); });
    REQUIRE(field<QPushButton>(window, "blcad.feature.create_another").isEnabled());
    const auto accept =
        measured([&] { click(field<QPushButton>(window, "blcad.feature.create_another")); });
    REQUIRE(window.session().part_document()->features().size() == 1);
    CHECK(volume(window, window.session().part_document()->bodies().front().id()) ==
          Catch::Approx(1600).margin(1e-6));
    REQUIRE(window.session().task().stage() == GuiTaskStage::CollectingSelection);
    const auto cancel =
        measured([&] { click(field<QPushButton>(window, "blcad.feature.cancel")); });
    CHECK_FALSE(window.feature_tools().active());
    REQUIRE(window.undo());
    CHECK(json(window) == before);
    samples.append(QJsonObject{{"trial", trial},
                               {"begin_ms", begin},
                               {"profile_keyboard_pick_ms", pick},
                               {"numeric_keyboard_input_ms", numeric},
                               {"create_another_ms", accept},
                               {"cancel_ms", cancel}});
  }
  const auto output = qEnvironmentVariable("BLCAD_WORKFLOW_INTERACTION_BASELINE");
  if (!output.isEmpty()) {
    rusage usage{};
    REQUIRE(getrusage(RUSAGE_SELF, &usage) == 0);
    const QJsonObject report{
        {"schema", "blcad.workflow-interaction-baseline.v1"},
        {"platform", QApplication::platformName()},
        {"device_pixel_ratio", QGuiApplication::primaryScreen()->devicePixelRatio()},
        {"measurement", "synchronous Qt input dispatch, kernel work and UI event publication; "
                        "fixture setup, startup and oracle checks excluded"},
        {"cache_state", "trial 0 is first measured workflow; trials 1-10 reuse the window after "
                        "complete Apply/Cancel/Undo"},
        {"source_json", QString::fromStdString(before)},
        {"process_peak_rss_kib", static_cast<double>(usage.ru_maxrss)},
        {"samples", samples}};
    QSaveFile file(output);
    REQUIRE(file.open(QIODevice::WriteOnly));
    const auto data = QJsonDocument(report).toJson(QJsonDocument::Indented);
    REQUIRE(file.write(data) == data.size());
    REQUIRE(file.commit());
  }
}
