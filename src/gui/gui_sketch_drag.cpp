#include "blcad/gui/gui_sketch_drag.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kEpsilon = 1.0e-12;
constexpr std::string_view kDragPointId = "__gui.drag.pointer";
constexpr std::string_view kDragCenterEntityId = "__gui.drag.center";
constexpr std::string_view kDragConstraintId = "zz.gui.drag.target";

[[nodiscard]] Error drag_error(std::string message) {
  return Error::validation("gui.sketch_drag", std::move(message));
}

[[nodiscard]] bool finite(Point2 point) noexcept {
  return std::isfinite(point.x) && std::isfinite(point.y);
}

[[nodiscard]] double distance(Point2 first, Point2 second) noexcept {
  return std::hypot(second.x - first.x, second.y - first.y);
}

[[nodiscard]] Point2 midpoint(Point2 first, Point2 second) noexcept {
  return {(first.x + second.x) * 0.5, (first.y + second.y) * 0.5};
}

struct ArcGeometry {
  Point2 center;
  double radius{0.0};
};

[[nodiscard]] std::optional<ArcGeometry>
arc_geometry(const SketchTopology& topology, const SketchTopologyEntity& entity) noexcept {
  if (entity.kind() != SketchTopologyEntityKind::Arc || entity.points().size() != 3U)
    return std::nullopt;
  const auto* start_point = topology.find_point(entity.points()[0]);
  const auto* mid_point = topology.find_point(entity.points()[1]);
  const auto* end_point = topology.find_point(entity.points()[2]);
  if (start_point == nullptr || mid_point == nullptr || end_point == nullptr)
    return std::nullopt;
  const Point2 start = start_point->position();
  const Point2 mid = mid_point->position();
  const Point2 end = end_point->position();
  const double denominator =
      2.0 * (start.x * (mid.y - end.y) + mid.x * (end.y - start.y) +
             end.x * (start.y - mid.y));
  if (!std::isfinite(denominator) || std::abs(denominator) <= kEpsilon)
    return std::nullopt;
  const double start_norm = start.x * start.x + start.y * start.y;
  const double mid_norm = mid.x * mid.x + mid.y * mid.y;
  const double end_norm = end.x * end.x + end.y * end.y;
  const Point2 center{
      (start_norm * (mid.y - end.y) + mid_norm * (end.y - start.y) +
       end_norm * (start.y - mid.y)) /
          denominator,
      (start_norm * (end.x - mid.x) + mid_norm * (start.x - end.x) +
       end_norm * (mid.x - start.x)) /
          denominator};
  const double radius = distance(center, start);
  if (!finite(center) || !std::isfinite(radius) || radius <= kEpsilon)
    return std::nullopt;
  return ArcGeometry{center, radius};
}

[[nodiscard]] std::string point_handle_id(const SketchId& sketch, const SketchPointId& point) {
  return "sketch/" + sketch.value() + "/handle/point/" + point.value();
}

[[nodiscard]] std::string entity_handle_id(const SketchId& sketch, std::string_view entity,
                                           std::string_view role) {
  return "sketch/" + sketch.value() + "/handle/entity/" + std::string(entity) + "/" +
         std::string(role);
}

[[nodiscard]] std::string dimension_handle_id(const SketchId& sketch,
                                              const SketchDimensionId& dimension) {
  return "sketch/" + sketch.value() + "/handle/dimension/" + dimension.value();
}

[[nodiscard]] const SketchTopologyPoint*
point_for_target(const SketchTopology& topology, const SketchReferenceTarget& target) noexcept {
  const auto* entity = topology.find_entity("entity/" + target.entity().value());
  if (entity == nullptr)
    return nullptr;
  switch (target.kind()) {
  case SketchReferenceTargetKind::LineSegmentStart:
    return entity->points().empty() ? nullptr : topology.find_point(entity->points().front());
  case SketchReferenceTargetKind::LineSegmentEnd:
    return entity->points().size() < 2U ? nullptr : topology.find_point(entity->points().back());
  case SketchReferenceTargetKind::LineSegment:
  case SketchReferenceTargetKind::ProjectedPoint:
  case SketchReferenceTargetKind::ProjectedLine:
    return nullptr;
  }
  return nullptr;
}

