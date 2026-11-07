#pragma once

#include "blcad/geometry/physical_properties.hpp"

#include <span>

namespace blcad::geometry::detail {
struct WeightedPhysicalProperties {
  double weight;
  Point3 centroid;
  std::array<double, 9> central_inertia;
};
[[nodiscard]] Result<WeightedPhysicalProperties>
sum_physical_properties(std::span<const WeightedPhysicalProperties> values);
[[nodiscard]] Result<PhysicalMassProperties>
physical_mass_properties(const WeightedPhysicalProperties& value);
[[nodiscard]] Result<PhysicalMassProperties>
physical_mass_properties(const VolumePhysicalProperties& value, const MassDensity& density);
[[nodiscard]] Result<PartPhysicalPropertiesData>
aggregate_physical_bodies(std::vector<BodyPhysicalProperties> bodies);
} // namespace blcad::geometry::detail
