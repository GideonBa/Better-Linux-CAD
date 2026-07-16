#pragma once

#include "blcad/core/sketch.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace blcad {

enum class SketchPatternKind { Rectangular, Circular };
enum class SketchPatternMode { Associative, Exploded };

struct SketchPatternIntent {
  std::string id;
  SketchPatternKind kind{SketchPatternKind::Rectangular};
  SketchPatternMode mode{SketchPatternMode::Associative};
  std::vector<SketchEntityId> source_entities;
  Point2 first_step{};
  Point2 second_step{};
  Point2 center{};
  std::size_t first_count{1};
  std::size_t second_count{1};
  double angle_step_rad{0.0};
};

struct SketchTransformPatternResult {
  Sketch sketch;
  std::vector<SketchEntityId> created_entities;
  std::optional<SketchPatternIntent> pattern_intent;
  // Constraint/dimension/continuity ids that reference a selected source and
  // are therefore not duplicated onto copied/mirrored/patterned instances. The
  // caller must surface this instead of silently discarding the relationship.
  std::vector<std::string> uncopied_references;
};

class SketchTransformPatternService {
public:
  [[nodiscard]] static Result<SketchTransformPatternResult>
  move(const Sketch& source, const std::vector<SketchEntityId>& ids, Point2 delta) {
    if (!finite(delta)) return fail("move", "move delta must be finite");
    return replace(source, ids, [=](Point2 p) { return Point2{p.x + delta.x, p.y + delta.y}; });
  }

  [[nodiscard]] static Result<SketchTransformPatternResult>
  rotate(const Sketch& source, const std::vector<SketchEntityId>& ids, Point2 center,
         double angle_rad) {
    if (!finite(center) || !std::isfinite(angle_rad))
      return fail("rotate", "rotation center and angle must be finite");
    const double c = std::cos(angle_rad), s = std::sin(angle_rad);
    return replace(source, ids, [=](Point2 p) {
      const double x = p.x - center.x, y = p.y - center.y;
      return Point2{center.x + c * x - s * y, center.y + s * x + c * y};
    });
  }

  [[nodiscard]] static Result<SketchTransformPatternResult>
  scale(const Sketch& source, const std::vector<SketchEntityId>& ids, Point2 center, double factor) {
    if (!finite(center) || !std::isfinite(factor) || std::abs(factor) <= 1.0e-12)
      return fail("scale", "scale factor must be finite and non-zero");
    return replace(source, ids, [=](Point2 p) {
      return Point2{center.x + (p.x - center.x) * factor,
                    center.y + (p.y - center.y) * factor};
    });
  }

  [[nodiscard]] static Result<SketchTransformPatternResult>
  copy(const Sketch& source, const std::vector<SketchEntityId>& ids, Point2 delta) {
    if (!finite(delta)) return fail("copy", "copy delta must be finite");
    return append(source, ids, ".copy.", [=](Point2 p) {
      return Point2{p.x + delta.x, p.y + delta.y};
    });
  }

  [[nodiscard]] static Result<SketchTransformPatternResult>
  mirror(const Sketch& source, const std::vector<SketchEntityId>& ids, Point2 axis_start,
         Point2 axis_end) {
    const Point2 d{axis_end.x - axis_start.x, axis_end.y - axis_start.y};
    const double n = d.x * d.x + d.y * d.y;
    if (!finite(axis_start) || !finite(axis_end) || n <= 1.0e-18)
      return fail("mirror", "mirror axis must be finite and non-degenerate");
    return append(source, ids, ".mirror.", [=](Point2 p) {
      const double t = ((p.x - axis_start.x) * d.x + (p.y - axis_start.y) * d.y) / n;
      const Point2 q{axis_start.x + t * d.x, axis_start.y + t * d.y};
      return Point2{2.0 * q.x - p.x, 2.0 * q.y - p.y};
    });
  }

