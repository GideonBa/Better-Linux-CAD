#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
using Command = ShellFeatureTools::Command;
template <class T> T& widget(ShellWindow& w, const char* name) {
  auto* result = w.findChild<T*>(QString::fromUtf8(name));
  REQUIRE(result);
  return *result;
}
std::string snapshot(const GuiDocumentSession& s) {
  return f::checked(serialize_part_document_to_json(*s.part_document()));
}
void fixture(ShellWindow& w) {
  REQUIRE(w.new_part(QStringLiteral("Path execution")));
  REQUIRE(w.session().commit_part_transaction("Spline source", [](PartDocument& part) {
    part = sf::straight_sweep();
    const auto feature = *part.find_sweep_feature(FeatureId("driven.sweep"));
    (void)f::checked(part.remove_sweep_feature(feature.id()));
    (void)f::checked(part.remove_path_curve(PathCurveId("driven.path")));
    auto owner = *part.find_sketch_3d(Sketch3DId("driven"));
    (void)f::checked(owner.add_point(f::point("c1", {0, 0, 3})));
    (void)f::checked(owner.add_point(f::point("c2", {0, 0, 7})));
    (void)f::checked(owner.add_spline(f::checked(SketchSpline3D::create(
        SketchEntityId("spline"), SketchSpline3DRepresentation::ControlPoints,
        {f::local("a"), f::local("c1"), f::local("c2"), f::local("b")}, 3,
        SketchSpline3DContinuity::Curvature))));
    (void)f::checked(part.update_sketch_3d(owner));
    (void)f::checked(part.add_path_curve(f::checked(PathCurve::create(
        PathCurveId("driven.path"), "Driven spline",
        {f::checked(PathSegmentReference::create_sketch_3d(owner.id(), SketchEntityId("spline"),
                                                           Sketch3DPathCurveKind::Spline))},
        PathClosure::Open, PathOrientationRule::MinimumTwist))));
    return part.add_sweep_feature(feature);
  }));
  w.refresh();
}
QComboBox& target(ShellWindow& w) {
  return widget<QComboBox>(w, "blcad.spatial.policy_target");
}
QComboBox& policy(ShellWindow& w) {
  return widget<QComboBox>(w, "blcad.spatial.execution_policy");
}
QLineEdit& tolerance(ShellWindow& w) {
  return widget<QLineEdit>(w, "blcad.spatial.modeling_tolerance");
}
QLabel& summary(ShellWindow& w) {
  return widget<QLabel>(w, "blcad.feature.summary");
}
QPushButton& apply(ShellWindow& w) {
  return widget<QPushButton>(w, "blcad.feature.apply");
}
void begin(ShellWindow& w) {
  REQUIRE(w.feature_tools().begin_spatial(Command::PathCurve));
  const auto row = target(w).findData(QStringLiteral("driven.path"));
  REQUIRE(row >= 0);
  target(w).setCurrentIndex(row);
  policy(w).setCurrentIndex(1);
}
double volume(const GuiDocumentSession& s) {
  const auto* shape = s.part_shape_cache()->find_body_shape(BodyId("driven.body"));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void history(ShellWindow& w, const std::string& before) {
  const auto after = snapshot(w.session());
  REQUIRE(w.undo());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.redo());
  CHECK(snapshot(w.session()) == after);
  CHECK(volume(w.session()) == Catch::Approx(6.4).margin(1e-6));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto file = dir.filePath("policy.blcad.json");
  REQUIRE(w.save_to(file));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(file.toStdString()));
  CHECK(snapshot(reopened) == after);
  CHECK(volume(reopened) == Catch::Approx(6.4).margin(1e-6));
}
void settle() {
  QEventLoop loop;
  QTimer::singleShot(50, &loop, &QEventLoop::quit);
  loop.exec();
}
void key(QWidget& widget, int code, QString text = {},
         Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent press(QEvent::KeyPress, code, modifiers, text),
      release(QEvent::KeyRelease, code, modifiers, text);
  QApplication::sendEvent(&widget, &press);
  QApplication::sendEvent(&widget, &release);
}
} // namespace

