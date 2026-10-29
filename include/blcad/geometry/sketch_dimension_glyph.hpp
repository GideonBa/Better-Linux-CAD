#pragma once

#include "blcad/core/sketch_dimension_authoring.hpp"
#include "blcad/geometry/sketch_dimension_layout.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::geometry {

enum class SketchDimensionGlyphState { Accepted, Preview, Conflict };

[[nodiscard]] constexpr std::string_view
to_string(SketchDimensionGlyphState state) noexcept {
  switch (state) {
  case SketchDimensionGlyphState::Accepted: return "accepted";
  case SketchDimensionGlyphState::Preview: return "preview";
  case SketchDimensionGlyphState::Conflict: return "conflict";
  }
  return "accepted";
}

struct SketchDimensionGlyph {
  std::string semantic_id;
  std::string candidate_id;
  std::string token;
  std::string value_text;
  Point2 anchor;
  SketchDimensionMode mode{SketchDimensionMode::Driving};
  SketchDimensionGlyphState state{SketchDimensionGlyphState::Accepted};
  std::vector<std::string> target_ids;
  // Compatibility span for linear/radial callers. The explicit layout is the
  // authority for extension geometry, arcs, terminators and label placement.
  std::vector<Point2> leader;
  SketchDimensionLayout layout;

  friend bool operator==(const SketchDimensionGlyph&, const SketchDimensionGlyph&) = default;
};

class SketchDimensionGlyphLayoutResolver {
public:
  [[nodiscard]] Result<SketchDimensionGlyph>
  resolve(const SketchTopology& topology, const SketchDimensionIntent& dimension,
          const SketchDimensionMeasurement& measurement,
          SketchDimensionGlyphState state = SketchDimensionGlyphState::Accepted) const {
    std::vector<Point2> anchors;
    std::vector<std::string> target_ids;
    anchors.reserve(dimension.targets().size());
    target_ids.reserve(dimension.targets().size());
    for (const auto& target : dimension.targets()) {
      auto anchor = target_anchor(topology, target);
      if (anchor.has_error())
        return Result<SketchDimensionGlyph>::failure(anchor.error());
      anchors.push_back(anchor.value());
      target_ids.push_back(target.id());
    }
    auto geometry = measured_geometry(topology, dimension, measurement, anchors);
    if (!geometry)
      return Result<SketchDimensionGlyph>::failure(geometry.error());
    auto anchor = default_dimension_label_anchor(geometry.value());
    anchor.x += dimension.label_offset().x;
    anchor.y += dimension.label_offset().y;
    auto layout = place_dimension_layout(geometry.value(), anchor);
    std::vector<Point2> span;
    if (!layout.dimension_lines.empty())
      span = {layout.dimension_lines.front().start, layout.dimension_lines.front().end};
    return Result<SketchDimensionGlyph>::success(
        {"sketch/" + topology.sketch().value() + "/dimension/" + dimension.id().value(),
         dimension.id().value(), token(dimension.kind()),
         format(measurement.value, dimension.mode()), anchor, dimension.mode(), state,
         std::move(target_ids), std::move(span), std::move(layout)});
  }

  [[nodiscard]] Result<std::vector<SketchDimensionGlyph>>
  resolve_catalog(const PartDocument& document, const SketchTopology& topology,
                  const SketchDimensionCatalog& catalog) const {
    if (document.id() != catalog.document() || topology.sketch() != catalog.sketch())
      return Result<std::vector<SketchDimensionGlyph>>::failure(Error::validation(
          catalog.sketch().value(), "dimension glyph catalog belongs to another Part or Sketch"));
    std::vector<SketchDimensionGlyph> glyphs;
    glyphs.reserve(catalog.dimensions().size());
    for (const auto& dimension : catalog.dimensions()) {
      auto measurement =
          SketchDimensionMeasurementEvaluator::measure(topology, dimension, &document);
      if (measurement.has_error())
        return Result<std::vector<SketchDimensionGlyph>>::failure(measurement.error());
      auto glyph = resolve(topology, dimension, measurement.value());
      if (glyph.has_error())
        return Result<std::vector<SketchDimensionGlyph>>::failure(glyph.error());
      glyphs.push_back(std::move(glyph.value()));
    }
    return Result<std::vector<SketchDimensionGlyph>>::success(std::move(glyphs));
  }

private:
  [[nodiscard]] static double characteristic_length(const SketchTopology& topology) noexcept {
    bool initialized = false;
    double min_x{}, max_x{}, min_y{}, max_y{};
    for (const auto& point : topology.points()) {
      if (point.id().value().starts_with("reference/"))
        continue;
      if (!initialized) {
        min_x = max_x = point.position().x;
        min_y = max_y = point.position().y;
        initialized = true;
      }
      min_x = std::min(min_x, point.position().x);
      max_x = std::max(max_x, point.position().x);
      min_y = std::min(min_y, point.position().y);
      max_y = std::max(max_y, point.position().y);
    }
    return initialized ? std::hypot(max_x - min_x, max_y - min_y) : 1.0;
  }

