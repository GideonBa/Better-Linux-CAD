#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>

namespace blcad {

enum class ParameterType {
  Length,
};

enum class ParameterScope {
  Part,
};

[[nodiscard]] std::string_view to_string(ParameterType type) noexcept;
[[nodiscard]] std::string_view to_string(ParameterScope scope) noexcept;

class Parameter {
public:
  [[nodiscard]] static Result<Parameter> create_length(ParameterId id, std::string name,
                                                       Quantity value,
                                                       ParameterScope scope = ParameterScope::Part);

  // Returns a copy with a new value and the same identity. The new value follows
  // the same validation path as parameter creation.
  [[nodiscard]] Result<Parameter> with_value(Quantity value) const;

  [[nodiscard]] const ParameterId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] ParameterType type() const noexcept;
  [[nodiscard]] ParameterScope scope() const noexcept;
  [[nodiscard]] const Quantity& value() const noexcept;

private:
  Parameter(ParameterId id, std::string name, ParameterType type, Quantity value,
            ParameterScope scope);

  ParameterId id_;
  std::string name_;
  ParameterType type_;
  Quantity value_;
  ParameterScope scope_;
};

} // namespace blcad
