#include "blcad/core/project_json.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_import_validation.hpp"
#include "blcad/geometry/assembly_interaction.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/step_reference_commands.hpp"

#include <TopoDS_Shape.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <unistd.h>

using namespace blcad;
using namespace blcad::geometry;

namespace {
template <class T> T checked(Result<T> value) {
  INFO((value ? "ok" : value.error().message()));
  REQUIRE(value);
  return std::move(value.value());
}
struct Fixture {
  std::filesystem::path root;
  Fixture() {
    char path[] = "/tmp/blcad-reference-interaction-XXXXXX";
    REQUIRE(::mkdtemp(path));
    root = path;
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  PartDocument reference() const {
    const auto shape = checked(RectangleExtrusionAdapter{}.make_extruded_rectangle(
        checked(Quantity::length_mm(24)), checked(Quantity::length_mm(11)),
        checked(Quantity::length_mm(7))));
    REQUIRE(StepExporter{}.write_step(shape, (root / "supplier.step").string()));
    const auto asset = checked(inspect_step_import_asset(root, "supplier.step"));
    REQUIRE(asset.observed_sha256_digest());
    const auto source = checked(
        StepImportSource::create(StepImportSourceId("supplier"), "Supplier", "supplier.step",
                                 *asset.observed_sha256_digest(), StepImportMode::Reference));
    const auto geometry = checked(StepImportReader{}.read(source, root));
    const auto catalog = checked(
        StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog"), geometry, 0));
    return checked(StepReferenceCommands{}.create_definition(
        source, catalog, DocumentId("reference"), "Reference", root));
  }
};
void same_bounds(const GeometryShapeBounds& actual, const GeometryShapeBounds& expected) {
  CHECK(actual.minimum.x == Catch::Approx(expected.minimum.x).margin(1e-6));
  CHECK(actual.minimum.y == Catch::Approx(expected.minimum.y).margin(1e-6));
  CHECK(actual.minimum.z == Catch::Approx(expected.minimum.z).margin(1e-6));
  CHECK(actual.maximum.x == Catch::Approx(expected.maximum.x).margin(1e-6));
  CHECK(actual.maximum.y == Catch::Approx(expected.maximum.y).margin(1e-6));
  CHECK(actual.maximum.z == Catch::Approx(expected.maximum.z).margin(1e-6));
}
} // namespace

TEST_CASE("Reference interaction presents exact trimmed topology with stable posed identities",
          "[geometry][assembly-step-reference-part][reference-part-interaction]") {
  Fixture fixture;
  const auto part = fixture.reference();
  auto assembly = checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
  REQUIRE(assembly.add_member_part(part.id()));
  const RigidTransform transform{{37, -19, 8}, {20, 35, 70}};
  for (const auto* id : {"first", "second"})
    REQUIRE(assembly.add_component_instance(checked(ComponentInstance::create(
        ComponentInstanceId(id), id, part.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Free, transform))));
  auto project = checked(Project::create(DocumentId("project"), "Project", assembly));
  REQUIRE(project.add_part_document(part));
  const auto before = checked(serialize_project_to_json(project));
  CHECK_FALSE(AssemblyInteractionGeometry::targets(project));
  StepImportTopologyContext context(fixture.root);
  const auto targets = checked(AssemblyInteractionGeometry::targets(project, &context));
  std::set<std::string> identities;
  std::size_t faces = 0, edges = 0, vertices = 0, planes = 0, frames = 0;
  for (const auto& target : targets) {
    CHECK(identities.insert(target.presentation.semantic_id).second);
    if (!target.resolved.source_metadata.imported_topology) {
      if (std::holds_alternative<AssemblyPlanarTargetDescriptor>(target.resolved.descriptor)) {
        CHECK(is_assembly_reference_target_spelling(target.reference.semantic_reference()));
        ++planes;
      } else {
        CHECK(std::holds_alternative<AssemblyFrameTargetDescriptor>(target.resolved.descriptor));
        CHECK(is_assembly_frame_target_spelling(target.reference.semantic_reference()));
        ++frames;
      }
      continue;
    }
    const auto& reference = *target.resolved.source_metadata.imported_topology;
    CHECK_FALSE(target.resolved.source_metadata.source_feature);
    const auto recovered = checked(context.resolve(part, reference));
    const auto* native = OcctShapeView::get(target.presentation.shape);
    const auto* original = OcctShapeView::get(recovered.shape);
    REQUIRE(native);
    REQUIRE(original);
    CHECK(native->ShapeType() == original->ShapeType());
    auto posed = recovered.shape;
    for (auto [axis, angle] :
         std::vector<std::pair<Vector3, double>>{{{1, 0, 0}, 20}, {{0, 1, 0}, 35}, {{0, 0, 1}, 70}})
      posed = checked(BodyTransformAdapter{}.rotate(posed, {}, axis, angle));
    posed = checked(BodyTransformAdapter{}.translate(posed, transform.translation_mm));
    same_bounds(checked(BodyTransformAdapter{}.bounds(target.presentation.shape)),
                checked(BodyTransformAdapter{}.bounds(posed)));
    if (native->ShapeType() == TopAbs_FACE)
      ++faces;
    else if (native->ShapeType() == TopAbs_EDGE)
      ++edges;
    else if (native->ShapeType() == TopAbs_VERTEX)
      ++vertices;
    else
      FAIL("Assembly analytic input must present its actual face, edge or vertex");
  }
  CHECK(faces == 12);
  CHECK(edges == 24);
  CHECK(vertices == 16);
  CHECK(planes == 6);
  CHECK(frames == 6);
  CHECK(checked(serialize_project_to_json(project)) == before);
  REQUIRE(validate_imported_assembly_inputs(project, &context));
  {
    std::ofstream changed(fixture.root / "supplier.step", std::ios::app);
    changed << "\n/* Unaccepted revision */\n";
  }
  CHECK_FALSE(AssemblyInteractionGeometry::targets(project, &context));
  StepImportTopologyContext changed(fixture.root);
  CHECK_FALSE(AssemblyInteractionGeometry::targets(project, &changed));
  CHECK_FALSE(validate_imported_assembly_inputs(project, &changed));
  CHECK(checked(serialize_project_to_json(project)) == before);
}

TEST_CASE("Imported input guard does not add global validation to legacy local subsets",
          "[geometry][assembly-step-reference-part][reference-part-interaction]") {
  auto assembly = checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
  REQUIRE(assembly.add_member_part(DocumentId("unloaded.legacy.part")));
  const auto project = checked(Project::create(DocumentId("project"), "Project", assembly));
  CHECK(checked(validate_imported_assembly_inputs(project, nullptr)) == 0);
  CHECK(checked(validate_imported_assembly_inputs(project, nullptr,
                                                  {DocumentId("unloaded.legacy.part")})) == 0);
}

TEST_CASE("Filtered imported validation never reads an unrelated child endpoint asset",
          "[geometry][assembly-step-reference-part][reference-part-interaction]") {
  const auto occurrence = GENERATE(0, 1, 2);
  INFO("child occurrence: " << occurrence);
  Fixture fixture;
  const auto reference = fixture.reference();
  const auto& original_source = reference.step_import_sources().front();
  const auto missing_source = checked(StepImportSource::create(
      original_source.id(), "Unavailable unrelated supplier", "missing.step",
      original_source.sha256_digest(), StepImportMode::Reference));
  const auto unrelated = checked(PartDocument::create_reference(
      DocumentId("unrelated"), "Unrelated Reference", missing_source,
      reference.imported_topology_catalogs().front(), *reference.reference_part_definition()));
  auto child = checked(AssemblyDocument::create(DocumentId("child"), "Child"));
  REQUIRE(child.add_member_part(reference.id()));
  REQUIRE(child.add_member_part(unrelated.id()));
  REQUIRE(child.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("child.reference"), "Reference", reference.id()))));
  REQUIRE(child.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("child.unrelated"), "Unrelated", unrelated.id()))));
  const auto origin = checked(make_assembly_reference_target_spelling(DatumPlaneId("datum.xy")));
  REQUIRE(child.add_constraint(checked(AssemblyConstraint::create(
      AssemblyConstraintId("child.mate"), "Child mate", AssemblyConstraintType::Mate,
      checked(AssemblyConstraintTarget::create(ComponentInstanceId("child.reference"), origin)),
      checked(AssemblyConstraintTarget::create(ComponentInstanceId("child.unrelated"), origin))))));
  auto root = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(root.add_member_part(reference.id()));
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("visible.reference"), "Visible Reference", reference.id()))));
  if (occurrence != 0)
    REQUIRE(root.add_subassembly_instance(checked(SubassemblyInstance::create(
        SubassemblyInstanceId("child.instance"), "Child", child.id(), ComponentVisibility::Visible,
        occurrence == 1 ? ComponentSuppressionState::Suppressed
                        : ComponentSuppressionState::Active))));
  auto project = checked(Project::create(DocumentId("project"), "Project", root));
  REQUIRE(project.add_part_document(reference));
  REQUIRE(project.add_part_document(unrelated));
  REQUIRE(project.add_child_assembly_document(child));
  const auto before = checked(serialize_project_to_json(project));
  StepImportTopologyContext filtered(fixture.root);
  REQUIRE(validate_imported_assembly_inputs(project, &filtered, {reference.id()}));
  CHECK(checked(filtered.validate_sources()) == 1);
  StepImportTopologyContext full(fixture.root);
  CHECK_FALSE(validate_imported_assembly_inputs(project, &full));
  CHECK_FALSE(validate_imported_assembly_inputs(project, &full, {reference.id(), unrelated.id()}));
  CHECK(checked(serialize_project_to_json(project)) == before);
}

