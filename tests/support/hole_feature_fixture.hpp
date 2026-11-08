#pragma once

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project.hpp"
#include "engineering_resource_fixture.hpp"
#include "part_history_fixture.hpp"

#include <nlohmann/json.hpp>

namespace blcad::test::hole_fixture {
using history_fixture::accept;
using history_fixture::checked;
inline EngineeringResourceKey key(std::string revision = "r1") {
  return engineering_fixture::key("synthetic.holes", std::move(revision));
}
inline EngineeringStandardTable table(std::string revision = "r1", double small = 6.6,
                                      double large = 8.4, bool include_large = true) {
  const auto source = engineering_fixture::provenance();
  std::vector<EngineeringTableRow> rows{
      {"small", "Synthetic small", {{"diameter", checked(Quantity::length_mm(small)), source}}}};
  if (include_large)
    rows.push_back(
        {"large", "Synthetic large", {{"diameter", checked(Quantity::length_mm(large)), source}}});
  return checked(EngineeringStandardTable::create(
      key(std::move(revision)), "Synthetic Hole table", source,
      {{"diameter", "Diameter", EngineeringTableValueKind::Length}}, std::move(rows)));
}
inline HoleStandardSelection standard(std::string revision = "r1", std::string row = "small") {
  return {key(std::move(revision)), std::move(row), "diameter"};
}
inline HoleFeature hole(std::string id = "hole", HoleDiameter diameter = ParameterId("radius"),
                        HoleExtent extent = HoleThroughAllExtent{}, HoleTip tip = HoleFlatTip{}) {
  const auto support = checked(PlaneReference::create_semantic_face(
      PartFeatureInputRole::HoleSupportPlane,
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  return checked(HoleFeature::create(
      FeatureId(std::move(id)), "Semantic Hole", HoleKind::Plain, BodyId("body.base"), support,
      {{HolePlacementId("center"), Point2{0, 0}}}, std::move(diameter), std::move(extent),
      HoleDirection::OppositeSupportNormal, std::move(tip)));
}
inline PartDocument part(std::string id = "hole.part", bool authored = false,
                         bool standard_diameter = true) {
  auto value = nlohmann::json::parse(
      checked(serialize_part_document_to_json(history_fixture::native_base())));
  value["document"]["id"] = id;
  auto result = checked(deserialize_part_document_from_json(value.dump()));
  if (authored)
    accept(result.apply_history_intent({{"base"}, {}}));
  if (standard_diameter)
    accept(result.add_engineering_resource(table()));
  accept(result.add_hole_feature(hole(
      "hole", standard_diameter ? HoleDiameter{standard()} : HoleDiameter{ParameterId("radius")})));
  return result;
}
inline Project project() {
  auto result =
      checked(Project::create(DocumentId("hole.project"), "Hole project",
                              checked(AssemblyDocument::create(DocumentId("hole.root"), "Root"))));
  accept(result.add_part_document(part("first", true)));
  accept(result.add_part_document(part("second", true)));
  return result;
}
} // namespace blcad::test::hole_fixture