  [[nodiscard]] static Result<SketchDimensionGeometry>
  measured_geometry(const SketchTopology& topology, const SketchDimensionIntent& dimension,
                    const SketchDimensionMeasurement& measurement,
                    const std::vector<Point2>& anchors) {
    using namespace dimension_layout_detail;
    SketchDimensionGeometry result;
    result.default_offset = .08 * std::max(1.0, characteristic_length(topology));
    const auto entity = [&](std::size_t index) {
      return topology.find_entity(dimension.targets()[index].id());
    };
    const auto line = [&](const SketchTopologyEntity& edge) {
      return SketchDimensionLine{topology.find_point(edge.points().front())->position(),
                                 topology.find_point(edge.points().back())->position()};
    };
    switch (dimension.kind()) {
    case SketchDimensionKind::HorizontalDistance:
      result.presentation = SketchDimensionPresentation::Horizontal;
      break;
    case SketchDimensionKind::VerticalDistance:
      result.presentation = SketchDimensionPresentation::Vertical;
      break;
    case SketchDimensionKind::Angle:
      result.presentation = SketchDimensionPresentation::Angle;
      break;
    case SketchDimensionKind::Radius:
      result.presentation = SketchDimensionPresentation::Radius;
      break;
    case SketchDimensionKind::Diameter:
      result.presentation = SketchDimensionPresentation::Diameter;
      break;
    case SketchDimensionKind::ArcLength:
      result.presentation = SketchDimensionPresentation::ArcLength;
      break;
    default:
      result.presentation = SketchDimensionPresentation::Aligned;
      break;
    }
    if (dimension.kind() == SketchDimensionKind::Angle) {
      const auto* first = entity(0);
      const auto* second = entity(1);
      if (!first || !second || first->points().size() != 2 || second->points().size() != 2)
        return Result<SketchDimensionGeometry>::failure(
            Error::validation(dimension.id().value(), "Angle layout requires two line entities"));
      const auto a = line(*first), b = line(*second);
      result.measured_lines = {a, b};
      result.measured_references = {a.start, a.end, b.start, b.end};
      const auto d1 = subtract(a.end, a.start), d2 = subtract(b.end, b.start);
      const double cross = d1.x * d2.y - d1.y * d2.x;
      result.start_angle = std::atan2(d1.y, d1.x);
      result.sweep_angle = std::atan2(cross, dot(d1, d2));
      if (std::abs(cross) <= 1e-12 * std::max(1.0, magnitude(d1) * magnitude(d2))) {
        result.angular_vertex = false;
        result.center = scale(add(add(a.start, a.end), add(b.start, b.end)), .25);
      } else {
        const auto offset = subtract(b.start, a.start);
        result.center = add(a.start, scale(d1, (offset.x * d2.y - offset.y * d2.x) / cross));
      }
      result.radius =
          std::max(result.default_offset * 3, .55 * std::min(magnitude(d1), magnitude(d2)));
    } else if (dimension.kind() == SketchDimensionKind::Radius ||
               dimension.kind() == SketchDimensionKind::Diameter ||
               dimension.kind() == SketchDimensionKind::ArcLength) {
      const auto* selected = entity(0);
      if (!selected)
        return Result<SketchDimensionGeometry>::failure(
            Error::validation(dimension.id().value(), "Circular layout target must exist"));
      if (selected->kind() == SketchTopologyEntityKind::Arc) {
        auto arc = SketchDimensionMeasurementEvaluator::arc_geometry(topology, *selected,
                                                                     dimension.id().value());
        if (!arc)
          return Result<SketchDimensionGeometry>::failure(arc.error());
        result.center = arc.value().center;
        result.radius = arc.value().radius;
        result.bounded_arc = true;
        for (const auto& id : selected->points())
          result.measured_references.push_back(topology.find_point(id)->position());
        const auto first = subtract(result.measured_references[0], result.center);
        const auto mid = subtract(result.measured_references[1], result.center);
        const auto last = subtract(result.measured_references[2], result.center);
        result.start_angle = std::atan2(first.y, first.x);
        const auto end = turn(std::atan2(last.y, last.x) - result.start_angle);
        const auto middle = turn(std::atan2(mid.y, mid.x) - result.start_angle);
        result.sweep_angle = middle <= end + 1e-10 ? end : end - 2 * std::numbers::pi;
      } else {
        result.center = anchors.front();
        result.radius = measurement.value.millimeters() *
                        (dimension.kind() == SketchDimensionKind::Radius ? 1 : .5);
        result.sweep_angle = 2 * std::numbers::pi;
        result.measured_references = {result.center, add(result.center, {result.radius, 0})};
      }
      if (result.bounded_arc || dimension.kind() == SketchDimensionKind::ArcLength)
        result.default_offset = std::max(result.default_offset, result.radius * .75);
    } else {
      if (dimension.targets().size() == 1 && entity(0) && entity(0)->points().size() >= 2) {
        const auto endpoints = line(*entity(0));
        result.measured_references = {endpoints.start, endpoints.end};
      } else
        result.measured_references = anchors;
    }
    return Result<SketchDimensionGeometry>::success(std::move(result));
  }

