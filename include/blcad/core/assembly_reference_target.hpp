#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>
#include <variant>

namespace blcad {

// Block-32 assembly-selectable reference geometry semantic sources. The
// identity reuses the owning PartDocument's persistent typed ids; no resolved
// Plane/Axis/Line/Point geometry is copied into assembly records.
enum class AssemblyReferenceTargetFamily {
  DatumPlane,
  DatumAxis,
  ConstructionLine,
  ConstructionPoint,
};

[[nodiscard]] std::string_view to_string(AssemblyReferenceTargetFamily family) noexcept;

using AssemblyReferenceTargetIdentity =
    std::variant<DatumPlaneId, DatumAxisId, ConstructionLineId, ConstructionPointId>;

[[nodiscard]] AssemblyReferenceTargetFamily
assembly_reference_target_family(const AssemblyReferenceTargetIdentity& identity) noexcept;

// Canonical persistent spelling:
//
//   ref:<family>:<encoded-id>
//
// with family tokens datum_plane / datum_axis / construction_line /
// construction_point. Every id byte outside [A-Za-z0-9_-] is escaped as %HH
// with uppercase hex. Because '.' is always escaped, a valid reference
// spelling never contains a dot, while every feature target spelling
// (<feature-id>.<role>) contains at least one; the two grammars therefore
// accept provably disjoint string sets and arbitrary ids containing '.', '/',
// ':', or '%' stay unambiguous. Each identity has exactly one canonical
// spelling, so endpoint strings remain byte-comparable.
[[nodiscard]] Result<std::string>
make_assembly_reference_target_spelling(const AssemblyReferenceTargetIdentity& identity);

[[nodiscard]] Result<AssemblyReferenceTargetIdentity>
parse_assembly_reference_target_spelling(std::string_view spelling);

// True when the string carries the reserved reference prefix. It does not
// imply the spelling is well formed; parsing still fails closed.
[[nodiscard]] bool is_assembly_reference_target_spelling(std::string_view spelling) noexcept;

} // namespace blcad
