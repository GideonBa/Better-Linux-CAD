#include "blcad/core/sketch_constraint_solver.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace blcad {
namespace {

using NumericVector = std::vector<double>;
using NumericMatrix = std::vector<NumericVector>;

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kPivotTolerance = 1.0e-14;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool positive_finite(double value) noexcept {
  return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] std::size_t expected_target_count(SketchSolverConstraintKind kind) noexcept {
  switch (kind) {
  case SketchSolverConstraintKind::Fixed:
  case SketchSolverConstraintKind::Horizontal:
  case SketchSolverConstraintKind::Vertical:
  case SketchSolverConstraintKind::Radial:
  case SketchSolverConstraintKind::Diameter:
    return 1U;
  case SketchSolverConstraintKind::Coincident:
  case SketchSolverConstraintKind::Parallel:
  case SketchSolverConstraintKind::Perpendicular:
  case SketchSolverConstraintKind::Collinear:
  case SketchSolverConstraintKind::Equal:
  case SketchSolverConstraintKind::Tangent:
  case SketchSolverConstraintKind::Concentric:
  case SketchSolverConstraintKind::Midpoint:
  case SketchSolverConstraintKind::PointOnObject:
  case SketchSolverConstraintKind::HorizontalDistance:
  case SketchSolverConstraintKind::VerticalDistance:
  case SketchSolverConstraintKind::AlignedDistance:
  case SketchSolverConstraintKind::Angular:
    return 2U;
  case SketchSolverConstraintKind::Symmetric:
    return 3U;
  }
  return 0U;
}

[[nodiscard]] bool requires_value(SketchSolverConstraintKind kind) noexcept {
  switch (kind) {
  case SketchSolverConstraintKind::HorizontalDistance:
  case SketchSolverConstraintKind::VerticalDistance:
  case SketchSolverConstraintKind::AlignedDistance:
  case SketchSolverConstraintKind::Radial:
  case SketchSolverConstraintKind::Diameter:
  case SketchSolverConstraintKind::Angular:
    return true;
  default:
    return false;
  }
}

[[nodiscard]] Result<std::size_t> validate_options(const SketchSolverOptions& options) {
  if (!positive_finite(options.convergence_rms))
    return Result<std::size_t>::failure(validation_error(
        "sketch.solver", "solver convergence RMS must be finite and positive"));
  if (!positive_finite(options.finite_difference_relative_step))
    return Result<std::size_t>::failure(validation_error(
        "sketch.solver", "solver finite-difference relative step must be finite and positive"));
  if (!positive_finite(options.rank_absolute_tolerance) ||
      !positive_finite(options.rank_relative_tolerance))
    return Result<std::size_t>::failure(validation_error(
        "sketch.solver", "solver rank tolerances must be finite and positive"));
  if (!positive_finite(options.initial_damping))
    return Result<std::size_t>::failure(
        validation_error("sketch.solver", "solver damping must be finite and positive"));
  if (options.maximum_iterations == 0U || options.maximum_damping_attempts == 0U ||
      options.maximum_line_search_steps == 0U)
    return Result<std::size_t>::failure(validation_error(
        "sketch.solver", "solver iteration and search limits must be greater than zero"));
  return Result<std::size_t>::success(0U);
}

[[nodiscard]] double characteristic_length(const SketchTopology& topology) noexcept {
  if (topology.points().empty()) return 1.0;
  double min_x = topology.points().front().position().x;
  double max_x = min_x;
  double min_y = topology.points().front().position().y;
  double max_y = min_y;
  for (const auto& point : topology.points()) {
    min_x = std::min(min_x, point.position().x);
    max_x = std::max(max_x, point.position().x);
    min_y = std::min(min_y, point.position().y);
    max_y = std::max(max_y, point.position().y);
  }
  return std::max(1.0, std::hypot(max_x - min_x, max_y - min_y));
}

struct SolveContext {
  const SketchTopology& topology;
  std::vector<SketchSolverVariable> variable_order;
  std::unordered_map<std::string, std::size_t> point_variable_base;
  double length_scale;
  // All constraints in the system, so a single constraint's residual can see
  // its neighbours (e.g. line↔circle tangent checks whether an endpoint is
  // held on the circle by a coincidence). Null while the context is built.
  const std::vector<SketchSolverConstraint>* constraints{nullptr};
};

[[nodiscard]] SolveContext make_context(const SketchTopology& topology) {
  SolveContext context{topology, {}, {}, characteristic_length(topology)};
  for (const auto& point : topology.points()) {
    if (point.reference()) continue;
    const std::size_t base = context.variable_order.size();
    context.point_variable_base.emplace(point.id().value(), base);
    context.variable_order.push_back({point.id(), SketchSolverVariableAxis::X});
    context.variable_order.push_back({point.id(), SketchSolverVariableAxis::Y});
  }
  return context;
}

[[nodiscard]] NumericVector initial_variables(const SolveContext& context) {
  NumericVector variables;
  variables.reserve(context.variable_order.size());
  for (const auto& variable : context.variable_order) {
    const auto* point = context.topology.find_point(variable.point);
    variables.push_back(variable.axis == SketchSolverVariableAxis::X ? point->position().x
                                                                     : point->position().y);
  }
  return variables;
}

[[nodiscard]] Point2 point_position(const SolveContext& context, const NumericVector& variables,
                                    const SketchPointId& id) {
  const auto* point = context.topology.find_point(id);
  if (point == nullptr) return {};
  const auto found = context.point_variable_base.find(id.value());
  if (found == context.point_variable_base.end()) return point->position();
  const std::size_t base = found->second;
  return Point2{variables[base], variables[base + 1U]};
}

[[nodiscard]] const SketchTopologyPoint*
resolved_point(const SolveContext& context, const SketchSolverTarget& target) noexcept {
  if (target.kind() != SketchSolverTargetKind::Point) return nullptr;
  return context.topology.find_point(SketchPointId(target.id()));
}

[[nodiscard]] const SketchTopologyEntity*
resolved_entity(const SolveContext& context, const SketchSolverTarget& target) noexcept {
  if (target.kind() != SketchSolverTargetKind::Entity) return nullptr;
  return context.topology.find_entity(target.id());
}

[[nodiscard]] bool is_line(const SketchTopologyEntity* entity) noexcept {
  return entity != nullptr && entity->kind() == SketchTopologyEntityKind::Line;
}

[[nodiscard]] bool is_curve(const SketchTopologyEntity* entity) noexcept {
  return entity != nullptr &&
         (entity->kind() == SketchTopologyEntityKind::Line ||
          entity->kind() == SketchTopologyEntityKind::Arc ||
          entity->kind() == SketchTopologyEntityKind::Spline);
}

[[nodiscard]] bool has_center(const SketchTopologyEntity* entity) noexcept {
  return entity != nullptr &&
         (entity->kind() == SketchTopologyEntityKind::Arc ||
          entity->kind() == SketchTopologyEntityKind::CircleProfile ||
          entity->kind() == SketchTopologyEntityKind::CircularHolePattern);
}

[[nodiscard]] bool has_length_measure(const SketchTopologyEntity* entity) noexcept {
  return entity != nullptr &&
         (entity->kind() == SketchTopologyEntityKind::Line ||
          entity->kind() == SketchTopologyEntityKind::Arc);
}

[[nodiscard]] bool is_point_on_supported_object(const SketchTopologyEntity* entity) noexcept {
  // CircleProfile needs the dimension-driven radius baked as constraint value.
  return entity != nullptr &&
         (entity->kind() == SketchTopologyEntityKind::Line ||
          entity->kind() == SketchTopologyEntityKind::Arc ||
          entity->kind() == SketchTopologyEntityKind::CircleProfile);
}

[[nodiscard]] Result<std::size_t>
validate_constraint_targets(const SolveContext& context, const SketchSolverConstraint& constraint) {
  const auto& targets = constraint.targets();
  const auto require_point = [&](std::size_t index) -> bool {
    return index < targets.size() && resolved_point(context, targets[index]) != nullptr;
  };
  const auto require_entity = [&](std::size_t index) -> const SketchTopologyEntity* {
    return index < targets.size() ? resolved_entity(context, targets[index]) : nullptr;
  };
  const auto fail = [&](std::string message) {
    return Result<std::size_t>::failure(validation_error(constraint.id(), std::move(message)));
  };

  switch (constraint.kind()) {
  case SketchSolverConstraintKind::Coincident:
    if (!require_point(0U) || !require_point(1U))
      return fail("coincident constraint requires two existing Sketch points");
    break;
  case SketchSolverConstraintKind::Fixed:
    if (targets[0].kind() == SketchSolverTargetKind::Point) {
      if (!require_point(0U)) return fail("fixed constraint references an unknown Sketch point");
    } else {
      const auto* entity = require_entity(0U);
      if (entity == nullptr || entity->points().empty())
        return fail("fixed constraint requires an entity with defining Sketch points");
    }
    break;
  case SketchSolverConstraintKind::Horizontal:
  case SketchSolverConstraintKind::Vertical:
    if (!is_line(require_entity(0U)))
      return fail("orientation constraint requires an existing line entity");
    break;
  case SketchSolverConstraintKind::Parallel:
  case SketchSolverConstraintKind::Perpendicular:
  case SketchSolverConstraintKind::Angular:
    if (!is_line(require_entity(0U)) || !is_line(require_entity(1U)))
      return fail("line relationship constraint requires two existing line entities");
    break;
  case SketchSolverConstraintKind::Collinear: {
    // Every target is either a line entity or a resolvable point; the mix must
    // define a direction (a line, or two points) plus something to align.
    std::size_t line_count = 0U;
    std::size_t point_count = 0U;
    for (std::size_t index = 0U; index < targets.size(); ++index) {
      if (targets[index].kind() == SketchSolverTargetKind::Point) {
        if (!require_point(index))
          return fail("collinear constraint references an unknown Sketch point");
        point_count += 1U;
      } else if (is_line(require_entity(index))) {
        line_count += 1U;
      } else {
        return fail("collinear constraint targets must be lines or points");
      }
    }
    if (!(line_count >= 2U || (line_count == 1U && point_count >= 1U) ||
          (line_count == 0U && point_count >= 3U)))
      return fail("collinear needs two-plus lines, a point and a line, or three-plus points");
    break;
  }
  case SketchSolverConstraintKind::Equal:
    if (!has_length_measure(require_entity(0U)) || !has_length_measure(require_entity(1U)))
      return fail("equal constraint requires two measurable line or arc entities");
    break;
  case SketchSolverConstraintKind::Tangent: {
    const auto* first_entity = require_entity(0U);
    const auto* second_entity = require_entity(1U);
    const bool curve_pair = is_curve(first_entity) && is_curve(second_entity);
    const auto is_circle_profile = [](const SketchTopologyEntity* entity) {
      return entity != nullptr && entity->kind() == SketchTopologyEntityKind::CircleProfile;
    };
    // Line↔circle tangency: the circle's radius is dimension-driven and baked
    // into the constraint value by the catalog system builder.
    const bool circle_line = constraint.value().has_value() &&
                             ((is_line(first_entity) && is_circle_profile(second_entity)) ||
                              (is_circle_profile(first_entity) && is_line(second_entity)));
    if (!curve_pair && !circle_line)
      return fail("tangent constraint requires two curves or a line and a circle with radius");
    break;
  }
  case SketchSolverConstraintKind::Concentric:
    if (!has_center(require_entity(0U)) || !has_center(require_entity(1U)))
      return fail("concentric constraint requires two center-bearing arc or circle entities");
    break;
  case SketchSolverConstraintKind::Midpoint:
    if (!require_point(0U) || !is_line(require_entity(1U)))
      return fail("midpoint constraint requires one Sketch point and one line entity");
    break;
  case SketchSolverConstraintKind::Symmetric:
    if (!require_point(0U) || !require_point(1U) || !is_line(require_entity(2U)))
      return fail("symmetric constraint requires two Sketch points and one line axis");
    break;
  case SketchSolverConstraintKind::PointOnObject:
    if (!require_point(0U) || !is_point_on_supported_object(require_entity(1U)))
      return fail("point-on-object constraint requires one Sketch point and one line or arc entity");
    break;
  case SketchSolverConstraintKind::HorizontalDistance:
  case SketchSolverConstraintKind::VerticalDistance:
  case SketchSolverConstraintKind::AlignedDistance:
    if (!require_point(0U) || !require_point(1U))
      return fail("distance constraint requires two existing Sketch points");
    break;
  case SketchSolverConstraintKind::Radial:
  case SketchSolverConstraintKind::Diameter:
    if (require_entity(0U) == nullptr ||
        require_entity(0U)->kind() != SketchTopologyEntityKind::Arc)
      return fail("radial or diameter constraint requires an existing arc entity");
    break;
  }
  return Result<std::size_t>::success(0U);
}

[[nodiscard]] Point2 subtract(Point2 left, Point2 right) noexcept {
  return Point2{left.x - right.x, left.y - right.y};
}

[[nodiscard]] Point2 add(Point2 left, Point2 right) noexcept {
  return Point2{left.x + right.x, left.y + right.y};
}

[[nodiscard]] Point2 scale(Point2 point, double factor) noexcept {
  return Point2{point.x * factor, point.y * factor};
}

[[nodiscard]] double dot(Point2 left, Point2 right) noexcept {
  return left.x * right.x + left.y * right.y;
}

[[nodiscard]] double cross(Point2 left, Point2 right) noexcept {
  return left.x * right.y - left.y * right.x;
}

[[nodiscard]] double magnitude(Point2 point) noexcept {
  return std::hypot(point.x, point.y);
}

[[nodiscard]] double safe_magnitude(Point2 point, double length_scale) noexcept {
  return std::max(magnitude(point), 1.0e-12 * length_scale);
}

struct ArcGeometry {
  Point2 center;
  double radius;
};

[[nodiscard]] ArcGeometry arc_geometry(const SolveContext& context,
                                       const NumericVector& variables,
                                       const SketchTopologyEntity& entity) noexcept {
  const Point2 start = point_position(context, variables, entity.points()[0]);
  const Point2 mid = point_position(context, variables, entity.points()[1]);
  const Point2 end = point_position(context, variables, entity.points()[2]);
  double denominator =
      2.0 * (start.x * (mid.y - end.y) + mid.x * (end.y - start.y) +
             end.x * (start.y - mid.y));
  const double floor = 1.0e-14 * context.length_scale * context.length_scale;
  if (std::abs(denominator) < floor)
    denominator = denominator < 0.0 ? -floor : floor;
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
  return ArcGeometry{center, magnitude(subtract(start, center))};
}

[[nodiscard]] Point2 line_vector(const SolveContext& context, const NumericVector& variables,
                                 const SketchTopologyEntity& entity) noexcept {
  return subtract(point_position(context, variables, entity.points()[1]),
                  point_position(context, variables, entity.points()[0]));
}

[[nodiscard]] Point2 entity_center(const SolveContext& context, const NumericVector& variables,
                                   const SketchTopologyEntity& entity) noexcept {
  if (entity.kind() == SketchTopologyEntityKind::Arc)
    return arc_geometry(context, variables, entity).center;
  return point_position(context, variables, entity.points()[0]);
}

[[nodiscard]] double entity_measure(const SolveContext& context, const NumericVector& variables,
                                    const SketchTopologyEntity& entity) noexcept {
  if (entity.kind() == SketchTopologyEntityKind::Arc)
    return arc_geometry(context, variables, entity).radius;
  return magnitude(line_vector(context, variables, entity));
}

[[nodiscard]] std::optional<SketchPointId>
shared_curve_endpoint(const SketchTopologyEntity& first,
                      const SketchTopologyEntity& second) noexcept {
  const auto endpoint_ids = [](const SketchTopologyEntity& entity) {
    std::vector<SketchPointId> result;
    if (entity.kind() == SketchTopologyEntityKind::Line) {
      result = {entity.points()[0], entity.points()[1]};
    } else if (entity.kind() == SketchTopologyEntityKind::Arc) {
      result = {entity.points()[0], entity.points()[2]};
    } else if (entity.kind() == SketchTopologyEntityKind::Spline) {
      result = {entity.points()[0], entity.points()[3]};
    }
    return result;
  };
  const auto first_endpoints = endpoint_ids(first);
  const auto second_endpoints = endpoint_ids(second);
  for (const auto& left : first_endpoints)
    for (const auto& right : second_endpoints)
      if (left == right) return left;
  return std::nullopt;
}

[[nodiscard]] Point2 curve_tangent(const SolveContext& context, const NumericVector& variables,
                                   const SketchTopologyEntity& entity,
                                   const SketchPointId& endpoint) noexcept {
  if (entity.kind() == SketchTopologyEntityKind::Line) {
    if (entity.points()[0] == endpoint)
      return subtract(point_position(context, variables, entity.points()[1]),
                      point_position(context, variables, entity.points()[0]));
    return subtract(point_position(context, variables, entity.points()[0]),
                    point_position(context, variables, entity.points()[1]));
  }
  if (entity.kind() == SketchTopologyEntityKind::Spline) {
    if (entity.points()[0] == endpoint)
      return subtract(point_position(context, variables, entity.points()[1]),
                      point_position(context, variables, entity.points()[0]));
    return subtract(point_position(context, variables, entity.points()[2]),
                    point_position(context, variables, entity.points()[3]));
  }
  const ArcGeometry arc = arc_geometry(context, variables, entity);
  const Point2 radial = subtract(point_position(context, variables, endpoint), arc.center);
  return Point2{-radial.y, radial.x};
}

// Tangent anchors: the endpoint of each curve at the shared corner. Curves
// connected through one shared topology point use it for both; curves coupled
// positionally (e.g. via a Coincident constraint between their distinct
// endpoints — the normal case for interactively drawn geometry) use the
// closest coincident endpoint pair.
struct SharedEndpointPair {
  SketchPointId first;
  SketchPointId second;
};

[[nodiscard]] std::optional<SharedEndpointPair>
shared_curve_endpoint_pair(const SolveContext& context, const SketchTopologyEntity& first,
                           const SketchTopologyEntity& second) noexcept {
  if (const auto shared = shared_curve_endpoint(first, second))
    return SharedEndpointPair{*shared, *shared};
  const auto endpoint_ids = [](const SketchTopologyEntity& entity) {
    std::vector<SketchPointId> result;
    if (entity.kind() == SketchTopologyEntityKind::Line) {
      result = {entity.points()[0], entity.points()[1]};
    } else if (entity.kind() == SketchTopologyEntityKind::Arc) {
      result = {entity.points()[0], entity.points()[2]};
    } else if (entity.kind() == SketchTopologyEntityKind::Spline) {
      result = {entity.points()[0], entity.points()[3]};
    }
    return result;
  };
  // Pair on the SOURCE topology positions so the pairing is stable across
  // solve iterations (the free corner may drift while iterating).
  const auto source_position = [&context](const SketchPointId& id) -> std::optional<Point2> {
    const auto* point = context.topology.find_point(id);
    return point == nullptr ? std::nullopt : std::optional(point->position());
  };
  std::optional<SharedEndpointPair> best;
  double best_distance = 1.0e-2; // squared (0.1 mm): touching corners only
  for (const auto& left : endpoint_ids(first)) {
    const auto left_position = source_position(left);
    if (!left_position)
      continue;
    for (const auto& right : endpoint_ids(second)) {
      const auto right_position = source_position(right);
      if (!right_position)
        continue;
      const double dx = left_position->x - right_position->x;
      const double dy = left_position->y - right_position->y;
      const double squared = dx * dx + dy * dy;
      if (squared < best_distance) {
        best_distance = squared;
        best = SharedEndpointPair{left, right};
      }
    }
  }
  return best;
}

[[nodiscard]] double wrapped_angle(double angle) noexcept {
  while (angle <= -kPi) angle += 2.0 * kPi;
  while (angle > kPi) angle -= 2.0 * kPi;
  return angle;
}

[[nodiscard]] Result<NumericVector>
evaluate_constraint(const SolveContext& context, const NumericVector& variables,
                    const SketchSolverConstraint& constraint) {
  const auto& targets = constraint.targets();
  const auto point = [&](std::size_t index) {
    return point_position(context, variables, SketchPointId(targets[index].id()));
  };
  const auto entity = [&](std::size_t index) -> const SketchTopologyEntity& {
    return *context.topology.find_entity(targets[index].id());
  };
  const double length_scale = context.length_scale;

  switch (constraint.kind()) {
  case SketchSolverConstraintKind::Coincident: {
    const Point2 delta = subtract(point(0U), point(1U));
    return Result<NumericVector>::success({delta.x / length_scale, delta.y / length_scale});
  }
  case SketchSolverConstraintKind::Fixed: {
    NumericVector residuals;
    if (targets[0].kind() == SketchSolverTargetKind::Point) {
      const SketchPointId id(targets[0].id());
      const Point2 current = point_position(context, variables, id);
      const Point2 source = context.topology.find_point(id)->position();
      residuals = {(current.x - source.x) / length_scale,
                   (current.y - source.y) / length_scale};
    } else {
      for (const auto& id : entity(0U).points()) {
        const Point2 current = point_position(context, variables, id);
        const Point2 source = context.topology.find_point(id)->position();
        residuals.push_back((current.x - source.x) / length_scale);
        residuals.push_back((current.y - source.y) / length_scale);
      }
    }
    return Result<NumericVector>::success(std::move(residuals));
  }
  case SketchSolverConstraintKind::Horizontal: {
    const Point2 direction = line_vector(context, variables, entity(0U));
    return Result<NumericVector>::success({direction.y / length_scale});
  }
  case SketchSolverConstraintKind::Vertical: {
    const Point2 direction = line_vector(context, variables, entity(0U));
    return Result<NumericVector>::success({direction.x / length_scale});
  }
  case SketchSolverConstraintKind::Parallel:
  case SketchSolverConstraintKind::Perpendicular: {
    const Point2 first_direction = line_vector(context, variables, entity(0U));
    const Point2 second_direction = line_vector(context, variables, entity(1U));
    const double denominator = safe_magnitude(first_direction, length_scale) *
                               safe_magnitude(second_direction, length_scale);
    if (constraint.kind() == SketchSolverConstraintKind::Parallel)
      return Result<NumericVector>::success(
          {cross(first_direction, second_direction) / denominator});
    return Result<NumericVector>::success(
        {dot(first_direction, second_direction) / denominator});
  }
  case SketchSolverConstraintKind::Collinear: {
    // Generalized: everything shares one infinite line. The reference direction
    // and anchor come from the first line target, or (all-points) from the
    // first two points. Each other line contributes parallelism + anchor-on-line
    // residuals; each aligned point contributes an on-line residual. Two lines
    // reduces to the original (parallel + second-start-on-first) formulation.
    Point2 reference_direction{};
    Point2 reference_anchor{};
    std::size_t first_point_to_align = 0U;
    const auto* first_line = [&]() -> const SketchTopologyEntity* {
      for (const auto& target : targets)
        if (target.kind() == SketchSolverTargetKind::Point)
          continue;
        else
          return context.topology.find_entity(target.id());
      return nullptr;
    }();
    if (first_line != nullptr) {
      reference_direction = line_vector(context, variables, *first_line);
      reference_anchor = point_position(context, variables, first_line->points()[0]);
    } else {
      // All points: the first two define the reference line, align the rest.
      reference_anchor = point(0U);
      reference_direction = subtract(point(1U), reference_anchor);
      first_point_to_align = 2U;
    }
    const double direction_scale = safe_magnitude(reference_direction, length_scale);
    NumericVector residuals;
    std::size_t point_index = 0U;
    for (const auto& target : targets) {
      if (target.kind() == SketchSolverTargetKind::Point) {
        const std::size_t current = point_index++;
        if (first_line == nullptr && current < first_point_to_align)
          continue; // reference points define the line, not aligned to it
        const Point2 position = point_position(context, variables, SketchPointId(target.id()));
        residuals.push_back(cross(reference_direction, subtract(position, reference_anchor)) /
                            (direction_scale * length_scale));
        continue;
      }
      const auto* line = context.topology.find_entity(target.id());
      if (line == first_line)
        continue; // the reference line is not constrained against itself
      const Point2 direction = line_vector(context, variables, *line);
      const Point2 start = point_position(context, variables, line->points()[0]);
      residuals.push_back(cross(reference_direction, direction) /
                          (direction_scale * safe_magnitude(direction, length_scale)));
      residuals.push_back(cross(reference_direction, subtract(start, reference_anchor)) /
                          (direction_scale * length_scale));
    }
    return Result<NumericVector>::success(std::move(residuals));
  }
  case SketchSolverConstraintKind::Equal:
    return Result<NumericVector>::success(
        {(entity_measure(context, variables, entity(0U)) -
          entity_measure(context, variables, entity(1U))) /
         length_scale});
  case SketchSolverConstraintKind::Tangent: {
    const bool first_is_circle =
        entity(0U).kind() == SketchTopologyEntityKind::CircleProfile;
    const bool second_is_circle =
        entity(1U).kind() == SketchTopologyEntityKind::CircleProfile;
    if (constraint.value() && (first_is_circle || second_is_circle)) {
      // Line↔circle tangency: perpendicular center-to-line distance equals the
      // (constant, dimension-driven) radius. The residual is first order in
      // length (distance − radius), matching coincidence/point-on-object, so a
      // line whose endpoint is coincident on the same circle stays well
      // conditioned. The earlier squared form (d²−r²) scaled with length² and
      // made such mixed systems ill-conditioned — the solver crept toward the
      // tolerance and hit the iteration limit (non_convergent). abs() admits
      // both tangent sides, matching the squared form's solution set.
      const auto& circle = first_is_circle ? entity(0U) : entity(1U);
      const auto& line = first_is_circle ? entity(1U) : entity(0U);
      const Point2 center = entity_center(context, variables, circle);
      const Point2 direction = line_vector(context, variables, line);
      const double length = std::sqrt(std::max(dot(direction, direction), 1.0e-12));
      const double radius = *constraint.value();
      // If a line endpoint is held on this circle by a coincidence
      // (PointOnObject — how the GUI models "coincident point-on-curve"), the
      // contact point IS that endpoint. Express tangency as the line being
      // perpendicular to the radius there (direction · (endpoint − center) = 0).
      // This stays orthogonal to the point-on-circle constraint, whereas the
      // distance form (below) shares its radial gradient at the solution and
      // leaves the coupled system degenerate — the solver then crept to the
      // iteration limit (non_convergent) even though the geometry was solvable.
      const auto endpoint_on_circle = [&]() -> std::optional<SketchPointId> {
        if (context.constraints == nullptr)
          return std::nullopt;
        for (const auto& other : *context.constraints) {
          if (other.kind() != SketchSolverConstraintKind::PointOnObject &&
              other.kind() != SketchSolverConstraintKind::Coincident)
            continue;
          const auto& other_targets = other.targets();
          if (other_targets.size() != 2U)
            continue;
          const bool holds_circle = std::any_of(
              other_targets.begin(), other_targets.end(), [&](const SketchSolverTarget& target) {
                return target.kind() == SketchSolverTargetKind::Entity &&
                       target.id() == circle.id();
              });
          if (!holds_circle)
            continue;
          for (const auto& target : other_targets) {
            if (target.kind() != SketchSolverTargetKind::Point)
              continue;
            const SketchPointId held(target.id());
            if (held == line.points()[0] || held == line.points()[1])
              return held;
          }
        }
        return std::nullopt;
      }();
      if (endpoint_on_circle) {
        const Point2 contact = point_position(context, variables, *endpoint_on_circle);
        return Result<NumericVector>::success(
            {dot(direction, subtract(contact, center)) / (length * length_scale)});
      }
      const Point2 start = point_position(context, variables, line.points()[0]);
      const double crossed = cross(direction, subtract(center, start));
      return Result<NumericVector>::success(
          {(std::abs(crossed) / length - radius) / length_scale});
    }
    const auto shared = shared_curve_endpoint_pair(context, entity(0U), entity(1U));
    if (!shared)
      return Result<NumericVector>::failure(validation_error(
          constraint.id(),
          "tangent constraint curves must share or touch at a common endpoint"));
    const Point2 first_tangent = curve_tangent(context, variables, entity(0U), shared->first);
    const Point2 second_tangent = curve_tangent(context, variables, entity(1U), shared->second);
    return Result<NumericVector>::success(
        {cross(first_tangent, second_tangent) /
         (safe_magnitude(first_tangent, length_scale) *
          safe_magnitude(second_tangent, length_scale))});
  }
  case SketchSolverConstraintKind::Concentric: {
    const Point2 delta = subtract(entity_center(context, variables, entity(0U)),
                                  entity_center(context, variables, entity(1U)));
    return Result<NumericVector>::success({delta.x / length_scale, delta.y / length_scale});
  }
  case SketchSolverConstraintKind::Midpoint: {
    const Point2 start = point_position(context, variables, entity(1U).points()[0]);
    const Point2 end = point_position(context, variables, entity(1U).points()[1]);
    const Point2 expected = scale(add(start, end), 0.5);
    const Point2 delta = subtract(point(0U), expected);
    return Result<NumericVector>::success({delta.x / length_scale, delta.y / length_scale});
  }
  case SketchSolverConstraintKind::Symmetric: {
    const Point2 first = point(0U);
    const Point2 second = point(1U);
    const Point2 axis_start = point_position(context, variables, entity(2U).points()[0]);
    const Point2 axis_direction = line_vector(context, variables, entity(2U));
    const Point2 midpoint = scale(add(first, second), 0.5);
    const Point2 pair_direction = subtract(second, first);
    return Result<NumericVector>::success(
        {cross(axis_direction, subtract(midpoint, axis_start)) /
             (safe_magnitude(axis_direction, length_scale) * length_scale),
         dot(pair_direction, axis_direction) /
             (safe_magnitude(pair_direction, length_scale) *
              safe_magnitude(axis_direction, length_scale))});
  }
  case SketchSolverConstraintKind::PointOnObject: {
    const Point2 candidate = point(0U);
    if (entity(1U).kind() == SketchTopologyEntityKind::Line) {
      const Point2 start = point_position(context, variables, entity(1U).points()[0]);
      const Point2 direction = line_vector(context, variables, entity(1U));
      return Result<NumericVector>::success(
          {cross(direction, subtract(candidate, start)) /
           (safe_magnitude(direction, length_scale) * length_scale)});
    }
    if (entity(1U).kind() == SketchTopologyEntityKind::CircleProfile) {
      if (!constraint.value())
        return Result<NumericVector>::failure(validation_error(
            constraint.id(), "point-on-circle requires the baked circle radius"));
      const Point2 center = entity_center(context, variables, entity(1U));
      return Result<NumericVector>::success(
          {(magnitude(subtract(candidate, center)) - *constraint.value()) / length_scale});
    }
    const ArcGeometry arc = arc_geometry(context, variables, entity(1U));
    return Result<NumericVector>::success(
        {(magnitude(subtract(candidate, arc.center)) - arc.radius) / length_scale});
  }
  case SketchSolverConstraintKind::HorizontalDistance:
    return Result<NumericVector>::success(
        {(point(1U).x - point(0U).x - *constraint.value()) / length_scale});
  case SketchSolverConstraintKind::VerticalDistance:
    return Result<NumericVector>::success(
        {(point(1U).y - point(0U).y - *constraint.value()) / length_scale});
  case SketchSolverConstraintKind::AlignedDistance:
    return Result<NumericVector>::success(
        {(magnitude(subtract(point(1U), point(0U))) - *constraint.value()) / length_scale});
  case SketchSolverConstraintKind::Radial: {
    const ArcGeometry arc = arc_geometry(context, variables, entity(0U));
    return Result<NumericVector>::success({(arc.radius - *constraint.value()) / length_scale});
  }
  case SketchSolverConstraintKind::Diameter: {
    const ArcGeometry arc = arc_geometry(context, variables, entity(0U));
    return Result<NumericVector>::success(
        {(2.0 * arc.radius - *constraint.value()) / length_scale});
  }
  case SketchSolverConstraintKind::Angular: {
    const Point2 first_direction = line_vector(context, variables, entity(0U));
    const Point2 second_direction = line_vector(context, variables, entity(1U));
    const double angle = std::atan2(cross(first_direction, second_direction),
                                    dot(first_direction, second_direction));
    const double target = *constraint.value() * kPi / 180.0;
    return Result<NumericVector>::success({wrapped_angle(angle - target) / kPi});
  }
  }
  return Result<NumericVector>::failure(
      Error::internal(constraint.id(), "unsupported Sketch solver constraint kind"));
}

[[nodiscard]] Result<NumericVector>
evaluate_constraints(const SolveContext& context, const NumericVector& variables,
                     const std::vector<SketchSolverConstraint>& constraints) {
  NumericVector residuals;
  for (const auto& constraint : constraints) {
    auto block = evaluate_constraint(context, variables, constraint);
    if (block.has_error()) return Result<NumericVector>::failure(block.error());
    residuals.insert(residuals.end(), block.value().begin(), block.value().end());
  }
  return Result<NumericVector>::success(std::move(residuals));
}

[[nodiscard]] double residual_rms(const NumericVector& residuals) noexcept {
  if (residuals.empty()) return 0.0;
  double sum = 0.0;
  for (double residual : residuals) sum += residual * residual;
  return std::sqrt(sum / static_cast<double>(residuals.size()));
}

[[nodiscard]] double residual_max_abs(const NumericVector& residuals) noexcept {
  double maximum = 0.0;
  for (double residual : residuals) maximum = std::max(maximum, std::abs(residual));
  return maximum;
}

[[nodiscard]] Result<NumericMatrix>
build_jacobian(const SolveContext& context, const NumericVector& variables,
               const std::vector<SketchSolverConstraint>& constraints,
               const SketchSolverOptions& options) {
  auto center = evaluate_constraints(context, variables, constraints);
  if (center.has_error()) return Result<NumericMatrix>::failure(center.error());
  NumericMatrix jacobian(center.value().size(), NumericVector(variables.size(), 0.0));
  if (variables.empty() || center.value().empty())
    return Result<NumericMatrix>::success(std::move(jacobian));

  const double step = options.finite_difference_relative_step * context.length_scale;
  for (std::size_t column = 0U; column < variables.size(); ++column) {
    NumericVector plus = variables;
    NumericVector minus = variables;
    plus[column] += step;
    minus[column] -= step;
    auto plus_residuals = evaluate_constraints(context, plus, constraints);
    if (plus_residuals.has_error()) return Result<NumericMatrix>::failure(plus_residuals.error());
    auto minus_residuals = evaluate_constraints(context, minus, constraints);
    if (minus_residuals.has_error()) return Result<NumericMatrix>::failure(minus_residuals.error());
    if (plus_residuals.value().size() != center.value().size() ||
        minus_residuals.value().size() != center.value().size())
      return Result<NumericMatrix>::failure(Error::internal(
          "sketch.solver", "solver residual dimension changed during finite difference"));
    for (std::size_t row = 0U; row < jacobian.size(); ++row)
      jacobian[row][column] =
          (plus_residuals.value()[row] - minus_residuals.value()[row]) / (2.0 * step);
  }
  return Result<NumericMatrix>::success(std::move(jacobian));
}

[[nodiscard]] bool solve_linear_system(NumericMatrix matrix, NumericVector right_hand_side,
                                       NumericVector& solution) {
  const std::size_t dimension = right_hand_side.size();
  if (matrix.size() != dimension) return false;
  for (const auto& row : matrix)
    if (row.size() != dimension) return false;

  for (std::size_t column = 0U; column < dimension; ++column) {
    std::size_t pivot_row = column;
    double pivot_magnitude = std::abs(matrix[column][column]);
    for (std::size_t row = column + 1U; row < dimension; ++row) {
      const double magnitude = std::abs(matrix[row][column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot_row = row;
      }
    }
    if (!std::isfinite(pivot_magnitude) || pivot_magnitude <= kPivotTolerance) return false;
    if (pivot_row != column) {
      std::swap(matrix[pivot_row], matrix[column]);
      std::swap(right_hand_side[pivot_row], right_hand_side[column]);
    }
    const double pivot = matrix[column][column];
    for (std::size_t row = column + 1U; row < dimension; ++row) {
      const double factor = matrix[row][column] / pivot;
      if (!std::isfinite(factor)) return false;
      for (std::size_t entry = column; entry < dimension; ++entry)
        matrix[row][entry] -= factor * matrix[column][entry];
      right_hand_side[row] -= factor * right_hand_side[column];
    }
  }

  solution.assign(dimension, 0.0);
  for (std::size_t reverse = 0U; reverse < dimension; ++reverse) {
    const std::size_t row = dimension - 1U - reverse;
    double value = right_hand_side[row];
    for (std::size_t column = row + 1U; column < dimension; ++column)
      value -= matrix[row][column] * solution[column];
    const double pivot = matrix[row][row];
    if (!std::isfinite(pivot) || std::abs(pivot) <= kPivotTolerance) return false;
    solution[row] = value / pivot;
    if (!std::isfinite(solution[row])) return false;
  }
  return true;
}

struct NormalEquations {
  NumericMatrix normal;
  NumericVector rhs;
};

// Builds J^T J and -J^T r once per iteration; damping only perturbs the diagonal
// per attempt, so the expensive accumulation is not repeated per damping attempt.
[[nodiscard]] bool build_normal_equations(const NumericMatrix& jacobian,
                                          const NumericVector& residuals,
                                          NormalEquations& equations) {
  if (jacobian.size() != residuals.size()) return false;
  const std::size_t variable_count = jacobian.empty() ? 0U : jacobian.front().size();
  equations.normal.assign(variable_count, NumericVector(variable_count, 0.0));
  equations.rhs.assign(variable_count, 0.0);
  for (std::size_t row = 0U; row < jacobian.size(); ++row) {
    if (jacobian[row].size() != variable_count) return false;
    for (std::size_t column = 0U; column < variable_count; ++column) {
      equations.rhs[column] -= jacobian[row][column] * residuals[row];
      for (std::size_t other = 0U; other < variable_count; ++other)
        equations.normal[column][other] += jacobian[row][column] * jacobian[row][other];
    }
  }
  return true;
}

[[nodiscard]] bool damped_gauss_newton_step(const NormalEquations& equations, double damping,
                                            NumericVector& step) {
  NumericMatrix normal = equations.normal;
  for (std::size_t index = 0U; index < normal.size(); ++index)
    normal[index][index] += damping;
  return solve_linear_system(std::move(normal), equations.rhs, step);
}

enum class NumericSolveState { Converged, Stalled, MaximumIterations };

struct NumericSolveOutcome {
  NumericSolveState state;
  std::size_t iterations;
  NumericVector initial_residuals;
  NumericVector final_residuals;
  NumericVector variables;
};

[[nodiscard]] Result<NumericSolveOutcome>
solve_numeric(const SolveContext& context, const std::vector<SketchSolverConstraint>& constraints,
              const SketchSolverOptions& options, const NumericVector& initial) {
  auto initial_residuals = evaluate_constraints(context, initial, constraints);
  if (initial_residuals.has_error())
    return Result<NumericSolveOutcome>::failure(initial_residuals.error());
  NumericVector variables = initial;
  NumericVector current = initial_residuals.value();
  if (current.empty() || residual_rms(current) <= options.convergence_rms)
    return Result<NumericSolveOutcome>::success(
        {NumericSolveState::Converged, 0U, initial_residuals.value(), current, variables});
  if (variables.empty())
    return Result<NumericSolveOutcome>::success(
        {NumericSolveState::Stalled, 0U, initial_residuals.value(), current, variables});

  std::size_t completed_iterations = 0U;
  for (std::size_t iteration = 0U; iteration < options.maximum_iterations; ++iteration) {
    auto jacobian = build_jacobian(context, variables, constraints, options);
    if (jacobian.has_error()) return Result<NumericSolveOutcome>::failure(jacobian.error());
    NormalEquations equations;
    if (!build_normal_equations(jacobian.value(), current, equations))
      return Result<NumericSolveOutcome>::failure(Error::internal(
          "sketch.solver", "solver Jacobian and residual dimensions are inconsistent"));
    const double current_rms = residual_rms(current);
    bool accepted = false;
    for (std::size_t damping_attempt = 0U;
         damping_attempt < options.maximum_damping_attempts && !accepted; ++damping_attempt) {
      const double damping = options.initial_damping * std::pow(10.0, damping_attempt);
      NumericVector step;
      if (!damped_gauss_newton_step(equations, damping, step)) continue;
      for (std::size_t line_search = 0U; line_search < options.maximum_line_search_steps;
           ++line_search) {
        const double factor = std::ldexp(1.0, -static_cast<int>(line_search));
        NumericVector candidate = variables;
        for (std::size_t index = 0U; index < candidate.size(); ++index)
          candidate[index] += factor * step[index];
        auto candidate_residuals = evaluate_constraints(context, candidate, constraints);
        if (candidate_residuals.has_error())
          return Result<NumericSolveOutcome>::failure(candidate_residuals.error());
        const double candidate_rms = residual_rms(candidate_residuals.value());
        if (candidate_rms <= options.convergence_rms || candidate_rms < current_rms) {
          variables = std::move(candidate);
          current = std::move(candidate_residuals.value());
          accepted = true;
          ++completed_iterations;
          break;
        }
      }
    }
    if (residual_rms(current) <= options.convergence_rms)
      return Result<NumericSolveOutcome>::success(
          {NumericSolveState::Converged, completed_iterations, initial_residuals.value(), current,
           variables});
    if (!accepted)
      return Result<NumericSolveOutcome>::success(
          {NumericSolveState::Stalled, completed_iterations, initial_residuals.value(), current,
           variables});
  }
  return Result<NumericSolveOutcome>::success(
      {NumericSolveState::MaximumIterations, completed_iterations, initial_residuals.value(), current,
       variables});
}

[[nodiscard]] Result<std::size_t> matrix_rank(NumericMatrix matrix, std::size_t column_count,
                                              const SketchSolverOptions& options) {
  double maximum = 0.0;
  for (const auto& row : matrix) {
    if (row.size() != column_count)
      return Result<std::size_t>::failure(Error::internal(
          "sketch.solver", "solver Jacobian row width does not match variable count"));
    for (double value : row) {
      if (!std::isfinite(value))
        return Result<std::size_t>::failure(
            Error::internal("sketch.solver", "solver Jacobian entries must be finite"));
      maximum = std::max(maximum, std::abs(value));
    }
  }
  const double threshold =
      std::max(options.rank_absolute_tolerance, options.rank_relative_tolerance * maximum);
  std::size_t rank = 0U;
  std::size_t column = 0U;
  while (rank < matrix.size() && column < column_count) {
    std::size_t pivot_row = rank;
    double pivot_magnitude = std::abs(matrix[rank][column]);
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double candidate = std::abs(matrix[row][column]);
      if (candidate > pivot_magnitude) {
        pivot_magnitude = candidate;
        pivot_row = row;
      }
    }
    if (pivot_magnitude <= threshold) {
      ++column;
      continue;
    }
    if (pivot_row != rank) std::swap(matrix[pivot_row], matrix[rank]);
    const double pivot = matrix[rank][column];
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double factor = matrix[row][column] / pivot;
      matrix[row][column] = 0.0;
      for (std::size_t entry = column + 1U; entry < column_count; ++entry)
        matrix[row][entry] -= factor * matrix[rank][entry];
    }
    ++rank;
    ++column;
  }
  return Result<std::size_t>::success(rank);
}

