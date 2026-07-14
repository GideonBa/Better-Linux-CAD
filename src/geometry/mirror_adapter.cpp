#include "blcad/geometry/mirror_adapter.hpp"

#include "blcad/core/error.hpp"

#include <cmath>
#include <string>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error mirror_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

} // namespace

Result<std::vector<GeometryShape>>
MirrorAdapter::reflect_sources(FeatureId feature_id, const std::vector<GeometryShape>& sources,
                               Point3 plane_origin, Vector3 plane_normal) const {
  if (feature_id.empty())
    return Result<std::vector<GeometryShape>>::failure(
        Error::validation("mirror_feature", "feature id must not be empty"));
  if (sources.empty())
    return Result<std::vector<GeometryShape>>::failure(
        mirror_error(feature_id, "MirrorFeature requires at least one source shape"));
  for (const auto& source : sources)
    if (source.empty())
      return Result<std::vector<GeometryShape>>::failure(
          mirror_error(feature_id, "MirrorFeature source shapes must not be empty"));
  if (!std::isfinite(plane_origin.x) || !std::isfinite(plane_origin.y) ||
      !std::isfinite(plane_origin.z))
    return Result<std::vector<GeometryShape>>::failure(
        mirror_error(feature_id, "MirrorFeature plane origin must be finite"));
  const double magnitude =
      std::sqrt(plane_normal.x * plane_normal.x + plane_normal.y * plane_normal.y +
                plane_normal.z * plane_normal.z);
  if (!std::isfinite(magnitude) || magnitude <= 1.0e-12)
    return Result<std::vector<GeometryShape>>::failure(
        mirror_error(feature_id, "MirrorFeature plane normal must not be zero length"));
  plane_normal = {plane_normal.x / magnitude, plane_normal.y / magnitude,
                  plane_normal.z / magnitude};

  std::vector<GeometryShape> reflected;
  reflected.reserve(sources.size());
  for (const auto& source : sources) {
    auto shape = transform_adapter_.mirror(source, plane_origin, plane_normal);
    if (shape.has_error())
      return Result<std::vector<GeometryShape>>::failure(
          mirror_error(feature_id, shape.error().message()));
    reflected.push_back(std::move(shape.value()));
  }
  return Result<std::vector<GeometryShape>>::success(std::move(reflected));
}

} // namespace blcad::geometry
