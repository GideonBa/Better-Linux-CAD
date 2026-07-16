#pragma once

#include "blcad/core/sketch_3d.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad {

struct SketchPoint3DValue {
  double x{0.0};
  double y{0.0};
  double z{0.0};
};

enum class Sketch3DLock { Free, AxisX, AxisY, AxisZ, PlaneXY, PlaneYZ, PlaneZX };

struct Sketch3DTypedInput {
  std::optional<double> x;
  std::optional<double> y;
  std::optional<double> z;
  std::optional<double> distance;
  std::optional<double> azimuth_radians;
  std::optional<double> elevation_radians;
};

struct Sketch3DProjectedPointIntent {
  SketchEntityId local_point;
  SketchId source_sketch;
  SketchReferenceTarget source_target;
};

struct Sketch3DInteractionResult {
  Sketch3D sketch;
  std::vector<SketchEntityId> created_entities;
  std::optional<Sketch3DProjectedPointIntent> projected_point;
};

class Sketch3DInteractionService {
public:
  [[nodiscard]] static Result<SketchPoint3DValue>
  resolve_point(SketchPoint3DValue anchor, SketchPoint3DValue pointer,
                Sketch3DLock lock, const Sketch3DTypedInput& typed = {}) {
    auto finite = [](double value) { return std::isfinite(value); };
    if (!finite(anchor.x) || !finite(anchor.y) || !finite(anchor.z) ||
        !finite(pointer.x) || !finite(pointer.y) || !finite(pointer.z))
      return Result<SketchPoint3DValue>::failure(
          Error::validation("sketch3d.input", "3D point input must be finite"));

    SketchPoint3DValue value = pointer;
    switch (lock) {
    case Sketch3DLock::Free: break;
    case Sketch3DLock::AxisX: value.y = anchor.y; value.z = anchor.z; break;
    case Sketch3DLock::AxisY: value.x = anchor.x; value.z = anchor.z; break;
    case Sketch3DLock::AxisZ: value.x = anchor.x; value.y = anchor.y; break;
    case Sketch3DLock::PlaneXY: value.z = anchor.z; break;
    case Sketch3DLock::PlaneYZ: value.x = anchor.x; break;
    case Sketch3DLock::PlaneZX: value.y = anchor.y; break;
    }

    if (typed.distance || typed.azimuth_radians || typed.elevation_radians) {
      if (!typed.distance || !typed.azimuth_radians || !typed.elevation_radians)
        return Result<SketchPoint3DValue>::failure(Error::validation(
            "sketch3d.input", "distance input requires azimuth and elevation"));
      if (!finite(*typed.distance) || *typed.distance < 0.0 ||
          !finite(*typed.azimuth_radians) || !finite(*typed.elevation_radians))
        return Result<SketchPoint3DValue>::failure(Error::validation(
            "sketch3d.input", "distance and angle input must be finite and non-negative"));
      const double planar = *typed.distance * std::cos(*typed.elevation_radians);
      value = {anchor.x + planar * std::cos(*typed.azimuth_radians),
               anchor.y + planar * std::sin(*typed.azimuth_radians),
               anchor.z + *typed.distance * std::sin(*typed.elevation_radians)};
    }
    if (typed.x) value.x = *typed.x;
    if (typed.y) value.y = *typed.y;
    if (typed.z) value.z = *typed.z;
    if (!finite(value.x) || !finite(value.y) || !finite(value.z))
      return Result<SketchPoint3DValue>::failure(
          Error::validation("sketch3d.input", "resolved 3D point must be finite"));
    return Result<SketchPoint3DValue>::success(value);
  }

  [[nodiscard]] static Result<Sketch3DInteractionResult>
  add_point(const Sketch3D& source, SketchEntityId id, SketchPoint3DValue value) {
    auto candidate = clone(source);
    if (candidate.has_error()) return Result<Sketch3DInteractionResult>::failure(candidate.error());
    auto point = make_point(std::move(id), value);
    if (point.has_error()) return Result<Sketch3DInteractionResult>::failure(point.error());
    const auto created = point.value().id();
    auto added = candidate.value().add_point(std::move(point.value()));
    if (added.has_error()) return Result<Sketch3DInteractionResult>::failure(added.error());
    return Result<Sketch3DInteractionResult>::success(
        {std::move(candidate.value()), {created}, std::nullopt});
  }