[[nodiscard]] Result<SketchTopology>
topology_from_variables(const SolveContext& context, const NumericVector& variables) {
  std::vector<SketchTopologyPoint> points;
  points.reserve(context.topology.points().size());
  for (const auto& point : context.topology.points()) {
    auto updated = point.with_position(point_position(context, variables, point.id()));
    if (updated.has_error()) return Result<SketchTopology>::failure(updated.error());
    points.push_back(std::move(updated.value()));
  }
  return SketchTopology::create(context.topology.sketch(), std::move(points),
                                context.topology.entities(), context.topology.dependencies());
}

[[nodiscard]] SketchSolveResult invalid_reference_result(
    const SolveContext& context, const SketchTopology& topology, const SketchSolverConstraint& constraint,
    const Error& error) {
  return SketchSolveResult{
      SketchSolveStatus::InvalidReference,
      0U,
      topology,
      context.variable_order,
      0U,
      context.variable_order.size(),
      {},
      {},
      {{SketchSolverDiagnosticKind::InvalidReference, constraint.id(), error.message()}},
      {0U, context.length_scale, 0.0, 0.0, 0.0}};
}

// Attributes canonical incremental redundancy by appending each constraint's
// rows of the already-built final Jacobian. Central-difference rows of one
// constraint are independent of the other constraints, so slicing the full
// Jacobian in canonical block order is numerically identical to rebuilding
// each single-constraint Jacobian.
[[nodiscard]] Result<std::vector<std::string>>
find_redundant_constraints(const SolveContext& context, const NumericVector& variables,
                           const std::vector<SketchSolverConstraint>& constraints,
                           const NumericMatrix& jacobian, const SketchSolverOptions& options) {
  NumericMatrix accumulated;
  accumulated.reserve(jacobian.size());
  std::size_t previous_rank = 0U;
  std::size_t row_offset = 0U;
  std::vector<std::string> redundant;
  for (const auto& constraint : constraints) {
    auto block = evaluate_constraint(context, variables, constraint);
    if (block.has_error()) return Result<std::vector<std::string>>::failure(block.error());
    const std::size_t block_rows = block.value().size();
    if (row_offset + block_rows > jacobian.size())
      return Result<std::vector<std::string>>::failure(Error::internal(
          "sketch.solver", "solver Jacobian rows do not cover the canonical constraint blocks"));
    accumulated.insert(accumulated.end(),
                       jacobian.begin() + static_cast<std::ptrdiff_t>(row_offset),
                       jacobian.begin() + static_cast<std::ptrdiff_t>(row_offset + block_rows));
    row_offset += block_rows;
    auto rank = matrix_rank(accumulated, variables.size(), options);
    if (rank.has_error()) return Result<std::vector<std::string>>::failure(rank.error());
    if (block_rows != 0U && rank.value() == previous_rank)
      redundant.push_back(constraint.id());
    previous_rank = rank.value();
  }
  if (row_offset != jacobian.size())
    return Result<std::vector<std::string>>::failure(Error::internal(
        "sketch.solver", "solver Jacobian rows do not cover the canonical constraint blocks"));
  return Result<std::vector<std::string>>::success(std::move(redundant));
}