  [[nodiscard]] static Result<SketchTransformPatternResult>
  rectangular_pattern(const Sketch& source, const std::vector<SketchEntityId>& ids,
                      Point2 first_step, std::size_t first_count, Point2 second_step,
                      std::size_t second_count, SketchPatternMode mode,
                      std::string intent_id = "sketch.pattern.rectangular") {
    if (!finite(first_step) || !finite(second_step) || first_count == 0U || second_count == 0U)
      return fail(intent_id, "rectangular pattern steps must be finite and counts positive");
    auto candidate = clone(source);
    if (candidate.has_error()) return Result<SketchTransformPatternResult>::failure(candidate.error());
    auto curves = selected(source, ids);
    if (curves.has_error()) return Result<SketchTransformPatternResult>::failure(curves.error());
    std::vector<SketchEntityId> created;
    for (std::size_t j = 0; j < second_count; ++j) {
      for (std::size_t i = 0; i < first_count; ++i) {
        if (i == 0U && j == 0U) continue;
        const Point2 d{first_step.x * static_cast<double>(i) + second_step.x * static_cast<double>(j),
                       first_step.y * static_cast<double>(i) + second_step.y * static_cast<double>(j)};
        auto added = append_to(candidate.value(), curves.value(), ".rect.", [=](Point2 p) {
          return Point2{p.x + d.x, p.y + d.y};
        });
        if (added.has_error()) return Result<SketchTransformPatternResult>::failure(added.error());
        created.insert(created.end(), added.value().begin(), added.value().end());
      }
    }
    std::optional<SketchPatternIntent> intent;
    if (mode == SketchPatternMode::Associative)
      intent = SketchPatternIntent{std::move(intent_id), SketchPatternKind::Rectangular, mode, ids,
                                   first_step, second_step, {}, first_count, second_count, 0.0};
    return Result<SketchTransformPatternResult>::success(
        {std::move(candidate.value()), std::move(created), std::move(intent),
         external_intent_on(source, ids)});
  }

  [[nodiscard]] static Result<SketchTransformPatternResult>
  circular_pattern(const Sketch& source, const std::vector<SketchEntityId>& ids, Point2 center,
                   std::size_t count, double total_angle_rad, SketchPatternMode mode,
                   std::string intent_id = "sketch.pattern.circular") {
    if (!finite(center) || !std::isfinite(total_angle_rad) || count == 0U)
      return fail(intent_id, "circular pattern center/angle must be finite and count positive");
    auto candidate = clone(source);
    if (candidate.has_error()) return Result<SketchTransformPatternResult>::failure(candidate.error());
    auto curves = selected(source, ids);
    if (curves.has_error()) return Result<SketchTransformPatternResult>::failure(curves.error());
    const double step = count > 1U ? total_angle_rad / static_cast<double>(count - 1U) : 0.0;
    std::vector<SketchEntityId> created;
    for (std::size_t i = 1; i < count; ++i) {
      const double a = step * static_cast<double>(i), c = std::cos(a), s = std::sin(a);
      auto added = append_to(candidate.value(), curves.value(), ".circular.", [=](Point2 p) {
        const double x = p.x - center.x, y = p.y - center.y;
        return Point2{center.x + c * x - s * y, center.y + s * x + c * y};
      });
      if (added.has_error()) return Result<SketchTransformPatternResult>::failure(added.error());
      created.insert(created.end(), added.value().begin(), added.value().end());
    }
    std::optional<SketchPatternIntent> intent;
    if (mode == SketchPatternMode::Associative)
      intent = SketchPatternIntent{std::move(intent_id), SketchPatternKind::Circular, mode, ids,
                                   {}, {}, center, count, 1U, step};
    return Result<SketchTransformPatternResult>::success(
        {std::move(candidate.value()), std::move(created), std::move(intent),
         external_intent_on(source, ids)});
  }

private:
  using Curve = std::variant<LineSegment, ArcSegment, SplineSegment>;

