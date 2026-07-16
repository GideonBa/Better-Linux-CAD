#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_constraint_solver.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sketch_finish.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {
const std::filesystem::path source_dir(BLCAD_SOURCE_DIR);

nlohmann::json read_json(const std::filesystem::path& path) {
  std::ifstream stream(path);
  REQUIRE(stream.good());
  nlohmann::json value;
  stream >> value;
  return value;
}

GuiSketchPlaneView xy_plane() {
  return {DatumPlaneId("datum.xy"), {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
          {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
}

SketchTopologyPoint topology_point(std::string id, Point2 position) {
  return SketchTopologyPoint::create(SketchPointId(std::move(id)), position).value();
}

SketchTopologyEntity topology_line(std::string id, std::string start, std::string end) {
  return SketchTopologyEntity::create(
             std::move(id), SketchTopologyEntityKind::Line,
             {SketchPointId(std::move(start)), SketchPointId(std::move(end))})
      .value();
}

SketchSolverTarget point_target(const char* id) {
  return SketchSolverTarget::point(SketchPointId(id)).value();
}

SketchSolverTarget entity_target(const char* id) {
  return SketchSolverTarget::entity(id).value();
}

SketchSolverConstraint solver_constraint(std::string id, SketchSolverConstraintKind kind,
                                         std::vector<SketchSolverTarget> targets,
                                         std::optional<double> value = std::nullopt) {
  return SketchSolverConstraint::create(std::move(id), kind, std::move(targets), value).value();
}

PartDocument region_benchmark_document(std::size_t square_count) {
  auto document = PartDocument::create(DocumentId("part.acceptance.regions"), "Region benchmark");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.acceptance.regions"), "Regions",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  for (std::size_t index = 0; index < square_count; ++index) {
    const double x = static_cast<double>(index) * 3.0;
    const std::string prefix = "square." + std::to_string(index) + ".";
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId(prefix + "a"), {x, 0.0}, {x + 1.0, 0.0}).value()));
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId(prefix + "b"), {x + 1.0, 0.0}, {x + 1.0, 1.0}).value()));
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId(prefix + "c"), {x + 1.0, 1.0}, {x, 1.0}).value()));
    REQUIRE(sketch.value().add_entity(
        LineSegment::create(SketchEntityId(prefix + "d"), {x, 1.0}, {x, 0.0}).value()));
  }
  REQUIRE(document.value().add_sketch(std::move(sketch.value())));
  return std::move(document.value());
}

template <typename Function> long long elapsed_milliseconds(Function&& function) {
  const auto start = std::chrono::steady_clock::now();
  function();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start)
      .count();
}
} // namespace

TEST_CASE("Block 121 manifest accounts for every Interactive Sketcher family and narrowing",
          "[integration][interactive-sketcher]") {
  const auto manifest =
      read_json(source_dir / "docs/interactive-sketcher-coverage-manifest-mvp8.json");
  CHECK(manifest.at("schema") == "blcad.interactive-sketcher-coverage.v1");
  CHECK(manifest.at("implemented_through") == 121);

  const std::set<std::string> required = {
      "workspace-command-lifecycle", "mapping-hit-test-snap-selection",
      "shared-planar-topology", "planar-solver-dof-conflicts", "solver-backed-drag",
      "basic-creation", "conics-and-slots", "splines-and-text", "constraint-authoring",
      "dimension-authoring", "trim-extend-and-corners", "offset-project-break-link",
      "transform-mirror-pattern", "regions-profiles-finish", "interactive-sketch3d",
      "integrated-acceptance", "reference-repair"};
  std::set<std::string> covered;
  for (const auto& family : manifest.at("families")) {
    CHECK(covered.insert(family.at("family").get<std::string>()).second);
    CHECK(family.at("owner").get<int>() >= 106);
    CHECK(family.at("owner").get<int>() <= 121);
    for (const char* field : {"contract", "implementation", "interaction", "proof", "disposition"})
      CHECK_FALSE(family.at(field).get<std::string>().empty());
    CHECK(std::filesystem::is_regular_file(source_dir / family.at("contract").get<std::string>()));
  }
  CHECK(covered == required);

  for (const auto& tutorial : manifest.at("tutorial_documents"))
    CHECK(std::filesystem::is_regular_file(source_dir / tutorial.get<std::string>()));
}