[[nodiscard]] Result<std::vector<std::string>>
find_conflicting_constraints(const SolveContext& context, const NumericVector& initial,
                             const std::vector<SketchSolverConstraint>& constraints,
                             const SketchSolverOptions& options) {
  std::vector<std::string> conflicts;
  for (std::size_t omitted = 0U; omitted < constraints.size(); ++omitted) {
    std::vector<SketchSolverConstraint> reduced;
    reduced.reserve(constraints.size() - 1U);
    for (std::size_t index = 0U; index < constraints.size(); ++index)
      if (index != omitted) reduced.push_back(constraints[index]);
    auto outcome = solve_numeric(context, reduced, options, initial);
    if (outcome.has_error()) return Result<std::vector<std::string>>::failure(outcome.error());
    if (outcome.value().state == NumericSolveState::Converged)
      conflicts.push_back(constraints[omitted].id());
  }
  return Result<std::vector<std::string>>::success(std::move(conflicts));
}

} // namespace

std::string_view to_string(SketchSolverTargetKind kind) noexcept {
  switch (kind) {
  case SketchSolverTargetKind::Point: return "point";
  case SketchSolverTargetKind::Entity: return "entity";
  }
  return "point";
}

Result<SketchSolverTarget> SketchSolverTarget::point(SketchPointId id) {
  if (id.empty())
    return Result<SketchSolverTarget>::failure(
        validation_error("sketch_solver_target", "Sketch solver point target id must not be empty"));
  return Result<SketchSolverTarget>::success(
      SketchSolverTarget(SketchSolverTargetKind::Point, id.value()));
}

