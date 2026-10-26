#pragma once

#include "blcad/core/sketch_dimension_authoring.hpp"
#include "blcad/geometry/sketch_dimension_glyph.hpp"

#include <optional>

namespace blcad::gui {

struct GuiRapidDimensionCandidate {
  SketchDimensionIntent intent;
  geometry::SketchDimensionGlyph glyph;
};

// Inference is disposable. All measurements, units, validation and publication
// still go through the existing Core dimension authority.
class GuiRapidDimensionInference {
public:
  [[nodiscard]] static Result<std::optional<GuiRapidDimensionCandidate>>
  infer(const PartDocument& document, const SketchTopology& topology,
        std::vector<SketchConstraintIntentTarget> targets, Point2 cursor,
        bool alternate_radial = false) {
    const auto none = [] {
      return Result<std::optional<GuiRapidDimensionCandidate>>::success(std::nullopt);
    };
    if (targets.empty() || targets.size() > 2U)
      return none();
    SketchDimensionKind kind = SketchDimensionKind::Length;
    const auto entity = [&](const auto& target) {
      return target.kind() == SketchConstraintIntentTargetKind::Entity
                 ? topology.find_entity(target.id())
                 : nullptr;
    };
    if (targets.size() == 1U) {
      const auto* curve = entity(targets[0]);
      if (!curve)
        return none();
      if (curve->kind() == SketchTopologyEntityKind::CircleProfile)
        kind = alternate_radial ? SketchDimensionKind::Radius : SketchDimensionKind::Diameter;
      else if (curve->kind() == SketchTopologyEntityKind::Arc)
        kind = alternate_radial ? SketchDimensionKind::Diameter : SketchDimensionKind::Radius;
      else if (curve->kind() == SketchTopologyEntityKind::Line && !curve->reference()) {
        const Point2 a = topology.find_point(curve->points()[0])->position();
        const Point2 b = topology.find_point(curve->points()[1])->position();
        if (std::abs(a.x - b.x) > 1e-9 && std::abs(a.y - b.y) > 1e-9) {
          if ((cursor.y < std::min(a.y, b.y) || cursor.y > std::max(a.y, b.y)) &&
              cursor.x >= std::min(a.x, b.x) && cursor.x <= std::max(a.x, b.x))
            kind = SketchDimensionKind::HorizontalDistance;
          else if ((cursor.x < std::min(a.x, b.x) || cursor.x > std::max(a.x, b.x)) &&
                   cursor.y >= std::min(a.y, b.y) && cursor.y <= std::max(a.y, b.y))
            kind = SketchDimensionKind::VerticalDistance;
          if (kind != SketchDimensionKind::Length)
            targets = {SketchConstraintIntentTarget::point(curve->points()[0]).value(),
                       SketchConstraintIntentTarget::point(curve->points()[1]).value()};
        }
      } else
        return none();
    } else if (targets[0].kind() == SketchConstraintIntentTargetKind::Point &&
               targets[1].kind() == SketchConstraintIntentTargetKind::Point) {
      const auto* first = topology.find_point(SketchPointId(targets[0].id()));
      const auto* second = topology.find_point(SketchPointId(targets[1].id()));
      if (!first || !second || first->id() == second->id())
        return none();
      const auto a = first->position(), b = second->position();
      kind = SketchDimensionKind::AlignedDistance;
      if (std::abs(a.y - b.y) < 1e-9 ||
          (std::abs(a.x - b.x) > 1e-9 && cursor.x >= std::min(a.x, b.x) &&
           cursor.x <= std::max(a.x, b.x) &&
           (cursor.y < std::min(a.y, b.y) || cursor.y > std::max(a.y, b.y))))
        kind = SketchDimensionKind::HorizontalDistance;
      else if (std::abs(a.x - b.x) < 1e-9 ||
               (cursor.y >= std::min(a.y, b.y) && cursor.y <= std::max(a.y, b.y) &&
                (cursor.x < std::min(a.x, b.x) || cursor.x > std::max(a.x, b.x))))
        kind = SketchDimensionKind::VerticalDistance;
    } else {
      const auto* first = entity(targets[0]);
      const auto* second = entity(targets[1]);
      if (!first || !second || first->id() == second->id() ||
          first->kind() != SketchTopologyEntityKind::Line ||
          second->kind() != SketchTopologyEntityKind::Line)
        return none();
      const auto a = topology.find_point(first->points()[0])->position();
      const auto b = topology.find_point(first->points()[1])->position();
      const auto c = topology.find_point(second->points()[0])->position();
      const auto d = topology.find_point(second->points()[1])->position();
      const double dx = b.x - a.x, dy = b.y - a.y, ex = d.x - c.x, ey = d.y - c.y;
      const double scale = std::hypot(dx, dy) * std::hypot(ex, ey);
      if (scale <= 1e-12)
        return none();
      if (std::abs(dx * ey - dy * ex) <= 1e-10 * scale) {
        if (std::abs(dy) <= 1e-10 * std::hypot(dx, dy))
          kind = SketchDimensionKind::VerticalDistance;
        else if (std::abs(dx) <= 1e-10 * std::hypot(dx, dy))
          kind = SketchDimensionKind::HorizontalDistance;
        else
          return none();
        targets = {SketchConstraintIntentTarget::point(first->points()[0]).value(),
                   SketchConstraintIntentTarget::point(second->points()[0]).value()};
      } else
        kind = SketchDimensionKind::Angle;
    }
    if (kind == SketchDimensionKind::HorizontalDistance ||
        kind == SketchDimensionKind::VerticalDistance) {
      const auto a = topology.find_point(SketchPointId(targets[0].id()))->position();
      const auto b = topology.find_point(SketchPointId(targets[1].id()))->position();
      if ((kind == SketchDimensionKind::HorizontalDistance && b.x < a.x) ||
          (kind == SketchDimensionKind::VerticalDistance && b.y < a.y))
        std::swap(targets[0], targets[1]);
    }
    auto intent = SketchDimensionIntent::create(SketchDimensionId("rapid.preview"), kind,
                                                SketchDimensionMode::Reference, std::move(targets),
                                                std::nullopt);
    if (!intent)
      return Result<std::optional<GuiRapidDimensionCandidate>>::failure(intent.error());
    auto measurement =
        SketchDimensionMeasurementEvaluator::measure(topology, intent.value(), &document);
    if (!measurement)
      return none();
    auto glyph = geometry::SketchDimensionGlyphLayoutResolver{}.resolve(
        topology, intent.value(), measurement.value(),
        geometry::SketchDimensionGlyphState::Preview);
    if (!glyph)
      return Result<std::optional<GuiRapidDimensionCandidate>>::failure(glyph.error());
    auto placed = intent.value().with_label_offset(
        {cursor.x - glyph.value().anchor.x, cursor.y - glyph.value().anchor.y});
    if (!placed)
      return Result<std::optional<GuiRapidDimensionCandidate>>::failure(placed.error());
    glyph.value().anchor = cursor;
    return Result<std::optional<GuiRapidDimensionCandidate>>::success(
        GuiRapidDimensionCandidate{std::move(placed.value()), std::move(glyph.value())});
  }
};

} // namespace blcad::gui
