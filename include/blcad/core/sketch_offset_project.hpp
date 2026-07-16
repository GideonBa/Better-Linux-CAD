#pragma once

#include "blcad/core/sketch_modify.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace blcad {

// Block 117 headless authority for line-chain/loop offset and associative
// Project/Include references. Projection keeps semantic/construction identity;
// break_link converts a resolved projected line into ordinary Sketch geometry.
struct SketchOffsetProjectResult {
  Sketch sketch;
  std::vector<SketchEntityId> created_entities;
};

class SketchOffsetProjectService {
public:
  [[nodiscard]] static Result<SketchOffsetProjectResult>
  offset_lines(const Sketch& source, const std::vector<SketchEntityId>& ordered_ids,
               double distance, bool closed_loop = false) {
    using namespace detail;
    if (ordered_ids.empty()) return fail("offset", "offset selection is empty");
    if (!std::isfinite(distance) || std::abs(distance) <= kSketchModifyIdentityTolerance)
      return fail("offset", "offset distance must be finite and non-zero");

    std::vector<const LineSegment*> lines;
    lines.reserve(ordered_ids.size());
    for (const auto& id : ordered_ids) {
      const auto* line = source.find_line_segment(id);
      if (line == nullptr) return fail(id.value(), "Block 117 offset currently requires line entities");
      lines.push_back(line);
    }
    for (std::size_t i = 1; i < lines.size(); ++i)
      if (sketch_modify_distance(lines[i - 1]->end(), lines[i]->start()) > kSketchModifyPointTolerance)
        return fail(ordered_ids[i].value(), "offset chain must be ordered and endpoint-connected");
    if (closed_loop &&
        sketch_modify_distance(lines.back()->end(), lines.front()->start()) > kSketchModifyPointTolerance)
      return fail("offset", "closed offset loop is not geometrically closed");

    struct Shifted { Point2 a; Point2 b; };
    std::vector<Shifted> shifted;
    shifted.reserve(lines.size());
    for (const auto* line : lines) {
      const Point2 d{line->end().x - line->start().x, line->end().y - line->start().y};
      const double length = std::hypot(d.x, d.y);
      if (length <= kSketchModifyIdentityTolerance)
        return fail(line->id().value(), "offset source line is degenerate");
      const Point2 n{-d.y / length * distance, d.x / length * distance};
      shifted.push_back({{line->start().x + n.x, line->start().y + n.y},
                         {line->end().x + n.x, line->end().y + n.y}});
    }

    const auto intersection = [](const Shifted& first, const Shifted& second) -> std::optional<Point2> {
      const Point2 r{first.b.x - first.a.x, first.b.y - first.a.y};
      const Point2 s{second.b.x - second.a.x, second.b.y - second.a.y};
      const double den = r.x * s.y - r.y * s.x;
      if (std::abs(den) <= detail::kSketchModifyIdentityTolerance) return std::nullopt;
      const Point2 q{second.a.x - first.a.x, second.a.y - first.a.y};
      const double t = (q.x * s.y - q.y * s.x) / den;
      return Point2{first.a.x + r.x * t, first.a.y + r.y * t};
    };

    for (std::size_t i = 1; i < shifted.size(); ++i) {
      const auto joint = intersection(shifted[i - 1], shifted[i]);
      if (!joint) return fail(ordered_ids[i].value(), "offset has an ambiguous parallel chain corner");
      shifted[i - 1].b = *joint;
      shifted[i].a = *joint;
    }
    if (closed_loop && shifted.size() > 1U) {
      const auto joint = intersection(shifted.back(), shifted.front());
      if (!joint) return fail("offset", "offset loop has an ambiguous parallel closing corner");
      shifted.back().b = *joint;
      shifted.front().a = *joint;
    }

    std::vector<detail::SketchModifyCurve> replacements;
    std::vector<SketchEntityId> created;
    for (std::size_t i = 0; i < lines.size(); ++i) {
      const auto id = next_id(source, lines[i]->id().value() + ".offset.");
      auto line = LineSegment::create(id, shifted[i].a, shifted[i].b);
      if (line.has_error()) return Result<SketchOffsetProjectResult>::failure(line.error());
      created.push_back(id);
      replacements.emplace_back(std::move(line.value()));
    }
    auto candidate = detail::rebuild_candidate(source, {}, replacements);
    if (candidate.has_error()) return Result<SketchOffsetProjectResult>::failure(candidate.error());
    return Result<SketchOffsetProjectResult>::success({std::move(candidate.value()), std::move(created)});
  }

  [[nodiscard]] static Result<SketchOffsetProjectResult>
  project_point(const Sketch& source, ProjectedSketchPoint reference) {
    auto candidate = clone(source);
    if (candidate.has_error()) return Result<SketchOffsetProjectResult>::failure(candidate.error());
    const auto id = reference.id();
    auto added = candidate.value().add_reference(std::move(reference));
    if (added.has_error()) return Result<SketchOffsetProjectResult>::failure(added.error());
    return Result<SketchOffsetProjectResult>::success({std::move(candidate.value()), {id}});
  }

