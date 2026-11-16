#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
namespace {
template <class T> T& control(ShellWindow& window, const char* name) {
  auto* value = window.findChild<T*>(name);
  REQUIRE(value);
  return *value;
}
std::string saved(ShellWindow& window) {
  return blcad::test::surface_offset::saved(*window.session().part_document());
}
void begin(ShellWindow& window, const char* id) {
  auto begun = window.feature_edit_tools().begin(FeatureId(id));
  INFO((begun ? "" : begun.error().message()));
  REQUIRE(begun);
  REQUIRE(window.feature_tools().active());
  INFO(control<QLabel>(window, "blcad.feature.summary").text().toStdString());
  REQUIRE(control<QPushButton>(window, "blcad.feature.apply").isEnabled());
}
} // namespace
TEST_CASE("Surface double-click editing loads the original typed controls and complete bindings",
          "[gui][advanced-surface-edit][parity-b173]") {
  const auto id = GENERATE("left", "left.extend", "tangent", "thick");
  ShellWindow window;
  REQUIRE(window.new_part("Edit Surface"));
  REQUIRE(window.session().commit_part_transaction("Inputs", [](PartDocument& p) {
    p = f::combination();
    const auto& old = std::get<TrimSurfaceFeature>(*p.find_surface_feature(FeatureId("left")));
    auto options = *old.options();
    options.direction = Vector3{0, 1.23456789012345, 0};
    return p.update_surface_feature(f::checked(old.with_options(options)));
  }));
  window.refresh();
  const auto before = saved(window);
  begin(window, id);
  auto& tools = window.feature_tools();
  if (std::string(id) == "left") {
    CHECK(control<QComboBox>(window, "blcad.surface.projection").currentIndex() == 1);
    CHECK(control<QLineEdit>(window, "blcad.surface.projection_1").text().toDouble() ==
          1.23456789012345);
  }
  if (std::string(id) == "tangent")
    CHECK(control<QComboBox>(window, "blcad.surface.extension_mode").currentIndex() == 1);
  if (std::string(id) == "thick")
    CHECK(control<QComboBox>(window, "blcad.surface.side").currentIndex() == 2);
  CHECK(saved(window) == before);
  tools.cancel();
  CHECK(saved(window) == before);
  begin(window, id);
  REQUIRE(tools.apply());
  CHECK(saved(window) == before);
}
TEST_CASE("Surface input selection and normal handles resolve the feature's historical Solid",
          "[gui][advanced-surface-edit][parity-b173]") {
  ShellWindow window;
  REQUIRE(window.new_part("Historical Surface"));
  REQUIRE(window.session().commit_part_transaction("Cut top skin", [](PartDocument& p) {
    p = blcad::test::spatial_fixture::part();
    blcad::test::surface_offset::length(p, "tolerance", 1e-6);
    f::checked(p.set_parameter_value(ParameterId("distance"), f::checked(Quantity::length_mm(2))));
    auto face = f::checked(FaceReference::create_planar(
        PartFeatureInputRole::SurfaceSourceFace,
        f::checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))));
    auto source = f::checked(SurfaceReference::create_faces(BodyId("base.body"), {face}));
    auto context = f::checked(
        FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("base.body"), {}));
    return p.add_surface_feature(
        f::checked(SurfaceOffsetFeature::create_thicken(FeatureId("cut"), "Cut skin", source,
                                                        {ParameterId("distance"),
                                                         ParameterId("tolerance"),
                                                         SurfaceOffsetSide::OppositeNormal,
                                                         SurfaceOffsetApproximation::ExactAnalytic,
                                                         {}},
                                                        context)));
  }));
  window.refresh();
  const auto before = saved(window);
  const auto volume = [&] {
    return blcad::test::integrated_solid_volume(
               *window.session().part_shape_cache()->find_body_shape(BodyId("base.body")))
        .volume_mm3;
  };
  CHECK(volume() == Catch::Approx(144).margin(1e-6));
  begin(window, "cut");
  auto& tools = window.feature_tools();
  const auto handles = tools.coordinator().surface().handles();
  REQUIRE(handles.size() == 1);
  CHECK(handles.front().origin.z == Catch::Approx(5).margin(1e-7));
  CHECK(handles.front().axis.z == Catch::Approx(-1).margin(1e-7));
  auto& sources = tools.surface_target_editor();
  bool has_input = false;
  sources.setCurrentIndex(0);
  for (int row = 1; row < sources.count(); ++row)
    if (sources.itemText(row).endsWith(" / top")) {
      sources.setCurrentIndex(row);
      has_input = true;
      break;
    }
  REQUIRE(has_input);
  REQUIRE(window.viewport().selected_semantic());
  CHECK(window.viewport().selected_semantic()->semantic_id.starts_with("surface-input/"));
  REQUIRE(tools.set_numeric_text("1.23456789012345 mm"));
  REQUIRE(tools.apply());
  CHECK(volume() == Catch::Approx(48 * (5 - 1.23456789012345)).margin(1e-6));
  const auto after = saved(window);
  REQUIRE(window.undo());
  CHECK(saved(window) == before);
  REQUIRE(window.session().redo());
  CHECK(saved(window) == after);
}
TEST_CASE(
    "Surface formula edits require explicit independent bindings and preserve shared formulas",
    "[gui][advanced-surface-edit][parity-b173]") {
  ShellWindow window;
  REQUIRE(window.new_part("Formula Surface"));
  REQUIRE(window.session().commit_part_transaction("Formula thickness", [](PartDocument& p) {
    p = f::combination();
    auto expression = p.add_expression_parameter(ParameterId("formula.wall"), "formula_wall",
                                                 ParameterType::Length, "wall * 1");
    if (!expression)
      return Result<std::size_t>::failure(expression.error());
    const auto& old = std::get<SurfaceOffsetFeature>(*p.find_surface_feature(FeatureId("thick")));
    auto parameters = old.parameters();
    parameters.distance = ParameterId("formula.wall");
    return p.update_surface_feature(f::checked(SurfaceOffsetFeature::create_thicken(
        old.id(), old.name(), old.target(), parameters, old.body_result_context())));
  }));
  window.refresh();
  const auto before = saved(window);
  begin(window, "thick");
  auto& tools = window.feature_tools();
  CHECK_FALSE(tools.set_numeric_text("1.5 mm"));
  CHECK(tools.coordinator().surface().distance() == 1);
  CHECK(saved(window) == before);
  control<QCheckBox>(window, "blcad.surface.override_formulas").setChecked(true);
  REQUIRE(tools.set_numeric_text("1.5 mm"));
  REQUIRE(control<QPushButton>(window, "blcad.feature.apply").isEnabled());
  REQUIRE(tools.apply());
  const auto* part = window.session().part_document();
  const auto& record =
      std::get<SurfaceOffsetFeature>(*part->find_surface_feature(FeatureId("thick")));
  CHECK(record.parameters().distance != ParameterId("formula.wall"));
  CHECK(part->find_parameter(record.parameters().distance)->value().millimeters() == 1.5);
  CHECK(part->find_parameter(ParameterId("formula.wall"))->formula() == "wall * 1");
  CHECK(part->find_parameter(ParameterId("formula.wall"))->value().millimeters() == 1);
  REQUIRE(window.undo());
  CHECK(saved(window) == before);
}
TEST_CASE("Native Surface browser editing preserves precise release and hidden historical sources",
          "[gui][advanced-surface-edit][native-advanced-surface-edit][parity-b173]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Surface editing");
  ShellWindow window;
  REQUIRE(window.new_part("Native Surface editing"));
  REQUIRE(window.session().commit_part_transaction("Inputs", [](PartDocument& p) {
    p = f::combination();
    for (const auto& body : std::vector<Body>(p.bodies().begin(), p.bodies().end()))
      if (body.id() != BodyId("thick.body"))
        f::checked(p.set_body_visibility(body.id(), BodyVisibility::Hidden));
    return Result<std::size_t>::success(1);
  }));
  window.refresh();
  auto size = qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800);
  window.resize(size.boundedTo(window.screen()->availableGeometry().size() - QSize(40, 40)));
  window.show();
  qApp->processEvents();
  window.viewport().fit_all();
  const auto original = saved(window);
  auto& tree = window.model_browser();
  tree.expandAll();
  QTreeWidgetItem* feature = nullptr;
  for (QTreeWidgetItemIterator it(&tree); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString() == "thick")
      feature = *it;
  REQUIRE(feature);
  tree.scrollToItem(feature);
  qApp->processEvents();
  const QPointF point = tree.visualItemRect(feature).center();
  for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease,
                          QEvent::MouseButtonDblClick, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, point, tree.viewport()->mapToGlobal(point.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(tree.viewport(), &event);
    qApp->processEvents();
  }
  auto& tools = window.feature_tools();
  REQUIRE(tools.active());
  REQUIRE(tools.surface_target_editor().currentText() == "Quilt");
  REQUIRE(window.session().part_document()->find_body(BodyId("quilt.body"))->visibility() ==
          BodyVisibility::Hidden);
  REQUIRE(control<QPushButton>(window, "blcad.feature.apply").isEnabled());
  QEventLoop frame;
  QTimer::singleShot(50, &frame, &QEventLoop::quit);
  frame.exec();
  const auto presentations = tools.manipulators().presentations();
  REQUIRE(presentations.size() == 1);
  const auto endpoint = presentations.front().endpoint;
  const auto send = [&](QEvent::Type type, GuiSketchScreenPoint point) {
    const QPointF local(point.x, point.y);
    QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &event);
    qApp->processEvents();
  };
  send(QEvent::MouseButtonPress, endpoint);
  REQUIRE(tools.manipulators().active_handle_id() == kSurfaceOffsetHandleId);
  const GuiSketchScreenPoint moved{endpoint.x + 8, endpoint.y - 8};
  send(QEvent::MouseMove, moved);
  REQUIRE(tools.set_numeric_text("1.23456789012345 mm"));
  send(QEvent::MouseButtonRelease, moved);
  CHECK(tools.coordinator().surface().distance() == 1.23456789012345);
  CHECK(saved(window) == original);
  auto& commit = control<QPushButton>(window, "blcad.feature.apply");
  REQUIRE(commit.isVisible());
  REQUIRE(window.childAt(commit.mapTo(&window, commit.rect().center())) == &commit);
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty()) {
    REQUIRE(QDir().mkpath(directory));
    CHECK(window.grab().save(QDir(directory).filePath("advanced-surface-edit.png")));
  }
  commit.click();
  REQUIRE_FALSE(tools.active());
  const auto after = saved(window);
  const auto actual =
      blcad::test::integrated_solid_volume(
          *window.session().part_shape_cache()->find_body_shape(BodyId("thick.body")))
          .volume_mm3;
  const auto area = (20 * std::acos(.8) + 3) * 7;
  CHECK(actual == Catch::Approx(area * 1.23456789012345).margin(5e-5));
  REQUIRE(window.undo());
  CHECK(saved(window) == original);
  REQUIRE(window.session().redo());
  CHECK(saved(window) == after);
}
TEST_CASE("Shared tolerance and deviation bindings cannot silently overwrite different UI values",
          "[gui][advanced-surface-edit][parity-b173]") {
  ShellWindow window;
  REQUIRE(window.new_part("Shared Surface budgets"));
  REQUIRE(window.session().commit_part_transaction("Shared budgets", [](PartDocument& p) {
    p = f::combination();
    const auto& old = std::get<ExtendSurfaceFeature>(*p.find_surface_feature(FeatureId("tangent")));
    auto options = *old.options();
    options.maximum_deviation = options.tolerance;
    return p.update_surface_feature(f::checked(old.with_options(options)));
  }));
  window.refresh();
  const auto before = saved(window);
  begin(window, "tangent");
  control<QLineEdit>(window, "blcad.surface.tolerance").setText("0.0000005");
  CHECK_FALSE(control<QPushButton>(window, "blcad.feature.apply").isEnabled());
  CHECK(control<QLabel>(window, "blcad.feature.summary").text().contains("Shared parameter"));
  CHECK(saved(window) == before);
  control<QCheckBox>(window, "blcad.surface.override_formulas").setChecked(true);
  REQUIRE(control<QPushButton>(window, "blcad.feature.apply").isEnabled());
  REQUIRE(window.feature_tools().apply());
  const auto* part = window.session().part_document();
  const auto& options =
      *std::get<ExtendSurfaceFeature>(*part->find_surface_feature(FeatureId("tangent"))).options();
  CHECK(options.tolerance != *options.maximum_deviation);
  CHECK(part->find_parameter(options.tolerance)->value().millimeters() == 5e-7);
  CHECK(part->find_parameter(*options.maximum_deviation)->value().millimeters() == 1e-6);
  REQUIRE(window.undo());
  CHECK(saved(window) == before);
}
TEST_CASE("Surface handles follow staged shared parameters that also drive the source",
          "[gui][advanced-surface-edit][parity-b173]") {
  ShellWindow window;
  REQUIRE(window.new_part("Shared source driver"));
  REQUIRE(window.session().commit_part_transaction("Inputs", [](PartDocument& p) {
    p = blcad::test::spatial_fixture::part();
    blcad::test::surface_offset::length(p, "tolerance", 1e-6);
    f::checked(
        p.add_body(f::checked(Body::create(BodyId("offset.body"), "Offset", BodyKind::Surface))));
    return p.add_surface_feature(f::checked(SurfaceOffsetFeature::create_offset(
        FeatureId("offset"), "Offset", blcad::test::surface_offset::plane(),
        {ParameterId("depth"),
         ParameterId("tolerance"),
         SurfaceOffsetSide::AlongNormal,
         SurfaceOffsetApproximation::ExactAnalytic,
         {}},
        f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {},
                                                    BodyId("offset.body"))))));
  }));
  window.refresh();
  const auto before = saved(window);
  begin(window, "offset");
  auto& tools = window.feature_tools();
  CHECK(tools.coordinator().surface().handles().front().origin.z == Catch::Approx(5).margin(1e-7));
  REQUIRE(tools.set_numeric_text("3 mm"));
  CHECK(tools.coordinator().surface().handles().front().origin.z == Catch::Approx(3).margin(1e-7));
  CHECK(tools.manipulators().handles().front().origin.z == Catch::Approx(3).margin(1e-7));
  const auto preview = f::checked(tools.coordinator().surface().preview_shape(window.session()));
  CHECK(blcad::test::distance_to_faces(preview, {0, 0, 6}) < 1e-7);
  CHECK(saved(window) == before);
  REQUIRE(tools.apply());
  CHECK(window.session()
            .part_document()
            ->find_parameter(ParameterId("depth"))
            ->value()
            .millimeters() == 3);
  CHECK(blcad::test::integrated_solid_volume(
            *window.session().part_shape_cache()->find_body_shape(BodyId("base.body")))
            .volume_mm3 == Catch::Approx(144).margin(1e-6));
  REQUIRE(window.undo());
  CHECK(saved(window) == before);
}
TEST_CASE("Repeated independent Surface values allocate unique names and preserve prior bindings",
          "[gui][advanced-surface-allocation][parity-b173]") {
  ShellWindow window;
  REQUIRE(window.new_part("Independent Surface values"));
  REQUIRE(window.session().commit_part_transaction("Inputs", [](PartDocument& p) {
    p = f::combination();
    return Result<std::size_t>::success(1);
  }));
  window.refresh();
  const auto before = saved(window);
  begin(window, "thick");
  control<QCheckBox>(window, "blcad.surface.override_formulas").setChecked(true);
  REQUIRE(window.feature_tools().set_numeric_text("1.1 mm"));
  REQUIRE(window.feature_tools().apply());
  const auto first =
      std::get<SurfaceOffsetFeature>(
          *window.session().part_document()->find_surface_feature(FeatureId("thick")))
          .parameters()
          .distance;
  begin(window, "thick");
  control<QCheckBox>(window, "blcad.surface.override_formulas").setChecked(true);
  REQUIRE(window.feature_tools().set_numeric_text("1.2 mm"));
  REQUIRE(window.feature_tools().apply());
  const auto* part = window.session().part_document();
  const auto second =
      std::get<SurfaceOffsetFeature>(*part->find_surface_feature(FeatureId("thick")))
          .parameters()
          .distance;
  CHECK(second != first);
  CHECK(part->find_parameter(first)->value().millimeters() == 1.1);
  CHECK(part->find_parameter(second)->value().millimeters() == 1.2);
  CHECK(part->find_parameter(first)->name() != part->find_parameter(second)->name());
  CHECK(part->find_parameter(ParameterId("wall"))->value().millimeters() == 1);
  REQUIRE(window.undo());
  REQUIRE(window.undo());
  CHECK(saved(window) == before);
}