Result<SketchSolverTarget> SketchSolverTarget::entity(std::string id) {
  if (id.empty())
    return Result<SketchSolverTarget>::failure(
        validation_error("sketch_solver_target", "Sketch solver entity target id must not be empty"));
  return Result<SketchSolverTarget>::success(
      SketchSolverTarget(SketchSolverTargetKind::Entity, std::move(id)));
}

SketchSolverTarget::SketchSolverTarget(SketchSolverTargetKind kind, std::string id)
    : kind_(kind), id_(std::move(id)) {}

SketchSolverTargetKind SketchSolverTarget::kind() const noexcept { return kind_; }
const std::string& SketchSolverTarget::id() const noexcept { return id_; }

std::string_view to_string(SketchSolverConstraintKind kind) noexcept {
  switch (kind) {
  case SketchSolverConstraintKind::Coincident: return "coincident";
  case SketchSolverConstraintKind::Fixed: return "fixed";
  case SketchSolverConstraintKind::Horizontal: return "horizontal";
  case SketchSolverConstraintKind::Vertical: return "vertical";
  case SketchSolverConstraintKind::Parallel: return "parallel";
  case SketchSolverConstraintKind::Perpendicular: return "perpendicular";
  case SketchSolverConstraintKind::Collinear: return "collinear";
  case SketchSolverConstraintKind::Equal: return "equal";
  case SketchSolverConstraintKind::Tangent: return "tangent";
  case SketchSolverConstraintKind::Concentric: return "concentric";
  case SketchSolverConstraintKind::Midpoint: return "midpoint";
  case SketchSolverConstraintKind::Symmetric: return "symmetric";
  case SketchSolverConstraintKind::PointOnObject: return "point_on_object";
  case SketchSolverConstraintKind::HorizontalDistance: return "horizontal_distance";
  case SketchSolverConstraintKind::VerticalDistance: return "vertical_distance";
  case SketchSolverConstraintKind::AlignedDistance: return "aligned_distance";
  case SketchSolverConstraintKind::Radial: return "radial";
  case SketchSolverConstraintKind::Diameter: return "diameter";
  case SketchSolverConstraintKind::Angular: return "angular";
  }
  return "coincident";
}

