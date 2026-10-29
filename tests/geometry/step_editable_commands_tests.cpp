#include "imported_body_test_fixture.hpp"

#include "blcad/core/project_json.hpp"
#include "blcad/geometry/step_editable_commands.hpp"

#include <BRepPrimAPI_MakeBox.hxx>
#include <cmath>

using namespace imported_body_test;

namespace {
Project project(const PartDocument& registered) {
  auto value = checked(
      Project::create(DocumentId("project.editable"), "Editable imports",
                      checked(AssemblyDocument::create(DocumentId("assembly.root"), "Root"))));
  REQUIRE(value.add_step_import_source(registered.step_import_sources().front()));
  REQUIRE(value.add_imported_topology_catalog(registered.imported_topology_catalogs().front()));
  return value;
}

ImportedTopologyId add_top_fillet(PartDocument& document) {
  const auto& catalog = document.imported_topology_catalogs().front();
  const auto found =
      std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
        return entry.descriptor.kind == ImportedTopologyKind::Edge &&
               entry.descriptor.geometry == ImportedGeometryClass::Line &&
               std::abs(entry.descriptor.centroid.z - 10) < 1e-8;
      });
  REQUIRE(found != catalog.entries().end());
  const auto id = found->id;
  auto reference = checked(EdgeReference::create_imported(
      PartFeatureInputRole::FilletEdge, checked(ImportedTopologyReference::create(
                                            catalog.id(), id, ImportedTopologyCapability::Edge))));
  REQUIRE(document.add_parameter(length("fillet.radius", 0.5)));
  REQUIRE(document.add_fillet_feature(
      checked(FilletFeature::create(FeatureId("fillet"), "Top edge", document.bodies().front().id(),
                                    {reference}, ParameterId("fillet.radius")))));
  return id;
}
} // namespace

TEST_CASE(
    "Direct editable commands create usable persistent bases and explicit Project definitions",
    "[geometry][step-editable-commands][imported-body-recompute]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  const auto original = fixture.source_bytes();
  const auto registered = fixture.editable();
  const auto& source = registered.step_import_sources().front();
  const auto& catalog = registered.imported_topology_catalogs().front();
  const StepEditableCommands commands;
  auto part = checked(commands.create_definition(source, catalog, DocumentId("part.direct"),
                                                 "Direct editable", FeatureId("base.direct"),
                                                 fixture.root));
  CHECK_FALSE(part.is_reference_part());
  REQUIRE(part.imported_body_features().size() == 1);
  CHECK(part.imported_body_features().front().id() == FeatureId("base.direct"));
  CHECK(part.datum_planes().size() == 3);
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(part, shapes));
  CHECK(shapes.feature_shape_count() == 1);
  CHECK(body_volume(shapes, part.bodies().front().id()) == Approx(4000));
  const auto initial = project(registered);
  const auto initial_json = checked(serialize_project_to_json(initial));
  const auto inserted = checked(commands.insert(
      initial, catalog.id(), DocumentId("part.first"), "First", FeatureId("base.first"),
      initial.assembly().id(), ComponentInstanceId("instance.first"), {{5, 6, 7}, {0, 0, 30}},
      fixture.root));
  const auto second = checked(commands.insert(
      inserted.project, catalog.id(), DocumentId("part.second"), "Independent editable copy",
      FeatureId("base.second"), initial.assembly().id(), ComponentInstanceId("instance.second"),
      identity_rigid_transform(), fixture.root));
  CHECK(second.project.part_document_count() == 2);
  CHECK(second.project.assembly().component_instance_count() == 2);
  CHECK(second.project.assembly().component_instances().front().transform().translation_mm.x == 5);
  CHECK(checked(serialize_project_to_json(initial)) == initial_json);
  CHECK_FALSE(commands.insert(initial, catalog.id(), DocumentId("part.invalid"), "Invalid",
                              FeatureId("base"), DocumentId("missing"),
                              ComponentInstanceId("instance.invalid"), identity_rigid_transform(),
                              fixture.root));
  auto reference = checked(
      StepImportSource::create(source.id(), source.name(), source.project_relative_asset_path(),
                               source.sha256_digest(), StepImportMode::Reference));
  CHECK_FALSE(commands.create_definition(reference, catalog, DocumentId("wrong.mode"), "Wrong mode",
                                         FeatureId("base"), fixture.root));
  CHECK(fixture.source_bytes() == original);
}

