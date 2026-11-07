#pragma once

#include "blcad/core/result.hpp"

#include <string_view>

namespace blcad {

class MassDensity {
public:
  [[nodiscard]] static Result<MassDensity>
  from_kg_per_m3(double value, std::string_view object_id = "engineering.density");
  [[nodiscard]] static Result<MassDensity>
  from_g_per_cm3(double value, std::string_view object_id = "engineering.density");
  [[nodiscard]] static Result<MassDensity>
  from_kg_per_mm3(double value, std::string_view object_id = "engineering.density");
  [[nodiscard]] double kg_per_m3() const noexcept {
    return kg_per_m3_;
  }
  [[nodiscard]] double g_per_cm3() const noexcept {
    return kg_per_m3_ * 1e-3;
  }
  [[nodiscard]] double kg_per_mm3() const noexcept {
    return kg_per_m3_ * 1e-9;
  }
  friend bool operator==(const MassDensity&, const MassDensity&) = default;

private:
  explicit MassDensity(double value) : kg_per_m3_(value) {}
  double kg_per_m3_;
};

class EngineeringPressure {
public:
  [[nodiscard]] static Result<EngineeringPressure>
  from_pa(double value, std::string_view object_id = "engineering.pressure");
  [[nodiscard]] static Result<EngineeringPressure>
  from_mpa(double value, std::string_view object_id = "engineering.pressure");
  [[nodiscard]] static Result<EngineeringPressure>
  from_gpa(double value, std::string_view object_id = "engineering.pressure");
  [[nodiscard]] double pascals() const noexcept {
    return pascals_;
  }
  [[nodiscard]] double megapascals() const noexcept {
    return pascals_ * 1e-6;
  }
  [[nodiscard]] double gigapascals() const noexcept {
    return pascals_ * 1e-9;
  }
  friend bool operator==(const EngineeringPressure&, const EngineeringPressure&) = default;

private:
  explicit EngineeringPressure(double value) : pascals_(value) {}
  double pascals_;
};

} // namespace blcad
