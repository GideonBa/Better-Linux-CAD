#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace blcad {

// The first frozen DatumAxis definition families. Explicit axes carry their
// own origin/direction intent; construction-line-derived axes reuse one owned
// ConstructionLine identity and resolve geometry later in the Geometry layer.
enum class DatumAxisKind {
  Explicit,
  FromConstructionLine,
};

[[nodiscard]] std::string_view to_string(DatumAxisKind kind) noexcept;

class DatumAxis {
public:
  [[nodiscard]] static Result<DatumAxis>
  create_explicit(DatumAxisId id, std::string name, Point3 origin, Vector3 direction,
                  std::vector<ParameterId> parameter_dependencies = {});

  [[nodiscard]] static Result<DatumAxis>
  create_from_construction_line(DatumAxisId id, std::string name,
                                ConstructionLineId source_construction_line);

  [[nodiscard]] const DatumAxisId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] DatumAxisKind kind() const noexcept;
  // Origin and direction are authored intent for Explicit axes only.
  // FromConstructionLine axes store zeroed placeholders; derived geometry is
  // resolved later at the Geometry target-resolution boundary.
  [[nodiscard]] Point3 origin() const noexcept;
  [[nodiscard]] Vector3 direction() const noexcept;
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept;
  [[nodiscard]] const ConstructionLineId& source_construction_line() const noexcept;

private:
  DatumAxis(DatumAxisId id, std::string name, DatumAxisKind kind, Point3 origin, Vector3 direction,
            std::vector<ParameterId> parameter_dependencies,
            ConstructionLineId source_construction_line);

  DatumAxisId id_;
  std::string name_;
  DatumAxisKind kind_;
  Point3 origin_;
  Vector3 direction_;
  std::vector<ParameterId> parameter_dependencies_;
  ConstructionLineId source_construction_line_;
};

} // namespace blcad
