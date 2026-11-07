#pragma once

#include "blcad/core/body.hpp"
#include "blcad/core/engineering_resource.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace blcad {
class PartDocument;
}
namespace blcad::geometry {
class GeometryShape;
class StepImportTopologyContext;
class BodyResultInspector;

inline constexpr double physical_relative_integration_tolerance = 1e-9;
inline constexpr double physical_principal_axis_ambiguity_tolerance = 1e-8;

struct VolumePhysicalProperties {
  double volume_mm3 = 0;
  Point3 centroid_mm;
  std::array<double, 9> central_inertia_mm5{};
};
struct ShapePhysicalProperties {
  double area_mm2 = 0;
  std::size_t solid_count = 0;
  std::size_t free_face_count = 0;
  std::optional<VolumePhysicalProperties> volume;
  double estimated_relative_area_error = 0;
  double estimated_relative_volume_error = 0;
};
struct PhysicalMassProperties {
  double mass_kg = 0;
  Point3 centroid_mm;
  std::array<double, 9> central_inertia_kg_mm2{};
  std::array<double, 3> principal_moments_kg_mm2{};
  std::array<Vector3, 3> principal_directions{};
  bool principal_axes_unique = false;
};
enum class PhysicalMassStatus { NotApplicable, Unavailable, Partial, Complete };
enum class BodyPhysicalMassStatus { NotSolid, UnassignedMaterial, MissingDensity, Known };
struct BodyPhysicalProperties {
  std::optional<BodyId> body;
  std::string name;
  BodyKind kind = BodyKind::Solid;
  BodyVisibility visibility = BodyVisibility::Visible;
  ShapePhysicalProperties geometry;
  std::optional<EngineeringResourceKey> material;
  BodyPhysicalMassStatus mass_status = BodyPhysicalMassStatus::NotSolid;
  std::optional<PhysicalMassProperties> mass;
};
struct PartPhysicalPropertiesData {
  std::vector<BodyPhysicalProperties> bodies;
  double area_mm2 = 0;
  double volume_mm3 = 0;
  std::optional<Point3> volume_centroid_mm;
  std::size_t solid_body_count = 0;
  std::size_t surface_body_count = 0;
  std::size_t missing_density_body_count = 0;
  PhysicalMassStatus mass_status = PhysicalMassStatus::NotApplicable;
  std::optional<PhysicalMassProperties> known_mass;
  double estimated_relative_area_error = 0;
  double estimated_relative_volume_error = 0;
  [[nodiscard]] const PhysicalMassProperties* complete_mass() const noexcept {
    return mass_status == PhysicalMassStatus::Complete && known_mass ? &*known_mass : nullptr;
  }
};

// Raw shape integration makes no document freshness claim.
[[nodiscard]] Result<ShapePhysicalProperties>
inspect_shape_physical_properties(const GeometryShape& shape);

class PartPhysicalProperties {
public:
  [[nodiscard]] const PartPhysicalPropertiesData& data() const noexcept {
    return data_;
  }
  [[nodiscard]] Result<std::size_t>
  validate_current(const PartDocument& document,
                   StepImportTopologyContext* imports = nullptr) const;

private:
  friend class BodyResultInspector;
  PartPhysicalProperties(PartPhysicalPropertiesData data, std::string source,
                         std::uint64_t generation);
  PartPhysicalPropertiesData data_;
  std::string source_;
  std::uint64_t generation_;
};
} // namespace blcad::geometry