  [[nodiscard]] static Result<Sketch3DInteractionResult>
  add_line(const Sketch3D& source, SketchEntityId line_id,
           SketchEntityId start_id, SketchPoint3DValue start,
           SketchEntityId end_id, SketchPoint3DValue end,
           std::optional<SketchGuideCurve3DRole> guide_role = std::nullopt) {
    auto first = add_point(source, std::move(start_id), start);
    if (first.has_error()) return first;
    auto second = add_point(first.value().sketch, std::move(end_id), end);
    if (second.has_error()) return second;
    auto line = SketchLine3D::create(line_id, first.value().created_entities.front(),
                                     second.value().created_entities.front());
    if (line.has_error()) return Result<Sketch3DInteractionResult>::failure(line.error());
    auto candidate = std::move(second.value().sketch);
    auto added = candidate.add_line(line.value());
    if (added.has_error()) return Result<Sketch3DInteractionResult>::failure(added.error());
    std::vector<SketchEntityId> created{first.value().created_entities.front(),
                                       second.value().created_entities.front(), line_id};
    if (guide_role) {
      SketchEntityId guide_id(line_id.value() + ".guide");
      auto guide = SketchGuideCurve3D::create(guide_id, line_id, *guide_role);
      if (guide.has_error()) return Result<Sketch3DInteractionResult>::failure(guide.error());
      auto guide_added = candidate.add_guide_curve(guide.value());
      if (guide_added.has_error())
        return Result<Sketch3DInteractionResult>::failure(guide_added.error());
      created.push_back(guide_id);
    }
    return Result<Sketch3DInteractionResult>::success(
        {std::move(candidate), std::move(created), std::nullopt});
  }

  [[nodiscard]] static Result<Sketch3DInteractionResult>
  move_point(const Sketch3D& source, const SketchEntityId& point_id, SketchPoint3DValue value) {
    if (source.find_point(point_id) == nullptr)
      return Result<Sketch3DInteractionResult>::failure(
          Error::validation(point_id.value(), "3D Sketch point handle does not exist"));
    auto replacement = make_point(point_id, value);
    if (replacement.has_error()) return Result<Sketch3DInteractionResult>::failure(replacement.error());
    auto candidate = clone(source, &point_id, &replacement.value());
    if (candidate.has_error()) return Result<Sketch3DInteractionResult>::failure(candidate.error());
    return Result<Sketch3DInteractionResult>::success(
        {std::move(candidate.value()), {}, std::nullopt});
  }

  [[nodiscard]] static Result<Sketch3DInteractionResult>
  project_planar_point(const Sketch3D& source, SketchEntityId local_id,
                       SketchPoint3DValue resolved_model_point, SketchId planar_sketch,
                       SketchReferenceTarget target) {
    if (planar_sketch.value().empty())
      return Result<Sketch3DInteractionResult>::failure(
          Error::validation("sketch3d.project", "source planar Sketch id must not be empty"));
    auto added = add_point(source, local_id, resolved_model_point);
    if (added.has_error()) return added;
    added.value().projected_point =
        Sketch3DProjectedPointIntent{local_id, std::move(planar_sketch), std::move(target)};
    return added;
  }

private:
  [[nodiscard]] static Result<SketchPoint3D> make_point(SketchEntityId id,
                                                         SketchPoint3DValue value) {
    auto coordinate = [](double v) -> Result<SketchCoordinate3D> {
      auto quantity = Quantity::linear_displacement_mm(v, "sketch3d.coordinate");
      if (quantity.has_error()) return Result<SketchCoordinate3D>::failure(quantity.error());
      return SketchCoordinate3D::create_explicit(quantity.value());
    };
    auto x = coordinate(value.x); if (x.has_error()) return Result<SketchPoint3D>::failure(x.error());
    auto y = coordinate(value.y); if (y.has_error()) return Result<SketchPoint3D>::failure(y.error());
    auto z = coordinate(value.z); if (z.has_error()) return Result<SketchPoint3D>::failure(z.error());
    return SketchPoint3D::create(std::move(id), x.value(), y.value(), z.value());
  }

  [[nodiscard]] static Result<Sketch3D>
  clone(const Sketch3D& source, const SketchEntityId* replaced_id = nullptr,
        const SketchPoint3D* replacement = nullptr) {
    auto candidate = Sketch3D::create(source.id(), source.name());
    if (candidate.has_error()) return candidate;
    for (const auto& point : source.points()) {
      const auto& value = replaced_id != nullptr && point.id() == *replaced_id ? *replacement : point;
      auto result = candidate.value().add_point(value);
      if (result.has_error()) return Result<Sketch3D>::failure(result.error());
    }
    for (const auto& item : source.lines()) { auto r = candidate.value().add_line(item); if (r.has_error()) return Result<Sketch3D>::failure(r.error()); }
    for (const auto& item : source.polylines()) { auto r = candidate.value().add_polyline(item); if (r.has_error()) return Result<Sketch3D>::failure(r.error()); }
    for (const auto& item : source.arcs()) { auto r = candidate.value().add_arc(item); if (r.has_error()) return Result<Sketch3D>::failure(r.error()); }
    for (const auto& item : source.splines()) { auto r = candidate.value().add_spline(item); if (r.has_error()) return Result<Sketch3D>::failure(r.error()); }
    for (const auto& item : source.helices()) { auto r = candidate.value().add_helix(item); if (r.has_error()) return Result<Sketch3D>::failure(r.error()); }
    for (const auto& item : source.guide_curves()) { auto r = candidate.value().add_guide_curve(item); if (r.has_error()) return Result<Sketch3D>::failure(r.error()); }
    return candidate;
  }
};

} // namespace blcad
