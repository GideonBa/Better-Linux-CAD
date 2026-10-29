#include "blcad/geometry/step_reference_commands.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/reference_part_geometry.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/geometry/body_boolean_adapter.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iterator>
#include <unistd.h>

using namespace blcad;
using namespace blcad::geometry;
namespace {
template <class T> T require_value(Result<T> value) {
  INFO((value ? "ok" : value.error().message()));
  REQUIRE(value);
  return std::move(value.value());
}
struct ReferenceFixture {
  std::filesystem::path root;
  StepImportSource source;
  ImportedTopologyCatalog catalog;
  ReferenceFixture() : root(make_root()), source(make_source()), catalog(make_catalog()) {}
  ~ReferenceFixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  static std::filesystem::path make_root() {
    static std::atomic<unsigned> sequence{};
    auto path = std::filesystem::temp_directory_path() /
                ("blcad-reference-commands-" + std::to_string(::getpid()) + "-" +
                 std::to_string(++sequence));
    REQUIRE(std::filesystem::create_directories(path));
    return path;
  }
  StepImportSource make_source() {
    auto shape = require_value(RectangleExtrusionAdapter{}.make_extruded_rectangle(
        require_value(Quantity::length_mm(20)), require_value(Quantity::length_mm(10)),
        require_value(Quantity::length_mm(5))));
    REQUIRE(StepExporter{}.write_step(shape, (root / "supplier.step").string()));
    auto inspected = require_value(inspect_step_import_asset(root, "supplier.step"));
    REQUIRE(inspected.observed_sha256_digest());
    return require_value(
        StepImportSource::create(StepImportSourceId("source.supplier"), "Supplier", "supplier.step",
                                 *inspected.observed_sha256_digest(), StepImportMode::Reference));
  }
  ImportedTopologyCatalog make_catalog() const {
    auto geometry = require_value(StepImportReader{}.read(source, root));
    return require_value(StepImportTopology{}.create_catalog(
        ImportedTopologyCatalogId("catalog.product"), geometry, 0));
  }
  Project project() const {
    auto assembly =
        require_value(AssemblyDocument::create(DocumentId("assembly.root"), "Assembly"));
    auto result =
        require_value(Project::create(DocumentId("project.reference"), "Reference", assembly));
    REQUIRE(result.add_step_import_source(source));
    REQUIRE(result.add_imported_topology_catalog(catalog));
    REQUIRE(result.add_child_assembly_document(
        require_value(AssemblyDocument::create(DocumentId("assembly.child"), "Nested assembly"))));
    REQUIRE(result.assembly().add_subassembly_instance(require_value(
        SubassemblyInstance::create(SubassemblyInstanceId("subassembly.child"), "Nested occurrence",
                                    DocumentId("assembly.child")))));
    return result;
  }
  void change_metadata() const {
    std::ifstream input(root / "supplier.step", std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(input), {});
    auto position = bytes.find("ENDSEC;");
    REQUIRE(position != std::string::npos);
    bytes.insert(position, "/* Supplier revision B: geometry unchanged */\n");
    input.close();
    std::ofstream output(root / "supplier.step", std::ios::binary | std::ios::trunc);
    output << bytes;
    REQUIRE(output.good());
  }
};
} // namespace