Result<SketchSolverConstraint>
SketchSolverConstraint::create(std::string id, SketchSolverConstraintKind kind,
                               std::vector<SketchSolverTarget> targets,
                               std::optional<double> value) {
  const std::string object_id = id.empty() ? "sketch_solver_constraint" : id;
  if (id.empty())
    return Result<SketchSolverConstraint>::failure(
        validation_error(object_id, "Sketch solver constraint id must not be empty"));
  // Collinear is variable-arity (two-plus lines, a point and a line, or
  // three-plus points); every other kind has a fixed target count.
  if (kind == SketchSolverConstraintKind::Collinear) {
    if (targets.size() < 2U)
      return Result<SketchSolverConstraint>::failure(validation_error(
          object_id, "collinear constraint needs at least two targets"));
  } else if (targets.size() != expected_target_count(kind)) {
    return Result<SketchSolverConstraint>::failure(validation_error(
        object_id, "Sketch solver constraint has the wrong number of targets"));
  }
  // Tangent and PointOnObject optionally carry a value: the dimension-driven
  // circle radius for circle-profile targets (other targets stay value-free).
  const bool value_permitted = requires_value(kind) ||
                               kind == SketchSolverConstraintKind::Tangent ||
                               kind == SketchSolverConstraintKind::PointOnObject;
  if (requires_value(kind) && !value.has_value())
    return Result<SketchSolverConstraint>::failure(validation_error(
        object_id, "Sketch solver constraint requires a numeric value"));
  if (!value_permitted && value.has_value())
    return Result<SketchSolverConstraint>::failure(validation_error(
        object_id, "Sketch solver constraint must not carry a numeric value"));
  if (value && !std::isfinite(*value))
    return Result<SketchSolverConstraint>::failure(
        validation_error(object_id, "Sketch solver constraint value must be finite"));
  if (value && (kind == SketchSolverConstraintKind::AlignedDistance ||
                kind == SketchSolverConstraintKind::Radial ||
                kind == SketchSolverConstraintKind::Diameter) &&
      *value <= 0.0)
    return Result<SketchSolverConstraint>::failure(validation_error(
        object_id, "aligned, radial, and diameter constraint values must be positive"));
  return Result<SketchSolverConstraint>::success(
      SketchSolverConstraint(std::move(id), kind, std::move(targets), value));
}

