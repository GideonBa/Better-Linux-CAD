#include "blcad/core/project_json.hpp"
#include "blcad/core/structured_step_import.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <utility>

using namespace blcad;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
StepImportSource source(StepImportMode mode = StepImportMode::Reference) {
  return checked(StepImportSource::create(StepImportSourceId("source"), "Supplier assembly",
                                          "assets/assembly.step", std::string(64, '1'), mode));
}
ImportedTopologyCatalog catalog(std::string suffix, bool multiple = false) {
  ImportedTopologyDescriptor body;
  body.kind = ImportedTopologyKind::Body;
  body.geometry = ImportedGeometryClass::Opaque;
  body.body_kind = BodyKind::Solid;
  body.bounds_mm = {0, 0, 0, 1, 1, 1};
  body.centroid = {0.5, 0.5, 0.5};
  body.measure = 1;
  body.geometry_key = std::string(64, 'a');
  std::vector<ImportedTopologyEntry> entries{
      {ImportedTopologyId("topology.body.a"), std::nullopt, body}};
  if (multiple) {
    body.body_kind = BodyKind::Surface;
    body.bounds_mm = {0, 0, 0, 1, 1, 0};
    body.centroid = {0.5, 0.5, 0};
    body.geometry_key = std::string(64, 'b');
    entries.push_back({ImportedTopologyId("topology.body.b"), std::nullopt, body});
  }
  return checked(ImportedTopologyCatalog::create(ImportedTopologyCatalogId("catalog." + suffix),
                                                 source().id(), source().sha256_digest(),
                                                 std::string(64, '2'), std::move(entries)));
}
PartDocument part(std::string id = "part.shared", StepImportMode mode = StepImportMode::Reference,
                  bool multiple = false, bool complete = true) {
  const auto product = catalog(id, multiple);
  std::vector<ImportedBodyOutput> outputs{
      {ImportedTopologyId("topology.body.a"), BodyId("body.a"), BodyKind::Solid}};
  if (multiple && complete)
    outputs.push_back({ImportedTopologyId("topology.body.b"), BodyId("body.b"), BodyKind::Surface});
  auto definition =
      checked(ImportedPartDefinition::create(source(mode).id(), product.id(), outputs));
  if (mode == StepImportMode::Reference)
    return checked(PartDocument::create_reference(DocumentId(id), "Part / " + id, source(mode),
                                                  product, definition));
  auto feature = checked(
      ImportedBodyFeature::create(FeatureId("feature.imported"), "Imported base", definition));
  return checked(PartDocument::create_editable_import(DocumentId(id), "Part / " + id, source(mode),
                                                      product, feature));
}
ComponentInstance component(const char* id, const char* part_id,
                            RigidTransform placement = identity_rigid_transform()) {
  return checked(ComponentInstance::create(
      ComponentInstanceId(id), std::string("Component / ") + id, DocumentId(part_id),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Free, placement));
}
SubassemblyInstance subassembly(const char* id, const char* assembly_id,
                                RigidTransform placement = identity_rigid_transform()) {
  return checked(SubassemblyInstance::create(
      SubassemblyInstanceId(id), std::string("Assembly / ") + id, DocumentId(assembly_id),
      ComponentVisibility::Visible, ComponentSuppressionState::Active, placement));
}
AssemblyDocument root(bool reverse = false) {
  auto assembly = checked(AssemblyDocument::create(DocumentId("assembly.root"), "Imported roots"));
  REQUIRE(assembly.add_member_part(DocumentId("part.shared")));
  REQUIRE(assembly.add_component_instance(
      component("occ.root.shared", "part.shared", {{7, 8, 9}, {11, 22, 33}})));
  std::vector<SubassemblyInstance> children{
      subassembly("occ.root.a", "assembly.a", {{100, 200, 300}, {10, 20, 30}}),
      subassembly("occ.root.b", "assembly.a", {{-40, 50, 60}, {70, 80, 90}})};
  if (reverse)
    std::reverse(children.begin(), children.end());
  for (const auto& child : children)
    REQUIRE(assembly.add_subassembly_instance(child));
  return assembly;
}
std::vector<AssemblyDocument> children() {
  auto outer = checked(AssemblyDocument::create(DocumentId("assembly.a"), "Module"));
  REQUIRE(outer.add_member_part(DocumentId("part.housing")));
  REQUIRE(outer.add_component_instance(
      component("occ.housing", "part.housing", {{1, 2, 3}, {4, 5, 6}})));
  REQUIRE(outer.add_subassembly_instance(
      subassembly("occ.inner", "assembly.z", {{10, 0, -20}, {90, 0, 0}})));
  auto inner = checked(AssemblyDocument::create(DocumentId("assembly.z"), "Shared leaf container"));
  REQUIRE(inner.add_member_part(DocumentId("part.shared")));
  REQUIRE(inner.add_component_instance(
      component("occ.leaf.shared", "part.shared", {{-1, -2, -3}, {-4, -5, -6}})));
  return {std::move(inner), std::move(outer)};
}
std::vector<PartDocument> parts(StepImportMode mode = StepImportMode::Reference) {
  return {part("part.shared", mode), part("part.housing", mode, true)};
}
Result<Project> create(AssemblyDocument assembly = root(),
                       std::vector<AssemblyDocument> definitions = children(),
                       std::vector<PartDocument> leaves = parts(),
                       StepImportSource asset = source(), DocumentId id = DocumentId("project")) {
  return create_structured_step_project(std::move(id), "Imported assembly", std::move(asset),
                                        std::move(assembly), std::move(definitions),
                                        std::move(leaves));
}
std::string saved(const Project& project) {
  return checked(serialize_project_to_json(project));
}
} // namespace

