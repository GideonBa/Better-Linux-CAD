#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"
#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <QTemporaryDir>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <utility>

using namespace blcad;
using namespace blcad::gui;
namespace {
const SketchId sid("g2.sketch");
auto snapshot(const GuiDocumentSession& session) {
  return std::pair{serialize_part_document_to_json(*session.part_document()).value(),
                   std::pair{serialize_sketch_constraint_catalog_to_json(
                                 session.sketch_constraint_catalog(sid).value())
                                 .value(),
                             serialize_sketch_dimension_catalog_to_json(
                                 session.sketch_dimension_catalog(sid).value())
                                 .value()}};
}
SketchTopology topology(const GuiDocumentSession& session) {
  return SketchTopology::migrate_legacy(*session.part_document()->find_sketch(sid)).value();
}
void publish(GuiDocumentSession& session, const SketchConstraintIntent& intent) {
  auto controller = GuiSketchConstraintController::create(session, sid, intent);
  REQUIRE(controller);
  REQUIRE(controller.value().preview().authoring.accepted);
  REQUIRE(controller.value().commit(session));
}
void seed(GuiDocumentSession& session) {
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("g2.part"), "G2 catalog"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(sid, "G2", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_entity(
      SplineSegment::create_cubic_bezier(SketchEntityId("spline"), {0, 0}, {2, 0}, {4, 1}, {6, 3})
          .value()));
  REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId("line"), {0, 0}, {-5, 0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch)));
  const auto source = topology(session);
  const auto spline = source.find_entity("entity/spline")->points();
  const auto line = source.find_entity("entity/line")->points();
  std::size_t index = 0;
  for (const auto& point : {spline[0], spline[1], spline[3], line[1]})
    publish(session,
            SketchConstraintIntent::create(SketchConstraintId("fixed." + std::to_string(index++)),
                                           SketchSolverConstraintKind::Fixed,
                                           {SketchConstraintIntentTarget::point(point).value()})
                .value());
}
SketchConstraintIntent smooth(const GuiDocumentSession& session) {
  const auto source = topology(session);
  return SketchConstraintIntent::create(
             SketchConstraintId("smooth"), SketchSolverConstraintKind::Smooth,
             {SketchConstraintIntentTarget::entity("entity/spline").value(),
              SketchConstraintIntentTarget::point(source.find_entity("entity/spline")->points()[0])
                  .value(),
              SketchConstraintIntentTarget::entity("entity/line").value(),
              SketchConstraintIntentTarget::point(source.find_entity("entity/line")->points()[0])
                  .value()})
      .value();
}
} // namespace

TEST_CASE("Accepted G2 catalogs survive final drag release exact history and reopening",
          "[gui][parity-b159][sketch-smooth-drag]") {
  GuiDocumentSession session;
  seed(session);
  const auto before = snapshot(session);
  auto controller = GuiSketchConstraintController::create(session, sid, smooth(session));
  REQUIRE(controller);
  CHECK(controller.value().preview().glyph.token == "G2");
  REQUIRE(controller.value().preview().authoring.accepted);
  REQUIRE(controller.value().commit(session));
  const auto constrained = snapshot(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == constrained);
  auto drag = GuiSketchDragController::create(session, sid);
  REQUIRE(drag);
  CHECK(drag.value().baseline_solve().remaining_dof == 1);
  const auto free = topology(session).find_entity("entity/spline")->points()[2];
  const auto handle = std::find_if(drag.value().handles().begin(), drag.value().handles().end(),
                                   [&](const auto& item) { return item.point_id == free; });
  REQUIRE(handle != drag.value().handles().end());
  REQUIRE(drag.value().begin(handle->id));
  REQUIRE(drag.value().queue_pointer({5, 0}));
  REQUIRE(drag.value().process_pending());
  CHECK(snapshot(session) == constrained);
  REQUIRE(drag.value().flush({6, 0}));
  REQUIRE(drag.value().commit_session_constraints(session));
  const auto accepted = snapshot(session);
  CHECK(session.part_document()
            ->find_sketch(sid)
            ->find_spline_segment(SketchEntityId("spline"))
            ->control2()
            .x == Catch::Approx(6).margin(1e-7));
  REQUIRE(session.undo());
  CHECK(snapshot(session) == constrained);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == accepted);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("g2.blcad.json").toStdString();
  REQUIRE(session.save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(snapshot(reopened) == accepted);
  auto invalid = GuiSketchDragController::create(reopened, sid);
  REQUIRE(invalid);
  const auto bad_handle =
      std::find_if(invalid.value().handles().begin(), invalid.value().handles().end(),
                   [&](const auto& item) { return item.point_id == free; });
  REQUIRE(bad_handle != invalid.value().handles().end());
  REQUIRE(invalid.value().begin(bad_handle->id));
  CHECK_FALSE(invalid.value().flush({6, 1}));
  CHECK(snapshot(reopened) == accepted);
  CHECK_FALSE(invalid.value().commit_session_constraints(reopened));
}