TEST_CASE("Block 121 tutorial documents are GUI and headless recompute equivalent",
          "[integration][interactive-sketcher][integration][sketch-gui-headless]") {
  const std::vector<std::filesystem::path> tutorials = {
      "examples/triangle_prism.blcad.json", "examples/arc_profile_prism.blcad.json",
      "examples/spline_profile_prism.blcad.json", "examples/projected_sketch_references.blcad.json"};

  for (const auto& relative : tutorials) {
    CAPTURE(relative.string());
    const auto path = source_dir / relative;
    auto headless = read_part_document_json_file(path);
    REQUIRE(headless);
    GuiDocumentSession session;
    REQUIRE(session.open_file(path));
    REQUIRE(session.part_document() != nullptr);
    CHECK(serialize_part_document_to_json(*session.part_document()).value() ==
          serialize_part_document_to_json(headless.value()).value());

    auto cache = geometry::ShapeCache::create(
        ShapeCacheId("block121.headless." + headless.value().id().value()));
    REQUIRE(cache);
    REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(headless.value(), cache.value()));
    REQUIRE(session.recompute());
    REQUIRE(session.part_shape_cache() != nullptr);
    CHECK(session.part_shape_cache()->feature_shape_count() == cache.value().feature_shape_count());
    CHECK(session.part_shape_cache()->body_shape_count() == cache.value().body_shape_count());
    CHECK(session.part_shape_cache()->has_final_shape() == cache.value().has_final_shape());
  }
}

