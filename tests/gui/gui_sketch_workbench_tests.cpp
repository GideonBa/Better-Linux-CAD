#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/main_window.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include "blcad/core/parameter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QTreeWidget>

using namespace blcad;
using namespace blcad::gui;

namespace {

SketchReferenceTarget line_target(const char* id) {
  return SketchReferenceTarget::create_line_segment(SketchEntityId(id)).value();
}

Parameter length_parameter(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}

Parameter count_parameter(const char* id, double value) {
  return Parameter::create_count(ParameterId(id), id, Quantity::count(value, id).value()).value();
}

QTreeWidgetItem* find_item(QTreeWidgetItem* item, QStringView id) {
  if (item->data(0, Qt::UserRole).toString() == id) return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_item(item->child(index), id)) return found;
  return nullptr;
}

} // namespace

TEST_CASE("Block 99 workbench creates datums construction geometry and editable planar sketches",
          "[gui][datum-workplane][gui][sketch-workbench]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_sketch"), "GUI Sketch"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(session.commit_part_transaction("Seed sketch parameters", [](PartDocument& part) {
    auto radius = part.add_parameter(length_parameter("pattern.radius", 20.0));
    if (radius.has_error()) return radius;
    auto count = part.add_parameter(count_parameter("pattern.count", 6.0));
    if (count.has_error()) return count;
    return part.add_parameter(length_parameter("pattern.diameter", 4.0));
  }));

  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z Axis", {0.0, 0.0, 0.0},
                                         {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(workbench.create_datum_axis(session, std::move(axis.value())));
  auto point = ConstructionPoint::create_explicit(ConstructionPointId("point.origin"), "Origin",
                                                   {0.0, 0.0, 0.0});
  auto construction_line = ConstructionLine::create_explicit(
      ConstructionLineId("construction.x"), "X Axis", {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0});
  REQUIRE(point);
  REQUIRE(construction_line);
  REQUIRE(workbench.create_construction_point(session, std::move(point.value())));
  REQUIRE(workbench.create_construction_line(session, std::move(construction_line.value())));

  auto sketch = Sketch::create(SketchId("sketch.gui"), "GUI Sketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));
  REQUIRE(workbench.add_line(session, SketchId("sketch.gui"),
                             LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {20.0, 0.0}).value()));
  REQUIRE(workbench.add_arc(session, SketchId("sketch.gui"),
                            ArcSegment::create_three_point(SketchEntityId("arc.a"), {0.0, 0.0},
                                                           {5.0, 5.0}, {10.0, 0.0}).value()));
  REQUIRE(workbench.add_spline(session, SketchId("sketch.gui"),
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.a"), {0.0, 0.0}, {2.0, 4.0},
                                         {8.0, 4.0}, {10.0, 0.0}).value()));
  REQUIRE(workbench.add_trim_extend(session, SketchId("sketch.gui"),
      SketchTrimExtendOperation::create_extend(SketchTrimOperationId("extend.a"),
                                                SketchEntityId("line.a"), {25.0, 0.0}).value()));
  REQUIRE(workbench.add_projected_point(session, SketchId("sketch.gui"),
      ProjectedSketchPoint::create_from_construction_point(SketchEntityId("projected.origin"),
                                                            ConstructionPointId("point.origin")).value()));
  REQUIRE(workbench.add_projected_line(session, SketchId("sketch.gui"),
      ProjectedSketchLine::create_from_construction_line(SketchEntityId("projected.x"),
                                                          ConstructionLineId("construction.x")).value()));
  REQUIRE(workbench.add_constraint(session, SketchId("sketch.gui"),
      SketchGeometricConstraint::create_horizontal(SketchConstraintId("constraint.horizontal"),
                                                    line_target("line.a")).value()));
  REQUIRE(workbench.add_tangent_continuity(session, SketchId("sketch.gui"),
      SketchTangentContinuity::create_tangent(SketchConstraintId("constraint.tangent"),
                                               SketchEntityId("line.a"),
                                               SketchEntityId("spline.a")).value()));
  REQUIRE(workbench.add_profile(session, SketchId("sketch.gui"),
      CircularHolePattern::create(ProfileId("pattern.holes"), ParameterId("pattern.radius"),
                                  ParameterId("pattern.count"),
                                  ParameterId("pattern.diameter")).value()));

  const auto plane = workbench.plane_view(session, SketchId("sketch.gui"));
  REQUIRE(plane);
  CHECK(plane.value().model_to_plane({4.0, 7.0, 0.0}) == Point2{4.0, 7.0});
  CHECK(plane.value().plane_to_model({4.0, 7.0}) == Point3{4.0, 7.0, 0.0});
  const auto* edited = session.part_document()->find_sketch(SketchId("sketch.gui"));
  REQUIRE(edited != nullptr);
  CHECK(edited->tangent_continuities().size() == 1);
  CHECK(edited->circular_hole_patterns().size() == 1);
  CHECK(session.can_undo());
}