  [[nodiscard]] static Result<SketchOffsetProjectResult>
  project_line(const Sketch& source, ProjectedSketchLine reference) {
    auto candidate = clone(source);
    if (candidate.has_error()) return Result<SketchOffsetProjectResult>::failure(candidate.error());
    const auto id = reference.id();
    auto added = candidate.value().add_reference(std::move(reference));
    if (added.has_error()) return Result<SketchOffsetProjectResult>::failure(added.error());
    return Result<SketchOffsetProjectResult>::success({std::move(candidate.value()), {id}});
  }

  [[nodiscard]] static Result<SketchOffsetProjectResult>
  break_link_line(const Sketch& source, const SketchEntityId& projected_id,
                  Point2 resolved_start, Point2 resolved_end) {
    const auto* projected = source.find_projected_line(projected_id);
    if (projected == nullptr) return fail(projected_id.value(), "projected line does not exist");
    for (const auto& constraint : source.constraints())
      if (constraint.constrained_target().entity() == projected_id ||
          constraint.reference_target().entity() == projected_id)
        return fail(projected_id.value(), "break-link requires dependent constraints to be removed first");
    auto candidate = clone(source, &projected_id);
    if (candidate.has_error()) return Result<SketchOffsetProjectResult>::failure(candidate.error());
    auto detached = LineSegment::create(projected_id, resolved_start, resolved_end);
    if (detached.has_error()) return Result<SketchOffsetProjectResult>::failure(detached.error());
    auto added = candidate.value().add_entity(std::move(detached.value()));
    if (added.has_error()) return Result<SketchOffsetProjectResult>::failure(added.error());
    return Result<SketchOffsetProjectResult>::success({std::move(candidate.value()), {projected_id}});
  }

private:
  [[nodiscard]] static Result<SketchOffsetProjectResult> fail(std::string id, std::string message) {
    return Result<SketchOffsetProjectResult>::failure(Error::validation(std::move(id), std::move(message)));
  }

  [[nodiscard]] static SketchEntityId next_id(const Sketch& sketch, const std::string& prefix) {
    for (std::size_t index = 1;; ++index) {
      SketchEntityId candidate(prefix + std::to_string(index));
      if (sketch.find_line_segment(candidate) == nullptr && sketch.find_arc_segment(candidate) == nullptr &&
          sketch.find_spline_segment(candidate) == nullptr && sketch.find_projected_point(candidate) == nullptr &&
          sketch.find_projected_line(candidate) == nullptr && sketch.find_reference_generated_line(candidate) == nullptr)
        return candidate;
    }
  }

  [[nodiscard]] static Result<Sketch> clone(const Sketch& source,
                                            const SketchEntityId* skipped_projected_line = nullptr) {
    auto candidate = Sketch::create(source.id(), source.name(), source.workplane());
    if (candidate.has_error()) return candidate;
#define BLCAD_COPY_RANGE(range, method) \
    for (const auto& item : source.range()) { auto result = candidate.value().method(item); \
      if (result.has_error()) return Result<Sketch>::failure(result.error()); }
    BLCAD_COPY_RANGE(line_segments, add_entity)
    BLCAD_COPY_RANGE(arc_segments, add_entity)
    BLCAD_COPY_RANGE(spline_segments, add_entity)
    BLCAD_COPY_RANGE(projected_points, add_reference)
    for (const auto& item : source.projected_lines()) {
      if (skipped_projected_line != nullptr && item.id() == *skipped_projected_line) continue;
      auto result = candidate.value().add_reference(item);
      if (result.has_error()) return Result<Sketch>::failure(result.error());
    }
    BLCAD_COPY_RANGE(reference_generated_lines, add_reference)
    BLCAD_COPY_RANGE(constraints, add_constraint)
    BLCAD_COPY_RANGE(geometric_constraints, add_constraint)
    BLCAD_COPY_RANGE(driving_dimensions, add_dimension)
    BLCAD_COPY_RANGE(trim_extend_operations, add_trim_extend_operation)
    BLCAD_COPY_RANGE(tangent_continuities, add_tangent_continuity)
    BLCAD_COPY_RANGE(rectangle_profiles, add_profile)
    BLCAD_COPY_RANGE(circle_profiles, add_profile)
    BLCAD_COPY_RANGE(closed_profiles, add_profile)
    BLCAD_COPY_RANGE(arc_closed_profiles, add_profile)
    BLCAD_COPY_RANGE(composite_closed_profiles, add_profile)
    BLCAD_COPY_RANGE(circular_hole_patterns, add_profile)
#undef BLCAD_COPY_RANGE
    return candidate;
  }
};

} // namespace blcad
