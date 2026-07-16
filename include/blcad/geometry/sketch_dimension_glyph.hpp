#pragma once

#include "blcad/core/sketch_dimension_authoring.hpp"

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
    Point2 centroid{};
    for (const auto anchor : anchors) {
      centroid.x += anchor.x;
      centroid.y += anchor.y;
    }
    centroid.x /= static_cast<double>(anchors.size());
    centroid.y /= static_cast<double>(anchors.size());
    const double offset = 0.08 * std::max(1.0, characteristic_length(topology));
    if (dimension.kind() == SketchDimensionKind::HorizontalDistance)
      centroid.y += offset;
    else if (dimension.kind() == SketchDimensionKind::VerticalDistance)
      centroid.x += offset;
    else {
      centroid.x += offset * 0.55;
      centroid.y += offset * 0.55;
    }
    return Result<SketchDimensionGlyph>::success(
        {"sketch/" + topology.sketch().value() + "/dimension/" + dimension.id().value(),
         dimension.id().value(), token(dimension.kind()), format(measurement.value, dimension.mode()),
         centroid, dimension.mode(), state, std::move(target_ids)});
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
      auto measurement = SketchDimensionMeasurementEvaluator::measure(topology, dimension);
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
    return std::hypot(max_x - min_x, max_y - min_y);
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