TEST_CASE("Explicit path upgrades compare native geometry before one reversible Apply",
          "[gui][parity-b166][path-policy-shell]") {
  ShellWindow w;
  fixture(w);
  const auto before = snapshot(w.session());
  const auto source = *w.session().part_document()->find_path_curve(PathCurveId("driven.path"));
  begin(w);
  CHECK(snapshot(w.session()) == before);
  CHECK(apply(w).isEnabled());
  CHECK(summary(w).text().contains("32 spans"));
  CHECK(summary(w).text().contains("1 spans"));
  CHECK(summary(w).text().contains("10 mm"));
  CHECK(w.viewport().feature_preview_count() == 2);
  CHECK(w.viewport().feature_preview_replaces("body/driven.body"));
  CHECK_FALSE(w.viewport().feature_preview_replaces("body/base.body"));
  CHECK_FALSE(w.viewport().feature_preview_replaces("path/driven.path"));
  REQUIRE(w.feature_tools().apply());
  const auto& upgraded = *w.session().part_document()->find_path_curve(source.id());
  CHECK(upgraded.execution_policy().tolerance_mm() == .0001);
  CHECK(upgraded.segments()[0].stable_key() == source.segments()[0].stable_key());
  CHECK(upgraded.orientation_rule() == source.orientation_rule());
  CHECK(volume(w.session()) == Catch::Approx(6.4).margin(1e-6));
  history(w, before);
  const auto modern = snapshot(w.session());
  REQUIRE(w.feature_tools().begin_spatial(Command::PathCurve));
  target(w).setCurrentIndex(target(w).findData(QStringLiteral("driven.path")));
  CHECK(policy(w).currentIndex() == 1);
  CHECK(tolerance(w).text().toDouble() == .0001);
  policy(w).setCurrentIndex(0);
  REQUIRE(w.feature_tools().apply());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.undo());
  CHECK(snapshot(w.session()) == modern);
}

TEST_CASE("Invalid path tolerance and Cancel preserve source history and selector context",
          "[gui][parity-b166][path-policy-shell]") {
  ShellWindow w;
  fixture(w);
  const auto before = snapshot(w.session());
  const auto source_label = std::string(w.session().undo_label());
  const auto mask = selection_kind_bit(GuiSelectionKind::SketchEntity);
  const std::optional<std::vector<std::string>> scope =
      std::vector<std::string>{"path/driven.path"};
  w.viewport().set_selection_filter_mask(mask);
  w.viewport().set_selection_semantic_scope(scope);
  begin(w);
  for (const auto* invalid : {"0", "-1", "2", "nan", "inf", "1e-10", "unfinished"}) {
    tolerance(w).setText(QString::fromUtf8(invalid));
    CHECK(tolerance(w).text() == QString::fromUtf8(invalid));
    CHECK_FALSE(apply(w).isEnabled());
    CHECK_FALSE(w.feature_tools().apply());
    CHECK(snapshot(w.session()) == before);
  }
  tolerance(w).setText("1.234567891234e-5");
  CHECK(apply(w).isEnabled());
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto file = dir.filePath("during-preview.blcad.json");
  REQUIRE(w.save_to(file));
  GuiDocumentSession saved;
  REQUIRE(saved.open_file(file.toStdString()));
  CHECK(snapshot(saved) == before);
  w.feature_tools().cancel();
  CHECK(snapshot(w.session()) == before);
  CHECK(w.session().undo_label() == source_label);
  CHECK(w.viewport().selection_filter_mask() == mask);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  CHECK(w.viewport().feature_preview_count() == 0);
  REQUIRE(w.undo());
  CHECK(w.session().part_document()->path_curves().empty());
}

