#include "blcad/geometry/circular_pattern_adapter.hpp"

#include "blcad/core/error.hpp"

#include <cmath>
#include <string>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error pattern_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

} // namespace

Result<std::vector<GeometryShape>> CircularPatternAdapter::generate_instances(
    FeatureId feature_id, const std::vector<GeometryShape>& sources, Point3 axis_origin,
    Vector3 axis_direction, std::size_t count, double total_angle_deg) const {
  if (feature_id.empty())
    return Result<std::vector<GeometryShape>>::failure(
        Error::validation("circular_pattern", "feature id must not be empty"));
  if (sources.empty())
    return Result<std::vector<GeometryShape>>::failure(
        pattern_error(feature_id, "circular pattern requires at least one source shape"));
  for (const auto& source : sources)
    if (source.empty())
      return Result<std::vector<GeometryShape>>::failure(
          pattern_error(feature_id, "circular pattern source shapes must not be empty"));
  if (count < 2U)
    return Result<std::vector<GeometryShape>>::failure(
        pattern_error(feature_id, "circular pattern count must be at least 2"));
  if (!std::isfinite(total_angle_deg) || total_angle_deg <= 0.0 || total_angle_deg > 360.0)
    return Result<std::vector<GeometryShape>>::failure(pattern_error(
        feature_id, "circular pattern total angle must be finite and in (0, 360] degrees"));
  if (!std::isfinite(axis_origin.x) || !std::isfinite(axis_origin.y) ||
      !std::isfinite(axis_origin.z))
    return Result<std::vector<GeometryShape>>::failure(
        pattern_error(feature_id, "circular pattern axis origin must be finite"));
  const double magnitude =
      std::sqrt(axis_direction.x * axis_direction.x + axis_direction.y * axis_direction.y +
                axis_direction.z * axis_direction.z);
  if (!std::isfinite(magnitude) || magnitude <= 1.0e-12)
    return Result<std::vector<GeometryShape>>::failure(
        pattern_error(feature_id, "circular pattern axis must not be zero length"));
  axis_direction = {axis_direction.x / magnitude, axis_direction.y / magnitude,
                    axis_direction.z / magnitude};

  const bool full_circle = total_angle_deg == 360.0;
  const double angle_step = total_angle_deg / static_cast<double>(full_circle ? count : count - 1U);
  std::vector<GeometryShape> instances;
  instances.reserve(count * sources.size());
  for (std::size_t instance_index = 0U; instance_index < count; ++instance_index) {
    const double angle_deg = angle_step * static_cast<double>(instance_index);
    for (const auto& source : sources) {
      if (instance_index == 0U) {
        instances.push_back(source);
        continue;
      }
      auto rotated = transform_adapter_.rotate(source, axis_origin, axis_direction, angle_deg);
      if (rotated.has_error())
        return Result<std::vector<GeometryShape>>::failure(
            pattern_error(feature_id, rotated.error().message()));
      instances.push_back(std::move(rotated.value()));
    }
  }
  return Result<std::vector<GeometryShape>>::success(std::move(instances));
}

} // namespace blcad::geometry