TEST_CASE("Block 99 diagnostics and explicit repair preview remain atomic",
          "[gui][sketch-workbench][gui][sketch-repair]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_repair"), "GUI Repair"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(session,
      Sketch::create(SketchId("sketch.repair"), "Repair", DatumPlaneId("datum.xy")).value()));
  REQUIRE(workbench.add_line(session, SketchId("sketch.repair"),
      LineSegment::create(SketchEntityId("line.free"), {0.0, 0.0}, {10.0, 0.0}).value()));

  auto inspection = workbench.inspect(session, SketchId("sketch.repair"));
  REQUIRE(inspection);
  CHECK(inspection.value().diagnostics.warning_count() == 2);
  REQUIRE_FALSE(inspection.value().repairs.empty());
  const auto suggestion = inspection.value().repairs.suggestions().front();
  const auto before = session.part_document()->find_sketch(SketchId("sketch.repair"))
                          ->geometric_constraints().size();
  auto preview = workbench.preview_repair(session, SketchId("sketch.repair"), suggestion);
  REQUIRE(preview);
  CHECK(preview.value().applied());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.repair"))
            ->geometric_constraints().size() == before);
  REQUIRE(workbench.apply_repair(session, SketchId("sketch.repair"), suggestion));
  CHECK(session.part_document()->find_sketch(SketchId("sketch.repair"))
            ->geometric_constraints().size() == before + 1);
  REQUIRE(session.undo());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.repair"))
            ->geometric_constraints().size() == before);
}

TEST_CASE("Block 99 selection prompts name semantic capabilities",
          "[gui][sketch-workbench]") {
  CHECK(GuiSketchWorkbench::prompt_for(GuiSketchCommand::CreateSketch).required_capability ==
        "planar_workplane");
  CHECK(GuiSketchWorkbench::prompt_for(GuiSketchCommand::ProjectReference).required_capability ==
        "projectable_reference");
  CHECK(GuiSketchWorkbench::prompt_for(GuiSketchCommand::AddDimension).text.find("two compatible") !=
        std::string::npos);
}

TEST_CASE("Block 99 main window enters orthographic normal-to-plane sketch editing",
          "[gui][sketch-workbench][gui][datum-workplane]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  REQUIRE(window.session().create_part(DocumentId("part.camera"), "Camera"));
  REQUIRE(window.sketch_workbench().create_xy_datum(window.session(), DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(window.sketch_workbench().create_sketch(
      window.session(), Sketch::create(SketchId("sketch.camera"), "Camera Sketch",
                                        DatumPlaneId("datum.xy")).value()));
  window.refresh_command_state();

  auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("blcad.model_browser"));
  auto* edit = window.findChild<QAction*>(QStringLiteral("blcad.action.edit_sketch"));
  auto* viewport = window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"));
  REQUIRE(tree != nullptr);
  REQUIRE(edit != nullptr);
  REQUIRE(viewport != nullptr);
  QTreeWidgetItem* sketch_item = nullptr;
  for (int index = 0; index < tree->topLevelItemCount() && !sketch_item; ++index)
    sketch_item = find_item(tree->topLevelItem(index), u"sketch.camera");
  REQUIRE(sketch_item != nullptr);
  tree->setCurrentItem(sketch_item);
  edit->trigger();
  qApp->processEvents();

  REQUIRE(window.active_sketch().has_value());
  CHECK(window.active_sketch()->value() == "sketch.camera");
  REQUIRE(viewport->plane_camera().has_value());
  CHECK(viewport->plane_camera()->view_direction == Vector3{0.0, 0.0, 1.0});
  CHECK(viewport->projection() == GuiViewportProjection::Orthographic);
}