SketchSolverConstraint::SketchSolverConstraint(std::string id, SketchSolverConstraintKind kind,
                                               std::vector<SketchSolverTarget> targets,
                                               std::optional<double> value)
    : id_(std::move(id)), kind_(kind), targets_(std::move(targets)), value_(value) {}

const std::string& SketchSolverConstraint::id() const noexcept { return id_; }
SketchSolverConstraintKind SketchSolverConstraint::kind() const noexcept { return kind_; }
const std::vector<SketchSolverTarget>& SketchSolverConstraint::targets() const noexcept {
  return targets_;
}
const std::optional<double>& SketchSolverConstraint::value() const noexcept { return value_; }

Result<SketchConstraintSystem>
SketchConstraintSystem::create(SketchId sketch, std::vector<SketchSolverConstraint> constraints) {
  const std::string object_id = sketch.empty() ? "sketch_constraint_system" : sketch.value();
  if (sketch.empty())
    return Result<SketchConstraintSystem>::failure(
        validation_error(object_id, "Sketch constraint system requires a Sketch id"));
  std::sort(constraints.begin(), constraints.end(), [](const auto& left, const auto& right) {
    return left.id() < right.id();
  });
  for (std::size_t index = 1U; index < constraints.size(); ++index)
    if (constraints[index - 1U].id() == constraints[index].id())
      return Result<SketchConstraintSystem>::failure(validation_error(
          constraints[index].id(), "Sketch solver constraint ids must be unique"));
  return Result<SketchConstraintSystem>::success(
      SketchConstraintSystem(std::move(sketch), std::move(constraints)));
}

