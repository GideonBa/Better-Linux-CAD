#include "../support/equation_curve_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"

#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QDir>
#include <QScreen>
#include <QTemporaryDir>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <utility>

using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::equation_fixture;
namespace {
void seed(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("equation.part"), "Equations"));
  REQUIRE(session.commit_part_transaction("Seed equation intent", [](PartDocument& part) {
    part = f::part();
    return Result<std::size_t>::success(1);
  }));
}
std::string snapshot(const GuiDocumentSession& session) {
  return serialize_part_document_to_json(*session.part_document()).value();
}
GuiSketchDragController drag(GuiDocumentSession& session, bool catalogs = true) {
  auto result = catalogs
                    ? GuiSketchDragController::create(session, SketchId("planar"))
                    : GuiSketchDragController::create(*session.part_document(), SketchId("planar"));
  REQUIRE(result);
  auto controller = std::move(result.value());
  const auto end = controller.source_topology().find_entity("entity/line")->points()[1];
  const auto found = std::find_if(controller.handles().begin(), controller.handles().end(),
                                  [&](const auto& h) { return h.point_id == end; });
  REQUIRE(found != controller.handles().end());
  REQUIRE(controller.begin(found->id));
  REQUIRE(controller.flush({4, 2}));
  return controller;
}
} // namespace
TEST_CASE("Equation V8 sessions retain formulas through drag constraints exact history and reopen",
          "[gui][parity-b160][equation-session]") {
  GuiDocumentSession session;
  seed(session);
  const auto before = snapshot(session);
  const auto equations =
      session.part_document()->find_sketch(SketchId("planar"))->equation_curves();
  auto controller = drag(session);
  CHECK(snapshot(session) == before);
  REQUIRE(controller.commit_session_constraints(session));
  const auto moved = snapshot(session);
  CHECK(session.part_document()
            ->find_sketch(SketchId("planar"))
            ->find_line_segment(SketchEntityId("line"))
            ->end() == Point2{4, 2});
  CHECK(session.part_document()->find_sketch(SketchId("planar"))->equation_curves() == equations);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == moved);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("equations.blcad.json").toStdString();
  REQUIRE(session.save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(snapshot(reopened) == moved);
  auto intent = SketchConstraintIntent::create(
                    SketchConstraintId("horizontal"), SketchSolverConstraintKind::Horizontal,
                    {SketchConstraintIntentTarget::entity("entity/line").value()})
                    .value();
  auto constraint = GuiSketchConstraintController::create(reopened, SketchId("planar"), intent);
  REQUIRE(constraint);
  REQUIRE(constraint.value().preview().authoring.accepted);
  REQUIRE(constraint.value().commit(reopened));
  CHECK(reopened.part_document()->find_sketch(SketchId("planar"))->equation_curves() == equations);
  REQUIRE(constraint.value().undo(reopened));
  CHECK(snapshot(reopened) == moved);
}
TEST_CASE("Equation definition and input changes refuse stale drags without losing accepted edits",
          "[gui][parity-b160][equation-session]") {
  for (bool catalogs : {false, true})
    for (bool inputs : {false, true}) {
      CAPTURE(catalogs, inputs);
      GuiDocumentSession session;
      seed(session);
      const auto before = snapshot(session);
      auto controller = drag(session, catalogs);
      const std::string label = inputs ? "Edit equation input" : "Edit equation definition";
      REQUIRE(session.commit_part_transaction(
          label, [inputs](PartDocument& part) -> Result<std::size_t> {
            if (inputs) {
              auto changed =
                  part.set_parameter_value(ParameterId("count"), Quantity::count(4).value());
              if (!changed)
                return Result<std::size_t>::failure(changed.error());
              return Result<std::size_t>::success(changed.value().size());
            }
            auto sketch = *part.find_sketch(SketchId("planar"));
            auto edited = sketch.replace_equation_curve(f::planar("@{radius}/1mm"));
            if (!edited)
              return edited;
            return part.update_sketch(std::move(sketch));
          }));
      const auto edited = snapshot(session);
      CHECK(edited != before);
      const auto failed =
          catalogs ? controller.commit_session_constraints(session) : controller.commit(session);
      REQUIRE(failed.has_error());
      CHECK(failed.error().message().find("Equation") != std::string::npos);
      CHECK(snapshot(session) == edited);
      CHECK(session.undo_label() == label);
      REQUIRE(session.undo());
      CHECK(snapshot(session) == before);
      REQUIRE(session.redo());
      CHECK(snapshot(session) == edited);
    }
}

TEST_CASE("Derived equation profiles render and retain solid history in the native shell",
          "[gui][parity-b161][equation-native]") {
  ShellWindow window;
  REQUIRE(window.new_part("Equation consumers"));
  REQUIRE(window.session().commit_part_transaction(
      "Equation profile and solid", [](PartDocument& part) {
        auto sketch = f::checked(Sketch::create(SketchId("equation.sketch"), "Equation circle",
                                                DatumPlaneId("datum.xy")));
        REQUIRE(part.add_parameter(f::checked(Parameter::create_length(
            ParameterId("r"), "Radius", f::checked(Quantity::length_mm(10))))));
        REQUIRE(part.add_parameter(f::checked(Parameter::create_length(
            ParameterId("h"), "Height", f::checked(Quantity::length_mm(6))))));
        auto source = f::checked(EquationCurve::create(
            SketchEntityId("circle"), EquationCurveMode::Parametric2D, {"u", CurveUnit::Scalar},
            "0", "2*pi", {"@{r}*cos(u)", "@{r}*sin(u)", ""}, .001, EquationCurveClosure::Periodic));
        REQUIRE(sketch.add_entity(source));
        REQUIRE(sketch.add_profile(f::checked(
            ArcClosedProfile::create_for_sketch(ProfileId("profile"), {source.id()}, sketch))));
        REQUIRE(part.add_sketch(sketch));
        auto feature = f::checked(Feature::create_additive_extrude(
            FeatureId("extrude"), "Equation solid", sketch.id(), ParameterId("h")));
        REQUIRE(part.add_feature(f::checked(feature.with_profile(ProfileId("profile")))));
        return Result<std::size_t>::success(1);
      }));
  window.resize(1100, 760);
  if (QApplication::platformName() == "xcb") {
    const auto available =
        QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
    window.move(available.topLeft());
    window.resize(std::min(1100, available.width()), std::min(760, available.height()));
  }
  window.show();
  window.raise();
  window.activateWindow();
  window.refresh();
  QGuiApplication::sync();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  REQUIRE(window.viewport().select_semantic("sketch/equation.sketch/entity/circle"));
  const auto before = snapshot(window.session());
  REQUIRE(
      window.session().commit_part_transaction("Change equation radius", [](PartDocument& part) {
        auto result =
            part.set_parameter_value(ParameterId("r"), f::checked(Quantity::length_mm(12)));
        return result ? Result<std::size_t>::success(result.value().size())
                      : Result<std::size_t>::failure(result.error());
      }));
  window.refresh();
  const auto edited = snapshot(window.session());
  CHECK(edited != before);
  REQUIRE(window.session().undo());
  CHECK(snapshot(window.session()) == before);
  REQUIRE(window.session().redo());
  CHECK(snapshot(window.session()) == edited);
  window.refresh();
  QGuiApplication::sync();
  qApp->processEvents();
  const auto captures = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!captures.isEmpty()) {
    REQUIRE(QDir().mkpath(captures));
    REQUIRE(window.grab().save(captures + "/equation-solid.png"));
  }
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("equation-solid.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(snapshot(reopened) == edited);
}
