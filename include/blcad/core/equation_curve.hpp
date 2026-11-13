#pragma once

#include "blcad/core/curve_expression.hpp"
#include "blcad/core/spatial.hpp"

#include <array>
#include <utility>

namespace blcad {
enum class EquationCurveMode { Cartesian2D, Polar2D, Parametric2D, Parametric3D };
enum class EquationCurveClosure { Open, Periodic };
[[nodiscard]] std::string_view to_string(CurveUnit unit) noexcept;
[[nodiscard]] std::string_view to_string(EquationCurveMode mode) noexcept;
[[nodiscard]] std::string_view to_string(EquationCurveClosure closure) noexcept;

class EquationCurve {
public:
  [[nodiscard]] static Result<EquationCurve>
  create(SketchEntityId id, EquationCurveMode mode, CurveVariable variable, std::string lower,
         std::string upper, std::array<std::string, 3> coordinates, double tolerance_mm = 0.001,
         EquationCurveClosure closure = EquationCurveClosure::Open, bool construction = false);

  [[nodiscard]] const SketchEntityId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] EquationCurveMode mode() const noexcept {
    return mode_;
  }
  [[nodiscard]] const CurveVariable& variable() const noexcept {
    return variable_;
  }
  [[nodiscard]] const std::string& lower() const noexcept {
    return lower_;
  }
  [[nodiscard]] const std::string& upper() const noexcept {
    return upper_;
  }
  [[nodiscard]] const std::array<std::string, 3>& coordinates() const noexcept {
    return coordinates_;
  }
  [[nodiscard]] static constexpr std::string_view policy() noexcept {
    return "adaptive_v1";
  }
  friend bool operator==(const EquationCurve&, const EquationCurve&) = default;
  [[nodiscard]] double tolerance_mm() const noexcept {
    return tolerance_mm_;
  }
  [[nodiscard]] EquationCurveClosure closure() const noexcept {
    return closure_;
  }
  [[nodiscard]] bool construction() const noexcept {
    return construction_;
  }
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept {
    return dependencies_;
  }
  [[nodiscard]] EquationCurve with_construction(bool value) const {
    auto copy = *this;
    copy.construction_ = value;
    return copy;
  }

private:
  EquationCurve(SketchEntityId id, EquationCurveMode mode, CurveVariable variable,
                std::string lower, std::string upper, std::array<std::string, 3> coordinates,
                double tolerance_mm, EquationCurveClosure closure, bool construction,
                std::vector<ParameterId> dependencies)
      : id_(std::move(id)), mode_(mode), variable_(std::move(variable)), lower_(std::move(lower)),
        upper_(std::move(upper)), coordinates_(std::move(coordinates)), tolerance_mm_(tolerance_mm),
        closure_(closure), construction_(construction), dependencies_(std::move(dependencies)) {}
  SketchEntityId id_;
  EquationCurveMode mode_;
  CurveVariable variable_;
  std::string lower_, upper_;
  std::array<std::string, 3> coordinates_;
  double tolerance_mm_;
  EquationCurveClosure closure_;
  bool construction_;
  std::vector<ParameterId> dependencies_;
};

struct EquationCurveJet {
  Point3 value, first, second;
};
struct EquationCurveIntervalAnalysis {
  double maximum_second_derivative{}, minimum_speed{}, maximum_speed{}, rounding_error_mm{};
};
class EquationCurveProgram {
public:
  [[nodiscard]] static Result<EquationCurveProgram> compile(const PartDocument& document,
                                                            const EquationCurve& curve);
  [[nodiscard]] Result<std::pair<double, double>> domain(const PartDocument& document) const;
  [[nodiscard]] Result<EquationCurveJet> evaluate(const PartDocument& document,
                                                  double parameter) const;
  [[nodiscard]] Result<EquationCurveIntervalAnalysis>
  analyze_interval(const PartDocument& document, double lower, double upper) const;
  [[nodiscard]] Result<bool> proves_closure(const PartDocument& document) const;
  [[nodiscard]] const EquationCurve& curve() const noexcept {
    return curve_;
  }

private:
  EquationCurveProgram(EquationCurve curve, CurveExpression lower, CurveExpression upper,
                       std::array<std::optional<CurveExpression>, 3> coordinates)
      : curve_(std::move(curve)), lower_(std::move(lower)), upper_(std::move(upper)),
        coordinates_(std::move(coordinates)) {}
  EquationCurve curve_;
  CurveExpression lower_, upper_;
  std::array<std::optional<CurveExpression>, 3> coordinates_;
};
} // namespace blcad