TEST_CASE("Reference commands atomically create definitions and reuse exact bindings in nested "
          "occurrences",
          "[geometry][step-reference-commands][block140-reference-parts]") {
  ReferenceFixture fixture;
  const StepReferenceCommands commands;
  const auto initial = fixture.project();
  const auto before = require_value(serialize_project_to_json(initial));
  auto inserted = require_value(
      commands.insert(initial, fixture.catalog.id(), DocumentId("part.reference"), "Supplier Part",
                      initial.assembly().id(), ComponentInstanceId("occurrence.first"),
                      identity_rigid_transform(), fixture.root));
  CHECK(inserted.project.part_document_count() == 1);
  const auto* reference = inserted.project.find_part_document(inserted.part);
  REQUIRE(reference);
  CHECK(reference->is_reference_part());
  CHECK(reference->features().empty());
  CHECK(reference->imported_body_features().empty());
  CHECK(reference->reference_part_definition()->outputs().size() == 1);
  CHECK(require_value(serialize_project_to_json(initial)) == before);
  auto repeated = require_value(commands.repeat(
      inserted.project, inserted.part, DocumentId("assembly.child"),
      ComponentInstanceId("occurrence.child"), {{60, 0, 0}, {0, 0, 30}}, fixture.root));
  CHECK(repeated.project.part_document_count() == 1);
  const auto* child = repeated.project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(child);
  REQUIRE(child->component_instances().size() == 1);
  CHECK(child->component_instances().front().referenced_part_document() == inserted.part);
  CHECK(child->component_instances().front().transform().translation_mm.x == 60);
  auto reused = require_value(commands.insert(
      repeated.project, fixture.catalog.id(), DocumentId("part.unused"), "Different requested name",
      initial.assembly().id(), ComponentInstanceId("occurrence.second"), identity_rigid_transform(),
      fixture.root));
  CHECK(reused.part == inserted.part);
  CHECK(reused.project.part_document_count() == 1);
  CHECK(reused.project.find_part_document(DocumentId("part.unused")) == nullptr);
  CHECK_FALSE(commands.repeat(reused.project, inserted.part, initial.assembly().id(),
                              ComponentInstanceId("occurrence.second"), identity_rigid_transform(),
                              fixture.root));
  const auto saved = require_value(serialize_project_to_json(reused.project));
  const auto loaded = require_value(deserialize_project_from_json(saved));
  CHECK(require_value(serialize_project_to_json(loaded)) == saved);
  StepImportTopologyContext context(fixture.root);
  auto cache = require_value(ShapeCache::create(ShapeCacheId("reference.scene")));
  REQUIRE(ReferencePartGeometry{}.seed_reference(*loaded.find_part_document(inserted.part), context,
                                                 cache));
  CHECK_FALSE(ViewportSceneBuilder{}.build_part(*loaded.find_part_document(inserted.part), cache));
  const auto scene = require_value(
      ViewportSceneBuilder{&context}.build_part(*loaded.find_part_document(inserted.part), cache));
  CHECK(std::count_if(scene.begin(), scene.end(), [](const auto& item) {
          return item.kind == ViewportSceneKind::SolidBody;
        }) == 1);
  auto project_scene = require_value(ViewportSceneBuilder{&context}.build_project(loaded));
  CHECK(project_scene.size() == 3);
  fixture.change_metadata();
  CHECK_FALSE(
      ViewportSceneBuilder{&context}.build_part(*loaded.find_part_document(inserted.part), cache));
  CHECK_FALSE(ViewportSceneBuilder{&context}.build_project(loaded));
}

