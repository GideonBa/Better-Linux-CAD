#pragma once

#include "blcad/geometry/sketch_region_finder.hpp"

#include <algorithm>
#include <cmath>
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

    const auto& lines = sketch.line_segments();
    std::vector<bool> visited(lines.size(), false);
    SketchRegionAnalysis analysis;
    SketchRegionFinder finder;
    std::size_t region_index = 0;

    for (std::size_t seed = 0; seed < lines.size(); ++seed) {
      if (visited[seed]) continue;
      std::vector<std::size_t> component{seed};
      visited[seed] = true;
      for (std::size_t cursor = 0; cursor < component.size(); ++cursor) {
        const auto& current = lines[component[cursor]];
        for (std::size_t index = 0; index < lines.size(); ++index) {
          if (visited[index]) continue;
          const auto& candidate = lines[index];
          if (same(current.start(), candidate.start()) || same(current.start(), candidate.end()) ||
              same(current.end(), candidate.start()) || same(current.end(), candidate.end())) {
            visited[index] = true;
            component.push_back(index);
          }
        }
      }

      auto subset = Sketch::create(sketch.id(), sketch.name(), sketch.workplane());
      if (subset.has_error()) return Result<SketchRegionAnalysis>::failure(subset.error());
      std::vector<SketchEntityId> ids;
      for (const auto index : component) {
        ids.push_back(lines[index].id());
        auto added = subset.value().add_entity(lines[index]);
        if (added.has_error()) return Result<SketchRegionAnalysis>::failure(added.error());
      }

      auto region = finder.find_single_region(document, subset.value());
      if (region) {
        region.value().id = ProfileId("generated.region." + sketch.id().value() + "." +
                                      std::to_string(region_index++));
        analysis.regions.push_back(std::move(region.value()));
        continue;
      }

      SketchRegionDiagnosticKind kind = SketchRegionDiagnosticKind::AmbiguousTopology;
      if (region.error().message().find("self-intersect") != std::string::npos)
        kind = SketchRegionDiagnosticKind::SelfIntersection;
      else if (region.error().message().find("gap") != std::string::npos ||
               region.error().message().find("at least three") != std::string::npos ||
               region.error().message().find("does not close") != std::string::npos)
        kind = SketchRegionDiagnosticKind::OpenContour;
      analysis.diagnostics.push_back({kind, std::move(ids), region.error().message()});
    }

    std::sort(analysis.regions.begin(), analysis.regions.end(),
              [](const auto& a, const auto& b) { return a.id.value() < b.id.value(); });
    return Result<SketchRegionAnalysis>::success(std::move(analysis));
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
    for (const auto& region : analysis.regions)
      if (contains(region.vertices)) return region.id;
    return std::nullopt;
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
    auto profile = finder.make_closed_profile(*found);
    if (profile.has_error()) return Result<SketchFinishResult>::failure(profile.error());
    auto added = completed.add_profile(std::move(profile.value()));
    if (added.has_error()) return Result<SketchFinishResult>::failure(added.error());

    PartDocument document = source;
    auto updated = document.update_sketch(std::move(completed));
    if (updated.has_error()) return Result<SketchFinishResult>::failure(updated.error());
    return Result<SketchFinishResult>::success(
        {std::move(document), *selected_profile, std::move(analysis.value().regions)});
  }
};

} // namespace blcad::geometry
