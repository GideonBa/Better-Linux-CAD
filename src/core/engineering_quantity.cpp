#include "blcad/core/engineering_quantity.hpp"

#include <cmath>
#include <string>

namespace blcad {
namespace {
bool representable_positive(double value) {
  return std::isfinite(value) && value > 0 && std::isfinite(value * 1e-9) && value * 1e-9 > 0;
}
} // namespace

Result<MassDensity> MassDensity::from_kg_per_m3(double value, std::string_view object_id) {
  if (!representable_positive(value))
    return Result<MassDensity>::failure(Error::validation(
        std::string(object_id),
        "density must be positive, finite and representable in kg/m3 and kg/mm3"));
  return Result<MassDensity>::success(MassDensity(value));
}
Result<MassDensity> MassDensity::from_g_per_cm3(double value, std::string_view object_id) {
  return from_kg_per_m3(value * 1e3, object_id);
}
Result<MassDensity> MassDensity::from_kg_per_mm3(double value, std::string_view object_id) {
  return from_kg_per_m3(value * 1e9, object_id);
}
Result<EngineeringPressure> EngineeringPressure::from_pa(double value, std::string_view object_id) {
  if (!representable_positive(value))
    return Result<EngineeringPressure>::failure(
        Error::validation(std::string(object_id),
                          "pressure must be positive, finite and representable in Pa and GPa"));
  return Result<EngineeringPressure>::success(EngineeringPressure(value));
}
Result<EngineeringPressure> EngineeringPressure::from_mpa(double value,
                                                          std::string_view object_id) {
  return from_pa(value * 1e6, object_id);
}
Result<EngineeringPressure> EngineeringPressure::from_gpa(double value,
                                                          std::string_view object_id) {
  return from_pa(value * 1e9, object_id);
}
} // namespace blcad
