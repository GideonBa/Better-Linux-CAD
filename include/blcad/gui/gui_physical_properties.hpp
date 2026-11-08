#pragma once

#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <variant>

namespace blcad::gui {
enum class GuiPhysicalLengthUnit { Millimeter, Centimeter, Meter };
enum class GuiPhysicalMassUnit { Kilogram, Gram };
struct GuiPhysicalUnits {
  GuiPhysicalLengthUnit length = GuiPhysicalLengthUnit::Millimeter;
  GuiPhysicalMassUnit mass = GuiPhysicalMassUnit::Kilogram;
  friend bool operator==(const GuiPhysicalUnits&, const GuiPhysicalUnits&) = default;
};
// IDs/path select a row from the trusted snapshot. Supplied poses never drive a query.
struct GuiPhysicalScope {
  std::optional<BodyId> body;
  std::optional<AssemblyLeafOccurrenceDescriptor> occurrence;
};
struct GuiPhysicalScopeChoice {
  GuiPhysicalScope scope;
  std::string label;
};
struct GuiPhysicalMassView {
  double mass = 0;
  Point3 centroid;
  std::array<double, 9> central_inertia{};
  std::array<double, 3> principal_moments{};
  std::array<Vector3, 3> principal_directions{};
  bool principal_axes_unique = false;
};
struct GuiPhysicalMaterialView {
  EngineeringResourceKey key;
  std::string name;
  std::optional<double> density;
  std::size_t body_count = 0;
};
struct GuiPhysicalPropertiesView {
  std::string scope_label, coordinate_frame;
  std::string length_unit, mass_unit, area_unit, volume_unit, inertia_unit, density_unit;
  double area = 0, volume = 0;
  std::optional<Point3> volume_centroid;
  geometry::PhysicalMassStatus mass_status = geometry::PhysicalMassStatus::NotApplicable;
  std::optional<GuiPhysicalMassView> known_mass;
  std::size_t solid_body_count = 0, surface_body_count = 0, missing_density_body_count = 0;
  std::size_t unassigned_body_count = 0;
  std::vector<GuiPhysicalMaterialView> materials;
  double requested_relative_precision = geometry::physical_relative_integration_tolerance;
  double estimated_relative_area_error = 0, estimated_relative_volume_error = 0;
  // Canonical frame/units for the transient centroid overlay, never display-scaled.
  geometry::PhysicalPropertiesTotals canonical;
};

class GuiPhysicalPropertiesController {
public:
  static constexpr std::string_view task_id = "inspect.physical_properties";
  [[nodiscard]] Result<std::size_t> begin(const GuiDocumentSession&);
  [[nodiscard]] bool active() const noexcept {
    return active_;
  }
  [[nodiscard]] Result<std::size_t> refresh(const GuiDocumentSession&);
  [[nodiscard]] Result<std::size_t> set_units(GuiPhysicalUnits);
  [[nodiscard]] GuiPhysicalUnits units() const noexcept {
    return units_;
  }
  [[nodiscard]] Result<std::size_t> select_scope(const GuiDocumentSession&, GuiPhysicalScope);
  [[nodiscard]] Result<std::vector<GuiPhysicalScopeChoice>> scopes(const GuiDocumentSession&) const;
  [[nodiscard]] Result<GuiPhysicalPropertiesView> view(const GuiDocumentSession&) const;
  [[nodiscard]] Result<std::string> clipboard_text(const GuiDocumentSession&) const;
  [[nodiscard]] bool geometry_reused() const noexcept {
    return geometry_reused_;
  }
  void cancel() noexcept;

private:
  using Snapshot = std::variant<std::monostate, geometry::PartPhysicalProperties,
                                geometry::ProjectPhysicalProperties>;
  struct Selection {
    geometry::PhysicalPropertiesTotals totals;
    std::string label;
    std::vector<const geometry::BodyPhysicalProperties*> bodies;
  };
  [[nodiscard]] Result<std::size_t> validate(const GuiDocumentSession&) const;
  [[nodiscard]] Result<Selection> selected(const GuiDocumentSession&,
                                           const GuiPhysicalScope&) const;
  Snapshot snapshot_;
  GuiPhysicalUnits units_;
  GuiPhysicalScope scope_;
  std::size_t revision_ = 0;
  bool active_ = false, current_ = false, geometry_reused_ = false;
};
} // namespace blcad::gui
