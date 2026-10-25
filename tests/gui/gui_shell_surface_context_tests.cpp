#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QToolButton>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace {
using Command = ShellFeatureTools::Command;
using Quad = std::array<Point3, 4>;
template <typename T> auto checked(T value) {
  INFO((value.has_error() ? value.error().message() : "ok"));
  REQUIRE(value);
  return value.value();
}
template <typename T> T& control(ShellWindow& window, const char* id) {
  auto* value = window.findChild<T*>(QString::fromLatin1(id));
  REQUIRE(value);
  return *value;
}
std::string snapshot(ShellWindow& window) {
  return checked(serialize_part_document_to_json(*window.session().part_document()));
}
bool curves(Command command) {
  return command == Command::BoundarySurface || command == Command::FillSurface;
}
const char* name(Command command) {
  switch (command) {
  case Command::BoundarySurface:
    return "surface_boundary";
  case Command::FillSurface:
    return "surface_fill";
  case Command::TrimSurface:
    return "surface_trim";
  case Command::ExtendSurface:
    return "surface_extend";
  case Command::SurfaceStitch:
    return "surface_stitch";
  default:
    return "surface_to_solid";
  }
}
QAction& action(ShellWindow& window, const char* id) {
  return control<QAction>(window, (std::string("blcad.action.context_") + id).c_str());
}
void context(ShellWindow& window, const char* id) {
  auto& button = control<QToolButton>(window, (std::string("blcad.context.") + id).c_str());
  REQUIRE(button.isVisible());
  REQUIRE(button.isEnabled());
  button.click();
  qApp->processEvents();
}
SketchCoordinate3D coordinate(double value) {
  return checked(
      SketchCoordinate3D::create_explicit(checked(Quantity::linear_displacement_mm(value))));
}
void quad(PartDocument& part, const std::string& name, const Quad& points, bool fill) {
  const Sketch3DId id("sketch3d." + name);
  auto sketch = checked(Sketch3D::create(id, name));
  std::vector<PathSegmentReference> segments;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto& p = points[i];
    REQUIRE(sketch.add_point(
        checked(SketchPoint3D::create(SketchEntityId("p" + std::to_string(i)), coordinate(p.x),
                                      coordinate(p.y), coordinate(p.z)))));
  }
  for (int i = 0; i < 4; ++i) {
    const SketchEntityId line("line." + std::to_string(i));
    REQUIRE(sketch.add_line(
        checked(SketchLine3D::create(line, SketchEntityId("p" + std::to_string(i)),
                                     SketchEntityId("p" + std::to_string((i + 1) % 4))))));
    segments.push_back(
        checked(PathSegmentReference::create_sketch_3d(id, line, Sketch3DPathCurveKind::Line)));
  }
  REQUIRE(part.add_sketch_3d(std::move(sketch)));
  REQUIRE(part.add_path_curve(
      checked(PathCurve::create(PathCurveId("path." + name), name + " boundary", segments,
                                PathClosure::Closed, PathOrientationRule::MinimumTwist))));
  REQUIRE(part.add_path_curve(
      checked(PathCurve::create(PathCurveId("edge." + name), name + " edge", {segments.front()},
                                PathClosure::Open, PathOrientationRule::MinimumTwist))));
  if (!fill)
    return;
  const BodyId body("body." + name);
  REQUIRE(part.add_body(checked(Body::create(body, name, BodyKind::Surface))));
  REQUIRE(part.add_surface_feature(checked(FillSurfaceFeature::create(
      FeatureId("feature." + name), name,
      {checked(BoundaryCurveReference::create_path_curve(PathCurveId("path." + name)))}, body))));
}

void fixture(ShellWindow& window) {
  REQUIRE(window.new_part("Surface context"));
  REQUIRE(window.session().commit_part_transaction("Surface sources", [](PartDocument& part) {
    quad(part, "a", {{{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}}}, true);
    quad(part, "b", {{{10, 0, 0}, {20, 0, 0}, {20, 10, 0}, {10, 10, 0}}}, true);
    quad(part, "solo", {{{0, 20, 0}, {10, 20, 0}, {10, 30, 0}, {0, 30, 0}}}, false);
    quad(part, "trim", {{{12, 2, 0}, {18, 2, 0}, {18, 8, 0}, {12, 8, 0}}}, false);
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId("size"), "Size", checked(Quantity::length_mm(10))))));
    auto sketch = checked(Sketch::create(SketchId("box"), "Box", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
        ProfileId("box"), ParameterId("size"), ParameterId("size"), {45, 5}))));
    REQUIRE(part.add_sketch(std::move(sketch)));
    REQUIRE(part.add_body(checked(Body::create(BodyId("solid"), "Solid", BodyKind::Solid))));
    auto feature = checked(Feature::create_additive_extrude(FeatureId("feature.box"), "Box",
                                                            SketchId("box"), ParameterId("size")));
    REQUIRE(part.add_feature(
        checked(feature.with_body_result_context(checked(FeatureBodyResultContext::create(
            FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("solid")))))));
    return Result<std::size_t>::success(1);
  }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
}
ShellFeatureTools::SurfaceContext select(ShellWindow& window, Command command) {
  const std::string id = command == Command::BoundarySurface ? "path/edge.b"
                         : command == Command::FillSurface   ? "path/path.b"
                                                             : "body/body.b";
  REQUIRE(window.viewport().select_semantic(id));
  const auto selected = window.feature_tools().surface_context(command);
  REQUIRE(selected);
  CHECK(selected->selection.semantic_id == id);
  return *selected;
}
} // namespace

