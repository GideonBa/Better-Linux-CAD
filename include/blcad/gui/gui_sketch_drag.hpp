#pragma once

#include "blcad/core/sketch_dimension_catalog_system.hpp"
#include "blcad/core/sketch_constraint_solver.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

enum class GuiSketchDragHandleKind {
  Endpoint,
  Midpoint,
  Center,
  Radius,
  Arc,
  Spline,
  Dimension,
};

[[nodiscard]] std::string_view to_string(GuiSketchDragHandleKind kind) noexcept;

enum class GuiSketchDragTargetKind {
  Point,
  LineMidpoint,
  ArcCenter,
  ArcRadius,
};

[[nodiscard]] std::string_view to_string(GuiSketchDragTargetKind kind) noexcept;

struct GuiSketchDragHandle {
  std::string id;
  GuiSketchDragHandleKind kind{GuiSketchDragHandleKind::Endpoint};
  GuiSketchDragTargetKind target_kind{GuiSketchDragTargetKind::Point};
  Point2 position;
  std::optional<SketchPointId> point_id;
  std::string entity_id;
  std::optional<SketchDimensionId> dimension_id;
  bool reference{false};

  friend bool operator==(const GuiSketchDragHandle&, const GuiSketchDragHandle&) = default;
};

class GuiSketchDragPreview {
public:
  GuiSketchDragPreview(Point2 pointer, SketchTopology topology, Sketch preview_sketch,
                       SketchSolveResult solve);

  [[nodiscard]] Point2 pointer() const noexcept;
  [[nodiscard]] const SketchTopology& topology() const noexcept;
  [[nodiscard]] const Sketch& preview_sketch() const noexcept;
  [[nodiscard]] const SketchSolveResult& solve() const noexcept;

private:
  Point2 pointer_;
  SketchTopology topology_;
  Sketch preview_sketch_;
  SketchSolveResult solve_;
};

class GuiSketchDragController {
public:
  [[nodiscard]] static Result<GuiSketchDragController>
  create(const PartDocument& document, SketchId sketch_id);

  [[nodiscard]] static Result<GuiSketchDragController>
  create(const GuiDocumentSession& session, SketchId sketch_id) {
    if (session.part_document() == nullptr)
      return Result<GuiSketchDragController>::failure(Error::validation(
          sketch_id.value(), "solver-backed drag requires an active Part session"));
    auto controller = create(*session.part_document(), sketch_id);
    if (controller.has_error()) return controller;
    auto constraints = session.sketch_constraint_catalog(sketch_id);
    if (constraints.has_error())
      return Result<GuiSketchDragController>::failure(constraints.error());
    auto dimensions = session.sketch_dimension_catalog(sketch_id);
    if (dimensions.has_error())
      return Result<GuiSketchDragController>::failure(dimensions.error());
    auto system = SketchDimensionCatalogSystemBuilder::build(
        *session.part_document(), controller.value().source_topology_,
        constraints.value(), dimensions.value());
    if (system.has_error())
      return Result<GuiSketchDragController>::failure(system.error());
    auto baseline = SketchConstraintSolver{}.solve(
        controller.value().source_topology_, system.value());
    if (baseline.has_error())
      return Result<GuiSketchDragController>::failure(baseline.error());
    const bool accepted = baseline.value().status == SketchSolveStatus::UnderConstrained ||
                          baseline.value().status == SketchSolveStatus::FullyConstrained;
    if (!accepted)
      return Result<GuiSketchDragController>::failure(Error::validation(
          sketch_id.value(), "Sketch dimensions do not have a valid drag baseline solve"));
    auto valid = SketchDimensionCatalogSystemBuilder::validate_measurements(
        *session.part_document(), baseline.value().topology, dimensions.value());
    if (valid.has_error())
      return Result<GuiSketchDragController>::failure(valid.error());
    controller.value().source_system_ = std::move(system.value());
    controller.value().baseline_solve_ = std::move(baseline.value());
    controller.value().source_catalog_ = std::move(constraints.value());
    controller.value().source_dimension_catalog_ = std::move(dimensions.value());
    return controller;
  }

  [[nodiscard]] const SketchId& sketch_id() const noexcept;
  [[nodiscard]] const Sketch& source_sketch() const noexcept;
  [[nodiscard]] const SketchTopology& source_topology() const noexcept;
  [[nodiscard]] const SketchConstraintSystem& source_system() const noexcept;
  [[nodiscard]] const SketchSolveResult& baseline_solve() const noexcept;
  [[nodiscard]] const std::vector<GuiSketchDragHandle>& handles() const noexcept;
  [[nodiscard]] std::vector<GuiSketchDragHandle>
  handles_for_topology(const SketchTopology& topology) const;