TEST_CASE(
    "Reference refresh includes all copied source owners and preserves baseline and placement",
    "[geometry][step-reference-commands][block140-reference-parts]") {
  ReferenceFixture fixture;
  const StepReferenceCommands commands;
  auto inserted = require_value(commands.insert(
      fixture.project(), fixture.catalog.id(), DocumentId("part.reference"), "Supplier Part",
      DocumentId("assembly.root"), ComponentInstanceId("occurrence.first"),
      {{7, 8, 9}, {10, 20, 30}}, fixture.root));
  std::filesystem::create_hard_link(fixture.root / "supplier.step",
                                    fixture.root / "supplier-alias.step");
  const auto alias = require_value(StepImportSource::create(
      StepImportSourceId("source.alias"), "Explicit hardlink binding", "supplier-alias.step",
      fixture.source.sha256_digest(), StepImportMode::EditableBody));
  REQUIRE(inserted.project.add_step_import_source(alias));
  REQUIRE(inserted.project.add_part_document(require_value(
      commands.create_definition(fixture.source, fixture.catalog, DocumentId("part.other"),
                                 "Other copied owner", fixture.root))));
  const auto before = require_value(serialize_project_to_json(inserted.project));
  const auto baseline =
      *inserted.project.find_part_document(inserted.part)->reference_part_definition();
  fixture.change_metadata();
  CHECK_FALSE(commands.repeat(inserted.project, inserted.part, DocumentId("assembly.root"),
                              ComponentInstanceId("occurrence.stale"), identity_rigid_transform(),
                              fixture.root));
  auto refreshed = require_value(commands.refresh(inserted.project, inserted.part, fixture.root));
  REQUIRE(refreshed.affected_owners.size() == 3);
  REQUIRE(refreshed.diagnostics.size() == 4);
  const auto* reference = refreshed.project.find_part_document(inserted.part);
  REQUIRE(reference);
  CHECK(*reference->reference_part_definition() == baseline);
  CHECK(*reference->find_imported_topology_catalog(fixture.catalog.id()) == fixture.catalog);
  CHECK(refreshed.project.find_step_import_source(fixture.source.id())->sha256_digest() !=
        fixture.source.sha256_digest());
  CHECK(reference->find_step_import_source(fixture.source.id())->sha256_digest() ==
        refreshed.project.find_step_import_source(fixture.source.id())->sha256_digest());
  const auto* refreshed_alias = refreshed.project.find_step_import_source(alias.id());
  REQUIRE(refreshed_alias);
  CHECK(refreshed_alias->mode() == alias.mode());
  CHECK(refreshed_alias->project_relative_asset_path() == alias.project_relative_asset_path());
  CHECK(refreshed_alias->sha256_digest() ==
        reference->find_step_import_source(fixture.source.id())->sha256_digest());
  CHECK(refreshed.project.find_part_document(DocumentId("part.other"))
            ->find_step_import_source(fixture.source.id())
            ->sha256_digest() == refreshed_alias->sha256_digest());
  CHECK(refreshed.project.assembly().component_instances().front().transform() ==
        inserted.project.assembly().component_instances().front().transform());
  CHECK(require_value(serialize_project_to_json(inserted.project)) == before);
}

TEST_CASE("Reference conversion creates a new usable uninstantiated Editable definition",
          "[geometry][step-reference-commands][block140-reference-parts]") {
  ReferenceFixture fixture;
  const StepReferenceCommands commands;
  auto inserted = require_value(commands.insert(
      fixture.project(), fixture.catalog.id(), DocumentId("part.reference"), "Supplier Part",
      DocumentId("assembly.root"), ComponentInstanceId("occurrence.first"),
      identity_rigid_transform(), fixture.root));
  const auto original_reference = require_value(
      serialize_part_document_to_json(*inserted.project.find_part_document(inserted.part)));
  auto converted = require_value(
      commands.convert(inserted.project, inserted.part, DocumentId("part.editable"),
                       "Editable Copy", FeatureId("feature.imported.base"), fixture.root));
  REQUIRE(converted.project.part_document_count() == 2);
  const auto* editable = converted.project.find_part_document(converted.part);
  REQUIRE(editable);
  CHECK_FALSE(editable->is_reference_part());
  REQUIRE(editable->imported_body_features().size() == 1);
  CHECK(editable->find_step_import_source(fixture.source.id())->mode() ==
        StepImportMode::EditableBody);
  CHECK(converted.project.assembly().component_instances().front().referenced_part_document() ==
        inserted.part);
  CHECK_FALSE(converted.project.assembly().has_member_part(converted.part));
  CHECK(require_value(serialize_part_document_to_json(
            *converted.project.find_part_document(inserted.part))) == original_reference);
  StepImportTopologyContext context(fixture.root);
  auto geometry = require_value(ReferencePartGeometry{}.resolve_imported_body_feature(
      *editable, editable->imported_body_features().front(), context));
  REQUIRE(geometry.bodies.size() == 1);
  CHECK_FALSE(geometry.bodies.front().shape.empty());
  CHECK(geometry.bodies.front().output.body != inserted.project.find_part_document(inserted.part)
                                                   ->reference_part_definition()
                                                   ->outputs()
                                                   .front()
                                                   .body);
  CHECK_FALSE(commands.convert(converted.project, inserted.part, converted.part, "Existing ID",
                               FeatureId("feature.base"), fixture.root));
}

