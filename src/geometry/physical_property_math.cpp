#include "physical_property_math.hpp"
#include "occt_failure_message.hpp"

#include <Standard_Failure.hxx>
#include <math_Jacobi.hxx>
#include <math_Matrix.hxx>
#include <math_Vector.hxx>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace blcad::geometry::detail {
namespace {
Error error(std::string message) {
  return Error::geometry("physical.properties", std::move(message));
}
double number(long double value) {
  const double result = static_cast<double>(value);
  if (!std::isfinite(value) || !std::isfinite(result) || (value != 0 && result == 0))
    throw std::runtime_error("Physical property exceeds the representable numeric range");
  return result;
}
void validate(const WeightedPhysicalProperties& value) {
  if (!std::isfinite(value.weight) || value.weight <= 0 || !std::isfinite(value.centroid.x) ||
      !std::isfinite(value.centroid.y) || !std::isfinite(value.centroid.z) ||
      !std::all_of(value.central_inertia.begin(), value.central_inertia.end(),
                   [](double x) { return std::isfinite(x); }))
    throw std::runtime_error(
        "Physical properties require positive finite weight and finite geometry");
}
} // namespace

Result<WeightedPhysicalProperties>
sum_physical_properties(std::span<const WeightedPhysicalProperties> values) {
  try {
    if (values.empty())
      throw std::runtime_error("Cannot aggregate an empty weighted physical scope");
    const auto anchor = values.front().centroid;
    long double weight = 0;
    std::array<long double, 3> offset{};
    for (const auto& value : values) {
      validate(value);
      weight += value.weight;
      offset[0] += value.weight * (static_cast<long double>(value.centroid.x) - anchor.x);
      offset[1] += value.weight * (static_cast<long double>(value.centroid.y) - anchor.y);
      offset[2] += value.weight * (static_cast<long double>(value.centroid.z) - anchor.z);
    }
    const std::array<long double, 3> center{anchor.x + offset[0] / weight,
                                            anchor.y + offset[1] / weight,
                                            anchor.z + offset[2] / weight};
    std::array<long double, 9> tensor{};
    for (const auto& value : values) {
      const std::array<long double, 3> d{value.centroid.x - center[0], value.centroid.y - center[1],
                                         value.centroid.z - center[2]};
      const auto squared = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
      for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
          tensor[3 * i + j] += value.central_inertia[3 * i + j] +
                               value.weight * ((i == j ? squared : 0) - d[i] * d[j]);
    }
    WeightedPhysicalProperties result{
        number(weight), {number(center[0]), number(center[1]), number(center[2])}, {}};
    for (std::size_t i = 0; i < 9; ++i)
      result.central_inertia[i] = number(tensor[i]);
    return Result<WeightedPhysicalProperties>::success(std::move(result));
  } catch (const std::exception& e) {
    return Result<WeightedPhysicalProperties>::failure(error(e.what()));
  }
}

Result<PhysicalMassProperties> physical_mass_properties(const WeightedPhysicalProperties& value) {
  try {
    validate(value);
    math_Matrix matrix(1, 3, 1, 3);
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        matrix(i + 1, j + 1) =
            .5 * value.central_inertia[3 * i + j] + .5 * value.central_inertia[3 * j + i];
    const math_Jacobi eigen(matrix);
    if (!eigen.IsDone())
      throw std::runtime_error("Principal inertia calculation did not converge");
    std::array<std::pair<double, Vector3>, 3> pairs;
    double scale = 0;
    for (int i = 1; i <= 3; ++i) {
      math_Vector axis(1, 3);
      eigen.Vector(i, axis);
      pairs[i - 1] = {eigen.Value(i), {axis(1), axis(2), axis(3)}};
      scale = std::max(scale, std::abs(eigen.Value(i)));
    }
    std::sort(pairs.begin(), pairs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    PhysicalMassProperties result;
    result.mass_kg = value.weight;
    result.centroid_mm = value.centroid;
    result.central_inertia_kg_mm2 = value.central_inertia;
    for (std::size_t i = 0; i < 3; ++i) {
      if (!std::isfinite(pairs[i].first) || pairs[i].first < -scale * 1e-10 ||
          !std::isfinite(length(pairs[i].second)) || length(pairs[i].second) <= 0)
        throw std::runtime_error("Invalid principal inertia result");
      result.principal_moments_kg_mm2[i] = std::max(0., pairs[i].first);
      result.principal_directions[i] = normalized(pairs[i].second);
    }
    result.principal_axes_unique =
        scale > 0 &&
        pairs[1].first - pairs[0].first > scale * physical_principal_axis_ambiguity_tolerance &&
        pairs[2].first - pairs[1].first > scale * physical_principal_axis_ambiguity_tolerance;
    return Result<PhysicalMassProperties>::success(std::move(result));
  } catch (const Standard_Failure& e) {
    return Result<PhysicalMassProperties>::failure(error(standard_failure_message(e)));
  } catch (const std::exception& e) {
    return Result<PhysicalMassProperties>::failure(error(e.what()));
  }
}
Result<PhysicalMassProperties> physical_mass_properties(const VolumePhysicalProperties& value,
                                                        const MassDensity& density) {
  try {
    const long double factor = density.kg_per_mm3();
    WeightedPhysicalProperties weighted{number(value.volume_mm3 * factor), value.centroid_mm, {}};
    for (std::size_t i = 0; i < 9; ++i)
      weighted.central_inertia[i] = number(value.central_inertia_mm5[i] * factor);
    return physical_mass_properties(weighted);
  } catch (const std::exception& e) {
    return Result<PhysicalMassProperties>::failure(error(e.what()));
  }
}
} // namespace blcad::geometry::detail
