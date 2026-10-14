#pragma once

#include "blcad/core/sketch_modify.hpp"
#include "blcad/core/sketch_topology.hpp"

namespace blcad {

struct SketchTangentAnchor {
  SketchEntityId entity;
  Point2 point;
  Point2 direction;
  std::optional<SketchPointId> endpoint;
};
struct SketchTangentCircle {
  Point2 center;
  double radius;
  double signed_radius;
};

class SketchTangentCreation {
public:
  // An arc continues an endpoint. A tangent circle may also touch a line's
  // interior, where a line-circle relation has unambiguous solver semantics.
  [[nodiscard]] static Result<SketchTangentAnchor> anchor(const Sketch& sketch, SketchEntityId id,
                                                          Point2 pick, bool endpoint_required) {
    using namespace detail;
    const auto fail = [&](std::string message) {
      return Result<SketchTangentAnchor>::failure(
          Error::validation(id.value(), std::move(message)));
    };
    if (!sketch_modify_finite(pick)) return fail("tangent contact must be finite");
    const auto curve = curve_sampler_for(sketch, id);
    if (!curve)
      return fail("tangent creation requires an editable line, arc or spline");
    const auto t = sampler_project(*curve, pick);
    if (!t || *t < -kSketchModifyParamTolerance || *t > 1 + kSketchModifyParamTolerance)
      return fail("tangent contact must lie on the selected curve");
    const bool at_start = sketch_modify_distance(pick, sampler_point(*curve, 0)) <= 1e-6;
    const bool at_end = sketch_modify_distance(pick, sampler_point(*curve, 1)) <= 1e-6;
    if ((endpoint_required || curve->kind != CurveSampler::Kind::Line) && !at_start && !at_end)
      return fail("select a curve endpoint as the tangent contact");
    const double parameter = at_start ? 0.0 : at_end ? 1.0 : *t;
    Point2 direction = sampler_derivative(*curve, parameter);
    const double length = std::hypot(direction.x, direction.y);
    if (length <= 1e-12)
      return fail("the selected curve has no regular tangent at the contact");
    const double orientation = at_start ? -1.0 : 1.0;
    direction = {orientation * direction.x / length, orientation * direction.y / length};
    std::optional<SketchPointId> endpoint;
    if (at_start || at_end) {
      auto topology = SketchTopology::migrate_legacy(sketch);
      if (!topology)
        return Result<SketchTangentAnchor>::failure(topology.error());
      const auto* entity = topology.value().find_entity("entity/" + id.value());
      if (!entity)
        return fail("tangent source has no topology");
      endpoint = at_start ? entity->points().front() : entity->points().back();
    }
    return Result<SketchTangentAnchor>::success(
        {std::move(id), sampler_point(*curve, parameter), direction, endpoint});
  }

  [[nodiscard]] static Result<SketchTangentCircle> circle(const SketchTangentAnchor& anchor,
                                                          Point2 rim) {
    const Point2 delta{rim.x - anchor.point.x, rim.y - anchor.point.y};
    const Point2 normal{-anchor.direction.y, anchor.direction.x};
    const double projection = delta.x * normal.x + delta.y * normal.y;
    const double squared = delta.x * delta.x + delta.y * delta.y;
    if (!std::isfinite(squared) || squared <= 1e-18 || std::abs(projection) <= 1e-10)
      return Result<SketchTangentCircle>::failure(Error::validation(
          anchor.entity.value(),
          "tangent circle needs a distinct rim point away from the tangent line"));
    const double radius = squared / (2 * projection);
    const Point2 center{anchor.point.x + radius * normal.x, anchor.point.y + radius * normal.y};
    if (!std::isfinite(radius) || !sketch_finite(center))
      return Result<SketchTangentCircle>::failure(
          Error::validation(anchor.entity.value(), "tangent circle is not finite"));
    return Result<SketchTangentCircle>::success({center, std::abs(radius), radius});
  }

  [[nodiscard]] static Result<ArcSegment> arc(SketchEntityId id, const SketchTangentAnchor& anchor,
                                              Point2 end) {
    auto geometry = circle(anchor, end);
    if (!geometry)
      return Result<ArcSegment>::failure(geometry.error());
    const auto center = geometry.value().center;
    const double start = std::atan2(anchor.point.y - center.y, anchor.point.x - center.x);
    const double finish = std::atan2(end.y - center.y, end.x - center.x);
    const double sweep = geometry.value().signed_radius > 0
                             ? detail::sketch_modify_wrap_two_pi(finish - start)
                             : -detail::sketch_modify_wrap_two_pi(start - finish);
    const double mid = start + sweep * .5;
    return ArcSegment::create_three_point(std::move(id), anchor.point,
                                          {center.x + geometry.value().radius * std::cos(mid),
                                           center.y + geometry.value().radius * std::sin(mid)},
                                          end);
  }

private:
  [[nodiscard]] static bool sketch_finite(Point2 p) {
    return std::isfinite(p.x) && std::isfinite(p.y);
  }
};

} // namespace blcad
