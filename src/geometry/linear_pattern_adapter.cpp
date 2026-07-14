#include "blcad/geometry/linear_pattern_adapter.hpp"

#include "blcad/core/error.hpp"

#include <cmath>
#include <string>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error pattern_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

} // namespace

Result<std::vector<GeometryShape>> LinearPatternAdapter::generate_instances(
    FeatureId feature_id, const std::vector<GeometryShape>& sources, Vector3 direction,
    std::size_t count, double spacing_mm) const {
  if (feature_id.empty())
    return Result<std::vector<GeometryShape>>::failure(
        Error::validation("linear_pattern", "feature id must not be empty"));
  if (sources.empty())
    return Result<std::vector<GeometryShape>>::failure(
        pattern_error(feature_id, "linear pattern requires at least one source shape"));
  for (const auto& source : sources)
    if (source.empty())
      return Result<std::vector<GeometryShape>>::failure(
          pattern_error(feature_id, "linear pattern source shapes must not be empty"));
  if (count < 2U)
    return Result<std::vector<GeometryShape>>::failure(
        pattern_error(feature_id, "linear pattern count must be at least 2"));
  if (!std::isfinite(spacing_mm) || spacing_mm <= 0.0)
    return Result<std::vector<GeometryShape>>::failure(
        pattern_error(feature_id, "linear pattern spacing must be finite and positive"));
  const double magnitude =
      std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
  if (!std::isfinite(magnitude) || magnitude <= 1.0e-12)
    return Result<std::vector<GeometryShape>>::failure(
        pattern_error(feature_id, "linear pattern direction must not be zero length"));
  direction = {direction.x / magnitude, direction.y / magnitude, direction.z / magnitude};

  std::vector<GeometryShape> instances;
  instances.reserve(count * sources.size());
  for (std::size_t instance_index = 0U; instance_index < count; ++instance_index) {
    const double distance = spacing_mm * static_cast<double>(instance_index);
    const Vector3 offset{direction.x * distance, direction.y * distance, direction.z * distance};
    for (const auto& source : sources) {
      if (instance_index == 0U) {
        instances.push_back(source);
        continue;
      }
      auto translated = transform_adapter_.translate(source, offset);
      if (translated.has_error())
        return Result<std::vector<GeometryShape>>::failure(
            pattern_error(feature_id, translated.error().message()));
      instances.push_back(std::move(translated.value()));
    }
  }
  return Result<std::vector<GeometryShape>>::success(std::move(instances));
}

} // namespace blcad::geometry