SketchConstraintSystem::SketchConstraintSystem(SketchId sketch,
                                               std::vector<SketchSolverConstraint> constraints)
    : sketch_(std::move(sketch)), constraints_(std::move(constraints)) {}

const SketchId& SketchConstraintSystem::sketch() const noexcept { return sketch_; }
const std::vector<SketchSolverConstraint>& SketchConstraintSystem::constraints() const noexcept {
  return constraints_;
}

std::string_view to_string(SketchSolverVariableAxis axis) noexcept {
  switch (axis) {
  case SketchSolverVariableAxis::X: return "x";
  case SketchSolverVariableAxis::Y: return "y";
  }
  return "x";
}

std::string_view to_string(SketchSolveStatus status) noexcept {
  switch (status) {
  case SketchSolveStatus::FullyConstrained: return "fully_constrained";
  case SketchSolveStatus::UnderConstrained: return "under_constrained";
  case SketchSolveStatus::Redundant: return "redundant";
  case SketchSolveStatus::Conflicting: return "conflicting";
  case SketchSolveStatus::NonConvergent: return "non_convergent";
  case SketchSolveStatus::InvalidReference: return "invalid_reference";
  }
  return "non_convergent";
}

std::string_view to_string(SketchSolverDiagnosticKind kind) noexcept {
  switch (kind) {
  case SketchSolverDiagnosticKind::InvalidReference: return "invalid_reference";
  case SketchSolverDiagnosticKind::RedundantConstraint: return "redundant_constraint";
  case SketchSolverDiagnosticKind::ConflictingConstraint: return "conflicting_constraint";
  case SketchSolverDiagnosticKind::NonConvergent: return "non_convergent";
  }
  return "non_convergent";
}