[[nodiscard]] std::vector<GuiSketchDragHandle>
build_handles(const Sketch& sketch, const SketchTopology& topology) {
  std::vector<GuiSketchDragHandle> handles;
  std::map<std::string, GuiSketchDragHandle> endpoint_handles;

  const auto append_endpoint = [&](const SketchPointId& point_id) {
    const auto* point = topology.find_point(point_id);
    if (point == nullptr)
      return;
    endpoint_handles.try_emplace(
        point_id.value(),
        GuiSketchDragHandle{point_handle_id(topology.sketch(), point_id),
                            GuiSketchDragHandleKind::Endpoint,
                            GuiSketchDragTargetKind::Point,
                            point->position(), point_id, {}, std::nullopt, point->reference()});
  };

  for (const auto& entity : topology.entities()) {
    if (entity.kind() == SketchTopologyEntityKind::Line) {
      append_endpoint(entity.points()[0]);
      append_endpoint(entity.points()[1]);
      const auto* start = topology.find_point(entity.points()[0]);
      const auto* end = topology.find_point(entity.points()[1]);
      if (start != nullptr && end != nullptr)
        handles.push_back({entity_handle_id(topology.sketch(), entity.id(), "midpoint"),
                           GuiSketchDragHandleKind::Midpoint,
                           GuiSketchDragTargetKind::LineMidpoint,
                           midpoint(start->position(), end->position()), std::nullopt, entity.id(),
                           std::nullopt, entity.reference()});
    } else if (entity.kind() == SketchTopologyEntityKind::Arc) {
      append_endpoint(entity.points()[0]);
      append_endpoint(entity.points()[2]);
      const auto* mid = topology.find_point(entity.points()[1]);
      if (mid != nullptr)
        handles.push_back({entity_handle_id(topology.sketch(), entity.id(), "arc"),
                           GuiSketchDragHandleKind::Arc, GuiSketchDragTargetKind::Point,
                           mid->position(), mid->id(), entity.id(), std::nullopt,
                           entity.reference() || mid->reference()});
      const auto geometry = arc_geometry(topology, entity);
      if (geometry) {
        handles.push_back({entity_handle_id(topology.sketch(), entity.id(), "center"),
                           GuiSketchDragHandleKind::Center, GuiSketchDragTargetKind::ArcCenter,
                           geometry->center, std::nullopt, entity.id(), std::nullopt,
                           entity.reference()});
        const Point2 radial_source = mid != nullptr ? mid->position() :
                                                        topology.find_point(entity.points()[0])->position();
        const double angle = std::atan2(radial_source.y - geometry->center.y,
                                        radial_source.x - geometry->center.x) +
                             kPi / 6.0;
        handles.push_back({entity_handle_id(topology.sketch(), entity.id(), "radius"),
                           GuiSketchDragHandleKind::Radius, GuiSketchDragTargetKind::ArcRadius,
                           {geometry->center.x + geometry->radius * std::cos(angle),
                            geometry->center.y + geometry->radius * std::sin(angle)},
                           std::nullopt, entity.id(), std::nullopt, entity.reference()});
      }
    } else if (entity.kind() == SketchTopologyEntityKind::Spline) {
      append_endpoint(entity.points()[0]);
      append_endpoint(entity.points()[3]);
      for (std::size_t index = 1U; index <= 2U; ++index) {
        const auto* point = topology.find_point(entity.points()[index]);
        if (point == nullptr)
          continue;
        handles.push_back({entity_handle_id(topology.sketch(), entity.id(),
                                            index == 1U ? "spline/control1" : "spline/control2"),
                           GuiSketchDragHandleKind::Spline, GuiSketchDragTargetKind::Point,
                           point->position(), point->id(), entity.id(), std::nullopt,
                           entity.reference() || point->reference()});
      }
    } else if (entity.kind() == SketchTopologyEntityKind::RectangleProfile ||
               entity.kind() == SketchTopologyEntityKind::CircleProfile ||
               entity.kind() == SketchTopologyEntityKind::CircularHolePattern) {
      const auto* center = topology.find_point(entity.points()[0]);
      if (center != nullptr)
        handles.push_back({entity_handle_id(topology.sketch(), entity.id(), "center"),
                           GuiSketchDragHandleKind::Center, GuiSketchDragTargetKind::Point,
                           center->position(), center->id(), entity.id(), std::nullopt,
                           entity.reference() || center->reference()});
    }
  }

  for (auto& [id, handle] : endpoint_handles) {
    (void)id;
    handles.push_back(std::move(handle));
  }

  for (const auto& dimension : sketch.driving_dimensions()) {
    const auto* target = point_for_target(topology, dimension.second_target());
    if (target == nullptr)
      continue;
    handles.push_back({dimension_handle_id(topology.sketch(), dimension.id()),
                       GuiSketchDragHandleKind::Dimension, GuiSketchDragTargetKind::Point,
                       target->position(), target->id(), {}, dimension.id(), target->reference()});
  }

  std::sort(handles.begin(), handles.end(), [](const auto& left, const auto& right) {
    return left.id < right.id;
  });
  return handles;
}