TEST_CASE("Editable shared refresh validates downstream features and all same-asset owner bindings",
          "[geometry][step-editable-commands][imported-body-downstream-feature]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto part = fixture.editable();
  add_top_fillet(part);
  StepImportTopologyContext before_context(fixture.root);
  auto before_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{&before_context}.execute_document(part, before_cache));
  const auto volume_before = body_volume(before_cache, part.bodies().front().id());
  auto owner = project(part);
  REQUIRE(owner.add_part_document(part));
  std::filesystem::create_hard_link(fixture.path(), fixture.root / "supplier-alias.step");
  REQUIRE(owner.add_step_import_source(checked(StepImportSource::create(
      StepImportSourceId("alias"), "Hardlink metadata", "supplier-alias.step",
      part.step_import_sources().front().sha256_digest(), StepImportMode::Reference))));
  const auto before = checked(serialize_project_to_json(owner));
  auto revised = fixture.source_bytes();
  revised.insert(revised.find("ENDSEC;"), "/* revision B */\n");
  fixture.write_bytes(revised);
  const auto refreshed = checked(StepEditableCommands{}.refresh(
      owner, part.id(), part.step_import_sources().front().id(), fixture.root));
  CHECK(refreshed.affected_owners.size() == 2);
  CHECK(refreshed.diagnostics.size() == 3);
  const auto* changed = refreshed.project.find_part_document(part.id());
  REQUIRE(changed);
  CHECK(changed->imported_body_features().front().definition() ==
        part.imported_body_features().front().definition());
  CHECK(changed->fillet_features().front().id() == FeatureId("fillet"));
  CHECK(changed->imported_topology_catalogs().front() == part.imported_topology_catalogs().front());
  CHECK(refreshed.project.find_step_import_source(StepImportSourceId("alias"))->sha256_digest() ==
        changed->step_import_sources().front().sha256_digest());
  StepImportTopologyContext after_context(fixture.root);
  auto after_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{&after_context}.execute_document(*changed, after_cache));
  CHECK(body_volume(after_cache, changed->bodies().front().id()) == Approx(volume_before));
  CHECK(checked(serialize_project_to_json(owner)) == before);
}

TEST_CASE(
    "Editable refresh rejects lost downstream topology despite a uniquely recovered base Body",
    "[geometry][step-editable-commands][imported-body-downstream-feature]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto part = fixture.editable();
  const auto edge = add_top_fillet(part);
  StepImportTopologyContext before_context(fixture.root);
  auto before_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{&before_context}.execute_document(part, before_cache));
  const auto before = checked(serialize_part_document_to_json(part));
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 14).Shape());
  auto accepted = part;
  accept_current(accepted, fixture);
  StepImportTopologyContext context(fixture.root);
  const auto recovery =
      checked(context.recover(accepted, accepted.imported_topology_catalogs().front().id()));
  REQUIRE(recovery->find(edge));
  CHECK(recovery->find(edge)->status == ImportedTopologyRecoveryStatus::Lost);
  const auto body =
      part.imported_body_features().front().definition().outputs().front().imported_body;
  REQUIRE(recovery->find(body));
  CHECK(recovery->find(body)->status == ImportedTopologyRecoveryStatus::Resolved);
  CHECK_FALSE(StepEditableCommands{}.refresh(part, {}, fixture.root));
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  auto owner = project(part);
  REQUIRE(owner.add_part_document(part));
  const auto project_before = checked(serialize_project_to_json(owner));
  CHECK_FALSE(StepEditableCommands{}.refresh(owner, part.id(), {}, fixture.root));
  CHECK(checked(serialize_project_to_json(owner)) == project_before);
}