TEST_CASE("Selected-product insertion never reuses a partial Reference Body mapping",
          "[geometry][step-reference-commands][block140-reference-parts]") {
  ReferenceFixture fixture;
  const RectangleExtrusionAdapter shapes;
  auto first = require_value(shapes.make_extruded_rectangle(require_value(Quantity::length_mm(20)),
                                                            require_value(Quantity::length_mm(10)),
                                                            require_value(Quantity::length_mm(5))));
  auto second = require_value(shapes.make_extruded_rectangle(
      require_value(Quantity::length_mm(5)), require_value(Quantity::length_mm(5)),
      require_value(Quantity::length_mm(5))));
  second = require_value(BodyTransformAdapter{}.translate(second, {50, 0, 0}));
  const auto compound = require_value(BodyBooleanAdapter{}.combine(
      FeatureId("supplier.compound"), BodyBooleanOperation::Add, first, {second}));
  // This fixture is one product containing two Bodies. The raw STEPControl
  // writer exports a compound as an assembly and two separate part products.
  Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
  XCAFDoc_DocumentTool::Set(document->Main());
  XCAFDoc_DocumentTool::SetLengthUnit(document, 1, UnitsMethods_LengthUnit_Millimeter);
  const auto native = OcctShapeView::get(compound);
  REQUIRE(native);
  const auto tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
  REQUIRE_FALSE(tool->AddShape(*native, false, false).IsNull());
  STEPCAFControl_Controller::Init();
  const std::string old_unit = Interface_Static::CVal("write.step.unit");
  REQUIRE(Interface_Static::SetCVal("write.step.unit", "MM"));
  STEPCAFControl_Writer writer;
  const bool transferred = writer.Transfer(document, STEPControl_AsIs);
  Interface_Static::SetCVal("write.step.unit", old_unit.c_str());
  REQUIRE(transferred);
  REQUIRE(writer.Write((fixture.root / "supplier.step").c_str()) == IFSelect_RetDone);
  const auto report = require_value(inspect_step_import_asset(fixture.root, "supplier.step"));
  const auto source =
      require_value(fixture.source.with_sha256_digest(*report.observed_sha256_digest()));
  const auto imported = require_value(StepImportReader{}.read(source, fixture.root));
  REQUIRE(imported.products.size() == 1);
  REQUIRE(imported.body_count() == 2);
  const auto catalog = require_value(StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("catalog.compound"), imported, 0));
  std::vector<ImportedBodyOutput> outputs;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Body)
      outputs.push_back({entry.id, BodyId("body.subset"), *entry.descriptor.body_kind});
  REQUIRE(outputs.size() == 2);
  outputs.resize(1);
  const auto subset = require_value(PartDocument::create_reference(
      DocumentId("part.subset"), "Explicit subset", source, catalog,
      require_value(ImportedPartDefinition::create(source.id(), catalog.id(), outputs))));
  const auto subset_json = require_value(serialize_part_document_to_json(subset));
  auto project = require_value(Project::create(
      DocumentId("project.compound"), "Compound",
      require_value(AssemblyDocument::create(DocumentId("assembly.root"), "Root"))));
  REQUIRE(project.add_step_import_source(source));
  REQUIRE(project.add_imported_topology_catalog(catalog));
  REQUIRE(project.add_part_document(subset));
  const StepReferenceCommands commands;
  auto repeat = require_value(commands.repeat(project, subset.id(), project.assembly().id(),
                                              ComponentInstanceId("subset.occurrence"),
                                              identity_rigid_transform(), fixture.root));
  REQUIRE(repeat.project.part_document_count() == 1);
  auto full = require_value(commands.insert(repeat.project, catalog.id(), DocumentId("part.full"),
                                            "Complete selected product", project.assembly().id(),
                                            ComponentInstanceId("full.first"),
                                            identity_rigid_transform(), fixture.root));
  REQUIRE(full.project.part_document_count() == 2);
  CHECK(full.part == DocumentId("part.full"));
  REQUIRE(
      full.project.find_part_document(full.part)->reference_part_definition()->outputs().size() ==
      2);
  CHECK(require_value(serialize_part_document_to_json(
            *full.project.find_part_document(subset.id()))) == subset_json);
  auto reused = require_value(commands.insert(
      full.project, catalog.id(), DocumentId("part.unused"), "Complete selected product again",
      project.assembly().id(), ComponentInstanceId("full.second"), identity_rigid_transform(),
      fixture.root));
  CHECK(reused.part == full.part);
  CHECK(reused.project.part_document_count() == 2);
}
