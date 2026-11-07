#include "../support/engineering_resource_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace resources = blcad::test::engineering_fixture;
namespace history = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> value) {
  INFO((value ? std::string{} : value.error().message()));
  REQUIRE(value);
  return std::move(value.value());
}
Catch::Approx approximately(double value) {
  return Catch::Approx(value).epsilon(1e-9).margin(1e-10);
}
PartDocument part(std::string id, bool assigned = true) {
  auto data =
      nlohmann::json::parse(checked(serialize_part_document_to_json(history::native_base())));
  data["document"]["id"] = id;
  auto result = checked(deserialize_part_document_from_json(data.dump()));
  resources::annotate(result);
  if (!assigned)
    REQUIRE(result.set_default_material(std::nullopt));
  return result;
}
AssemblyDocument assembly(std::string id) {
  return checked(AssemblyDocument::create(DocumentId(id), id));
}
Project project(std::vector<PartDocument> parts) {
  auto root = assembly("assembly.root");
  for (const auto& p : parts)
    REQUIRE(root.add_member_part(p.id()));
  auto result = checked(Project::create(DocumentId("physical.project"), "Physical project", root));
  for (auto& p : parts)
    REQUIRE(result.add_part_document(std::move(p)));
  return result;
}
void component(AssemblyDocument& a, std::string id, std::string part_id, RigidTransform pose = {},
               ComponentVisibility visibility = ComponentVisibility::Visible,
               ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  if (!a.has_member_part(DocumentId(part_id)))
    REQUIRE(a.add_member_part(DocumentId(part_id)));
  REQUIRE(a.add_component_instance(checked(
      ComponentInstance::create(ComponentInstanceId(id), id, DocumentId(part_id), visibility,
                                suppression, ComponentGroundingState::Free, pose))));
}
std::string source(const Project& p) {
  return checked(serialize_project_to_json(p));
}
void tensor(const std::array<double, 9>& actual, const std::array<double, 9>& expected) {
  for (std::size_t i = 0; i < 9; ++i) {
    CAPTURE(i);
    CHECK(actual[i] == approximately(expected[i]));
  }
}
} // namespace

TEST_CASE(
    "Physical Project totals include hidden active occurrences and compute each definition once",
    "[geometry][parity-b150][assembly-physical-properties]") {
  auto p = project({part("shared"), part("unused")});
  component(p.assembly(), "a", "shared");
  component(p.assembly(), "b", "shared", {{30, 0, 0}, {}}, ComponentVisibility::Hidden);
  component(p.assembly(), "c", "shared", {{1000, 0, 0}, {}}, ComponentVisibility::Visible,
            ComponentSuppressionState::Suppressed);
  const auto before = source(p);
  const auto visible = checked(AssemblyLeafOccurrenceResolver{}.resolve(p));
  CHECK(visible.size() == 1);
  const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
  const auto& d = result.data();
  CHECK(d.occurrences.size() == 2);
  CHECK(d.computed_part_count == 1);
  CHECK(d.volume_mm3 == approximately(3200));
  CHECK(d.area_mm2 == approximately(1760));
  REQUIRE(d.complete_mass());
  CHECK(d.complete_mass()->mass_kg == approximately(.0128));
  CHECK(d.complete_mass()->centroid_mm.x == approximately(15));
  CHECK(d.complete_mass()->centroid_mm.z == approximately(4));
  tensor(d.complete_mass()->central_inertia_kg_mm2,
         {.1749333333333333, 0, 0, 0, 3.374933333333333, 0, 0, 0, 3.413333333333333});
  REQUIRE(result.validate_current(p));
  CHECK(source(p) == before);
}

TEST_CASE(
    "Repeated nested physical occurrences rotate central tensors through exact authored chains",
    "[geometry][parity-b150][assembly-physical-properties]") {
  auto p = project({part("shared")});
  auto child = assembly("assembly.child");
  component(child, "inner", "shared", {{0, 2, 0}, {90, 0, 0}});
  REQUIRE(p.add_child_assembly_document(child));
  REQUIRE(p.assembly().add_subassembly_instance(checked(SubassemblyInstance::create(
      SubassemblyInstanceId("first"), "First", child.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, {{10, 20, 30}, {0, 0, 90}}))));
  REQUIRE(p.assembly().add_subassembly_instance(checked(SubassemblyInstance::create(
      SubassemblyInstanceId("second"), "Second", child.id(), ComponentVisibility::Hidden,
      ComponentSuppressionState::Active, {{100, 0, 0}, {}}))));
  const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
  const auto& d = result.data();
  REQUIRE(d.occurrences.size() == 2);
  CHECK(d.computed_part_count == 1);
  CHECK(d.occurrences[0].occurrence.component_instance ==
        d.occurrences[1].occurrence.component_instance);
  CHECK(d.occurrences[0].occurrence.subassembly_path !=
        d.occurrences[1].occurrence.subassembly_path);
  const auto& first = *d.occurrences[0].bodies.front().mass;
  CHECK(first.centroid_mm.x == approximately(12));
  CHECK(first.centroid_mm.y == approximately(20));
  CHECK(first.centroid_mm.z == approximately(30));
  tensor(first.central_inertia_kg_mm2,
         {.0064 * 500 / 12, 0, 0, 0, .0064 * 164 / 12, 0, 0, 0, .0064 * 464 / 12});
  REQUIRE(d.complete_mass());
  const auto& m = *d.complete_mass();
  CHECK(m.mass_kg == approximately(.0128));
  CHECK(m.centroid_mm.x == approximately(56));
  CHECK(m.centroid_mm.y == approximately(9));
  CHECK(m.centroid_mm.z == approximately(15));
  tensor(m.central_inertia_kg_mm2, {4.782933333333333, 6.1952, 8.448, 6.1952, 28.01493333333333,
                                    -2.112, 8.448, -2.112, 26.82453333333333});
}

