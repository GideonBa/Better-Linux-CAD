#include "blcad/core/assembly_reference_target.hpp"

#include "blcad/core/error.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace blcad {
namespace {

constexpr std::string_view kReferencePrefix = "ref:";
constexpr std::string_view kDatumPlaneToken = "datum_plane";
constexpr std::string_view kDatumAxisToken = "datum_axis";
constexpr std::string_view kConstructionLineToken = "construction_line";
constexpr std::string_view kConstructionPointToken = "construction_point";

[[nodiscard]] Error spelling_error(std::string_view spelling, std::string message) {
  return Error::validation(std::string(spelling), std::move(message));
}

[[nodiscard]] bool is_unreserved(unsigned char byte) noexcept {
  return (byte >= static_cast<unsigned char>('A') && byte <= static_cast<unsigned char>('Z')) ||
         (byte >= static_cast<unsigned char>('a') && byte <= static_cast<unsigned char>('z')) ||
         (byte >= static_cast<unsigned char>('0') && byte <= static_cast<unsigned char>('9')) ||
         byte == static_cast<unsigned char>('_') || byte == static_cast<unsigned char>('-');
}

[[nodiscard]] std::string encode_reference_id(const std::string& value) {
  constexpr char kHexDigits[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size());
  for (const unsigned char byte : value) {
    if (is_unreserved(byte)) {
      encoded.push_back(static_cast<char>(byte));
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(kHexDigits[(byte >> 4U) & 0x0FU]);
    encoded.push_back(kHexDigits[byte & 0x0FU]);
  }
  return encoded;
}

[[nodiscard]] std::optional<unsigned char> decode_hex_digit(char digit) noexcept {
  if (digit >= '0' && digit <= '9') {
    return static_cast<unsigned char>(digit - '0');
  }
  if (digit >= 'A' && digit <= 'F') {
    return static_cast<unsigned char>(10 + (digit - 'A'));
  }
  return std::nullopt;
}

[[nodiscard]] Result<std::string> decode_reference_id(std::string_view spelling,
                                                      std::string_view encoded) {
  std::string decoded;
  decoded.reserve(encoded.size());
  for (std::size_t index = 0U; index < encoded.size(); ++index) {
    const char byte = encoded[index];
    if (byte == '%') {
      if (index + 2U >= encoded.size()) {
        return Result<std::string>::failure(
            spelling_error(spelling, "assembly reference target escape sequence is truncated"));
      }
      const auto high = decode_hex_digit(encoded[index + 1U]);
      const auto low = decode_hex_digit(encoded[index + 2U]);
      if (!high.has_value() || !low.has_value()) {
        return Result<std::string>::failure(spelling_error(
            spelling,
            "assembly reference target escape sequence must use two uppercase hex digits"));
      }
      const auto decoded_byte = static_cast<unsigned char>((high.value() << 4U) | low.value());
      if (is_unreserved(decoded_byte)) {
        return Result<std::string>::failure(spelling_error(
            spelling, "assembly reference target spelling must not escape unreserved id bytes"));
      }
      decoded.push_back(static_cast<char>(decoded_byte));
      index += 2U;
      continue;
    }
    if (!is_unreserved(static_cast<unsigned char>(byte))) {
      return Result<std::string>::failure(spelling_error(
          spelling, "assembly reference target id byte must be unreserved or escaped"));
    }
    decoded.push_back(byte);
  }
  if (decoded.empty()) {
    return Result<std::string>::failure(
        spelling_error(spelling, "assembly reference target id must not be empty"));
  }
  return Result<std::string>::success(std::move(decoded));
}

[[nodiscard]] std::string_view family_token(AssemblyReferenceTargetFamily family) noexcept {
  switch (family) {
  case AssemblyReferenceTargetFamily::DatumPlane:
    return kDatumPlaneToken;
  case AssemblyReferenceTargetFamily::DatumAxis:
    return kDatumAxisToken;
  case AssemblyReferenceTargetFamily::ConstructionLine:
    return kConstructionLineToken;
  case AssemblyReferenceTargetFamily::ConstructionPoint:
    return kConstructionPointToken;
  }
  return "unknown";
}

[[nodiscard]] std::optional<AssemblyReferenceTargetFamily>
parse_family_token(std::string_view token) noexcept {
  if (token == kDatumPlaneToken) {
    return AssemblyReferenceTargetFamily::DatumPlane;
  }
  if (token == kDatumAxisToken) {
    return AssemblyReferenceTargetFamily::DatumAxis;
  }
  if (token == kConstructionLineToken) {
    return AssemblyReferenceTargetFamily::ConstructionLine;
  }
  if (token == kConstructionPointToken) {
    return AssemblyReferenceTargetFamily::ConstructionPoint;
  }
  return std::nullopt;
}

[[nodiscard]] const std::string& identity_value(const AssemblyReferenceTargetIdentity& identity) {
  return std::visit([](const auto& id) -> const std::string& { return id.value(); }, identity);
}

[[nodiscard]] AssemblyReferenceTargetIdentity make_identity(AssemblyReferenceTargetFamily family,
                                                            std::string id) {
  switch (family) {
  case AssemblyReferenceTargetFamily::DatumPlane:
    return DatumPlaneId(std::move(id));
  case AssemblyReferenceTargetFamily::DatumAxis:
    return DatumAxisId(std::move(id));
  case AssemblyReferenceTargetFamily::ConstructionLine:
    return ConstructionLineId(std::move(id));
  case AssemblyReferenceTargetFamily::ConstructionPoint:
    return ConstructionPointId(std::move(id));
  }
  return DatumPlaneId(std::move(id));
}

} // namespace

std::string_view to_string(AssemblyReferenceTargetFamily family) noexcept {
  switch (family) {
  case AssemblyReferenceTargetFamily::DatumPlane:
    return "DatumPlane";
  case AssemblyReferenceTargetFamily::DatumAxis:
    return "DatumAxis";
  case AssemblyReferenceTargetFamily::ConstructionLine:
    return "ConstructionLine";
  case AssemblyReferenceTargetFamily::ConstructionPoint:
    return "ConstructionPoint";
  }
  return "Unknown";
}

AssemblyReferenceTargetFamily
assembly_reference_target_family(const AssemblyReferenceTargetIdentity& identity) noexcept {
  switch (identity.index()) {
  case 0U:
    return AssemblyReferenceTargetFamily::DatumPlane;
  case 1U:
    return AssemblyReferenceTargetFamily::DatumAxis;
  case 2U:
    return AssemblyReferenceTargetFamily::ConstructionLine;
  default:
    return AssemblyReferenceTargetFamily::ConstructionPoint;
  }
}

Result<std::string>
make_assembly_reference_target_spelling(const AssemblyReferenceTargetIdentity& identity) {
  const std::string& id = identity_value(identity);
  if (id.empty()) {
    return Result<std::string>::failure(Error::validation(
        "assembly_reference_target", "assembly reference target id must not be empty"));
  }
  const AssemblyReferenceTargetFamily family = assembly_reference_target_family(identity);
  std::string spelling(kReferencePrefix);
  spelling += family_token(family);
  spelling += ':';
  spelling += encode_reference_id(id);
  return Result<std::string>::success(std::move(spelling));
}

Result<AssemblyReferenceTargetIdentity>
parse_assembly_reference_target_spelling(std::string_view spelling) {
  if (!is_assembly_reference_target_spelling(spelling)) {
    return Result<AssemblyReferenceTargetIdentity>::failure(spelling_error(
        spelling, "assembly reference target spelling must start with the ref: prefix"));
  }

  const std::string_view remainder = spelling.substr(kReferencePrefix.size());
  const std::size_t separator = remainder.find(':');
  if (separator == std::string_view::npos) {
    return Result<AssemblyReferenceTargetIdentity>::failure(spelling_error(
        spelling, "assembly reference target spelling must carry a family and an id"));
  }

  const auto family = parse_family_token(remainder.substr(0U, separator));
  if (!family.has_value()) {
    return Result<AssemblyReferenceTargetIdentity>::failure(
        spelling_error(spelling, "assembly reference target family is unsupported"));
  }

  auto decoded = decode_reference_id(spelling, remainder.substr(separator + 1U));
  if (decoded.has_error()) {
    return Result<AssemblyReferenceTargetIdentity>::failure(decoded.error());
  }
  return Result<AssemblyReferenceTargetIdentity>::success(
      make_identity(family.value(), std::move(decoded.value())));
}

bool is_assembly_reference_target_spelling(std::string_view spelling) noexcept {
  return spelling.size() > kReferencePrefix.size() &&
         spelling.substr(0U, kReferencePrefix.size()) == kReferencePrefix;
}

} // namespace blcad