TEST_CASE("Block 121 pointer targets, drag history, cancel and stale previews remain equivalent",
          "[integration][interactive-sketcher][integration][sketch-gui-headless]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  GuiSketchWorkspace workspace;
  REQUIRE(session.create_part(DocumentId("part.block121.drag"), "Block 121 drag"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(
      session, Sketch::create(SketchId("sketch.drag"), "Drag", DatumPlaneId("datum.xy")).value()));
  REQUIRE(workbench.add_line(
      session, SketchId("sketch.drag"),
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {8.0, 2.0}).value()));

  REQUIRE(workspace.enter(session, workbench, SketchId("sketch.drag")));
  const std::string before_cancel = serialize_part_document_to_json(*session.part_document()).value();
  REQUIRE(workspace.begin_command(session, "sketch.line", "Pick line start"));
  REQUIRE(workspace.begin_numeric_input(session));
  REQUIRE(workspace.set_numeric_input("25 mm"));
  REQUIRE(workspace.show_preview(session));
  REQUIRE(workspace.escape(session));
  REQUIRE(workspace.escape(session));
  REQUIRE(workspace.escape(session));
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == before_cancel);
  CHECK_FALSE(session.task().active());
  REQUIRE(workspace.begin_command(session, "sketch.line", "Pick line start"));
  REQUIRE(workspace.begin_numeric_input(session));
  REQUIRE(workspace.show_preview(session));
  REQUIRE(workspace.commit_command(session));
  CHECK_FALSE(session.task().active());

  auto direct = GuiSketchDragController::create(session, SketchId("sketch.drag"));
  auto pointer = GuiSketchDragController::create(session, SketchId("sketch.drag"));
  REQUIRE(direct);
  REQUIRE(pointer);
  const auto endpoint = std::find_if(
      direct.value().handles().begin(), direct.value().handles().end(), [](const auto& handle) {
        return handle.kind == GuiSketchDragHandleKind::Endpoint && handle.position == Point2{8.0, 2.0};
      });
  REQUIRE(endpoint != direct.value().handles().end());
  REQUIRE(direct.value().begin(endpoint->id));
  REQUIRE(pointer.value().begin(endpoint->id));
  auto mapping = GuiSketchPlaneMapping::create_orthographic(
      xy_plane(), 200.0, 200.0, {0.0, 0.0}, 0.5);
  REQUIRE(mapping);
  const auto mapped_target = mapping.value().screen_to_plane({140.0, 100.0});
  REQUIRE(mapped_target);
  CHECK(mapped_target.value() == Point2{20.0, 0.0});
  auto direct_preview = direct.value().flush({20.0, 0.0});
  auto pointer_preview = pointer.value().flush(mapped_target.value());
  REQUIRE(direct_preview);
  REQUIRE(pointer_preview);
  CHECK(direct_preview.value().topology() == pointer_preview.value().topology());

  const std::string before_drag = serialize_part_document_to_json(*session.part_document()).value();
  REQUIRE(direct.value().commit(session));
  const std::string after_drag = serialize_part_document_to_json(*session.part_document()).value();
  CHECK(after_drag != before_drag);
  REQUIRE(session.undo());
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == before_drag);
  REQUIRE(session.redo());
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == after_drag);

  auto stale = GuiSketchDragController::create(session, SketchId("sketch.drag"));
  REQUIRE(stale);
  const auto stale_endpoint = std::find_if(
      stale.value().handles().begin(), stale.value().handles().end(), [](const auto& handle) {
        return handle.kind == GuiSketchDragHandleKind::Endpoint && handle.position.x > 10.0;
      });
  REQUIRE(stale_endpoint != stale.value().handles().end());
  REQUIRE(stale.value().begin(stale_endpoint->id));
  REQUIRE(stale.value().flush({24.0, 0.0}));
  REQUIRE(workbench.add_line(
      session, SketchId("sketch.drag"),
      LineSegment::create(SketchEntityId("line.external"), {0.0, 5.0}, {5.0, 5.0}).value()));
  const std::string externally_changed =
      serialize_part_document_to_json(*session.part_document()).value();
  CHECK(stale.value().commit(session).has_error());
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == externally_changed);

  auto inspection = workbench.inspect(session, SketchId("sketch.drag"));
  REQUIRE(inspection);
  REQUIRE_FALSE(inspection.value().repairs.suggestions().empty());
  const auto suggestion = inspection.value().repairs.suggestions().front();
  const std::string before_repair =
      serialize_part_document_to_json(*session.part_document()).value();
  REQUIRE(workbench.preview_repair(session, SketchId("sketch.drag"), suggestion));
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == before_repair);
  REQUIRE(workbench.apply_repair(session, SketchId("sketch.drag"), suggestion));
  REQUIRE(session.undo());
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == before_repair);
}

