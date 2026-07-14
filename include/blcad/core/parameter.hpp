#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace blcad {

enum class ParameterType {
  Length,
  Count,
  Angle,
};

enum class ParameterScope {
  Part,
  Assembly,
};

[[nodiscard]] std::string_view to_string(ParameterType type) noexcept;
[[nodiscard]] std::string_view to_string(ParameterScope scope) noexcept;

class Parameter {
public:
  [[nodiscard]] static Result<Parameter> create_length(ParameterId id, std::string name,
                                                       Quantity value,
                                                       ParameterScope scope = ParameterScope::Part);
  // A dimensionless positive integer parameter, for example a hole count.
  [[nodiscard]] static Result<Parameter> create_count(ParameterId id, std::string name,
                                                      Quantity value,
                                                      ParameterScope scope = ParameterScope::Part);
  [[nodiscard]] static Result<Parameter> create_angle(ParameterId id, std::string name,
                                                      Quantity value,
                                                      ParameterScope scope = ParameterScope::Part);

  // A computed parameter: the value is derived from a unit-aware formula over
  // other parameters of the same document. The already-evaluated value is
  // passed in; evaluation itself lives in ParameterExpressionEvaluator.
  [[nodiscard]] static Result<Parameter>
  create_expression(ParameterId id, std::string name, ParameterType type, std::string formula,
                    Quantity evaluated_value, ParameterScope scope = ParameterScope::Part);

  // Returns a copy with a new value and the same identity. The new value follows
  // the same validation path as parameter creation.
  [[nodiscard]] Result<Parameter> with_value(Quantity value) const;

  [[nodiscard]] const ParameterId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] ParameterType type() const noexcept;
  [[nodiscard]] ParameterScope scope() const noexcept;
  [[nodiscard]] const Quantity& value() const noexcept;
  [[nodiscard]] const std::optional<std::string>& formula() const noexcept;
  [[nodiscard]] bool is_expression() const noexcept;

private:
  Parameter(ParameterId id, std::string name, ParameterType type, Quantity value,
            ParameterScope scope, std::optional<std::string> formula = std::nullopt);

  ParameterId id_;
  std::string name_;
  ParameterType type_;
  Quantity value_;
  ParameterScope scope_;
  std::optional<std::string> formula_;
};

} // namespace blcad