TEST_CASE(
    "Structured import retains shared definitions names and exact direct occurrence placement",
    "[core][structured-step-import]") {
  for (const auto mode : {StepImportMode::Reference, StepImportMode::EditableBody}) {
    DYNAMIC_SECTION(to_string(mode)) {
      const auto original_root = root();
      const auto original_children = children();
      const auto project =
          checked(create(original_root, original_children, parts(mode), source(mode)));
      CHECK(project.part_document_count() == 2);
      CHECK(project.child_assembly_document_count() == 2);
      CHECK(project.step_import_sources() == std::vector{source(mode)});
      CHECK(project.imported_topology_catalogs().empty());
      CHECK(project.assembly().name() == "Imported roots");
      CHECK(checked(validate_structured_step_import_candidate(project, source(mode).id())) == 6);
      CHECK(project.assembly().component_instances().front().referenced_part_document() ==
            DocumentId("part.shared"));
      CHECK(project.find_assembly_document(DocumentId("assembly.z"))
                ->component_instances()
                .front()
                .referenced_part_document() == DocumentId("part.shared"));
      CHECK(project.assembly().subassembly_instances()[0].referenced_assembly_document() ==
            DocumentId("assembly.a"));
      CHECK(project.assembly().subassembly_instances()[1].referenced_assembly_document() ==
            DocumentId("assembly.a"));
      std::vector<const AssemblyDocument*> expected{&original_root};
      for (const auto& child : original_children)
        expected.push_back(&child);
      for (const auto* assembly : expected) {
        const auto* current = project.find_assembly_document(assembly->id());
        REQUIRE(current);
        CHECK(current->name() == assembly->name());
        for (const auto& occurrence : assembly->component_instances()) {
          const auto* copy = current->find_component_instance(occurrence.id());
          REQUIRE(copy);
          CHECK(copy->name() == occurrence.name());
          CHECK(copy->transform() == occurrence.transform());
        }
        for (const auto& occurrence : assembly->subassembly_instances()) {
          const auto* copy = current->find_subassembly_instance(occurrence.id());
          REQUIRE(copy);
          CHECK(copy->name() == occurrence.name());
          CHECK(copy->transform() == occurrence.transform());
        }
      }
      for (const auto& owned : project.part_documents()) {
        CHECK(owned.is_reference_part() == (mode == StepImportMode::Reference));
        CHECK(owned.imported_topology_catalogs().size() == 1);
        CHECK(owned.step_import_source_count() == 1);
      }
      const auto original = saved(project);
      const auto loaded = checked(deserialize_project_from_json(original));
      CHECK(saved(loaded) == original);
      CHECK(validate_structured_step_import_candidate(loaded, source(mode).id()));
      const auto json = nlohmann::json::parse(original);
      CHECK_FALSE(json.contains("structured_step_import"));
      CHECK_FALSE(json.contains("foreign_definitions"));
    }
  }
}