[[nodiscard]] bool accepted_status(SketchSolveStatus status) noexcept {
  return status == SketchSolveStatus::FullyConstrained ||
         status == SketchSolveStatus::UnderConstrained ||
         status == SketchSolveStatus::Redundant;
}

[[nodiscard]] std::string solve_failure_message(const SketchSolveResult& solve) {
  switch (solve.status) {
  case SketchSolveStatus::Conflicting:
    return "drag target conflicts with the current Sketch constraints";
  case SketchSolveStatus::NonConvergent:
    return "drag target did not converge";
  case SketchSolveStatus::InvalidReference:
    return "drag target cannot solve because a Sketch reference is invalid";
  case SketchSolveStatus::FullyConstrained:
  case SketchSolveStatus::UnderConstrained:
  case SketchSolveStatus::Redundant:
    return {};
  }
  return "drag target could not be solved";
}

[[nodiscard]] Result<SketchTopology>
strip_transient_topology(const SketchTopology& source, const SketchTopology& solved) {
  std::vector<SketchTopologyPoint> points;
  points.reserve(source.points().size());
  for (const auto& source_point : source.points()) {
    const auto* solved_point = solved.find_point(source_point.id());
    if (solved_point == nullptr)
      return Result<SketchTopology>::failure(
          drag_error("solver result lost a source Sketch point identity"));
    auto point = SketchTopologyPoint::create(source_point.id(), solved_point->position(),
                                             source_point.flags());
    if (point.has_error())
      return Result<SketchTopology>::failure(point.error());
    points.push_back(std::move(point.value()));
  }
  return SketchTopology::create(source.sketch(), std::move(points), source.entities(),
                                source.dependencies());
}

[[nodiscard]] Result<SketchTopology>
augment_topology(const SketchTopology& source, Point2 pointer, bool add_center_entity) {
  std::vector<SketchTopologyPoint> points = source.points();
  auto target = SketchTopologyPoint::create(
      SketchPointId(std::string(kDragPointId)), pointer,
      SketchTopologyFlags{.construction = false, .reference = true});
  if (target.has_error())
    return Result<SketchTopology>::failure(target.error());
  points.push_back(std::move(target.value()));

  std::vector<SketchTopologyEntity> entities = source.entities();
  if (add_center_entity) {
    auto center = SketchTopologyEntity::create(
        std::string(kDragCenterEntityId), SketchTopologyEntityKind::CircleProfile,
        {SketchPointId(std::string(kDragPointId))}, {},
        SketchTopologyFlags{.construction = false, .reference = true});
    if (center.has_error())
      return Result<SketchTopology>::failure(center.error());
    entities.push_back(std::move(center.value()));
  }
  return SketchTopology::create(source.sketch(), std::move(points), std::move(entities),
                                source.dependencies());
}

} // namespace

std::string_view to_string(GuiSketchDragHandleKind kind) noexcept {
  switch (kind) {
  case GuiSketchDragHandleKind::Endpoint: return "endpoint";
  case GuiSketchDragHandleKind::Midpoint: return "midpoint";
  case GuiSketchDragHandleKind::Center: return "center";
  case GuiSketchDragHandleKind::Radius: return "radius";
  case GuiSketchDragHandleKind::Arc: return "arc";
  case GuiSketchDragHandleKind::Spline: return "spline";
  case GuiSketchDragHandleKind::Dimension: return "dimension";
  }
  return "endpoint";
}

std::string_view to_string(GuiSketchDragTargetKind kind) noexcept {
  switch (kind) {
  case GuiSketchDragTargetKind::Point: return "point";
  case GuiSketchDragTargetKind::LineMidpoint: return "line_midpoint";
  case GuiSketchDragTargetKind::ArcCenter: return "arc_center";
  case GuiSketchDragTargetKind::ArcRadius: return "arc_radius";
  }
  return "point";
}