TEST_CASE("Partial assembly mass retains the known-subset centroid independently of remote unknown "
          "material",
          "[geometry][parity-b150][assembly-physical-properties]") {
  auto p = project({part("known"), part("unknown", false)});
  component(p.assembly(), "known", "known");
  component(p.assembly(), "unknown", "unknown", {{1000, 0, 0}, {}});
  const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
  const auto& d = result.data();
  CHECK(d.mass_status == PhysicalMassStatus::Partial);
  CHECK_FALSE(d.complete_mass());
  REQUIRE(d.known_mass);
  CHECK(d.known_mass->mass_kg == approximately(.0064));
  CHECK(d.known_mass->centroid_mm.x == approximately(0));
  REQUIRE(d.volume_centroid_mm);
  CHECK(d.volume_centroid_mm->x == approximately(500));
  CHECK(d.missing_density_body_count == 1);
}

TEST_CASE(
    "Physical assembly snapshots reject pose, material and same-source executable lifetime changes",
    "[geometry][parity-b150][assembly-physical-properties]") {
  auto p = project({part("shared")});
  component(p.assembly(), "instance", "shared");
  const auto original = p;
  const auto before = source(p);
  const auto result = checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(result.validate_current(p));
  REQUIRE(p.assembly().set_component_instance_transform(ComponentInstanceId("instance"),
                                                        {{20, 0, 0}, {0, 0, 90}}));
  CHECK_FALSE(result.validate_current(p));
  const auto moved = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(moved.data().complete_mass()->centroid_mm.x == approximately(20));
  REQUIRE(result.validate_current(original));
  p = original;
  auto* shared = p.find_part_document(DocumentId("shared"));
  REQUIRE(shared);
  REQUIRE(shared->set_default_material(resources::key("synthetic.material", "r2")));
  CHECK_FALSE(result.validate_current(p));
  const auto denser = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(denser.data().complete_mass()->mass_kg == approximately(.0096));
  p = original;
  shared = p.find_part_document(DocumentId("shared"));
  const auto feature = *shared->find_feature(FeatureId("base"));
  REQUIRE(shared->remove_feature(feature.id()));
  REQUIRE(shared->add_feature(feature));
  CHECK(source(p) == before);
  CHECK_FALSE(result.validate_current(p));
  const auto rebuilt = checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(rebuilt.validate_current(p));
  const auto reopened = checked(deserialize_project_from_json(source(p)));
  const auto restored = checked(BodyResultInspector{}.inspect_physical_properties(reopened));
  CHECK(restored.data().volume_mm3 == approximately(1600));
  CHECK(source(p) == before);
}

TEST_CASE("Physical empty and suppressed scopes do not evaluate unused broken Part definitions",
          "[geometry][parity-b150][assembly-physical-properties]") {
  auto p = project({part("unused"), history::imported()});
  const auto empty = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(empty.data().occurrences.empty());
  CHECK(empty.data().computed_part_count == 0);
  CHECK(empty.data().mass_status == PhysicalMassStatus::NotApplicable);
  CHECK(empty.data().area_mm2 == 0);
  CHECK_FALSE(empty.data().volume_centroid_mm);
  component(p.assembly(), "broken", "history", {}, ComponentVisibility::Hidden,
            ComponentSuppressionState::Suppressed);
  const auto suppressed = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(suppressed.data().occurrences.empty());
  component(p.assembly(), "good", "unused");
  const auto good = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(good.data().computed_part_count == 1);
  CHECK(good.data().volume_mm3 == approximately(1600));
  REQUIRE(p.assembly().set_component_instance_suppression_state(ComponentInstanceId("broken"),
                                                                ComponentSuppressionState::Active));
  const auto before = source(p);
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK_FALSE(good.validate_current(p));
  CHECK(source(p) == before);
}

TEST_CASE(
    "Physical assembly queries reject ambiguous ownership and inconsistent resource snapshots",
    "[geometry][parity-b150][assembly-physical-properties]") {
  auto p = project({part("shared")});
  component(p.assembly(), "instance", "shared");
  const auto original = p;
  p.part_documents().push_back(p.part_documents().front());
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(p));
  p = original;
  auto* shared = p.find_part_document(DocumentId("shared"));
  REQUIRE(shared);
  REQUIRE(shared->remove_engineering_resource(resources::key("synthetic.material", "r2")));
  // A local revision unknown to the Project must be synchronized before publication.
  auto replacement = resources::material("r3", 5000);
  REQUIRE(shared->add_engineering_resource(replacement));
  REQUIRE(shared->set_default_material(replacement.key()));
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(p.synchronize_engineering_resources());
  const auto synchronized = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(synchronized.data().complete_mass()->mass_kg == approximately(.008));
}
