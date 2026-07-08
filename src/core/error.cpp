#include "blcad/core/error.hpp"

#include <utility>

namespace blcad {

std::string_view to_string(ErrorCategory category) noexcept {
  switch (category) {
  case ErrorCategory::Validation:
    return "validation";
  case ErrorCategory::Dependency:
    return "dependency";
  case ErrorCategory::Geometry:
    return "geometry";
  case ErrorCategory::Export:
    return "export";
  case ErrorCategory::Internal:
    return "internal";
  }

  return "internal";
}

Error::Error(ErrorCategory category, std::string object_id, std::string message)
    : category_(category), object_id_(std::move(object_id)), message_(std::move(message)) {}

Error Error::validation(std::string object_id, std::string message) {
  return Error(ErrorCategory::Validation, std::move(object_id), std::move(message));
}

Error Error::dependency(std::string object_id, std::string message) {
  return Error(ErrorCategory::Dependency, std::move(object_id), std::move(message));
}

Error Error::geometry(std::string object_id, std::string message) {
  return Error(ErrorCategory::Geometry, std::move(object_id), std::move(message));
}

Error Error::export_error(std::string object_id, std::string message) {
  return Error(ErrorCategory::Export, std::move(object_id), std::move(message));
}

Error Error::internal(std::string object_id, std::string message) {
  return Error(ErrorCategory::Internal, std::move(object_id), std::move(message));
}

ErrorCategory Error::category() const noexcept {
  return category_;
}

const std::string& Error::object_id() const noexcept {
  return object_id_;
}

const std::string& Error::message() const noexcept {
  return message_;
}

} // namespace blcad
