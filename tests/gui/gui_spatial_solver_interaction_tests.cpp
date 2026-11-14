#include "../../src/gui/shell/spatial_sketch_task_context.hpp"
#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_3d_interaction.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
template <class T> T& control(ShellWindow& window, const char* name) {
  auto* value = window.findChild<T*>(QStringLiteral("blcad.sketch3d.") + name);
  REQUIRE(value);
  return *value;
}
std::string snapshot(const GuiDocumentSession& session) {
  return f::checked(serialize_part_document_to_json(*session.part_document()));
}
void select_point(ShellWindow& window, const char* id) {
  auto& box = control<QComboBox>(window, "point");
  const auto row = box.findData(QString::fromUtf8(id));
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
}
void type_point(ShellWindow& window, Point3 value) {
  control<QDoubleSpinBox>(window, "x").setValue(value.x);
  control<QDoubleSpinBox>(window, "y").setValue(value.y);
  control<QDoubleSpinBox>(window, "z").setValue(value.z);
  control<QPushButton>(window, "stage_point").click();
}
} // namespace
TEST_CASE("Spatial typed point edits solve the remaining wire and commit exact reversible intent",
          "[gui][parity-b165][spatial-solver-task]") {
  ShellWindow window;
  REQUIRE(window.new_part("Spatial pins"));
  REQUIRE(window.session().commit_part_transaction("Source", [](PartDocument& part) {
    part = f::part();
    auto owner = f::checked(Sketch3D::create(Sketch3DId("wire"), "Wire"));
    (void)f::checked(owner.add_point(f::point("a", {})));
    (void)f::checked(owner.add_point(f::point("b", {0, 0, 10})));
    (void)f::checked(owner.add_line(f::checked(
        SketchLine3D::create(SketchEntityId("line"), SketchEntityId("a"), SketchEntityId("b")))));
    (void)f::checked(owner.add_constraint(f::checked(
        SpatialConstraint::create(SketchConstraintId("length"), SpatialConstraintKind::Distance,
                                  {f::target("a"), f::target("b")}, f::length(10)))));
    return part.add_sketch_3d(owner);
  }));
  const auto before = snapshot(window.session());
  window.refresh();
  REQUIRE(window.sketch_3d_tools().begin_edit(Sketch3DId("wire")));
  select_point(window, "b");
  const Point3 requested{3.123456789, 4.123456789, 12.123456789};
  type_point(window, requested);
  CHECK(snapshot(window.session()) == before);
  CHECK(control<QLabel>(window, "solver_info").text().contains("5 total"));
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto after = snapshot(window.session());
  REQUIRE(after != before);
  const auto* owner = window.session().part_document()->find_sketch_3d(Sketch3DId("wire"));
  const auto position = [&](const char* id) {
    const auto* p = owner->find_point(SketchEntityId(id));
    return Point3{p->x().explicit_coordinate()->millimeters(),
                  p->y().explicit_coordinate()->millimeters(),
                  p->z().explicit_coordinate()->millimeters()};
  };
  CHECK(position("b") == requested);
  CHECK(distance(position("a"), position("b")) == Catch::Approx(10).margin(1e-7));
  REQUIRE(window.session().undo());
  CHECK(snapshot(window.session()) == before);
  REQUIRE(window.session().redo());
  CHECK(snapshot(window.session()) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(window.session().save_as(directory.filePath("wire.blcad.json").toStdString()));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(directory.filePath("wire.blcad.json").toStdString()));
  CHECK(snapshot(reopened) == after);
}
TEST_CASE("Spatial conflicting typed edits stay correctable without losing prior local edits",
          "[gui][parity-b165][spatial-solver-task]") {
  ShellWindow window;
  REQUIRE(window.new_part("Spatial conflict"));
  REQUIRE(window.session().commit_part_transaction("Source", [](PartDocument& part) {
    part = sf::straight_sweep();
    auto owner = *part.find_sketch_3d(Sketch3DId("driven"));
    (void)f::checked(owner.add_point(f::point("free", {1, 1, 1})));
    return part.update_sketch_3d(owner);
  }));
  window.refresh();
  const auto before = snapshot(window.session());
  REQUIRE(window.sketch_3d_tools().begin_edit(Sketch3DId("driven")));
  select_point(window, "free");
  type_point(window, {2, 3, 4});
  select_point(window, "b");
  type_point(window, {0, 0, 12});
  CHECK(control<QDoubleSpinBox>(window, "z").value() == 12);
  CHECK_FALSE(window.sketch_3d_tools().preview());
  CHECK(snapshot(window.session()) == before);
  select_point(window, "free");
  select_point(window, "b");
  CHECK(control<QDoubleSpinBox>(window, "z").value() == 10);
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto* owner = window.session().part_document()->find_sketch_3d(Sketch3DId("driven"));
  CHECK(owner->find_point(SketchEntityId("free"))->z().explicit_coordinate()->millimeters() == 4);
  CHECK(owner->find_point(SketchEntityId("b"))->z().explicit_coordinate()->millimeters() == 10);
  CHECK(window.session()
            .part_document()
            ->find_parameter(ParameterId("spatial.length"))
            ->value()
            .millimeters() == 10);
  REQUIRE(window.session().undo());
  CHECK(snapshot(window.session()) == before);
}
TEST_CASE("Spatial task context resolves native support before consumers and rejects source cycles",
          "[gui][parity-b165][spatial-task-context]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("task.part"), "Task"));
  REQUIRE(session.commit_part_transaction("Source", [](PartDocument& part) {
    part = sf::straight_sweep();
    return Result<std::size_t>::success(1);
  }));
  const auto before = snapshot(session);
  auto context = f::checked(SpatialSketchTaskContext::create(session, Sketch3DId("driven"), {}));
  auto candidate = *session.part_document()->find_sketch_3d(Sketch3DId("driven"));
  REQUIRE(candidate.remove_constraint(SketchConstraintId("anchor")));
  REQUIRE(candidate.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("support"), SpatialConstraintKind::PointOnFace,
                                {f::target("a"), f::face()}))));
  const auto solved = context->solve(candidate);
  INFO(solved.diagnostic);
  REQUIRE(solved.accepted());
  CHECK(solved.sketch.find_point(SketchEntityId("a"))->z().explicit_coordinate()->millimeters() ==
        Catch::Approx(5).margin(1e-7));
  const auto own_face = f::checked(SpatialConstraintTarget::face(f::checked(
      FaceReference::create_planar(PartFeatureInputRole::SpatialConstraint,
                                   f::checked(SemanticFaceReference::create(
                                       FeatureId("driven.sweep"), SemanticFace::Top))))));
  REQUIRE(candidate.replace_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("support"), SpatialConstraintKind::PointOnFace,
                                {f::target("a"), own_face}))));
  const auto failed = context->solve(candidate);
  CHECK_FALSE(failed.accepted());
  CHECK(snapshot(session) == before);
}
