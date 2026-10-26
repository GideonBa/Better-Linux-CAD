#pragma once

#include "blcad/geometry/sketch_region_finder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {

enum class SketchRegionDiagnosticKind { OpenContour, SelfIntersection, AmbiguousTopology };

struct SketchRegionDiagnostic {
  SketchRegionDiagnosticKind kind{SketchRegionDiagnosticKind::AmbiguousTopology};
  std::vector<SketchEntityId> entities;
  std::string message;
};

struct SketchRegionAnalysis {
  std::vector<GeneratedClosedProfileCandidate> regions;
  std::vector<SketchRegionDiagnostic> diagnostics;
};

struct SketchFinishResult {
  PartDocument document;
  ProfileId selected_profile;
  std::vector<GeneratedClosedProfileCandidate> available_regions;
};

class SketchFinishService {
public:
  [[nodiscard]] static Result<SketchRegionAnalysis> analyze(const PartDocument& document,
                                                            const Sketch& sketch) {
    constexpr double tolerance = 1.0e-9;
    const auto same = [tolerance](Point2 a, Point2 b) {
      return std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance;
    };

    std::vector<ResolvedSketchRegionLine> curves;
    const auto collect = [&curves](const auto& entities) {
      for (const auto& entity : entities)
        curves.push_back({entity.id(), entity.start(), entity.end()});
    };
    for (const auto& line : sketch.line_segments())
      if (!line.construction())
        curves.push_back({line.id(), line.start(), line.end()});
    collect(sketch.arc_segments());
    collect(sketch.spline_segments());
    std::sort(curves.begin(), curves.end(), [](const auto& a, const auto& b) {
      return a.id.value() < b.id.value();
    });
    std::vector<bool> visited(curves.size(), false);
    SketchRegionAnalysis analysis;
    SketchRegionFinder finder;
    std::size_t region_index = 0;

    for (std::size_t seed = 0; seed < curves.size(); ++seed) {
      if (visited[seed]) continue;
      std::vector<std::size_t> component{seed};
      visited[seed] = true;
      for (std::size_t cursor = 0; cursor < component.size(); ++cursor) {
        const auto& current = curves[component[cursor]];
        for (std::size_t index = 0; index < curves.size(); ++index) {
          if (visited[index]) continue;
          const auto& candidate = curves[index];
          if (same(current.start, candidate.start) || same(current.start, candidate.end) ||
              same(current.end, candidate.start) || same(current.end, candidate.end)) {
            visited[index] = true;
            component.push_back(index);
          }
        }
      }

      auto subset = Sketch::create(sketch.id(), sketch.name(), sketch.workplane());
      if (subset.has_error()) return Result<SketchRegionAnalysis>::failure(subset.error());
      std::vector<SketchEntityId> ids;
      for (const auto index : component) {
        const auto& id = curves[index].id;
        ids.push_back(id);
        auto added = [&] {
          if (const auto* line = sketch.find_line_segment(id))
            return subset.value().add_entity(*line);
          if (const auto* arc = sketch.find_arc_segment(id))
            return subset.value().add_entity(*arc);
          return subset.value().add_entity(*sketch.find_spline_segment(id));
        }();
        if (added.has_error()) return Result<SketchRegionAnalysis>::failure(added.error());
      }

      auto region = finder.find_single_curve_region(document, subset.value());
      if (region) {
        region.value().id = ProfileId("generated.region." + sketch.id().value() + "." +
                                      std::to_string(region_index++));
        analysis.regions.push_back(std::move(region.value()));
        continue;
      }

      SketchRegionDiagnosticKind kind = SketchRegionDiagnosticKind::AmbiguousTopology;
      bool branched = false;
      for (const auto index : component) {
        for (const Point2 endpoint : {curves[index].start, curves[index].end}) {
          std::size_t degree = 0U;
          for (const auto other : component) {
            degree += same(endpoint, curves[other].start) ? 1U : 0U;
            degree += same(endpoint, curves[other].end) ? 1U : 0U;
          }
          branched = branched || degree > 2U;
        }
      }
      if (branched)
        kind = SketchRegionDiagnosticKind::AmbiguousTopology;
      else if (region.error().message().find("self-intersect") != std::string::npos)
        kind = SketchRegionDiagnosticKind::SelfIntersection;
      else if (region.error().message().find("gap") != std::string::npos ||
               region.error().message().find("at least three") != std::string::npos ||
               region.error().message().find("at least two") != std::string::npos ||
               region.error().message().find("single-contour loop") != std::string::npos ||
               region.error().message().find("does not close") != std::string::npos)
        kind = SketchRegionDiagnosticKind::OpenContour;
      analysis.diagnostics.push_back({kind, std::move(ids), region.error().message()});
    }

    std::sort(analysis.regions.begin(), analysis.regions.end(),
              [](const auto& a, const auto& b) { return a.id.value() < b.id.value(); });
    return Result<SketchRegionAnalysis>::success(std::move(analysis));
  }