  [[nodiscard]] static Result<Point2>
  target_anchor(const SketchTopology& topology,
                const SketchConstraintIntentTarget& target) {
    if (target.kind() == SketchConstraintIntentTargetKind::Point) {
      const auto* point = topology.find_point(SketchPointId(target.id()));
      if (point == nullptr)
        return Result<Point2>::failure(Error::validation(
            target.id(), "dimension glyph references an unknown Sketch point"));
      return Result<Point2>::success(point->position());
    }
    const auto* entity = topology.find_entity(target.id());
    if (entity == nullptr || entity->points().empty())
      return Result<Point2>::failure(Error::validation(
          target.id(), "dimension glyph references an invalid Sketch entity"));
    Point2 centroid{};
    for (const auto& point_id : entity->points()) {
      const auto* point = topology.find_point(point_id);
      if (point == nullptr)
        return Result<Point2>::failure(Error::validation(
            target.id(), "dimension glyph entity references an unknown Sketch point"));
      centroid.x += point->position().x;
      centroid.y += point->position().y;
    }
    centroid.x /= static_cast<double>(entity->points().size());
    centroid.y /= static_cast<double>(entity->points().size());
    return Result<Point2>::success(centroid);
  }

  [[nodiscard]] static std::string token(SketchDimensionKind kind) {
    switch (kind) {
    case SketchDimensionKind::HorizontalDistance: return "H";
    case SketchDimensionKind::VerticalDistance: return "V";
    case SketchDimensionKind::AlignedDistance: return "↗";
    case SketchDimensionKind::PointToPointDistance: return "↔";
    case SketchDimensionKind::Length: return "L";
    case SketchDimensionKind::Radius: return "R";
    case SketchDimensionKind::Diameter: return "Ø";
    case SketchDimensionKind::Angle: return "∠";
    case SketchDimensionKind::ArcLength: return "⌒";
    }
    return "D";
  }

  [[nodiscard]] static std::string format(const Quantity& quantity, SketchDimensionMode mode) {
    std::ostringstream stream;
    if (mode == SketchDimensionMode::Reference) stream << '(';
    stream << std::fixed << std::setprecision(3);
    if (quantity.kind() == QuantityKind::AngleDeg)
      stream << quantity.degrees() << " deg";
    else
      stream << quantity.millimeters() << " mm";
    if (mode == SketchDimensionMode::Reference) stream << ')';
    return stream.str();
  }
};

} // namespace blcad::geometry