TEST_CASE("Native path comparison retains invalid keyboard input and visible pinned actions",
          "[gui][parity-b166][path-policy-shell][native-path-policy]") {
  if (QApplication::platformName() != QStringLiteral("xcb"))
    SKIP("Requires native path preview");
  ShellWindow w;
  fixture(w);
  w.resize(qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800));
  w.show();
  w.raise();
  w.activateWindow();
  settle();
  const auto before = snapshot(w.session());
  REQUIRE(w.feature_tools().begin_spatial(Command::PathCurve));
  target(w).setFocus();
  REQUIRE(target(w).hasFocus());
  const int row = target(w).findData(QStringLiteral("driven.path"));
  REQUIRE(row > 0);
  for (int i = 0; i < row; ++i)
    key(target(w), Qt::Key_Down);
  CHECK(target(w).currentData().toString() == QStringLiteral("driven.path"));
  policy(w).setFocus();
  key(policy(w), Qt::Key_Down);
  CHECK(policy(w).currentIndex() == 1);
  tolerance(w).setFocus();
  REQUIRE(tolerance(w).hasFocus());
  tolerance(w).selectAll();
  key(tolerance(w), Qt::Key_0, QStringLiteral("0"));
  key(tolerance(w), Qt::Key_Return);
  CHECK(tolerance(w).text() == QStringLiteral("0"));
  CHECK_FALSE(apply(w).isEnabled());
  CHECK(snapshot(w.session()) == before);
  tolerance(w).selectAll();
  for (QChar c : QStringLiteral("1e-5"))
    key(tolerance(w), c.toUpper().unicode(), QString(c));
  key(tolerance(w), Qt::Key_Return);
  REQUIRE(apply(w).isEnabled());
  CHECK(summary(w).text().contains("32 spans"));
  CHECK(summary(w).text().contains("1 spans"));
  w.viewport().fit_feature_preview();
  settle();
  auto& scroll = widget<QScrollArea>(w, "blcad.feature.inputs_scroll");
  CHECK(scroll.horizontalScrollBar()->maximum() == 0);
  CHECK(apply(w).isVisible());
  CHECK(apply(w).visibleRegion().boundingRect().height() == apply(w).height());
  CHECK(summary(w).height() >= summary(w).heightForWidth(summary(w).width()));
  auto& label = widget<QLabel>(w, "blcad.spatial.modeling_tolerance_label");
  INFO("Tolerance label " << label.width() << 'x' << label.height() << ", required height "
                          << label.heightForWidth(label.width()) << ", min/max height "
                          << label.minimumHeight() << '/' << label.maximumHeight());
  CHECK(label.isVisible());
  CHECK_FALSE(label.visibleRegion().isEmpty());
  CHECK(label.height() >= label.heightForWidth(label.width()));
  CHECK(w.viewport().feature_preview_replaces("body/driven.body"));
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty()) {
    REQUIRE(QDir().mkpath(directory));
    CHECK(w.grab().save(QDir(directory).filePath("path-policy-comparison.png")));
  }
  apply(w).click();
  REQUIRE_FALSE(w.feature_tools().active());
  CHECK(w.session()
            .part_document()
            ->find_path_curve(PathCurveId("driven.path"))
            ->execution_policy()
            .tolerance_mm() == 1e-5);
  history(w, before);
}

TEST_CASE("Path comparison rejects stale controller intent after an upstream edit",
          "[gui][parity-b166][path-policy-shell]") {
  ShellWindow w;
  fixture(w);
  GuiInteractivePathSweepController controller;
  REQUIRE(controller.begin_path_policy(w.session(), PathCurveId("driven.path")));
  controller.set_execution_policy(f::checked(PathExecutionPolicy::curve_preserving(1e-5)));
  REQUIRE(controller.preview_shape(w.session()));
  REQUIRE(w.session().commit_part_transaction("Change input", [](PartDocument& part) {
    auto changed =
        part.set_parameter_value(ParameterId("depth"), f::checked(Quantity::length_mm(6)));
    return changed ? Result<std::size_t>::success(changed.value().size())
                   : Result<std::size_t>::failure(changed.error());
  }));
  const auto before = snapshot(w.session());
  const auto label = std::string(w.session().undo_label());
  CHECK_FALSE(controller.preview(w.session()));
  CHECK_FALSE(controller.preview_shape(w.session()));
  CHECK_FALSE(controller.apply(w.session()));
  CHECK(snapshot(w.session()) == before);
  CHECK(w.session().undo_label() == label);
}