GuiSketchDragPreview::GuiSketchDragPreview(Point2 pointer, SketchTopology topology,
                                           Sketch preview_sketch, SketchSolveResult solve)
    : pointer_(pointer), topology_(std::move(topology)),
      preview_sketch_(std::move(preview_sketch)), solve_(std::move(solve)) {}

Point2 GuiSketchDragPreview::pointer() const noexcept { return pointer_; }
const SketchTopology& GuiSketchDragPreview::topology() const noexcept { return topology_; }
const Sketch& GuiSketchDragPreview::preview_sketch() const noexcept { return preview_sketch_; }
const SketchSolveResult& GuiSketchDragPreview::solve() const noexcept { return solve_; }

Result<GuiSketchDragController>
GuiSketchDragController::create(const PartDocument& document, SketchId sketch_id) {
  const auto* sketch = document.find_sketch(sketch_id);
  if (sketch == nullptr)
    return Result<GuiSketchDragController>::failure(
        drag_error("solver-backed drag requires an existing planar Sketch"));
  Sketch source_sketch = *sketch;
  auto topology = SketchTopology::migrate_legacy(source_sketch);
  if (topology.has_error())
    return Result<GuiSketchDragController>::failure(topology.error());
  auto system = SketchConstraintSystemBuilder::from_legacy(topology.value(), source_sketch, document);
  if (system.has_error())
    return Result<GuiSketchDragController>::failure(system.error());
  auto baseline = SketchConstraintSolver{}.solve(topology.value(), system.value());
  if (baseline.has_error())
    return Result<GuiSketchDragController>::failure(baseline.error());
  auto handles = build_handles(source_sketch, topology.value());
  return Result<GuiSketchDragController>::success(GuiSketchDragController(
      std::move(source_sketch), std::move(topology.value()), std::move(system.value()),
      std::move(baseline.value()), std::move(handles)));
}

GuiSketchDragController::GuiSketchDragController(
    Sketch source_sketch, SketchTopology source_topology, SketchConstraintSystem source_system,
    SketchSolveResult baseline_solve, std::vector<GuiSketchDragHandle> handles)
    : source_sketch_(std::move(source_sketch)), source_topology_(std::move(source_topology)),
      source_system_(std::move(source_system)), baseline_solve_(std::move(baseline_solve)),
      handles_(std::move(handles)) {}

const SketchId& GuiSketchDragController::sketch_id() const noexcept {
  return source_topology_.sketch();
}

const Sketch& GuiSketchDragController::source_sketch() const noexcept { return source_sketch_; }

const SketchTopology& GuiSketchDragController::source_topology() const noexcept {
  return source_topology_;
}

const SketchConstraintSystem& GuiSketchDragController::source_system() const noexcept {
  return source_system_;
}

const SketchSolveResult& GuiSketchDragController::baseline_solve() const noexcept {
  return baseline_solve_;
}

const std::vector<GuiSketchDragHandle>& GuiSketchDragController::handles() const noexcept {
  return handles_;
}

std::vector<GuiSketchDragHandle>
GuiSketchDragController::handles_for_topology(const SketchTopology& topology) const {
  return build_handles(source_sketch_, topology);
}

