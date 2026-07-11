#include "blcad/core/parameter.hpp"

#include <utility>

namespace blcad {

std::string_view to_string(ParameterType type) noexcept {
  switch (type) {
  case ParameterType::Length:
    return "length";
  case ParameterType::Count:
    return "count";
  }

  return "length";
}

std::string_view to_string(ParameterScope scope) noexcept {
  switch (scope) {
  case ParameterScope::Part:
    return "part";
  case ParameterScope::Assembly:
    return "assembly";
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

Result<Parameter> Parameter::create_count(ParameterId id, std::string name, Quantity value,
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

  if (!value.is_valid_count()) {
    return Result<Parameter>::failure(
        Error::validation(object_id, "count must be a whole number of at least 1"));
  }

  return Result<Parameter>::success(
      Parameter(std::move(id), std::move(name), ParameterType::Count, value, scope));
}

Result<Parameter> Parameter::create_expression(ParameterId id, std::string name, ParameterType type,
                                               std::string formula, Quantity evaluated_value,
                                               ParameterScope scope) {
  const auto object_id = id.empty() ? std::string("parameter") : id.value();

  if (formula.empty()) {
    return Result<Parameter>::failure(
        Error::validation(object_id, "expression parameter formula must not be empty"));
  }

  auto plain = type == ParameterType::Count
                   ? create_count(std::move(id), std::move(name), evaluated_value, scope)
                   : create_length(std::move(id), std::move(name), evaluated_value, scope);
  if (plain.has_error()) {
    return plain;
  }

  Parameter parameter = std::move(plain.value());
  parameter.formula_ = std::move(formula);
  return Result<Parameter>::success(std::move(parameter));
}

Result<Parameter> Parameter::with_value(Quantity value) const {
  auto updated = type_ == ParameterType::Count ? create_count(id_, name_, value, scope_)
                                               : create_length(id_, name_, value, scope_);
  if (updated.has_error()) {
    return updated;
  }
  updated.value().formula_ = formula_;
  return updated;
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

const std::optional<std::string>& Parameter::formula() const noexcept {
  return formula_;
}

bool Parameter::is_expression() const noexcept {
  return formula_.has_value();
}

Parameter::Parameter(ParameterId id, std::string name, ParameterType type, Quantity value,
                     ParameterScope scope, std::optional<std::string> formula)
    : id_(std::move(id)), name_(std::move(name)), type_(type), value_(value), scope_(scope),
      formula_(std::move(formula)) {}

} // namespace blcad
