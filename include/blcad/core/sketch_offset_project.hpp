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
  // Orders an unordered connected selection and reverses local traversal where
  // needed. Original entities and their orientation are never changed.
  [[nodiscard]] static Result<SketchOffsetProjectResult>
  offset_chain(const Sketch& source, std::vector<SketchEntityId> ids, double distance) {
    using namespace detail;
    if (ids.empty())
      return fail("offset", "offset selection is empty");
    std::sort(ids.begin(), ids.end(),
              [](const auto& a, const auto& b) { return a.value() < b.value(); });
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end())
      return fail("offset", "offset selection contains duplicate entities");
    if (ids.size() == 1U && source.find_arc_segment(ids.front())) {
      if (!std::isfinite(distance) || std::abs(distance) <= kSketchModifyIdentityTolerance)
        return fail("offset", "offset distance must be finite and non-zero");
      const auto sampler = curve_sampler_for(source, ids.front());
      const auto* arc = source.find_arc_segment(ids.front());
      const auto center = sampler->arc.center;
      const double radius = sampler->arc.radius;
      const double r = radius - (sampler->arc.sweep > 0.0 ? distance : -distance);
      if (r <= kSketchModifyPointTolerance)
        return fail(ids.front().value(), "offset consumes the arc radius");
      const auto radial = [&](Point2 p) {
        return Point2{center.x + (p.x - center.x) * r / radius,
                      center.y + (p.y - center.y) * r / radius};
      };
      auto id = next_id(source, ids.front().value() + ".offset.");
      auto curve = ArcSegment::create_three_point(id, radial(arc->start()), radial(arc->mid()),
                                                  radial(arc->end()));
      if (!curve)
        return Result<SketchOffsetProjectResult>::failure(curve.error());
      Sketch candidate = source;
      auto added = candidate.add_entity(std::move(curve.value()));
      if (!added)
        return Result<SketchOffsetProjectResult>::failure(added.error());
      return Result<SketchOffsetProjectResult>::success({std::move(candidate), {id}});
    }
    std::vector<const LineSegment*> lines;
    std::vector<Point2> vertices;
    std::vector<std::pair<std::size_t, std::size_t>> ends;
    std::vector<std::vector<std::size_t>> adjacent;
    const auto vertex = [&](Point2 point) {
      for (std::size_t i = 0; i < vertices.size(); ++i)
        if (sketch_modify_distance(vertices[i], point) <= kSketchModifyPointTolerance)
          return i;
      vertices.push_back(point);
      adjacent.emplace_back();
      return vertices.size() - 1U;
    };
    for (const auto& id : ids) {
      const auto* line = source.find_line_segment(id);
      if (!line)
        return fail(id.value(), "select a line chain or one circular arc for offset");
      const auto a = vertex(line->start()), b = vertex(line->end());
      if (a == b)
        return fail(id.value(), "offset source line is degenerate");
      adjacent[a].push_back(lines.size());
      adjacent[b].push_back(lines.size());
      ends.emplace_back(a, b);
      lines.push_back(line);
    }
    std::vector<std::size_t> open;
    for (std::size_t i = 0; i < adjacent.size(); ++i) {
      if (adjacent[i].size() > 2U)
        return fail("offset", "offset chain branches at an endpoint");
      if (adjacent[i].size() == 1U)
        open.push_back(i);
    }
    if (!open.empty() && open.size() != 2U)
      return fail("offset", "offset selection is disconnected");
    const bool closed = open.empty();
    std::size_t current = ends.front().first;
    if (!closed) {
      const auto a = vertices[open[0]], b = vertices[open[1]];
      current = (a.x < b.x || (a.x == b.x && a.y < b.y)) ? open[0] : open[1];
    }
    std::vector<bool> visited(lines.size());
    std::vector<SketchModifyCurve> oriented;
    std::vector<SketchEntityId> ordered;
    for (std::size_t step = 0; step < lines.size(); ++step) {
      auto found = std::find_if(adjacent[current].begin(), adjacent[current].end(),
                                [&](auto i) { return !visited[i]; });
      if (found == adjacent[current].end())
        return fail("offset", "offset selection is disconnected");
      const auto i = *found;
      const bool forward = ends[i].first == current;
      auto line = LineSegment::create(lines[i]->id(), forward ? lines[i]->start() : lines[i]->end(),
                                      forward ? lines[i]->end() : lines[i]->start());
      if (!line)
        return Result<SketchOffsetProjectResult>::failure(line.error());
      oriented.emplace_back(std::move(line.value()));
      ordered.push_back(lines[i]->id());
      visited[i] = true;
      current = forward ? ends[i].second : ends[i].first;
    }
    std::vector<std::string> originals;
    for (const auto& id : ids)
      originals.push_back(id.value());
    auto traversal = rebuild_candidate(source, originals, oriented);
    if (!traversal)
      return Result<SketchOffsetProjectResult>::failure(traversal.error());
    auto offset = offset_lines(traversal.value(), ordered, distance, closed);
    if (!offset)
      return offset;
    Sketch candidate = source;
    for (const auto& id : offset.value().created_entities) {
      auto added = candidate.add_entity(*offset.value().sketch.find_line_segment(id));
      if (!added)
        return Result<SketchOffsetProjectResult>::failure(added.error());
    }
    if (closed) {
      std::size_t n = 1;
      while (candidate.has_profile_id(ProfileId("profile.offset." + std::to_string(n))))
        ++n;
      auto profile = ClosedProfile::create(ProfileId("profile.offset." + std::to_string(n)),
                                           offset.value().created_entities);
      if (!profile)
        return Result<SketchOffsetProjectResult>::failure(profile.error());
      auto added = candidate.add_profile(std::move(profile.value()));
      if (!added)
        return Result<SketchOffsetProjectResult>::failure(added.error());
    }
    return Result<SketchOffsetProjectResult>::success(
        {std::move(candidate), std::move(offset.value().created_entities)});
  }

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
      if (std::any_of(lines.begin(), lines.end(),
                      [&](const auto* line) { return line->id() == id; }))
        return fail(id.value(), "duplicate offset source");
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
      if (std::abs(den) <= detail::kSketchModifyIdentityTolerance) {
        if (r.x * s.x + r.y * s.y > 0 &&
            sketch_modify_distance(first.b, second.a) <= kSketchModifyPointTolerance)
          return first.b;
        return std::nullopt;
      }
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
      const Point2 d{lines[i]->end().x - lines[i]->start().x,
                     lines[i]->end().y - lines[i]->start().y};
      if ((shifted[i].b.x - shifted[i].a.x) * d.x + (shifted[i].b.y - shifted[i].a.y) * d.y <= 0)
        return fail(id.value(), "offset consumes or reverses an edge");
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
      if (sketch.find_line_segment(candidate) == nullptr &&
          sketch.find_arc_segment(candidate) == nullptr &&
          sketch.find_spline_segment(candidate) == nullptr &&
          sketch.find_projected_point(candidate) == nullptr &&
          sketch.find_projected_line(candidate) == nullptr &&
          sketch.find_reference_generated_line(candidate) == nullptr &&
          sketch.find_point(candidate) == nullptr)
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
    BLCAD_COPY_RANGE(points, add_entity)
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