TEST_CASE(
    "Surface contexts verify typed path or surface identity and reject incompatible selections",
    "[gui][block136-surface-context]") {
  ShellWindow window;
  fixture(window);
  select(window, Command::FillSurface);
  CHECK(action(window, "surface_fill").isEnabled());
  CHECK(action(window, "surface_boundary").isEnabled());
  CHECK_FALSE(action(window, "surface_trim").isVisible());
  const auto source = select(window, Command::TrimSurface);
  for (const auto id : {"surface_trim", "surface_extend", "surface_stitch", "surface_to_solid"})
    CHECK(action(window, id).isEnabled());
  CHECK_FALSE(window.feature_tools().surface_context(Command::Extrude));
  auto forged = source;
  forged.source = checked(SurfaceReference::create_body(BodyId("body.a")));
  CHECK_FALSE(window.feature_tools().surface_context_available(forged));
  REQUIRE(window.session().selection().add({GuiSelectionKind::Body, "body/body.a"}));
  window.refresh();
  CHECK_FALSE(action(window, "surface_trim").isVisible());
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::Face, "body/body.b"}));
  CHECK_FALSE(window.feature_tools().surface_context(Command::TrimSurface));
  select(window, Command::TrimSurface);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"body/body.a"});
  REQUIRE(window.session().selection().add({GuiSelectionKind::Body, "body/body.b"}));
  CHECK_FALSE(window.feature_tools().surface_context(Command::TrimSurface));
}

TEST_CASE("Surface context cancellation restores complete selection and prior role scope",
          "[gui][block136-surface-context]") {
  const auto command =
      GENERATE(Command::BoundarySurface, Command::FillSurface, Command::TrimSurface,
               Command::ExtendSurface, Command::SurfaceStitch, Command::ClosedShellToSolid);
  const auto route = GENERATE(0, 1, 2);
  CAPTURE(name(command), route);
  ShellWindow window;
  fixture(window);
  const auto source = select(window, command);
  const auto mask = selection_kind_bit(source.selection.kind);
  window.viewport().set_selection_filter_mask(mask);
  window.viewport().set_selection_semantic_scope(
      std::vector<std::string>{source.selection.semantic_id});
  const auto scope = window.viewport().selection_semantic_scope();
  const auto before = snapshot(window);
  context(window, name(command));
  auto& tools = window.feature_tools();
  REQUIRE(tools.select_surface_context(source));
  REQUIRE(tools.select_surface_context(source));
  CHECK(window.session().selection().empty());
  if (curves(command) || command == Command::SurfaceStitch) {
    REQUIRE(tools.surface_order_editor().count() == 1);
    CHECK(tools.surface_order_editor().item(0)->text() ==
          (command == Command::BoundarySurface ? "b edge"
           : command == Command::FillSurface   ? "b boundary"
                                               : "b"));
  } else
    CHECK(tools.surface_target_editor().currentText() == "b");
  if (command == Command::TrimSurface || command == Command::ExtendSurface) {
    CHECK(tools.surface_pick_editor().currentIndex() == 1);
    CHECK_FALSE(window.viewport().select_semantic("body/body.a"));
    REQUIRE(window.viewport().select_semantic(command == Command::TrimSurface ? "path/path.trim"
                                                                              : "path/edge.b"));
  }
  CHECK(snapshot(window) == before);
  if (route == 0)
    context(window, "cancel");
  else if (route == 1)
    control<QPushButton>(window, "blcad.feature.cancel").click();
  else {
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escape);
  }
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  REQUIRE(window.viewport().selected_semantic());
  CHECK(window.viewport().selected_semantic()->semantic_id == source.selection.semantic_id);
  REQUIRE(window.session().selection().size() == 1);
  CHECK(window.session().selection().entries().front().semantic_id == source.selection.semantic_id);
  CHECK(action(window, name(command)).isEnabled());
}