Result<SketchSolveResult>
SketchConstraintSolver::solve(const SketchTopology& topology, const SketchConstraintSystem& system,
                              SketchSolverOptions options) const {
  auto option_validation = validate_options(options);
  if (option_validation.has_error())
    return Result<SketchSolveResult>::failure(option_validation.error());
  if (system.sketch() != topology.sketch())
    return Result<SketchSolveResult>::failure(validation_error(
        system.sketch().value(), "Sketch constraint system and topology must reference the same Sketch"));

  SolveContext context = make_context(topology);
  context.constraints = &system.constraints();
  const NumericVector initial = initial_variables(context);
  for (const auto& constraint : system.constraints()) {
    auto valid = validate_constraint_targets(context, constraint);
    if (valid.has_error())
      return Result<SketchSolveResult>::success(
          invalid_reference_result(context, topology, constraint, valid.error()));
    auto initial_block = evaluate_constraint(context, initial, constraint);
    if (initial_block.has_error())
      return Result<SketchSolveResult>::success(
          invalid_reference_result(context, topology, constraint, initial_block.error()));
  }

  auto outcome = solve_numeric(context, system.constraints(), options, initial);
  if (outcome.has_error()) return Result<SketchSolveResult>::failure(outcome.error());
  auto solved_topology = topology_from_variables(context, outcome.value().variables);
  if (solved_topology.has_error()) return Result<SketchSolveResult>::failure(solved_topology.error());

  const SketchSolverResidualSummary residual_summary{
      outcome.value().final_residuals.size(), context.length_scale,
      residual_rms(outcome.value().initial_residuals), residual_rms(outcome.value().final_residuals),
      residual_max_abs(outcome.value().final_residuals)};

  if (outcome.value().state == NumericSolveState::MaximumIterations) {
    return Result<SketchSolveResult>::success(SketchSolveResult{
        SketchSolveStatus::NonConvergent,
        outcome.value().iterations,
        std::move(solved_topology.value()),
        context.variable_order,
        0U,
        context.variable_order.size(),
        {},
        {},
        {{SketchSolverDiagnosticKind::NonConvergent, "",
          "Sketch solver reached the deterministic maximum iteration limit"}},
        residual_summary});
  }

  if (outcome.value().state == NumericSolveState::Stalled) {
    auto conflicts =
        find_conflicting_constraints(context, initial, system.constraints(), options);
    if (conflicts.has_error()) return Result<SketchSolveResult>::failure(conflicts.error());
    std::vector<SketchSolverDiagnostic> diagnostics;
    for (const auto& id : conflicts.value())
      diagnostics.push_back({SketchSolverDiagnosticKind::ConflictingConstraint, id,
                             "removing this constraint makes the deterministic system convergent"});
    const SketchSolveStatus status = conflicts.value().empty() ? SketchSolveStatus::NonConvergent
                                                               : SketchSolveStatus::Conflicting;
    if (diagnostics.empty())
      diagnostics.push_back({SketchSolverDiagnosticKind::NonConvergent, "",
                             "Sketch solver stalled before reaching the convergence tolerance"});
    return Result<SketchSolveResult>::success(SketchSolveResult{
        status,
        outcome.value().iterations,
        std::move(solved_topology.value()),
        context.variable_order,
        0U,
        context.variable_order.size(),
        {},
        std::move(conflicts.value()),
        std::move(diagnostics),
        residual_summary});
  }

  auto jacobian = build_jacobian(context, outcome.value().variables, system.constraints(), options);
  if (jacobian.has_error()) return Result<SketchSolveResult>::failure(jacobian.error());
  auto rank = matrix_rank(jacobian.value(), outcome.value().variables.size(), options);
  if (rank.has_error()) return Result<SketchSolveResult>::failure(rank.error());
  auto redundant = find_redundant_constraints(context, outcome.value().variables,
                                               system.constraints(), jacobian.value(), options);
  if (redundant.has_error()) return Result<SketchSolveResult>::failure(redundant.error());
  std::vector<SketchSolverDiagnostic> diagnostics;
  for (const auto& id : redundant.value())
    diagnostics.push_back({SketchSolverDiagnosticKind::RedundantConstraint, id,
                           "constraint does not increase the canonical Jacobian rank"});
  const std::size_t remaining_dof = outcome.value().variables.size() - rank.value();
  const SketchSolveStatus status = !redundant.value().empty()
                                       ? SketchSolveStatus::Redundant
                                       : remaining_dof == 0U ? SketchSolveStatus::FullyConstrained
                                                             : SketchSolveStatus::UnderConstrained;
  return Result<SketchSolveResult>::success(SketchSolveResult{
      status,
      outcome.value().iterations,
      std::move(solved_topology.value()),
      context.variable_order,
      rank.value(),
      remaining_dof,
      std::move(redundant.value()),
      {},
      std::move(diagnostics),
      residual_summary});
}

} // namespace blcad