  [[nodiscard]] Result<std::size_t> begin(std::string_view handle_id);
  [[nodiscard]] Result<std::size_t> queue_pointer(Point2 pointer);
  [[nodiscard]] Result<GuiSketchDragPreview> process_pending();
  [[nodiscard]] Result<GuiSketchDragPreview> flush(Point2 final_pointer);
  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session);

  [[nodiscard]] Result<std::size_t> commit_session_constraints(
      GuiDocumentSession& session) {
    if (!active() || !latest_preview_ || pending_pointer_)
      return Result<std::size_t>::failure(Error::validation(
          source_topology_.sketch().value(),
          "committing a Sketch drag requires a flushed valid final preview"));
    if (session.part_document() == nullptr || !source_catalog_ ||
        !source_dimension_catalog_)
      return Result<std::size_t>::failure(Error::validation(
          source_topology_.sketch().value(),
          "session drag requires its original constraint and dimension snapshots"));
    auto current_constraints = session.sketch_constraint_catalog(source_topology_.sketch());
    if (current_constraints.has_error())
      return Result<std::size_t>::failure(current_constraints.error());
    if (current_constraints.value() != *source_catalog_)
      return Result<std::size_t>::failure(Error::dependency(
          source_topology_.sketch().value(),
          "Sketch constraint catalog changed after drag preview; commit refused"));
    auto current_dimensions = session.sketch_dimension_catalog(source_topology_.sketch());
    if (current_dimensions.has_error())
      return Result<std::size_t>::failure(current_dimensions.error());
    if (current_dimensions.value() != *source_dimension_catalog_)
      return Result<std::size_t>::failure(Error::dependency(
          source_topology_.sketch().value(),
          "Sketch dimension catalog changed after drag preview; commit refused"));
    auto valid = SketchDimensionCatalogSystemBuilder::validate_measurements(
        *session.part_document(), latest_preview_->topology(), *source_dimension_catalog_);
    if (valid.has_error())
      return Result<std::size_t>::failure(Error::validation(
          source_topology_.sketch().value(),
          "dragged topology violates a driving Sketch dimension"));

    const SketchTopology expected_source = source_topology_;
    const SketchConstraintSystem expected_system = source_system_;
    const SketchTopology final_topology = latest_preview_->topology();
    const SketchId target_sketch = source_topology_.sketch();
    const SketchConstraintCatalog constraints = *source_catalog_;
    const SketchDimensionCatalog dimensions = *source_dimension_catalog_;
    auto committed = session.commit_part_transaction(
        "Drag sketch handle",
        [expected_source, expected_system, final_topology, target_sketch,
         constraints, dimensions](PartDocument& document) -> Result<std::size_t> {
          const auto* current = document.find_sketch(target_sketch);
          if (current == nullptr)
            return Result<std::size_t>::failure(
                Error::validation(target_sketch.value(), "dragged Sketch no longer exists"));
          const Sketch current_snapshot = *current;
          auto current_topology = SketchTopology::migrate_legacy(current_snapshot);
          if (current_topology.has_error())
            return Result<std::size_t>::failure(current_topology.error());
          if (current_topology.value() != expected_source)
            return Result<std::size_t>::failure(Error::validation(
                target_sketch.value(), "Sketch changed after drag preview; commit refused"));
          auto current_system = SketchDimensionCatalogSystemBuilder::build(
              document, current_topology.value(), constraints, dimensions);
          if (current_system.has_error())
            return Result<std::size_t>::failure(current_system.error());
          if (current_system.value() != expected_system)
            return Result<std::size_t>::failure(Error::validation(
                target_sketch.value(),
                "Sketch constraint or dimension system changed after drag preview"));
          auto valid_dimensions = SketchDimensionCatalogSystemBuilder::validate_measurements(
              document, final_topology, dimensions);
          if (valid_dimensions.has_error())
            return Result<std::size_t>::failure(Error::validation(
                target_sketch.value(), "dragged topology violates a driving dimension"));
          auto materialized = SketchTopologyLegacyMaterializer{}.materialize(
              current_snapshot, final_topology);
          if (materialized.has_error())
            return Result<std::size_t>::failure(materialized.error());
          auto represented = SketchTopology::migrate_legacy(materialized.value());
          if (represented.has_error())
            return Result<std::size_t>::failure(represented.error());
          if (represented.value() != final_topology)
            return Result<std::size_t>::failure(Error::validation(
                target_sketch.value(),
                "dragged topology cannot be represented without shared-point identity loss"));
          return document.update_sketch(std::move(materialized.value()));
        });
    if (committed.has_error()) return committed;
    cancel();
    return committed;
  }

  void cancel() noexcept;

  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] bool has_pending() const noexcept;
  [[nodiscard]] const GuiSketchDragHandle* active_handle() const noexcept;
  [[nodiscard]] const std::optional<GuiSketchDragPreview>& latest_preview() const noexcept;
  [[nodiscard]] const std::optional<Point2>& pending_pointer() const noexcept;
  [[nodiscard]] const std::optional<Point2>& processed_pointer() const noexcept;
  [[nodiscard]] std::size_t solve_count() const noexcept;

private:
  GuiSketchDragController(Sketch source_sketch, SketchTopology source_topology,
                          SketchConstraintSystem source_system,
                          SketchSolveResult baseline_solve,
                          std::vector<GuiSketchDragHandle> handles);

  [[nodiscard]] Result<GuiSketchDragPreview> solve_pointer(Point2 pointer);

  Sketch source_sketch_;
  SketchTopology source_topology_;
  SketchConstraintSystem source_system_;
  SketchSolveResult baseline_solve_;
  std::vector<GuiSketchDragHandle> handles_;
  std::optional<SketchConstraintCatalog> source_catalog_;
  std::optional<SketchDimensionCatalog> source_dimension_catalog_;
  std::optional<std::size_t> active_handle_index_;
  std::optional<Point2> pending_pointer_;
  std::optional<Point2> processed_pointer_;
  std::optional<GuiSketchDragPreview> latest_preview_;
  std::size_t solve_count_{0U};
};

} // namespace blcad::gui