  // Detect bounded curve regions that are not already owned by a persistent
  // profile. This is the common identity boundary for transient viewport
  // selection and later profile materialization: mixed legacy/current
  // sketches produce the same collision-free generated ids in both paths.
  [[nodiscard]] static Result<SketchRegionAnalysis>
  analyze_unmaterialized(const PartDocument& document, const Sketch& sketch) {
    std::vector<SketchEntityId> materialized_curves;
    const auto append = [&materialized_curves](const auto& curves) {
      materialized_curves.insert(materialized_curves.end(), curves.begin(), curves.end());
    };
    for (const auto& profile : sketch.closed_profiles())
      append(profile.line_segments());
    for (const auto& profile : sketch.arc_closed_profiles())
      append(profile.curve_segments());
    for (const auto& profile : sketch.composite_closed_profiles()) {
      append(profile.outer_contour());
      for (const auto& contour : profile.inner_contours())
        append(contour);
    }
    std::sort(materialized_curves.begin(), materialized_curves.end(),
              [](const auto& left, const auto& right) {
                return left.value() < right.value();
              });
    materialized_curves.erase(
        std::unique(materialized_curves.begin(), materialized_curves.end()),
        materialized_curves.end());

    auto raw = Sketch::create(sketch.id(), sketch.name(), sketch.workplane());
    if (raw.has_error())
      return Result<SketchRegionAnalysis>::failure(raw.error());
    const auto copy_unowned = [&](const auto& entities) -> Result<std::size_t> {
      for (const auto& curve : entities) {
        if (std::binary_search(materialized_curves.begin(), materialized_curves.end(), curve.id(),
                             [](const auto& left, const auto& right) {
                               return left.value() < right.value();
                             }))
          continue;
        auto added = raw.value().add_entity(curve);
        if (added.has_error()) return added;
      }
      return Result<std::size_t>::success(entities.size());
    };
    for (const auto& copied : {copy_unowned(sketch.line_segments()),
                              copy_unowned(sketch.arc_segments()),
                              copy_unowned(sketch.spline_segments())})
      if (!copied) return Result<SketchRegionAnalysis>::failure(copied.error());

    auto analysis = analyze(document, raw.value());
    if (analysis.has_error())
      return analysis;
    const auto profile_exists = [&sketch](const ProfileId& id) {
      return sketch.find_rectangle_profile(id) != nullptr ||
             sketch.find_circle_profile(id) != nullptr ||
             sketch.find_closed_profile(id) != nullptr ||
             sketch.find_arc_closed_profile(id) != nullptr ||
             sketch.find_composite_closed_profile(id) != nullptr ||
             sketch.find_circular_hole_pattern(id) != nullptr;
    };
    std::vector<ProfileId> assigned;
    std::size_t next_index = 0U;
    for (auto& region : analysis.value().regions) {
      ProfileId candidate("");
      do {
        candidate = ProfileId("generated.region." + sketch.id().value() + "." +
                              std::to_string(next_index++));
      } while (profile_exists(candidate) ||
               std::find(assigned.begin(), assigned.end(), candidate) != assigned.end());
      region.id = candidate;
      assigned.push_back(std::move(candidate));
    }
    return analysis;
  }

  [[nodiscard]] static std::optional<ProfileId> select_region_at(const SketchRegionAnalysis& analysis,
                                                                 Point2 point) {
    const auto contains = [point](const std::vector<Point2>& polygon) {
      bool inside = false;
      if (polygon.size() < 3U) return false;
      for (std::size_t i = 0, j = polygon.size() - 1U; i < polygon.size(); j = i++) {
        const auto a = polygon[i];
        const auto b = polygon[j];
        const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
                             (point.x < (b.x - a.x) * (point.y - a.y) /
                                                ((b.y - a.y) == 0.0 ? 1.0e-30 : (b.y - a.y)) +
                                            a.x);
        if (crosses) inside = !inside;
      }
      return inside;
    };
    std::optional<ProfileId> selected;
    double smallest_area = std::numeric_limits<double>::infinity();
    for (const auto& region : analysis.regions) {
      if (!contains(region.vertices))
        continue;
      double area = 0;
      for (std::size_t i = 0; i < region.vertices.size(); ++i) {
        const auto a = region.vertices[i], b = region.vertices[(i + 1) % region.vertices.size()];
        area += a.x * b.y - a.y * b.x;
      }
      if (std::abs(area) < smallest_area) {
        selected = region.id;
        smallest_area = std::abs(area);
      }
    }
    return selected;
  }

  [[nodiscard]] static Result<SketchFinishResult>
  finish(const PartDocument& source, const SketchId& sketch_id,
         std::optional<ProfileId> selected_profile = std::nullopt) {
    const auto* sketch = source.find_sketch(sketch_id);
    if (sketch == nullptr)
      return Result<SketchFinishResult>::failure(
          Error::validation(sketch_id.value(), "Finish Sketch target does not exist"));
    auto analysis = analyze(source, *sketch);
    if (analysis.has_error()) return Result<SketchFinishResult>::failure(analysis.error());
    if (!analysis.value().diagnostics.empty())
      return Result<SketchFinishResult>::failure(Error::validation(
          sketch_id.value(), "Finish Sketch requires all visible contours to be closed and simple"));
    if (analysis.value().regions.empty())
      return Result<SketchFinishResult>::failure(
          Error::validation(sketch_id.value(), "Finish Sketch found no bounded region"));
    if (!selected_profile && analysis.value().regions.size() == 1U)
      selected_profile = analysis.value().regions.front().id;
    if (!selected_profile)
      return Result<SketchFinishResult>::failure(Error::validation(
          sketch_id.value(), "Finish Sketch requires an explicit region selection"));

    const auto found = std::find_if(analysis.value().regions.begin(), analysis.value().regions.end(),
                                    [&](const auto& region) { return region.id == *selected_profile; });
    if (found == analysis.value().regions.end())
      return Result<SketchFinishResult>::failure(
          Error::validation(selected_profile->value(), "selected Sketch region is not available"));

    Sketch completed = *sketch;
    SketchRegionFinder finder;
    auto added = finder.materialize_profile(completed, *found);
    if (added.has_error()) return Result<SketchFinishResult>::failure(added.error());

    PartDocument document = source;
    auto updated = document.update_sketch(std::move(completed));
    if (updated.has_error()) return Result<SketchFinishResult>::failure(updated.error());
    return Result<SketchFinishResult>::success(
        {std::move(document), *selected_profile, std::move(analysis.value().regions)});
  }
};

} // namespace blcad::geometry
