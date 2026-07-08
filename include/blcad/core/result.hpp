#pragma once

#include "blcad/core/error.hpp"

#include <utility>
#include <variant>

namespace blcad {

template <typename T> class Result {
public:
  [[nodiscard]] static Result success(T value) {
    return Result(std::move(value));
  }

  [[nodiscard]] static Result failure(Error error) {
    return Result(std::move(error));
  }

  [[nodiscard]] bool has_value() const noexcept {
    return std::holds_alternative<T>(data_);
  }
  [[nodiscard]] bool has_error() const noexcept {
    return std::holds_alternative<Error>(data_);
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return has_value();
  }

  [[nodiscard]] const T& value() const {
    return std::get<T>(data_);
  }
  [[nodiscard]] T& value() {
    return std::get<T>(data_);
  }

  [[nodiscard]] const Error& error() const {
    return std::get<Error>(data_);
  }

private:
  explicit Result(T value) : data_(std::move(value)) {}
  explicit Result(Error error) : data_(std::move(error)) {}

  std::variant<T, Error> data_;
};

} // namespace blcad