  [[nodiscard]] static bool finite(Point2 p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y);
  }
  [[nodiscard]] static Result<SketchTransformPatternResult> fail(std::string id,
                                                                 std::string message) {
    return Result<SketchTransformPatternResult>::failure(
        Error::validation(std::move(id), std::move(message)));
  }
  [[nodiscard]] static const SketchEntityId& id_of(const Curve& curve) noexcept {
    return std::visit([](const auto& value) -> const SketchEntityId& { return value.id(); }, curve);
  }
  [[nodiscard]] static Result<std::vector<Curve>> selected(
      const Sketch& source, const std::vector<SketchEntityId>& ids) {
    if (ids.empty())
      return Result<std::vector<Curve>>::failure(Error::validation("selection", "selection is empty"));
    std::vector<Curve> result;
    for (const auto& id : ids) {
      if (const auto* v = source.find_line_segment(id)) result.emplace_back(*v);
      else if (const auto* v = source.find_arc_segment(id)) result.emplace_back(*v);
      else if (const auto* v = source.find_spline_segment(id)) result.emplace_back(*v);
      else return Result<std::vector<Curve>>::failure(
          Error::validation(id.value(), "selected entity is not a transformable ordinary curve"));
    }
    return Result<std::vector<Curve>>::success(std::move(result));
  }
  template <typename F>
  [[nodiscard]] static Result<Curve> transformed(const Curve& curve, SketchEntityId id, F f) {
    return std::visit([&](const auto& v) -> Result<Curve> {
      using T = std::decay_t<decltype(v)>;
      if constexpr (std::is_same_v<T, LineSegment>) {
        auto r = LineSegment::create(std::move(id), f(v.start()), f(v.end()));
        if (r.has_error()) return Result<Curve>::failure(r.error());
        return Result<Curve>::success(Curve(std::move(r.value())));
      } else if constexpr (std::is_same_v<T, ArcSegment>) {
        auto r = ArcSegment::create_three_point(std::move(id), f(v.start()), f(v.mid()), f(v.end()));
        if (r.has_error()) return Result<Curve>::failure(r.error());
        return Result<Curve>::success(Curve(std::move(r.value())));
      } else {
        auto r = SplineSegment::create_cubic_bezier(std::move(id), f(v.start()), f(v.control1()),
                                                    f(v.control2()), f(v.end()));
        if (r.has_error()) return Result<Curve>::failure(r.error());
        return Result<Curve>::success(Curve(std::move(r.value())));
      }
    }, curve);
  }
  [[nodiscard]] static Result<std::size_t> add(Sketch& sketch, const Curve& curve) {
    return std::visit([&](const auto& v) { return sketch.add_entity(v); }, curve);
  }
  [[nodiscard]] static SketchEntityId next_id(const Sketch& sketch, const std::string& prefix) {
    for (std::size_t i = 1;; ++i) {
      SketchEntityId id(prefix + std::to_string(i));
      if (!sketch.find_line_segment(id) && !sketch.find_arc_segment(id) &&
          !sketch.find_spline_segment(id) && !sketch.find_projected_point(id) &&
          !sketch.find_projected_line(id) && !sketch.find_reference_generated_line(id)) return id;
    }
  }
  template <typename F>
  [[nodiscard]] static Result<std::vector<SketchEntityId>> append_to(
      Sketch& candidate, const std::vector<Curve>& curves, const std::string& suffix, F f) {
    std::vector<SketchEntityId> created;
    for (const auto& curve : curves) {
      const auto id = next_id(candidate, id_of(curve).value() + suffix);
      auto copy = transformed(curve, id, f);
      if (copy.has_error()) return Result<std::vector<SketchEntityId>>::failure(copy.error());
      auto added = add(candidate, copy.value());
      if (added.has_error()) return Result<std::vector<SketchEntityId>>::failure(added.error());
      created.push_back(id);
    }
    return Result<std::vector<SketchEntityId>>::success(std::move(created));
  }
  template <typename F>
  [[nodiscard]] static Result<SketchTransformPatternResult> append(
      const Sketch& source, const std::vector<SketchEntityId>& ids, const std::string& suffix, F f) {
    auto candidate = clone(source);
    if (candidate.has_error()) return Result<SketchTransformPatternResult>::failure(candidate.error());
    auto curves = selected(source, ids);
    if (curves.has_error()) return Result<SketchTransformPatternResult>::failure(curves.error());
    auto created = append_to(candidate.value(), curves.value(), suffix, f);
    if (created.has_error()) return Result<SketchTransformPatternResult>::failure(created.error());
    return Result<SketchTransformPatternResult>::success(
        {std::move(candidate.value()), std::move(created.value()), std::nullopt,
         external_intent_on(source, ids)});
  }
  template <typename F>
  [[nodiscard]] static Result<SketchTransformPatternResult> replace(
      const Sketch& source, const std::vector<SketchEntityId>& ids, F f) {
    auto curves = selected(source, ids);
    if (curves.has_error()) return Result<SketchTransformPatternResult>::failure(curves.error());
    // The transformed curves keep their ids and must exist before referencing
    // constraints/dimensions/profiles are copied, so they are woven into the
    // clone instead of being appended afterwards.
    std::vector<Curve> replacements;
    replacements.reserve(curves.value().size());
    for (const auto& curve : curves.value()) {
      auto value = transformed(curve, id_of(curve), f);
      if (value.has_error()) return Result<SketchTransformPatternResult>::failure(value.error());
      replacements.push_back(std::move(value.value()));
    }
    auto candidate = clone(source, ids, &replacements);
    if (candidate.has_error()) return Result<SketchTransformPatternResult>::failure(candidate.error());
    return Result<SketchTransformPatternResult>::success(
        {std::move(candidate.value()), {}, std::nullopt, {}});
  }
  // Stable ids of embedded intent that references any selected source entity.
  [[nodiscard]] static std::vector<std::string> external_intent_on(
      const Sketch& source, const std::vector<SketchEntityId>& ids) {
    const auto selected_id = [&ids](const SketchEntityId& id) {
      return std::find(ids.begin(), ids.end(), id) != ids.end();
    };
    const auto on = [&selected_id](const SketchReferenceTarget& target) {
      return selected_id(target.entity());
    };
    std::vector<std::string> result;
    for (const auto& constraint : source.constraints())
      if (on(constraint.constrained_target()) || on(constraint.reference_target()))
        result.push_back(constraint.id().value());
    for (const auto& constraint : source.geometric_constraints())
      if (on(constraint.first_target()) ||
          (constraint.second_target() && on(*constraint.second_target())))
        result.push_back(constraint.id().value());
    for (const auto& dimension : source.driving_dimensions())
      if (on(dimension.first_target()) || on(dimension.second_target()))
        result.push_back(dimension.id().value());
    for (const auto& continuity : source.tangent_continuities())
      if (selected_id(continuity.first_entity()) || selected_id(continuity.second_entity()))
        result.push_back(continuity.id().value());
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }
  [[nodiscard]] static bool omitted(const std::vector<SketchEntityId>& ids, const SketchEntityId& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
  }
  [[nodiscard]] static Result<Sketch> clone(const Sketch& source,
                                            const std::vector<SketchEntityId>& omitted_ids = {},
                                            const std::vector<Curve>* replacements = nullptr) {
    auto candidate = Sketch::create(source.id(), source.name(), source.workplane());
    if (candidate.has_error()) return candidate;
#define BLCAD_COPY(range, method) for (const auto& item : source.range()) { auto r = candidate.value().method(item); if (r.has_error()) return Result<Sketch>::failure(r.error()); }
    for (const auto& v : source.line_segments()) if (!omitted(omitted_ids, v.id())) { auto r = candidate.value().add_entity(v); if (r.has_error()) return Result<Sketch>::failure(r.error()); }
    for (const auto& v : source.arc_segments()) if (!omitted(omitted_ids, v.id())) { auto r = candidate.value().add_entity(v); if (r.has_error()) return Result<Sketch>::failure(r.error()); }
    for (const auto& v : source.spline_segments()) if (!omitted(omitted_ids, v.id())) { auto r = candidate.value().add_entity(v); if (r.has_error()) return Result<Sketch>::failure(r.error()); }
    if (replacements != nullptr)
      for (const auto& curve : *replacements) {
        auto r = add(candidate.value(), curve);
        if (r.has_error()) return Result<Sketch>::failure(r.error());
      }
    BLCAD_COPY(projected_points, add_reference)
    BLCAD_COPY(projected_lines, add_reference)
    BLCAD_COPY(reference_generated_lines, add_reference)
    BLCAD_COPY(constraints, add_constraint)
    BLCAD_COPY(geometric_constraints, add_constraint)
    BLCAD_COPY(driving_dimensions, add_dimension)
    BLCAD_COPY(trim_extend_operations, add_trim_extend_operation)
    BLCAD_COPY(tangent_continuities, add_tangent_continuity)
    BLCAD_COPY(rectangle_profiles, add_profile)
    BLCAD_COPY(circle_profiles, add_profile)
    BLCAD_COPY(closed_profiles, add_profile)
    BLCAD_COPY(arc_closed_profiles, add_profile)
    BLCAD_COPY(composite_closed_profiles, add_profile)
    BLCAD_COPY(circular_hole_patterns, add_profile)
#undef BLCAD_COPY
    return candidate;
  }
};

} // namespace blcad
