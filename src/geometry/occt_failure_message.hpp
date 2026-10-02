#pragma once

#include <Standard_Failure.hxx>

#include <string>
#include <string_view>

namespace blcad::geometry {

// OCCT raises `Standard_Failure` with an optional message. Every Geometry
// translation unit that catches one needs the same fallback, so the wording of a
// missing OCCT message lives here once. Pass `fallback` where a caller wants
// operation-specific wording.
[[nodiscard]] inline std::string
standard_failure_message(const Standard_Failure& failure,
                         std::string_view fallback = "OCCT operation failed") {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0')
    return std::string(fallback);
  return message;
}

} // namespace blcad::geometry
