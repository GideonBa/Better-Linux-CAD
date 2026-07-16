#pragma once

#include "blcad/core/sketch_constraint_authoring.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::geometry {

enum class SketchConstraintGlyphState { Accepted, Preview, Conflict, Redundant };

[[nodiscard]] constexpr std::string_view
to_string(SketchConstraintGlyphState state) noexcept {
  switch (state) {
  case SketchConstraintGlyphState::Accepted: return "accepted";
  case SketchConstraintGlyphState::Preview: return "preview";
  case SketchConstraintGlyphState::Conflict: return "conflict";
  case SketchConstraintGlyphState::Redundant: return "redundant";
  }
  return "accepted";
}

struct SketchConstraintGlyph {
  std::string semantic_id;
  std::string candidate_id;
  std::string token;
  Point2 anchor;
  SketchConstraintGlyphState state{SketchConstraintGlyphState::Accepted};
  std::vector<std::string> target_ids;

  friend bool operator==(const SketchConstraintGlyph&, const SketchConstraintGlyph&) = default;
};

class SketchConstraintGlyphLayoutResolver {
public:
  [[nodiscard]] Result<SketchConstraintGlyph>
  resolve(const SketchTopology& topology, const SketchConstraintIntent& constraint,
          SketchConstraintGlyphState state = SketchConstraintGlyphState::Accepted) const {
    std::vector<Point2> anchors;
    std::vector<std::string> target_ids;
    anchors.reserve(constraint.targets().size());
    target_ids.reserve(constraint.targets().size());
    for (const auto& target : constraint.targets()) {
      auto anchor = target_anchor(topology, target);
      if (anchor.has_error())
        return Result<SketchConstraintGlyph>::failure(anchor.error());
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
    return Result<SketchConstraintGlyph>::success(
        {"sketch/" + topology.sketch().value() + "/constraint/" + constraint.id().value(),
         constraint.id().value(), token(constraint.kind()), centroid, state,
         std::move(target_ids)});
  }

private:
  [[nodiscard]] static Result<Point2>
  target_anchor(const SketchTopology& topology,
                const SketchConstraintIntentTarget& target) {
    if (target.kind() == SketchConstraintIntentTargetKind::Point) {
      const auto* point = topology.find_point(SketchPointId(target.id()));
      if (point == nullptr)
        return Result<Point2>::failure(Error::validation(
            target.id(), "constraint glyph references an unknown Sketch point"));
      return Result<Point2>::success(point->position());
    }
    const auto* entity = topology.find_entity(target.id());
    if (entity == nullptr)
      return Result<Point2>::failure(Error::validation(
          target.id(), "constraint glyph references an unknown Sketch entity"));
    if (entity->points().empty())
      return Result<Point2>::failure(Error::validation(
          target.id(), "constraint glyph entity has no defining Sketch points"));
    Point2 centroid{};
    for (const auto& point_id : entity->points()) {
      const auto* point = topology.find_point(point_id);
      if (point == nullptr)
        return Result<Point2>::failure(Error::validation(
            target.id(), "constraint glyph entity references an unknown Sketch point"));
      centroid.x += point->position().x;
      centroid.y += point->position().y;
    }
    centroid.x /= static_cast<double>(entity->points().size());
    centroid.y /= static_cast<double>(entity->points().size());
    return Result<Point2>::success(centroid);
  }

  [[nodiscard]] static std::string token(SketchSolverConstraintKind kind) {
    switch (kind) {
    case SketchSolverConstraintKind::Coincident: return "●";
    case SketchSolverConstraintKind::Fixed: return "🔒";
    case SketchSolverConstraintKind::Horizontal: return "H";
    case SketchSolverConstraintKind::Vertical: return "V";
    case SketchSolverConstraintKind::Parallel: return "∥";
    case SketchSolverConstraintKind::Perpendicular: return "⊥";
    case SketchSolverConstraintKind::Collinear: return "—·—";
    case SketchSolverConstraintKind::Equal: return "=";
    case SketchSolverConstraintKind::Tangent: return "T";
    case SketchSolverConstraintKind::Concentric: return "◎";
    case SketchSolverConstraintKind::Midpoint: return "M";
    case SketchSolverConstraintKind::Symmetric: return "↔";
    case SketchSolverConstraintKind::PointOnObject: return "⊙";
    default: return "?";
    }
  }
};

} // namespace blcad::geometry