TEST_CASE("Surface context Apply preserves exact first input with one atomic history entry",
          "[gui][block136-surface-context]") {
  const auto command =
      GENERATE(Command::BoundarySurface, Command::FillSurface, Command::TrimSurface,
               Command::ExtendSurface, Command::SurfaceStitch);
  ShellWindow window;
  fixture(window);
  const auto source = select(window, command);
  const auto before = snapshot(window);
  context(window, name(command));
  auto& tools = window.feature_tools();
  if (command == Command::BoundarySurface)
    REQUIRE(window.viewport().select_semantic("path/edge.solo"));
  else if (command == Command::TrimSurface || command == Command::ExtendSurface)
    REQUIRE(window.viewport().select_semantic(command == Command::TrimSurface ? "path/path.trim"
                                                                              : "path/edge.b"));
  else if (command == Command::SurfaceStitch)
    REQUIRE(window.viewport().select_semantic("body/body.a"));
  CHECK(snapshot(window) == before);
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.apply());
  const auto& feature = window.session().part_document()->surface_features().back();
  std::visit(
      [&](const auto& record) {
        if constexpr (requires { record.boundaries(); })
          CHECK(record.boundaries().front().stable_key() ==
                std::get<BoundaryCurveReference>(source.source).stable_key());
        else if constexpr (requires { record.surfaces(); })
          CHECK(record.surfaces().front().stable_key() ==
                std::get<SurfaceReference>(source.source).stable_key());
        else if constexpr (requires { record.target(); })
          CHECK(record.target().stable_key() ==
                std::get<SurfaceReference>(source.source).stable_key());
      },
      feature);
  CHECK_FALSE(window.viewport().selection_semantic_scope());
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
}

TEST_CASE("Surface contexts guard captured foreign hidden and removed sources",
          "[gui][block136-surface-context]") {
  ShellWindow window;
  fixture(window);
  const auto source = select(window, Command::TrimSurface);
  window.viewport().grab_interaction_mouse();
  CHECK_FALSE(action(window, "surface_trim").isVisible());
  action(window, "surface_trim").trigger();
  CHECK_FALSE(window.session().task().active());
  window.viewport().release_interaction_mouse();
  REQUIRE(window.session().task().begin("foreign"));
  window.refresh();
  action(window, "cancel").trigger();
  CHECK(window.session().task().command_id() == "foreign");
  REQUIRE(window.session().task().cancel());
  REQUIRE(window.session().commit_part_transaction("Hide source", [](PartDocument& part) {
    return part.set_body_visibility(BodyId("body.b"), BodyVisibility::Hidden);
  }));
  window.refresh();
  CHECK_FALSE(window.feature_tools().surface_context_available(source));
  CHECK_FALSE(action(window, "surface_trim").isVisible());
  REQUIRE(window.undo());
  REQUIRE(window.undo());
  CHECK_FALSE(window.feature_tools().surface_context_available(source));
  CHECK_FALSE(window.feature_tools().select_surface_context(source));
}

TEST_CASE("Surface context Save and replacement exclude disposable drafts from persisted intent",
          "[gui][block136-surface-context]") {
  const auto command =
      GENERATE(Command::BoundarySurface, Command::FillSurface, Command::TrimSurface,
               Command::ExtendSurface, Command::SurfaceStitch, Command::ClosedShellToSolid);
  ShellWindow window;
  fixture(window);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("surface-context.blcad");
  REQUIRE(window.save_to(path));
  select(window, command);
  const auto before = snapshot(window);
  context(window, name(command));
  REQUIRE(window.save_to(path));
  CHECK(window.session().task().active());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == before);
  SECTION("Open") {
    REQUIRE(window.open_path(path));
  }
  SECTION("New") {
    REQUIRE(window.new_part("Next"));
  }
  SECTION("Failed Open") {
    CHECK_FALSE(window.open_path(directory.filePath("missing.blcad")));
    CHECK(snapshot(window) == before);
  }
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
}

TEST_CASE("Surface contexts resolve stable solid subshapes and discard task-only targets on Cancel",
          "[gui][block136-surface-context]") {
  const bool edge = GENERATE(false, true);
  ShellWindow window;
  fixture(window);
  auto& filter = control<QComboBox>(window, "blcad.selection.filter");
  const auto kind = edge ? GuiSelectionKind::Edge : GuiSelectionKind::Face;
  filter.setCurrentIndex(filter.findData(selection_kind_bit(kind)));
  const auto targets = checked(geometry::FinishingSelectionBuilder::build(
      *window.session().part_document(), *window.session().part_shape_cache(), BodyId("solid"),
      edge ? PartFeatureInputRole::FilletEdge : PartFeatureInputRole::ShellRemovalFace));
  REQUIRE_FALSE(targets.empty());
  const auto id = targets.front().semantic_id;
  REQUIRE(window.viewport().select_semantic(id));
  const auto command = edge ? Command::BoundarySurface : Command::TrimSurface;
  const auto source = window.feature_tools().surface_context(command);
  REQUIRE(source);
  CHECK_FALSE(window.feature_tools().surface_context(Command::ClosedShellToSolid));
  context(window, name(command));
  REQUIRE(window.feature_tools().select_surface_context(*source));
  context(window, "cancel");
  REQUIRE(window.viewport().selected_semantic());
  CHECK(window.viewport().selected_semantic()->semantic_id == id);
  filter.setCurrentIndex(0);
  CHECK_FALSE(window.viewport().select_semantic(id));
  select(window, Command::TrimSurface);
  context(window, "surface_trim");
  context(window, "cancel");
  // All has no idle subshape presentations; the Surface task must remove its own.
  CHECK_FALSE(window.viewport().select_semantic(id));
}
