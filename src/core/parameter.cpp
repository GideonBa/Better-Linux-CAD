#include "blcad/core/parameter.hpp"

#include <utility>

namespace blcad {

std::string_view to_string(ParameterType type) noexcept {
  switch (type) {
  case ParameterType::Length:
    return "length";
  }

  return "length";
}

std::string_view to_string(ParameterScope scope) noexcept {
  switch (scope) {
  case ParameterScope::Part:
    return "part";
  }

  return "part";
}

Result<Parameter> Parameter::create_length(ParameterId id, std::string name, Quantity value,
                                           ParameterScope scope) {
  const auto object_id = id.empty() ? std::string("parameter") : id.value();

  if (id.empty()) {
    return Result<Parameter>::failure(
        Error::validation(object_id, "parameter id must not be empty"));
  }

  if (name.empty()) {
    return Result<Parameter>::failure(
        Error::validation(object_id, "parameter name must not be empty"));
  }

  if (!value.is_positive_length()) {
    return Result<Parameter>::failure(
        Error::validation(object_id, "length must be greater than 0 mm"));
  }

  return Result<Parameter>::success(
      Parameter(std::move(id), std::move(name), ParameterType::Length, value, scope));
}

Result<Parameter> Parameter::with_value(Quantity value) const {
  switch (type_) {
  case ParameterType::Length:
    return create_length(id_, name_, value, scope_);
  }

  return create_length(id_, name_, value, scope_);
}

const ParameterId& Parameter::id() const noexcept {
  return id_;
}

const std::string& Parameter::name() const noexcept {
  return name_;
}

ParameterType Parameter::type() const noexcept {
  return type_;
}

ParameterScope Parameter::scope() const noexcept {
  return scope_;
}

const Quantity& Parameter::value() const noexcept {
  return value_;
}

Parameter::Parameter(ParameterId id, std::string name, ParameterType type, Quantity value,
                     ParameterScope scope)
    : id_(std::move(id)), name_(std::move(name)), type_(type), value_(value), scope_(scope) {}

} // namespace blcad
