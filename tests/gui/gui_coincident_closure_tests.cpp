#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_sketch_constraints.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QTemporaryDir>

using namespace blcad;
using namespace blcad::gui;
using namespace blcad::geometry;

namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}

Sketch loop(double scale, double gap, bool mixed) {
  auto sketch = checked(Sketch::create(SketchId("loop"), "Loop", DatumPlaneId("datum.xy")));
  const auto point = [scale](double x, double y) { return Point2{x * scale, y * scale}; };
  checked(sketch.add_entity(
      checked(LineSegment::create(SketchEntityId("bottom"), point(0, 0), point(100, 0)))));
  checked(sketch.add_entity(
      checked(LineSegment::create(SketchEntityId("right"), point(100, 0), point(100, 50)))));
  if (mixed) {
    checked(sketch.add_entity(checked(ArcSegment::create_three_point(
        SketchEntityId("top"), point(100, 50), point(50, 75), point(0, 50)))));
    checked(sketch.add_entity(checked(SplineSegment::create_cubic_bezier(
        SketchEntityId("left"), point(0, 50), point(-10, 40), point(-10, 10), point(0, gap)))));
  } else {
    checked(sketch.add_entity(
        checked(LineSegment::create(SketchEntityId("top"), point(100, 50), point(0, 50)))));
    checked(sketch.add_entity(
        checked(LineSegment::create(SketchEntityId("left"), point(0, 50), point(0, gap)))));
  }
  return sketch;
}
} // namespace

TEST_CASE(
    "Committed Coincident repairs produce strict Finish regions across scales and curve families",
    "[gui][integration][coincident-closure]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  for (double scale : {0.01, 1.0, 1000.0}) {
    for (double gap : {1.0, 0.001, 0.000001, 0.00000005, 0.0}) {
      for (bool mixed : {false, true}) {
        CAPTURE(scale, gap, mixed);
        GuiDocumentSession session;
        checked(session.create_part(DocumentId("closure"), "Closure"));
        checked(session.commit_part_transaction("Prepare contour", [&](PartDocument& document) {
          checked(document.add_datum_plane(checked(DatumPlane::xy())));
          return document.add_sketch(loop(scale, gap, mixed));
        }));
        const auto source = *session.part_document()->find_sketch(SketchId("loop"));
        const auto before = checked(serialize_part_document_to_json(*session.part_document()));
        const auto topology = checked(SketchTopology::migrate_legacy(source));
        const auto first = topology.find_entity("entity/bottom")->points().front();
        const auto last = topology.find_entity("entity/left")->points().back();
        auto intent = checked(SketchConstraintIntent::create(
            SketchConstraintId("close"), SketchSolverConstraintKind::Coincident,
            {checked(SketchConstraintIntentTarget::point(first)),
             checked(SketchConstraintIntentTarget::point(last))}));
        auto controller =
            checked(GuiSketchConstraintController::create(session, source.id(), intent));
        REQUIRE(controller.preview().authoring.accepted);
        checked(controller.commit(session));
        const auto repaired = *session.part_document()->find_sketch(source.id());
        const auto after = checked(serialize_part_document_to_json(*session.part_document()));
        const auto repaired_topology = checked(SketchTopology::migrate_legacy(repaired));
        CHECK(repaired_topology.find_point(first)->position() ==
              repaired_topology.find_point(last)->position());
        const auto analysis = checked(
            SketchFinishService::analyze_unmaterialized(*session.part_document(), repaired));
        REQUIRE(analysis.regions.size() == 1U);
        CHECK(analysis.diagnostics.empty());
        REQUIRE(SketchFinishService::finish(*session.part_document(), source.id()));
        REQUIRE(checked(session.sketch_constraint_catalog(source.id())).constraints().size() == 1U);

        checked(session.undo());
        CHECK(checked(serialize_part_document_to_json(*session.part_document())) == before);
        CHECK(checked(session.sketch_constraint_catalog(source.id())).constraints().empty());
        checked(session.redo());
        CHECK(checked(serialize_part_document_to_json(*session.part_document())) == after);
        const auto path = directory.filePath("closure.blcad.json").toStdString();
        checked(session.save_as(path));
        GuiDocumentSession reopened;
        checked(reopened.open_file(path));
        CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == after);
        CHECK(checked(reopened.sketch_constraint_catalog(source.id())).constraints() ==
              checked(session.sketch_constraint_catalog(source.id())).constraints());
        REQUIRE(SketchFinishService::finish(*reopened.part_document(), source.id()));
      }
    }
  }
}

TEST_CASE("Exact Coincident solving leaves unrelated open branched and crossing contours invalid",
          "[gui][integration][coincident-closure]") {
  for (const auto& failure : {"open", "branch", "crossing"}) {
    CAPTURE(failure);
    GuiDocumentSession session;
    checked(session.create_part(DocumentId("invalid"), "Invalid contour"));
    checked(session.commit_part_transaction("Prepare invalid contour", [&](PartDocument& document) {
      checked(document.add_datum_plane(checked(DatumPlane::xy())));
      auto sketch = loop(1, 0.00000005, false);
      if (std::string(failure) == "branch") {
        checked(sketch.add_entity(
            checked(LineSegment::create(SketchEntityId("branch"), {100, 0}, {120, -10}))));
      } else if (std::string(failure) == "crossing") {
        sketch = checked(Sketch::create(SketchId("loop"), "Crossing", DatumPlaneId("datum.xy")));
        const std::vector<Point2> points{{0, 0}, {100, 50}, {0, 50}, {100, 0}, {0, 0}};
        for (std::size_t i = 0; i + 1 < points.size(); ++i)
          checked(sketch.add_entity(checked(LineSegment::create(
              SketchEntityId("cross." + std::to_string(i)), points[i], points[i + 1]))));
      }
      return document.add_sketch(std::move(sketch));
    }));
    // An unrelated, already satisfied relation does not authorize closing any gap.
    const auto topology = checked(
        SketchTopology::migrate_legacy(*session.part_document()->find_sketch(SketchId("loop"))));
    const auto point = topology.entities().front().points().front();
    auto fixed = checked(SketchConstraintIntent::create(
        SketchConstraintId("fixed"), SketchSolverConstraintKind::Fixed,
        {checked(SketchConstraintIntentTarget::point(point))}));
    auto controller =
        checked(GuiSketchConstraintController::create(session, SketchId("loop"), fixed));
    REQUIRE(controller.preview().authoring.accepted);
    checked(controller.commit(session));
    const auto finished = SketchFinishService::finish(*session.part_document(), SketchId("loop"));
    CHECK_FALSE(finished);
    const auto analysis = checked(SketchFinishService::analyze_unmaterialized(
        *session.part_document(), *session.part_document()->find_sketch(SketchId("loop"))));
    CHECK_FALSE(analysis.diagnostics.empty());
  }
}