TEST_CASE(
    "Filtered imported validation evaluates shared-Part child intent only on active rooted paths",
    "[geometry][assembly-step-reference-part][reference-part-interaction]") {
  const auto occurrence = GENERATE(0, 1, 2);
  INFO("child occurrence: " << occurrence);
  Fixture fixture;
  const auto part = fixture.reference();
  auto child = checked(AssemblyDocument::create(DocumentId("child"), "Child"));
  REQUIRE(child.add_member_part(part.id()));
  for (const auto* id : {"a", "b"})
    REQUIRE(child.add_component_instance(
        checked(ComponentInstance::create(ComponentInstanceId(id), id, part.id()))));
  const auto lost =
      checked(make_imported_topology_target_spelling(checked(ImportedTopologyReference::create(
          part.reference_part_definition()->catalog(), ImportedTopologyId("absent.face"),
          ImportedTopologyCapability::Plane))));
  const auto origin = checked(make_assembly_reference_target_spelling(DatumPlaneId("datum.xy")));
  REQUIRE(child.add_constraint(checked(AssemblyConstraint::create(
      AssemblyConstraintId("lost.child.mate"), "Lost child face", AssemblyConstraintType::Mate,
      checked(AssemblyConstraintTarget::create(ComponentInstanceId("a"), lost)),
      checked(AssemblyConstraintTarget::create(ComponentInstanceId("b"), origin))))));
  auto root = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(root.add_member_part(part.id()));
  REQUIRE(root.add_component_instance(checked(
      ComponentInstance::create(ComponentInstanceId("root.part"), "Root Part", part.id()))));
  if (occurrence != 0)
    REQUIRE(root.add_subassembly_instance(checked(SubassemblyInstance::create(
        SubassemblyInstanceId("child.instance"), "Child", child.id(), ComponentVisibility::Visible,
        occurrence == 1 ? ComponentSuppressionState::Suppressed
                        : ComponentSuppressionState::Active))));
  auto project = checked(Project::create(DocumentId("project"), "Project", root));
  REQUIRE(project.add_part_document(part));
  REQUIRE(project.add_child_assembly_document(child));
  StepImportTopologyContext filtered(fixture.root);
  const auto valid = validate_imported_assembly_inputs(project, &filtered, {part.id()});
  CHECK(static_cast<bool>(valid) == (occurrence != 2));
  StepImportTopologyContext full(fixture.root);
  CHECK_FALSE(validate_imported_assembly_inputs(project, &full));
}