TEST_CASE("Block 121 measures representative hit solver drag and region interaction paths",
          "[integration][interactive-sketcher][performance][sketch-interaction]") {
  GuiSketchInteractionScene scene;
  scene.sketch = SketchId("sketch.performance");
  for (std::size_t index = 0; index < 250U; ++index) {
    const double y = static_cast<double>(index) * 2.0;
    const std::string id = "line." + std::to_string(index);
    scene.curves.push_back({"sketch/performance/entity/" + id, id, GuiSketchCurveKind::Line,
                            {{-100.0, y}, {100.0, y}}, false});
    scene.points.push_back({"sketch/performance/entity/" + id, id + ":start",
                            {-100.0, y}, GuiSketchSnapKind::Endpoint});
  }
  auto mapping = GuiSketchPlaneMapping::create_orthographic(
      xy_plane(), 800.0, 600.0, {0.0, 250.0}, 1.0);
  REQUIRE(mapping);
  auto controller = GuiSketchInteractionController::create(mapping.value(), std::move(scene));
  REQUIRE(controller);
  const long long hover_ms = elapsed_milliseconds([&] {
    for (std::size_t index = 0; index < 1000U; ++index) {
      auto hits = controller.value().hits_at(
          {400.0 + static_cast<double>(index % 5U), 300.0});
      REQUIRE(hits);
    }
  });

  auto topology = SketchTopology::create(
      SketchId("sketch.performance.solver"),
      {topology_point("point.a", {0.0, 0.0}), topology_point("point.b", {8.0, 2.0})},
      {topology_line("entity/line.a", "point.a", "point.b")});
  REQUIRE(topology);
  auto system = SketchConstraintSystem::create(
      topology.value().sketch(),
      {solver_constraint("fixed", SketchSolverConstraintKind::Fixed, {point_target("point.a")}),
       solver_constraint("horizontal", SketchSolverConstraintKind::Horizontal,
                         {entity_target("entity/line.a")}),
       solver_constraint("width", SketchSolverConstraintKind::HorizontalDistance,
                         {point_target("point.a"), point_target("point.b")}, 10.0)});
  REQUIRE(system);
  const long long solve_ms = elapsed_milliseconds([&] {
    for (std::size_t index = 0; index < 100U; ++index) {
      auto solved = SketchConstraintSolver{}.solve(topology.value(), system.value());
      REQUIRE(solved);
      CHECK(solved.value().status == SketchSolveStatus::FullyConstrained);
    }
  });

  GuiDocumentSession drag_session;
  GuiSketchWorkbench workbench;
  REQUIRE(drag_session.create_part(DocumentId("part.performance.drag"), "Drag performance"));
  REQUIRE(workbench.create_xy_datum(drag_session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(
      drag_session,
      Sketch::create(SketchId("sketch.performance.drag"), "Drag", DatumPlaneId("datum.xy")).value()));
  REQUIRE(workbench.add_line(
      drag_session, SketchId("sketch.performance.drag"),
      LineSegment::create(SketchEntityId("line.drag"), {0.0, 0.0}, {8.0, 2.0}).value()));
  auto drag = GuiSketchDragController::create(drag_session, SketchId("sketch.performance.drag"));
  REQUIRE(drag);
  const auto drag_endpoint = std::find_if(
      drag.value().handles().begin(), drag.value().handles().end(), [](const auto& handle) {
        return handle.kind == GuiSketchDragHandleKind::Endpoint && handle.position.x > 4.0;
      });
  REQUIRE(drag_endpoint != drag.value().handles().end());
  REQUIRE(drag.value().begin(drag_endpoint->id));
  const long long drag_ms = elapsed_milliseconds([&] {
    for (std::size_t index = 0; index < 100U; ++index) {
      REQUIRE(drag.value().queue_pointer({10.0 + static_cast<double>(index) * 0.01, 0.0}));
      REQUIRE(drag.value().process_pending());
    }
  });

  auto regions = region_benchmark_document(40U);
  const auto* sketch = regions.find_sketch(SketchId("sketch.acceptance.regions"));
  REQUIRE(sketch != nullptr);
  long long region_ms = 0;
  geometry::SketchRegionAnalysis analysis;
  region_ms = elapsed_milliseconds([&] {
    auto measured = geometry::SketchFinishService::analyze(regions, *sketch);
    REQUIRE(measured);
    analysis = std::move(measured.value());
  });
  CHECK(analysis.regions.size() == 40U);
  CHECK(analysis.diagnostics.empty());

  INFO("1000 hit tests: " << hover_ms << " ms");
  INFO("100 deterministic solves: " << solve_ms << " ms");
  INFO("100 drag candidate solves: " << drag_ms << " ms");
  INFO("40-region recognition: " << region_ms << " ms");
  CHECK(hover_ms < 5000);
  CHECK(solve_ms < 5000);
  CHECK(drag_ms < 5000);
  CHECK(region_ms < 5000);
}
