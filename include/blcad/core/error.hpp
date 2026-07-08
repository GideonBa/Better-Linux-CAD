#pragma once

#include <string>
#include <string_view>

namespace blcad {

enum class ErrorCategory {
  Validation,
  Dependency,
  Geometry,
  Export,
  Internal,
};

[[nodiscard]] std::string_view to_string(ErrorCategory category) noexcept;

class Error {
public:
  Error(ErrorCategory category, std::string object_id, std::string message);

  [[nodiscard]] static Error validation(std::string object_id, std::string message);
  [[nodiscard]] static Error dependency(std::string object_id, std::string message);
  [[nodiscard]] static Error geometry(std::string object_id, std::string message);
  [[nodiscard]] static Error export_error(std::string object_id, std::string message);
  [[nodiscard]] static Error internal(std::string object_id, std::string message);

  [[nodiscard]] ErrorCategory category() const noexcept;
  [[nodiscard]] const std::string& object_id() const noexcept;
  [[nodiscard]] const std::string& message() const noexcept;

private:
  ErrorCategory category_;
  std::string object_id_;
  std::string message_;
};

} // namespace blcad
