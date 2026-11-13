#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {
class PartDocument;
enum class CurveUnit { Scalar, Length, Angle };

struct CurveExpressionUnit {
  int length_power{};
  int angle_power{};
  [[nodiscard]] static constexpr CurveExpressionUnit from(CurveUnit unit) {
    return {unit == CurveUnit::Length ? 1 : 0, unit == CurveUnit::Angle ? 1 : 0};
  }
  friend bool operator==(const CurveExpressionUnit&, const CurveExpressionUnit&) = default;
};

struct CurveVariable {
  std::string name;
  CurveUnit unit{CurveUnit::Scalar};
  friend bool operator==(const CurveVariable&, const CurveVariable&) = default;
};

// Derivatives are with respect to the numeric independent variable, measured in
// its declared canonical unit (mm, radians or scalar). They are disposable data.
struct CurveExpressionJet {
  double value{};
  double first{};
  double second{};
};

// Conservative disposable analysis; interval errors request subdivision or rejection.
struct CurveInterval {
  double lower{}, upper{};
};
struct CurveExpressionBounds {
  CurveInterval value, first, second;
  double rounding_error{};
};
// Exact supported polynomial plus one affine-phase harmonic. Unrecognized
// expressions still use interval-controlled Geometry; they are not approximated here.
struct CurveAnalyticForm {
  std::array<double, 4> polynomial{};
  double frequency{}, cosine{}, sine{};
  std::array<CurveInterval, 4> polynomial_bounds{};
  CurveInterval frequency_bounds{}, cosine_bounds{}, sine_bounds{};
  double frequency_sensitivity{};
};

namespace curve_expression_detail {
struct CurveExpressionData;
}

class CurveExpression {
public:
  [[nodiscard]] static Result<CurveExpression>
  compile(const PartDocument& document, std::string_view source,
          std::optional<CurveVariable> variable = std::nullopt,
          std::string object_id = "curve.expression");

  // Syntax and stable references can be checked before a Sketch belongs to a
  // Part. Parameter existence/types are subsequently checked by compile().
  [[nodiscard]] static Result<std::vector<ParameterId>>
  references(std::string_view source, std::optional<CurveVariable> variable = std::nullopt,
             std::string object_id = "curve.expression");

  [[nodiscard]] CurveExpressionUnit unit() const noexcept;
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept;
  [[nodiscard]] Result<CurveExpressionJet> evaluate(const PartDocument& document,
                                                    double parameter = 0) const;

  [[nodiscard]] Result<CurveExpressionBounds> bounds(const PartDocument& document, double lower,
                                                     double upper) const;
  [[nodiscard]] Result<std::optional<CurveAnalyticForm>>
  analytic_form(const PartDocument& document) const;
  [[nodiscard]] Result<bool> proves_period(const PartDocument& document,
                                           const CurveExpression& lower,
                                           const CurveExpression& upper) const;

  [[nodiscard]] Result<bool> proves_endpoint_identity(const PartDocument& document,
                                                      const CurveExpression& lower,
                                                      const CurveExpression& upper) const;

private:
  explicit CurveExpression(std::shared_ptr<const curve_expression_detail::CurveExpressionData> data)
      : data_(std::move(data)) {}
  std::shared_ptr<const curve_expression_detail::CurveExpressionData> data_;
};
} // namespace blcad