TEST_CASE("Structured candidate construction is deterministic across definition and occurrence "
          "input order",
          "[core][structured-step-import]") {
  auto definitions = children();
  auto leaves = parts();
  std::reverse(definitions.begin(), definitions.end());
  std::reverse(leaves.begin(), leaves.end());
  const auto normal = checked(create());
  const auto shuffled = checked(create(root(true), definitions, leaves));
  CHECK(saved(normal) == saved(shuffled));
  CHECK(normal.child_assembly_documents()[0].id() == DocumentId("assembly.a"));
  CHECK(normal.child_assembly_documents()[1].id() == DocumentId("assembly.z"));
}

TEST_CASE("Structured import rejects missing orphaned cyclic and duplicate definitions without "
          "mutating inputs",
          "[core][structured-step-import]") {
  const auto valid = checked(create());
  const auto original = saved(valid);
  SECTION("Missing child definition") {
    CHECK_FALSE(create(root(), {}, parts()));
  }
  SECTION("Missing Part definition") {
    CHECK_FALSE(create(root(), children(), {part()}));
  }
  SECTION("Orphan Part definition") {
    auto leaves = parts();
    leaves.push_back(part("part.orphan"));
    CHECK_FALSE(create(root(), children(), leaves));
  }
  SECTION("Orphan Assembly definition") {
    auto definitions = children();
    definitions.push_back(checked(AssemblyDocument::create(DocumentId("orphan"), "Orphan")));
    CHECK_FALSE(create(root(), definitions));
  }
  SECTION("Unused member Part") {
    auto assembly = root();
    REQUIRE(assembly.add_member_part(DocumentId("part.housing")));
    CHECK_FALSE(create(assembly));
  }
  SECTION("Indirect cycle") {
    auto definitions = children();
    REQUIRE(definitions[0].add_subassembly_instance(subassembly("occ.cycle", "assembly.a")));
    CHECK_FALSE(create(root(), definitions));
  }
  SECTION("Duplicate Assembly identity") {
    auto definitions = children();
    definitions.push_back(definitions.front());
    CHECK_FALSE(create(root(), definitions));
  }
  SECTION("Duplicate Part identity") {
    auto leaves = parts();
    leaves.push_back(leaves.front());
    CHECK_FALSE(create(root(), children(), leaves));
  }
  SECTION("Project and root Document identity collision") {
    CHECK_FALSE(create(root(), children(), parts(), source(), DocumentId("assembly.root")));
  }
  SECTION("Part and Assembly Document identity collision") {
    auto leaves = parts();
    leaves.push_back(part("assembly.a"));
    CHECK_FALSE(create(root(), children(), leaves));
  }
  CHECK(saved(valid) == original);
}

TEST_CASE(
    "Structured import rejects duplicate occurrence identities across stored definitions and kinds",
    "[core][structured-step-import]") {
  auto assembly = root();
  SECTION("Components in distinct definitions") {
    REQUIRE(assembly.add_component_instance(component("occ.leaf.shared", "part.shared")));
  }
  SECTION("Component and Subassembly identity") {
    REQUIRE(assembly.add_component_instance(component("occ.root.a", "part.shared")));
  }
  CHECK_FALSE(create(assembly));
}

TEST_CASE(
    "Structured candidate validation rejects inferred authoring and noninitial occurrence states",
    "[core][structured-step-import]") {
  auto project = checked(create());
  auto& assembly = project.assembly();
  SECTION("Assembly parameters") {
    REQUIRE(assembly.add_parameter(checked(
        Parameter::create_length(ParameterId("authored"), "Authored",
                                 checked(Quantity::length_mm(2)), ParameterScope::Assembly))));
  }
  SECTION("Constraints") {
    REQUIRE(assembly.add_component_instance(component("occ.extra", "part.shared")));
    auto first = checked(AssemblyConstraintTarget::create(ComponentInstanceId("occ.root.shared"),
                                                          "feature.base.top"));
    auto second = checked(
        AssemblyConstraintTarget::create(ComponentInstanceId("occ.extra"), "feature.base.bottom"));
    REQUIRE(assembly.add_constraint(
        checked(AssemblyConstraint::create(AssemblyConstraintId("constraint"), "Inferred",
                                           AssemblyConstraintType::Mate, first, second))));
  }
  SECTION("Joints") {
    REQUIRE(assembly.add_component_instance(component("occ.extra", "part.shared")));
    const auto first = checked(AssemblyConstraintTarget::create(
        ComponentInstanceId("occ.root.shared"), "feature.base.point"));
    const auto second = checked(
        AssemblyConstraintTarget::create(ComponentInstanceId("occ.extra"), "feature.base.point"));
    REQUIRE(assembly.add_joint(checked(AssemblyJoint::create(
        AssemblyJointId("joint"), "Inferred", AssemblyJointType::Spherical, first, second,
        AssemblyJointState::Active, std::vector<AssemblyJointCoordinateSlot>{}))));
  }
  SECTION("Hidden component") {
    REQUIRE(assembly.set_component_instance_visibility(ComponentInstanceId("occ.root.shared"),
                                                       ComponentVisibility::Hidden));
  }
  SECTION("Suppressed component") {
    REQUIRE(assembly.set_component_instance_suppression_state(
        ComponentInstanceId("occ.root.shared"), ComponentSuppressionState::Suppressed));
  }
  SECTION("Grounded component") {
    REQUIRE(assembly.set_component_instance_grounding_state(ComponentInstanceId("occ.root.shared"),
                                                            ComponentGroundingState::Grounded));
  }
  SECTION("Hidden subassembly") {
    REQUIRE(assembly.set_subassembly_instance_visibility(SubassemblyInstanceId("occ.root.a"),
                                                         ComponentVisibility::Hidden));
  }
  SECTION("Suppressed subassembly") {
    REQUIRE(assembly.set_subassembly_instance_suppression_state(
        SubassemblyInstanceId("occ.root.a"), ComponentSuppressionState::Suppressed));
  }
  CHECK(project.validate_assembly_structure());
  CHECK_FALSE(validate_structured_step_import_candidate(project, source().id()));
}