TEST_CASE("G2 authoring composes driving sidecars and refuses stale or conflicting previews",
          "[gui][parity-b159][sketch-smooth-dimensions]") {
  for (bool conflict : {false, true}) {
    CAPTURE(conflict);
    GuiDocumentSession session;
    seed(session);
    const auto source = topology(session);
    const auto spline = source.find_entity("entity/spline")->points();
    REQUIRE(session.commit_part_dimension_transaction(
        "Bind G2 dimension",
        [&](PartDocument& part,
            std::vector<SketchDimensionCatalog>& catalogs) -> Result<std::size_t> {
          auto added = part.add_parameter(
              Parameter::create_length(ParameterId("drive"), "Drive",
                                       Quantity::length_mm(conflict ? 1 : 4).value())
                  .value());
          if (!added)
            return added;
          auto catalog = SketchDimensionCatalog::create(
                             part.id(), sid,
                             {SketchDimensionIntent::create(
                                  SketchDimensionId("dimension"),
                                  conflict ? SketchDimensionKind::VerticalDistance
                                           : SketchDimensionKind::HorizontalDistance,
                                  SketchDimensionMode::Driving,
                                  {SketchConstraintIntentTarget::point(spline[0]).value(),
                                   SketchConstraintIntentTarget::point(spline[2]).value()},
                                  ParameterId("drive"))
                                  .value()})
                             .value();
          catalogs.push_back(catalog);
          return Result<std::size_t>::success(1);
        }));
    const auto before = snapshot(session);
    auto controller = GuiSketchConstraintController::create(session, sid, smooth(session));
    REQUIRE(controller);
    CHECK(controller.value().preview().authoring.accepted == !conflict);
    CHECK(snapshot(session) == before);
    if (conflict) {
      CHECK_FALSE(controller.value().preview().authoring.solved_sketch);
      CHECK(controller.value().preview().authoring.catalog_after ==
            session.sketch_constraint_catalog(sid).value());
      CHECK_FALSE(controller.value().commit(session));
      CHECK(snapshot(session) == before);
    } else {
      CHECK(controller.value().preview().authoring.solve.remaining_dof == 0);
      auto stale = GuiSketchConstraintController::create(session, sid, smooth(session));
      REQUIRE(stale);
      REQUIRE(controller.value().commit(session));
      const auto accepted = snapshot(session);
      CHECK(topology(session).find_point(spline[2])->position().x == Catch::Approx(4).margin(1e-7));
      CHECK(topology(session).find_point(spline[2])->position().y == Catch::Approx(0).margin(1e-7));
      REQUIRE(session.undo());
      CHECK(snapshot(session) == before);
      REQUIRE(session.redo());
      CHECK(snapshot(session) == accepted);
      REQUIRE(session.undo());
      REQUIRE(session.commit_part_transaction("Change bound value", [](PartDocument& part) {
        auto changed =
            part.set_parameter_value(ParameterId("drive"), Quantity::length_mm(5).value());
        if (!changed)
          return Result<std::size_t>::failure(changed.error());
        return Result<std::size_t>::success(1);
      }));
      const auto changed = snapshot(session);
      CHECK_FALSE(stale.value().commit(session));
      CHECK(snapshot(session) == changed);
    }
  }
}
