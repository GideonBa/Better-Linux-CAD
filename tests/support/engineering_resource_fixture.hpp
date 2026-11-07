#pragma once

#include "blcad/core/engineering_resource.hpp"
#include <stdexcept>

namespace blcad::test::engineering_fixture {
template <typename T> T checked(Result<T> value) {
  if (!value)
    throw std::runtime_error(value.error().message());
  return std::move(value.value());
}
inline EngineeringProvenance provenance() {
  return checked(EngineeringProvenance::create(EngineeringDataOrigin::UserDefined,
                                               "Synthetic resource fixture",
                                               "Synthetic revision; no normative values"));
}
inline EngineeringResourceKey key(std::string id = "synthetic.material",
                                  std::string revision = "r1") {
  return checked(
      EngineeringResourceKey::create(EngineeringResourceId(std::move(id)), std::move(revision)));
}
inline EngineeringMaterial material(std::string revision = "r1", double density = 4000) {
  EngineeringMaterialProperties props;
  props.density =
      SourcedEngineeringValue{checked(MassDensity::from_kg_per_m3(density)), provenance()};
  props.young_modulus =
      SourcedEngineeringValue{checked(EngineeringPressure::from_gpa(100)), provenance()};
  props.yield_strength =
      SourcedEngineeringValue{checked(EngineeringPressure::from_mpa(40)), provenance()};
  props.tensile_strength =
      SourcedEngineeringValue{checked(EngineeringPressure::from_mpa(60)), provenance()};
  return checked(EngineeringMaterial::create(key("synthetic.material", std::move(revision)),
                                             "Synthetic material", provenance(), props));
}
inline EngineeringStandardTable table() {
  using K = EngineeringTableValueKind;
  std::vector<EngineeringTableColumn> cols{{"length", "Length", K::Length},
                                           {"offset", "Offset", K::LinearDisplacement},
                                           {"angle", "Angle", K::Angle},
                                           {"count", "Count", K::Count},
                                           {"density", "Density", K::Density},
                                           {"pressure", "Pressure", K::Pressure},
                                           {"scalar", "Scalar", K::Scalar},
                                           {"text", "Text", K::Text},
                                           {"boolean", "Boolean", K::Boolean, false}};
  std::vector<EngineeringTableCell> cells{
      {"length", checked(Quantity::length_mm(3)), provenance()},
      {"offset", checked(Quantity::linear_displacement_mm(-2)), provenance()},
      {"angle", checked(Quantity::angle_deg(30)), provenance()},
      {"count", checked(Quantity::count(4)), provenance()},
      {"density", checked(MassDensity::from_g_per_cm3(4)), provenance()},
      {"pressure", checked(EngineeringPressure::from_mpa(250)), provenance()},
      {"scalar", 0.25, provenance()},
      {"text", std::string("Synthetic size"), provenance()},
      {"boolean", false, provenance()}};
  return checked(EngineeringStandardTable::create(
      key("synthetic.table"), "Synthetic table", provenance(), std::move(cols),
      {{"synthetic.row", "Synthetic row", std::move(cells)}}));
}
inline EngineeringResourceCatalog catalog() {
  EngineeringResourceCatalog result;
  (void)checked(result.insert(material()));
  (void)checked(result.insert(material("r2", 6000)));
  (void)checked(result.insert(table()));
  return result;
}
template <typename Part> void annotate(Part& part) {
  const auto resources = catalog();
  for (const auto& resource : resources.entries())
    (void)checked(part.add_engineering_resource(resource));
  (void)checked(part.set_default_material(key()));
  if (part.find_body(BodyId("body.tool")))
    (void)checked(part.set_body_material(BodyId("body.tool"), key("synthetic.material", "r2")));
}
} // namespace blcad::test::engineering_fixture