Result<std::size_t> GuiSketchDragController::begin(std::string_view handle_id) {
  if (active())
    return Result<std::size_t>::failure(drag_error("a Sketch handle drag is already active"));
  if (!accepted_status(baseline_solve_.status))
    return Result<std::size_t>::failure(
        drag_error("Sketch must have a convergent valid baseline solve before dragging"));
  const auto found = std::find_if(handles_.begin(), handles_.end(),
                                  [handle_id](const auto& handle) { return handle.id == handle_id; });
  if (found == handles_.end())
    return Result<std::size_t>::failure(drag_error("selected Sketch drag handle does not exist"));
  if (found->reference)
    return Result<std::size_t>::failure(drag_error("reference Sketch geometry is read-only"));
  active_handle_index_ = static_cast<std::size_t>(std::distance(handles_.begin(), found));
  pending_pointer_.reset();
  processed_pointer_.reset();
  latest_preview_.reset();
  solve_count_ = 0U;
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> GuiSketchDragController::queue_pointer(Point2 pointer) {
  if (!active())
    return Result<std::size_t>::failure(drag_error("queueing a pointer requires an active handle drag"));
  if (!finite(pointer))
    return Result<std::size_t>::failure(drag_error("drag pointer coordinates must be finite"));
  pending_pointer_ = pointer;
  return Result<std::size_t>::success(1U);
}

Result<GuiSketchDragPreview> GuiSketchDragController::process_pending() {
  if (!active() || !pending_pointer_)
    return Result<GuiSketchDragPreview>::failure(
        drag_error("processing a drag preview requires one queued pointer sample"));
  const Point2 pointer = *pending_pointer_;
  pending_pointer_.reset();
  return solve_pointer(pointer);
}

Result<GuiSketchDragPreview> GuiSketchDragController::flush(Point2 final_pointer) {
  auto queued = queue_pointer(final_pointer);
  if (queued.has_error())
    return Result<GuiSketchDragPreview>::failure(queued.error());
  return process_pending();
}

Result<GuiSketchDragPreview> GuiSketchDragController::solve_pointer(Point2 pointer) {
  const auto* handle = active_handle();
  if (handle == nullptr)
    return Result<GuiSketchDragPreview>::failure(drag_error("drag handle identity was lost"));

  const bool center_target = handle->target_kind == GuiSketchDragTargetKind::ArcCenter;
  auto augmented = augment_topology(source_topology_, pointer, center_target);
  if (augmented.has_error())
    return Result<GuiSketchDragPreview>::failure(augmented.error());

  std::vector<SketchSolverConstraint> constraints = source_system_.constraints();
  Result<SketchSolverConstraint> drag_constraint = [&]() -> Result<SketchSolverConstraint> {
    switch (handle->target_kind) {
    case GuiSketchDragTargetKind::Point: {
      if (!handle->point_id)
        return Result<SketchSolverConstraint>::failure(drag_error("point drag handle has no point id"));
      auto controlled = SketchSolverTarget::point(*handle->point_id);
      auto target = SketchSolverTarget::point(SketchPointId(std::string(kDragPointId)));
      if (controlled.has_error()) return Result<SketchSolverConstraint>::failure(controlled.error());
      if (target.has_error()) return Result<SketchSolverConstraint>::failure(target.error());
      return SketchSolverConstraint::create(
          std::string(kDragConstraintId), SketchSolverConstraintKind::Coincident,
          {std::move(controlled.value()), std::move(target.value())});
    }
    case GuiSketchDragTargetKind::LineMidpoint: {
      auto target = SketchSolverTarget::point(SketchPointId(std::string(kDragPointId)));
      auto line = SketchSolverTarget::entity(handle->entity_id);
      if (target.has_error()) return Result<SketchSolverConstraint>::failure(target.error());
      if (line.has_error()) return Result<SketchSolverConstraint>::failure(line.error());
      return SketchSolverConstraint::create(
          std::string(kDragConstraintId), SketchSolverConstraintKind::Midpoint,
          {std::move(target.value()), std::move(line.value())});
    }
    case GuiSketchDragTargetKind::ArcCenter: {
      auto arc = SketchSolverTarget::entity(handle->entity_id);
      auto center = SketchSolverTarget::entity(std::string(kDragCenterEntityId));
      if (arc.has_error()) return Result<SketchSolverConstraint>::failure(arc.error());
      if (center.has_error()) return Result<SketchSolverConstraint>::failure(center.error());
      return SketchSolverConstraint::create(
          std::string(kDragConstraintId), SketchSolverConstraintKind::Concentric,
          {std::move(arc.value()), std::move(center.value())});
    }
    case GuiSketchDragTargetKind::ArcRadius: {
      const auto* entity = source_topology_.find_entity(handle->entity_id);
      const auto geometry = entity == nullptr ? std::nullopt : arc_geometry(source_topology_, *entity);
      if (!geometry)
        return Result<SketchSolverConstraint>::failure(
            drag_error("arc radius handle references degenerate arc geometry"));
      const double radius = distance(geometry->center, pointer);
      if (!std::isfinite(radius) || radius <= kEpsilon)
        return Result<SketchSolverConstraint>::failure(
            drag_error("arc radius drag requires a positive finite radius"));
      auto arc = SketchSolverTarget::entity(handle->entity_id);
      if (arc.has_error()) return Result<SketchSolverConstraint>::failure(arc.error());
      return SketchSolverConstraint::create(
          std::string(kDragConstraintId), SketchSolverConstraintKind::Radial,
          {std::move(arc.value())}, radius);
    }
    }
    return Result<SketchSolverConstraint>::failure(drag_error("unsupported Sketch drag target"));
  }();
  if (drag_constraint.has_error())
    return Result<GuiSketchDragPreview>::failure(drag_constraint.error());
  constraints.push_back(std::move(drag_constraint.value()));
  auto system = SketchConstraintSystem::create(source_topology_.sketch(), std::move(constraints));
  if (system.has_error())
    return Result<GuiSketchDragPreview>::failure(system.error());

  auto solve = SketchConstraintSolver{}.solve(augmented.value(), system.value());
  ++solve_count_;
  processed_pointer_ = pointer;
  if (solve.has_error())
    return Result<GuiSketchDragPreview>::failure(solve.error());
  if (!accepted_status(solve.value().status)) {
    latest_preview_.reset();
    return Result<GuiSketchDragPreview>::failure(drag_error(solve_failure_message(solve.value())));
  }

  auto stripped = strip_transient_topology(source_topology_, solve.value().topology);
  if (stripped.has_error())
    return Result<GuiSketchDragPreview>::failure(stripped.error());
  auto materialized = SketchTopologyLegacyMaterializer{}.materialize(source_sketch_, stripped.value());
  if (materialized.has_error())
    return Result<GuiSketchDragPreview>::failure(materialized.error());
  auto represented = SketchTopology::migrate_legacy(materialized.value());
  if (represented.has_error())
    return Result<GuiSketchDragPreview>::failure(represented.error());
  if (represented.value() != stripped.value())
    return Result<GuiSketchDragPreview>::failure(drag_error(
        "solver drag preview cannot be represented by legacy PartDocument Sketch JSON without identity loss"));

  SketchSolveResult published_solve = std::move(solve.value());
  published_solve.topology = stripped.value();
  latest_preview_.emplace(pointer, std::move(stripped.value()), std::move(materialized.value()),
                          std::move(published_solve));
  return Result<GuiSketchDragPreview>::success(*latest_preview_);
}

Result<std::size_t> GuiSketchDragController::commit(GuiDocumentSession& session) {
  if (!active() || !latest_preview_ || pending_pointer_)
    return Result<std::size_t>::failure(
        drag_error("committing a Sketch drag requires a flushed valid final preview"));
  if (session.document_kind() != GuiDocumentKind::Part || session.part_document() == nullptr)
    return Result<std::size_t>::failure(drag_error("Sketch drag commit requires an active Part document"));

  const SketchTopology expected_source = source_topology_;
  const SketchConstraintSystem expected_system = source_system_;
  const SketchTopology final_topology = latest_preview_->topology();
  const SketchId target_sketch = source_topology_.sketch();
  auto committed = session.commit_part_transaction(
      "Drag sketch handle",
      [expected_source, expected_system, final_topology, target_sketch](PartDocument& document) {
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
        auto current_system = SketchConstraintSystemBuilder::from_legacy(
            current_topology.value(), current_snapshot, document);
        if (current_system.has_error())
          return Result<std::size_t>::failure(current_system.error());
        if (current_system.value() != expected_system)
          return Result<std::size_t>::failure(Error::validation(
              target_sketch.value(), "Sketch constraint system changed after drag preview; commit refused"));
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
  if (committed.has_error())
    return committed;
  cancel();
  return committed;
}

void GuiSketchDragController::cancel() noexcept {
  active_handle_index_.reset();
  pending_pointer_.reset();
  processed_pointer_.reset();
  latest_preview_.reset();
  solve_count_ = 0U;
}

bool GuiSketchDragController::active() const noexcept { return active_handle_index_.has_value(); }
bool GuiSketchDragController::has_pending() const noexcept { return pending_pointer_.has_value(); }

const GuiSketchDragHandle* GuiSketchDragController::active_handle() const noexcept {
  return active_handle_index_ && *active_handle_index_ < handles_.size()
             ? &handles_[*active_handle_index_]
             : nullptr;
}

const std::optional<GuiSketchDragPreview>&
GuiSketchDragController::latest_preview() const noexcept {
  return latest_preview_;
}

const std::optional<Point2>& GuiSketchDragController::pending_pointer() const noexcept {
  return pending_pointer_;
}

const std::optional<Point2>& GuiSketchDragController::processed_pointer() const noexcept {
  return processed_pointer_;
}

std::size_t GuiSketchDragController::solve_count() const noexcept { return solve_count_; }

} // namespace blcad::gui
