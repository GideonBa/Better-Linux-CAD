#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/surface_selection_geometry.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTemporaryDir>
#include <QTimer>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
namespace {
using C = ShellFeatureTools::Command;
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* p = w.findChild<T*>(name);
  REQUIRE(p);
  return *p;
}
std::string saved(ShellWindow& w) {
  return blcad::test::surface_offset::saved(*w.session().part_document());
}
void fixture(ShellWindow& w, bool hidden = false) {
  REQUIRE(w.new_part("Explicit Surface sources"));
  REQUIRE(w.session().commit_part_transaction("Inputs", [&](PartDocument& p) {
    p = f::combination();
    std::vector<BodyId> ids;
    for (const auto& body : p.bodies())
      if (hidden || body.id() != BodyId("quilt.body"))
        ids.push_back(body.id());
    for (const auto& id : ids)
      f::checked(p.set_body_visibility(id, BodyVisibility::Hidden));
    return Result<std::size_t>::success(1);
  }));
  w.refresh();
}
void choose(ShellWindow& w, const std::string& label) {
  auto& box = w.feature_tools().surface_target_editor();
  const auto row = box.findText(QString::fromStdString(label));
  INFO(label);
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
}
std::vector<geometry::FinishingSelectionTarget> cylindrical_faces(ShellWindow& w) {
  auto faces = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("quilt.body"),
      PartFeatureInputRole::SurfaceSourceFace));
  std::erase_if(faces, [](const auto& face) { return face.anchor.x <= 8; });
  REQUIRE(faces.size() == 2);
  return faces;
}
void add(ShellWindow& w, const geometry::FinishingSelectionTarget& face) {
  auto& box = w.feature_tools().surface_target_editor();
  int row = box.findData(QString::fromStdString(face.semantic_id), Qt::UserRole + 1);
  if (row < 0)
    row =
        box.findData(QString::fromStdString("surface-input/" + face.semantic_id), Qt::UserRole + 1);
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
  CHECK_FALSE(box.currentText().contains("surface/trim/"));
  control<QPushButton>(w, "blcad.surface.face_add").click();
}
void settle() {
  QEventLoop loop;
  QTimer::singleShot(35, &loop, &QEventLoop::quit);
  loop.exec();
}
void capture(ShellWindow& w, const QString& name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  CHECK(w.grab().save(QDir(dir).filePath(name + ".png")));
}
} // namespace
TEST_CASE("Hidden Surface sources are explicit and leave document visibility unchanged",
          "[gui][advanced-surface-inputs][parity-b173]") {
  const bool transient = GENERATE(false, true);
  const auto command = GENERATE(C::OffsetSurface, C::Thicken);
  ShellWindow w;
  fixture(w, !transient);
  if (transient) {
    REQUIRE(w.session().set_presentation_visible("body/quilt.body", false));
    w.refresh();
  }
  const auto before = saved(w);
  auto& viewport = w.viewport();
  viewport.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Body));
  viewport.set_selection_semantic_scope(std::vector<std::string>{"body/quilt.body"});
  const auto mask = viewport.selection_filter_mask();
  const auto scope = viewport.selection_semantic_scope();
  REQUIRE(w.feature_tools().surface_action(command)->isEnabled());
  REQUIRE(w.feature_tools().begin_surface(command));
  CHECK(w.feature_tools().surface_target_editor().count() == 1);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  control<QCheckBox>(w, "blcad.surface.include_hidden").setChecked(true);
  choose(w, "Quilt");
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK(saved(w) == before);
  CHECK(w.session().presentation_visible("body/quilt.body") == !transient);
  control<QCheckBox>(w, "blcad.surface.include_hidden").setChecked(false);
  CHECK(w.feature_tools().surface_target_editor().count() == 1);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  w.feature_tools().cancel();
  CHECK(saved(w) == before);
  CHECK(viewport.selection_filter_mask() == mask);
  CHECK(viewport.selection_semantic_scope() == scope);
  CHECK_FALSE(viewport.scene_presentation_visible("surface-input/body/quilt.body"));
}
TEST_CASE("Explicit Surface face sets retain one owner and exact editing history",
          "[gui][advanced-surface-inputs][parity-b173]") {
  ShellWindow w;
  fixture(w);
  const auto faces = cylindrical_faces(w);
  const auto before = saved(w);
  REQUIRE(w.feature_tools().begin_surface(C::Thicken));
  for (const auto& face : faces)
    add(w, face);
  auto& rows = control<QListWidget>(w, "blcad.surface.selected_faces");
  REQUIRE(rows.count() == 2);
  add(w, faces.front());
  CHECK(rows.count() == 2);
  control<QCheckBox>(w, "blcad.surface.include_hidden").setChecked(true);
  const auto other = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("source.body"),
      PartFeatureInputRole::SurfaceSourceFace, nullptr, {},
      geometry::FinishingSelectionVisibility::IncludeHiddenInputs));
  REQUIRE_FALSE(other.empty());
  add(w, other.front());
  CHECK(rows.count() == 2);
  control<QComboBox>(w, "blcad.surface.side").setCurrentIndex(2);
  REQUIRE(w.feature_tools().apply());
  const auto& record =
      std::get<SurfaceOffsetFeature>(w.session().part_document()->surface_features().back());
  const auto id = record.id();
  const auto body = record.result_body();
  const auto source = record.target().stable_key();
  CHECK(std::get<SurfaceFaceSelection>(record.target().source()).body == BodyId("quilt.body"));
  CHECK(std::get<SurfaceFaceSelection>(record.target().source()).faces.size() == 2);
  CHECK(blcad::test::integrated_solid_volume(*w.session().part_shape_cache()->find_body_shape(body))
            .volume_mm3 == Catch::Approx(140 * std::acos(.8)).margin(5e-5));
  const auto after = saved(w);
  REQUIRE(w.feature_edit_tools().begin(id));
  CHECK(control<QCheckBox>(w, "blcad.surface.face_set").isChecked());
  CHECK(rows.count() == 2);
  control<QCheckBox>(w, "blcad.surface.face_set").setChecked(false);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  control<QCheckBox>(w, "blcad.surface.face_set").setChecked(true);
  REQUIRE(w.feature_tools().apply());
  CHECK(saved(w) == after);
  CHECK(std::get<SurfaceOffsetFeature>(*w.session().part_document()->find_surface_feature(id))
            .target()
            .stable_key() == source);
  REQUIRE(w.undo());
  CHECK(saved(w) == after);
  REQUIRE(w.undo());
  CHECK(saved(w) == before);
  REQUIRE(w.session().redo());
  REQUIRE(w.session().redo());
  CHECK(saved(w) == after);
}
TEST_CASE("Native selected faces offset and thicken with physical STEP and exact history",
          "[gui][advanced-surface-inputs][native-advanced-surface-inputs][parity-b173]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native face selection");
  ShellWindow w;
  fixture(w);
  const auto faces = cylindrical_faces(w);
  const auto before = saved(w);
  auto size = qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800);
  w.resize(size.boundedTo(w.screen()->availableGeometry().size() - QSize(40, 40)));
  w.show();
  settle();
  w.viewport().set_standard_view(GuiStandardView::Right);
  w.viewport().fit_all();
  REQUIRE(w.feature_tools().begin_surface(C::OffsetSurface));
  control<QCheckBox>(w, "blcad.surface.face_set").setChecked(true);
  settle();
  for (const auto& face : faces) {
    auto frame = f::checked(geometry::SurfaceSelectionGeometry::normal_frame(face.shape));
    const auto screen = w.viewport().model_to_screen(frame.origin);
    REQUIRE(screen);
    const QPointF point(screen.value().x, screen.value().y);
    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
      QMouseEvent event(type, point, w.viewport().mapToGlobal(point.toPoint()), Qt::LeftButton,
                        type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                        Qt::NoModifier);
      QApplication::sendEvent(&w.viewport(), &event);
      qApp->processEvents();
    }
    const auto& box = w.feature_tools().surface_target_editor();
    CHECK(box.currentData(Qt::UserRole + 1).toString() == QString::fromStdString(face.semantic_id));
    CHECK_FALSE(box.currentText().contains("surface/trim/"));
    control<QPushButton>(w, "blcad.surface.face_add").click();
  }
  REQUIRE(control<QListWidget>(w, "blcad.surface.selected_faces").count() == 2);
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  capture(w, "native-surface-face-set");
  REQUIRE(w.feature_tools().apply());
  const auto offset_name =
      surface_feature_name(w.session().part_document()->surface_features().back());
  REQUIRE(w.feature_tools().begin_surface(C::Thicken));
  choose(w, offset_name);
  control<QComboBox>(w, "blcad.surface.side").setCurrentIndex(2);
  REQUIRE(w.feature_tools().set_numeric_text("1 mm"));
  settle();
  capture(w, "native-offset-face-set-thicken");
  auto& commit = control<QPushButton>(w, "blcad.feature.apply");
  REQUIRE(commit.isVisible());
  REQUIRE(w.childAt(commit.mapTo(&w, commit.rect().center())) == &commit);
  commit.click();
  REQUIRE_FALSE(w.feature_tools().active());
  const auto body =
      surface_feature_result_body(w.session().part_document()->surface_features().back());
  const double expected = 154 * std::acos(.8);
  CHECK(blcad::test::integrated_solid_volume(*w.session().part_shape_cache()->find_body_shape(body))
            .volume_mm3 == Catch::Approx(expected).margin(5e-5));
  const auto after = saved(w);
  REQUIRE(w.undo());
  REQUIRE(w.undo());
  CHECK(saved(w) == before);
  REQUIRE(w.session().redo());
  REQUIRE(w.session().redo());
  CHECK(saved(w) == after);
  QTemporaryDir files;
  REQUIRE(files.isValid());
  const auto json = files.filePath("faces.json");
  REQUIRE(w.save_to(json));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(json.toStdString()));
  CHECK(blcad::test::surface_offset::saved(*reopened.part_document()) == after);
  const auto step = files.filePath("faces.step");
  REQUIRE(w.inspect_tools().export_step({GuiStepExportMode::PartMultiBody, step.toStdString()}));
  QFile file(step);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  const auto source = f::checked(StepImportSource::create(
      StepImportSourceId("faces"), "Faces", "faces.step",
      f::checked(step_import_sha256(std::string_view(bytes.constData(), bytes.size()))),
      StepImportMode::Reference));
  const auto imported =
      f::checked(geometry::StepImportReader{}.read(source, files.path().toStdString()));
  const auto shapes = f::checked(geometry::StepImportReader{}.preview_shapes(imported));
  std::size_t solids = 0;
  for (const auto& shape : shapes)
    if (geometry::RectangleExtrusionAdapter{}.summarize(shape).solid_count == 1) {
      ++solids;
      CHECK(blcad::test::integrated_solid_volume(shape).volume_mm3 ==
            Catch::Approx(expected).margin(5e-5));
      CHECK(blcad::test::classify_solid_point(shape, {11, 0, 10}) ==
            blcad::test::SolidPointLocation::Inside);
      CHECK(blcad::test::classify_solid_point(shape, {10, 0, 10}) ==
            blcad::test::SolidPointLocation::Outside);
    }
  CHECK(solids == 1);
  CHECK(saved(w) == after);
}
TEST_CASE("Native curved Trim keep-remove and native-arc Extend reach a checked thickness solid",
          "[gui][advanced-surface-inputs][native-advanced-surface-inputs][parity-b173]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native curved Surface controls");
  ShellWindow w;
  REQUIRE(w.new_part("Curved Surface controls"));
  REQUIRE(w.session().commit_part_transaction("Inputs", [](PartDocument& p) {
    p = f::part();
    return p.set_body_visibility(BodyId("source.body"), BodyVisibility::Hidden);
  }));
  w.refresh();
  auto size = qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800);
  w.resize(size.boundedTo(w.screen()->availableGeometry().size() - QSize(40, 40)));
  w.show();
  settle();
  w.viewport().set_standard_view(GuiStandardView::Right);
  w.viewport().fit_all();
  const auto before = saved(w);
  REQUIRE(w.feature_tools().begin_surface(C::TrimSurface));
  control<QCheckBox>(w, "blcad.surface.include_hidden").setChecked(true);
  const auto faces = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("source.body"),
      PartFeatureInputRole::SurfaceSourceFace, nullptr, {},
      geometry::FinishingSelectionVisibility::IncludeHiddenInputs));
  const auto face = std::find_if(faces.begin(), faces.end(), [](const auto& value) {
    return value.anchor.x > 1 && value.anchor.y < -1;
  });
  REQUIRE(face != faces.end());
  auto& target = w.feature_tools().surface_target_editor();
  const auto row = target.findData(QString::fromStdString("surface-input/" + face->semantic_id),
                                   Qt::UserRole + 1);
  REQUIRE(row >= 0);
  target.setCurrentIndex(row);
  auto& contour = w.feature_tools().surface_source_editor();
  const auto profile = contour.findText("Window / window");
  REQUIRE(profile >= 0);
  contour.setCurrentIndex(profile);
  control<QComboBox>(w, "blcad.surface.projection").setCurrentIndex(1);
  control<QLineEdit>(w, "blcad.surface.projection_1").setText("1");
  control<QLineEdit>(w, "blcad.surface.projection_2").setText("0");
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  const double alpha = std::acos(.8);
  const Point3 inside{10 * std::cos(alpha / 2), -10 * std::sin(alpha / 2), 10};
  const Point3 outside{10 * std::cos(1.5 * alpha), -10 * std::sin(1.5 * alpha), 10};
  const auto preview = [&] {
    return f::checked(w.feature_tools().coordinator().surface().preview_shape(w.session()));
  };
  CHECK(blcad::test::distance_to_faces(preview(), inside) < 1e-7);
  CHECK(blcad::test::distance_to_faces(preview(), outside) > 1);
  control<QComboBox>(w, "blcad.surface.trim_side").setCurrentIndex(1);
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK(blcad::test::distance_to_faces(preview(), inside) > 1.9);
  CHECK(blcad::test::distance_to_faces(preview(), outside) < 1e-7);
  CHECK(saved(w) == before);
  capture(w, "native-curved-trim-remove");
  control<QComboBox>(w, "blcad.surface.trim_side").setCurrentIndex(0);
  REQUIRE(w.feature_tools().apply());
  const auto trim = w.session().part_document()->surface_features().back();
  const auto trim_body = surface_feature_result_body(trim);
  const auto trim_name = surface_feature_name(trim);
  const auto trimmed = saved(w);
  const auto edges = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), trim_body,
      PartFeatureInputRole::PathSegment));
  const auto edge = std::find_if(edges.begin(), edges.end(), [](const auto& value) {
    const auto* reference = std::get_if<EdgeReference>(&value.reference);
    if (!reference)
      return false;
    const auto* curve = std::get_if<ResultEdgeReference>(&reference->source());
    return curve && curve->curve_type() == 1 &&
           std::all_of(curve->samples().begin(), curve->samples().end(),
                       [](Point3 p) { return std::abs(p.z - 12) < 1e-7; });
  });
  REQUIRE(edge != edges.end());
  REQUIRE(w.feature_tools().begin_surface(C::ExtendSurface));
  choose(w, trim_name);
  w.feature_tools().surface_pick_editor().setCurrentIndex(1);
  settle();
  const auto& witness =
      std::get<ResultEdgeReference>(std::get<EdgeReference>(edge->reference).source());
  const auto point = f::checked(w.viewport().model_to_screen(witness.samples()[8]));
  const QPointF local(point.x, point.y);
  for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, local, w.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&w.viewport(), &event);
    qApp->processEvents();
  }
  CHECK(w.feature_tools().surface_source_editor().currentData(Qt::UserRole + 1).toString() ==
        QString::fromStdString(edge->semantic_id));
  control<QComboBox>(w, "blcad.surface.extension_mode").setCurrentIndex(1);
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  control<QComboBox>(w, "blcad.surface.extension_mode").setCurrentIndex(0);
  control<QLineEdit>(w, "blcad.surface.deviation").setText("invalid inactive budget");
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  constexpr double extension = 1.23456789012345;
  REQUIRE(w.feature_tools().set_numeric_text("1.23456789012345 mm"));
  CHECK(saved(w) == trimmed);
  capture(w, "native-curved-extend-arc");
  REQUIRE(w.feature_tools().apply());
  const auto extend = w.session().part_document()->surface_features().back();
  REQUIRE(w.feature_tools().begin_surface(C::Thicken));
  choose(w, surface_feature_name(extend));
  control<QComboBox>(w, "blcad.surface.side").setCurrentIndex(2);
  REQUIRE(w.feature_tools().apply());
  const auto result =
      surface_feature_result_body(w.session().part_document()->surface_features().back());
  const auto* shape = w.session().part_shape_cache()->find_body_shape(result);
  REQUIRE(shape);
  const auto expected = 10 * alpha * (4 + extension);
  CHECK(blcad::test::integrated_solid_volume(*shape).volume_mm3 ==
        Catch::Approx(expected).margin(5e-5));
  CHECK(blcad::test::classify_solid_point(*shape, inside) ==
        blcad::test::SolidPointLocation::Inside);
  const auto after = saved(w);
  for (int i = 0; i < 3; ++i)
    REQUIRE(w.undo());
  CHECK(saved(w) == before);
  for (int i = 0; i < 3; ++i)
    REQUIRE(w.session().redo());
  CHECK(saved(w) == after);
  QTemporaryDir files;
  REQUIRE(files.isValid());
  const auto json = files.filePath("curved.json");
  REQUIRE(w.save_to(json));
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(json.toStdString()));
  CHECK(blcad::test::surface_offset::saved(*opened.part_document()) == after);
  const auto step = files.filePath("curved.step");
  REQUIRE(w.inspect_tools().export_step({GuiStepExportMode::PartMultiBody, step.toStdString()}));
  QFile file(step);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  const auto source = f::checked(StepImportSource::create(
      StepImportSourceId("curved"), "Curved", "curved.step",
      f::checked(step_import_sha256(std::string_view(bytes.constData(), bytes.size()))),
      StepImportMode::Reference));
  const auto imported =
      f::checked(geometry::StepImportReader{}.read(source, files.path().toStdString()));
  const auto shapes = f::checked(geometry::StepImportReader{}.preview_shapes(imported));
  std::size_t solids = 0;
  for (const auto& value : shapes)
    if (geometry::RectangleExtrusionAdapter{}.summarize(value).solid_count == 1) {
      ++solids;
      CHECK(blcad::test::integrated_solid_volume(value).volume_mm3 ==
            Catch::Approx(expected).margin(5e-5));
      CHECK(blcad::test::classify_solid_point(value, inside) ==
            blcad::test::SolidPointLocation::Inside);
      CHECK(blcad::test::classify_solid_point(value, outside) ==
            blcad::test::SolidPointLocation::Outside);
    }
  CHECK(solids == 1);
  CHECK(saved(w) == after);
}
TEST_CASE("Surface parameter allocation preserves existing user IDs and display names",
          "[gui][advanced-surface-allocation][parity-b173]") {
  const bool by_name = GENERATE(false, true);
  ShellWindow w;
  fixture(w);
  const ParameterId reserved(by_name ? "reserved.user" : "parameter.surface.1.tolerance");
  REQUIRE(w.session().commit_part_transaction("User parameter", [&](PartDocument& p) {
    return p.add_parameter(f::checked(
        Parameter::create_length(reserved, by_name ? "Thicken 1 Tolerance" : "User tolerance",
                                 f::checked(Quantity::length_mm(.25)))));
  }));
  w.refresh();
  const auto before = saved(w);
  const auto count = w.session().part_document()->parameters().size();
  REQUIRE(w.feature_tools().begin_surface(C::Thicken));
  choose(w, "Quilt");
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK(saved(w) == before);
  REQUIRE(w.feature_tools().apply());
  const auto* part = w.session().part_document();
  const auto& feature = std::get<SurfaceOffsetFeature>(part->surface_features().back());
  CHECK(feature.id() == FeatureId("feature.surface.2"));
  CHECK(feature.parameters().tolerance != reserved);
  CHECK(part->find_parameter(feature.parameters().tolerance)->value().millimeters() == 1e-6);
  CHECK(part->find_parameter(reserved)->value().millimeters() == .25);
  CHECK(part->parameters().size() == count + 2);
  REQUIRE(w.undo());
  CHECK(saved(w) == before);
}