TEST_CASE(
    "Structured import requires fresh whole-product Parts with exact selected mode and source",
    "[core][structured-step-import]") {
  SECTION("Mixed modes") {
    CHECK_FALSE(create(root(), children(), parts(StepImportMode::EditableBody), source()));
  }
  SECTION("An ordinary Part is not an imported definition") {
    auto leaves = parts();
    leaves[0] = checked(PartDocument::create(DocumentId("part.shared"), "Ordinary"));
    CHECK_FALSE(create(root(), children(), leaves));
  }
  SECTION("Incomplete product Body selection") {
    auto leaves = parts();
    leaves[1] = part("part.housing", StepImportMode::Reference, true, false);
    CHECK_FALSE(create(root(), children(), leaves));
  }
  SECTION("Source accepted digest mismatch") {
    CHECK_FALSE(create(root(), children(), parts(),
                       checked(source().with_sha256_digest(std::string(64, '3')))));
  }
  SECTION("Old captured catalog after explicit refresh") {
    auto leaves = parts();
    for (auto& leaf : leaves)
      REQUIRE(leaf.refresh_step_import_source(source(), std::string(64, '3')));
    CHECK_FALSE(create(root(), children(), leaves,
                       checked(source().with_sha256_digest(std::string(64, '3')))));
  }
  SECTION("Downstream authored Editable Part") {
    auto leaves = parts(StepImportMode::EditableBody);
    REQUIRE(leaves[0].add_parameter(checked(
        Parameter::create_length(ParameterId("depth"), "Depth", checked(Quantity::length_mm(2))))));
    CHECK_FALSE(create(root(), children(), leaves, source(StepImportMode::EditableBody)));
  }
  SECTION("Project catalog is not imported Part ownership") {
    auto project = checked(create());
    REQUIRE(project.add_imported_topology_catalog(catalog("extra")));
    CHECK_FALSE(validate_structured_step_import_candidate(project, source().id()));
  }
  SECTION("Extra source metadata") {
    auto project = checked(create());
    REQUIRE(project.add_step_import_source(
        checked(StepImportSource::create(StepImportSourceId("extra"), "Extra", "assets/extra.step",
                                         std::string(64, '1'), StepImportMode::Reference))));
    CHECK_FALSE(validate_structured_step_import_candidate(project, source().id()));
  }
}

TEST_CASE(
    "Structured occurrence transforms reject nonfinite typed input before candidate publication",
    "[core][structured-step-import]") {
  for (const auto value :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    RigidTransform placement{};
    placement.translation_mm.x = value;
    CHECK_FALSE(ComponentInstance::create(
        ComponentInstanceId("bad"), "Bad", DocumentId("part"), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Free, placement));
    placement = {};
    placement.rotation_deg.z = value;
    CHECK_FALSE(SubassemblyInstance::create(SubassemblyInstanceId("bad"), "Bad",
                                            DocumentId("child"), ComponentVisibility::Visible,
                                            ComponentSuppressionState::Active, placement));
  }
}
