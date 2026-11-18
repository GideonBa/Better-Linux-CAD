#pragma once
#include "blcad/core/assembly_hole_feature.hpp"
#include "blcad/core/assembly_occurrence_pose.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project.hpp"
#include "engineering_resource_fixture.hpp"
#include "part_history_fixture.hpp"
#include <nlohmann/json.hpp>
namespace blcad::test::assembly_hole {
namespace h = history_fixture;
inline EngineeringResourceKey pin() {
  return engineering_fixture::key("assembly.mounting", "r7");
}
inline EngineeringStandardTable table() {
  using Kind = EngineeringTableValueKind;
  const auto provenance = engineering_fixture::provenance();
  std::vector<EngineeringTableColumn> columns;
  std::vector<EngineeringTableCell> cells;
  for (const auto& [key, value] : {std::pair{"clearance", 2.4},
                                   {"major", 2.},
                                   {"minor", 1.6},
                                   {"pitch", .4},
                                   {"angle", 60.}}) {
    const bool angle = std::string_view(key) == "angle";
    columns.push_back({key, key, angle ? Kind::Angle : Kind::Length});
    cells.push_back({key,
                     h::checked(angle ? Quantity::angle_deg(value) : Quantity::length_mm(value)),
                     provenance});
  }
  for (const auto& [key, value] : {std::pair{"system", "Synthetic"},
                                   {"designation", "M2 fixture"},
                                   {"tolerance", "Authored"}}) {
    columns.push_back({key, key, Kind::Text});
    cells.push_back({key, std::string(value), provenance});
  }
  return h::checked(EngineeringStandardTable::create(pin(), "Synthetic mounting data", provenance,
                                                     columns, {{"m2", "Mounting fixture", cells}}));
}
inline void length(PartDocument& p, const char* id, double value) {
  h::accept(p.add_parameter(h::checked(
      Parameter::create_length(ParameterId(id), id, h::checked(Quantity::length_mm(value))))));
}
inline PartDocument plate(const char* id, double depth) {
  auto source =
      nlohmann::json::parse(h::checked(serialize_part_document_to_json(h::native_base())));
  source["document"]["id"] = id;
  source["document"]["name"] = id;
  auto p = h::checked(deserialize_part_document_from_json(source.dump()));
  (void)h::checked(
      p.set_parameter_value(ParameterId("depth"), h::checked(Quantity::length_mm(depth))));
  h::accept(p.add_construction_plane(h::checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("hole.plane"), "Entry plane",
                                         {0, 0, depth}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  h::accept(p.add_engineering_resource(table()));
  h::accept(p.add_engineering_resource(engineering_fixture::material()));
  h::accept(p.set_default_material(engineering_fixture::key()));
  return p;
}
inline PlaneReference support() {
  return h::checked(PlaneReference::create_construction_plane(
      PartFeatureInputRole::HoleSupportPlane, ConstructionPlaneId("hole.plane")));
}
inline HoleFeature clearance() {
  return h::checked(HoleFeature::create(
      FeatureId("layout.hole"), "Cover clearance", HoleKind::Clearance, BodyId("body.base"),
      support(), {{HolePlacementId("seed"), Point2{-3, 0}}},
      HoleStandardSelection{pin(), "m2", "clearance"}, HoleThroughAllExtent{},
      HoleDirection::OppositeSupportNormal, HoleFlatTip{}));
}
inline HoleFeature threaded() {
  auto thread = h::checked(
      ThreadDefinition::create(ThreadStandardProfile{pin(),
                                                     "m2",
                                                     {"system", "designation", "tolerance", "major",
                                                      "minor", "pitch", "angle", std::nullopt}},
                               ParameterId("thread.length")));
  return h::checked(HoleFeature::create(
      FeatureId("mounting.hole"), "Base blind thread", HoleKind::Plain, BodyId("body.base"),
      support(), {{HolePlacementId("seed"), Point2{0, 0}}},
      HoleStandardSelection{pin(), "m2", "minor"}, HoleBlindExtent{ParameterId("blind.depth")},
      HoleDirection::OppositeSupportNormal, HoleFlatTip{}, {}, thread));
}
inline PartDocument cover(bool patterned = true) {
  auto p = plate("cover.part", 2);
  length(p, "layout.spacing", 6);
  h::accept(p.add_parameter(h::checked(Parameter::create_count(
      ParameterId("layout.count"), "Layout count", h::checked(Quantity::count(2))))));
  h::accept(p.add_datum_axis(h::checked(
      DatumAxis::create_explicit(DatumAxisId("layout.axis"), "Layout X", {}, {1, 0, 0}))));
  h::accept(p.add_hole_feature(clearance()));
  if (patterned)
    h::accept(p.add_linear_pattern_feature(h::checked(LinearPatternFeature::create(
        FeatureId("layout.pattern"), "Cover pattern",
        {h::checked(PatternSourceReference::feature(FeatureId("layout.hole")))},
        h::checked(AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis,
                                                    DatumAxisId("layout.axis"))),
        ParameterId("layout.count"), LinearPatternExtentMode::Spacing,
        ParameterId("layout.spacing"), PatternDirectionSign::Positive,
        h::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                    BodyId("body.base"), std::nullopt)),
        PatternPlacementOptions{}))));
  return p;
}
inline PartDocument base() {
  auto p = plate("base.part", 8);
  length(p, "blind.depth", 4);
  length(p, "thread.length", 3);
  return p;
}
inline RigidTransform parent_pose() {
  return {{30, 40, 50}, {20, -15, 40}};
}
inline RigidTransform cover_pose() {
  return {{2, 0, 8}, {0, 0, 90}};
}
inline Project project(bool patterned = true, bool repeated = false, bool reference_base = false) {
  auto root = h::checked(AssemblyDocument::create(DocumentId("mounting.root"), "Mounting root"));
  auto child = h::checked(AssemblyDocument::create(DocumentId("mounting.child"), "Cover and base"));
  auto top = cover(patterned);
  auto bottom = reference_base ? h::imported(true) : base();
  auto unrelated = plate("unrelated.part", 3);
  (void)h::checked(
      unrelated.set_parameter_value(ParameterId("width"), h::checked(Quantity::length_mm(6))));
  (void)h::checked(
      unrelated.set_parameter_value(ParameterId("height"), h::checked(Quantity::length_mm(4))));
  for (const auto* p : {&top, &bottom, &unrelated})
    h::accept(child.add_member_part(p->id()));
  h::accept(child.add_component_instance(h::checked(ComponentInstance::create(
      ComponentInstanceId("cover"), "Cover", top.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, cover_pose()))));
  h::accept(child.add_component_instance(h::checked(ComponentInstance::create(
      ComponentInstanceId("base"), "Base", bottom.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Grounded))));
  h::accept(child.add_component_instance(h::checked(ComponentInstance::create(
      ComponentInstanceId("unrelated"), "Unrelated", unrelated.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Grounded, {{50, 0, 0}, {}}))));
  if (repeated)
    h::accept(child.add_component_instance(h::checked(
        ComponentInstance::create(ComponentInstanceId("base.other"), "Same definition", bottom.id(),
                                  ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                  ComponentGroundingState::Grounded, {{30, 0, 0}, {}}))));
  h::accept(root.add_subassembly_instance(h::checked(SubassemblyInstance::create(
      SubassemblyInstanceId("pair"), "Nested pair", child.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, parent_pose()))));
  auto result =
      h::checked(Project::create(DocumentId("mounting.project"), "Coordinated mounting", root));
  h::accept(result.add_child_assembly_document(child));
  h::accept(result.add_part_document(top));
  h::accept(result.add_part_document(bottom));
  h::accept(result.add_part_document(unrelated));
  h::accept(result.synchronize_engineering_resources());
  return result;
}
inline ProjectDesignOccurrence occurrence(const char* component, const char* part) {
  return {{SubassemblyInstanceId("pair")}, ComponentInstanceId(component), DocumentId(part)};
}
inline AssemblyHoleFeature
feature(bool patterned = true,
        AssemblyHoleDefinitionPolicy policy = AssemblyHoleDefinitionPolicy::SingleOccurrence,
        ModelStateId base_state = primary_model_state_id(), bool reference_base = false) {
  AssemblyHoleLayout layout{occurrence("cover", "cover.part"), primary_model_state_id(),
                            FeatureId("layout.hole"),
                            patterned ? std::optional(FeatureId("layout.pattern")) : std::nullopt};
  return h::checked(AssemblyHoleFeature::create(
      FeatureId("assembly.mounting"), "Coordinated cover/base", layout,
      {{"cover", occurrence("cover", "cover.part"), primary_model_state_id(),
        AssemblyHoleParticipantRole::Clearance, AssemblyHoleDefinitionPolicy::SingleOccurrence,
        clearance()},
       {"base", occurrence("base", reference_base ? "history.reference" : "base.part"), base_state,
        AssemblyHoleParticipantRole::Threaded, policy, threaded()}}));
}
} // namespace blcad::test::assembly_hole
