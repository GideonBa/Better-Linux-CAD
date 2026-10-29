#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/core/structured_step_import.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_hierarchy_spherical_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_import_validation.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/step_structured_import_commands.hpp"

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPConstruct_ExternRefs.hxx>
#include <StepBasic_ProductDefinition.hxx>
#include <StepData_StepModel.hxx>
#include <StepRepr_AssemblyComponentUsage.hxx>
#include <StepRepr_NextAssemblyUsageOccurrence.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TCollection_HAsciiString.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Ax1.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <numbers>
#include <regex>
#include <set>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;
namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().object_id() + ": " + result.error().message());
  return std::move(result.value());
}
std::string bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  REQUIRE(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}
gp_Trsf pose(Vector3 translation = {}, Vector3 degrees = {}) {
  gp_Trsf x, y, z, t;
  x.SetRotation(gp_Ax1({}, gp_Dir(1, 0, 0)), degrees.x * std::numbers::pi / 180);
  y.SetRotation(gp_Ax1({}, gp_Dir(0, 1, 0)), degrees.y * std::numbers::pi / 180);
  z.SetRotation(gp_Ax1({}, gp_Dir(0, 0, 1)), degrees.z * std::numbers::pi / 180);
  t.SetTranslation(gp_Vec(translation.x, translation.y, translation.z));
  return t * z * y * x;
}
struct Fixture {
  std::filesystem::path root;
  Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
  Handle(XCAFDoc_ShapeTool) tool;
  TDF_Label nested_rotor;
  Fixture() {
    char name[] = "/tmp/blcad-step-structured-XXXXXX";
    const auto* directory = ::mkdtemp(name);
    REQUIRE(directory);
    root = directory;
    XCAFDoc_DocumentTool::Set(document->Main());
    XCAFDoc_DocumentTool::SetLengthUnit(document, 1, UnitsMethods_LengthUnit_Millimeter);
    tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  std::filesystem::path path() const {
    return root / "asset.step";
  }
  TDF_Label part(const TopoDS_Shape& shape, const char* name) {
    auto result = tool->AddShape(shape, false, false);
    REQUIRE(!result.IsNull());
    TDataStd_Name::Set(result, TCollection_ExtendedString(name, true));
    return result;
  }
  TDF_Label assembly(const char* name) {
    auto result = tool->NewShape();
    TDataStd_Name::Set(result, TCollection_ExtendedString(name, true));
    return result;
  }
  void occurrence(TDF_Label parent, TDF_Label child, const char* name, gp_Trsf transform = {}) {
    auto result = tool->AddComponent(parent, child, TopLoc_Location(transform));
    REQUIRE(!result.IsNull());
    TDataStd_Name::Set(result, TCollection_ExtendedString(name, true));
  }
  void nested(bool reverse = false) {
    TDF_Label box, pin;
    if (reverse) {
      pin = part(BRepPrimAPI_MakeCylinder(1, 4).Shape(), "Pin");
      box = part(BRepPrimAPI_MakeBox(2, 3, 5).Shape(), "Rotor");
    } else {
      box = part(BRepPrimAPI_MakeBox(2, 3, 5).Shape(), "Rotor");
      pin = part(BRepPrimAPI_MakeCylinder(1, 4).Shape(), "Pin");
    }
    nested_rotor = box;
    auto module = assembly("Module");
    const auto rotor = [&] {
      occurrence(module, box, "Rotor seat", pose({7, 1, 3}, {11, 22, 33}));
    };
    const auto bearing = [&] {
      occurrence(module, pin, "Pin seat", pose({1, -2, 0}, {0, 0, -15}));
    };
    if (reverse) {
      bearing();
      rotor();
    } else {
      rotor();
      bearing();
    }
    auto machine = assembly("Machine");
    const auto first = [&] {
      occurrence(machine, module, "Module A", pose({40, -10, 2}, {90, 0, 0}));
    };
    const auto second = [&] {
      occurrence(machine, module, "Module B", pose({-5, 20, 8}, {0, 90, 25}));
    };
    const auto direct = [&] {
      occurrence(machine, box, "Spare rotor", pose({0, 0, -20}, {0, 0, 45}));
    };
    if (reverse) {
      direct();
      second();
      first();
    } else {
      first();
      second();
      direct();
    }
  }
  void write(const char* unit = "MM",
             std::function<void(const Handle(StepData_StepModel) &)> mutate = {},
             bool external = false) {
    tool->UpdateAssemblies();
    STEPCAFControl_Controller::Init();
    const std::string previous_unit = Interface_Static::CVal("write.step.unit");
    const std::string previous_schema = Interface_Static::CVal("write.step.schema");
    REQUIRE(Interface_Static::SetCVal("write.step.unit", unit));
    REQUIRE(Interface_Static::SetCVal("write.step.schema", "AP214IS"));
    STEPCAFControl_Writer writer;
    const bool transferred = writer.Transfer(document, STEPControl_AsIs);
    Interface_Static::SetCVal("write.step.unit", previous_unit.c_str());
    Interface_Static::SetCVal("write.step.schema", previous_schema.c_str());
    REQUIRE(transferred);
    auto model = writer.ChangeWriter().Model();
    if (mutate)
      mutate(model);
    if (external) {
      Handle(StepBasic_ProductDefinition) product;
      for (int index = 1; index <= model->NbEntities() && product.IsNull(); ++index)
        product = Handle(StepBasic_ProductDefinition)::DownCast(model->Value(index));
      REQUIRE(!product.IsNull());
      STEPConstruct_ExternRefs refs(writer.ChangeWriter().WS());
      REQUIRE(refs.AddExternRef("../../unregistered.step", product, "STEP AP214") > 0);
      REQUIRE(refs.WriteExternRefs(1) > 0);
    }
    REQUIRE(writer.Write(path().c_str()) == IFSelect_RetDone);
  }
  StepImportSource source(StepImportMode mode = StepImportMode::Reference) const {
    return checked(StepImportSource::create(StepImportSourceId("source.structured"), "Supplier",
                                            "asset.step",
                                            checked(step_import_sha256(bytes(path()))), mode));
  }
  StepStructuredImportCandidate create(StepImportMode mode = StepImportMode::Reference) const {
    return checked(StepStructuredImportCommands{}.create_project(
        source(mode), DocumentId("imported"), "Structured supplier", root));
  }
};

using SolidRecord = std::array<double, 7>;
std::vector<SolidRecord> solids(const std::vector<GeometryShape>& shapes) {
  std::vector<SolidRecord> result;
  for (const auto& shape : shapes) {
    const auto* topology = OcctShapeView::get(shape);
    REQUIRE(topology);
    TopTools_IndexedMapOfShape entries;
    TopExp::MapShapes(*topology, TopAbs_SOLID, entries);
    for (int index = 1; index <= entries.Extent(); ++index) {
      Bnd_Box bounds;
      BRepBndLib::AddOptimal(entries(index), bounds, false, false);
      SolidRecord record;
      bounds.Get(record[0], record[1], record[2], record[3], record[4], record[5]);
      GProp_GProps properties;
      BRepGProp::VolumeProperties(entries(index), properties);
      record[6] = properties.Mass();
      result.push_back(record);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}
void same_solids(const std::vector<SolidRecord>& first, const std::vector<SolidRecord>& second) {
  REQUIRE(first.size() == second.size());
  for (std::size_t index = 0; index < first.size(); ++index)
    for (std::size_t value = 0; value < 7; ++value)
      CHECK(first[index][value] == Approx(second[index][value]).margin(1e-6));
}
std::vector<SolidRecord> project_solids(const Project& project, const std::filesystem::path& root) {
  StepImportTopologyContext context(root);
  return solids({checked(AssemblyStepExporter{&context}.build_posed_shape(project))});
}
std::vector<SolidRecord> source_solids(const StepImportSource& source,
                                       const std::filesystem::path& root) {
  auto imported = checked(StepImportReader{}.read(source, root));
  return solids(checked(StepImportReader{}.preview_shapes(imported)));
}
nlohmann::json without_capture_hashes(nlohmann::json value) {
  if (value.is_object()) {
    value.erase("sha256_digest");
    value.erase("captured_sha256_digest");
    for (auto& item : value.items())
      item.value() = without_capture_hashes(std::move(item.value()));
  } else if (value.is_array())
    for (auto& item : value)
      item = without_capture_hashes(std::move(item));
  return value;
}

Project supplier_project() {
  auto part = checked(PartDocument::create(DocumentId("supplier.part"), "Shared rotor"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  for (const auto& [id, length] :
       std::vector<std::pair<std::string, double>>{{"width", 2}, {"height", 3}, {"depth", 5}})
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(length))))));
  auto sketch = checked(Sketch::create(SketchId("outline"), "Outline", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("rectangle"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_body(checked(Body::create(BodyId("body"), "Rotor", BodyKind::Solid))));
  auto extrude = checked(Feature::create_additive_extrude(FeatureId("extrude"), "Rotor",
                                                          sketch.id(), ParameterId("depth")));
  extrude = checked(extrude.with_body_result_context(checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body")))));
  REQUIRE(part.add_feature(extrude));
  const auto component = [&](const char* id, RigidTransform transform) {
    return checked(ComponentInstance::create(
        ComponentInstanceId(id), id, part.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Free, transform));
  };
  const auto boundary = [](const char* id, DocumentId child, RigidTransform transform) {
    return checked(SubassemblyInstance::create(SubassemblyInstanceId(id), id, child,
                                               ComponentVisibility::Visible,
                                               ComponentSuppressionState::Active, transform));
  };
  auto rotor = checked(AssemblyDocument::create(DocumentId("supplier.rotor"), "Rotor pair"));
  REQUIRE(rotor.add_member_part(part.id()));
  REQUIRE(rotor.add_component_instance(component("rotor.left", {{0, 0, 0}, {0, 0, 0}})));
  REQUIRE(rotor.add_component_instance(component("rotor.right", {{9, 0, 0}, {0, 0, 25}})));
  auto carrier = checked(AssemblyDocument::create(DocumentId("supplier.carrier"), "Carrier"));
  REQUIRE(carrier.add_subassembly_instance(
      boundary("inner.rotor", rotor.id(), {{0, 12, 4}, {20, 0, 35}})));
  auto assembly = checked(AssemblyDocument::create(DocumentId("supplier.root"), "Supplier"));
  REQUIRE(assembly.add_member_part(part.id()));
  REQUIRE(assembly.add_component_instance(component("direct", {{-35, -20, 0}, {0, 0, -20}})));
  REQUIRE(assembly.add_subassembly_instance(
      boundary("left.carrier", carrier.id(), {{25, 20, 0}, {0, 0, 90}})));
  REQUIRE(assembly.add_subassembly_instance(
      boundary("right.carrier", carrier.id(), {{-30, 30, 10}, {35, -15, 30}})));
  auto project = checked(Project::create(DocumentId("supplier.project"), "Supplier", assembly));
  REQUIRE(project.add_part_document(part));
  REQUIRE(project.add_child_assembly_document(rotor));
  REQUIRE(project.add_child_assembly_document(carrier));
  REQUIRE(project.validate_assembly_structure());
  return project;
}
} // namespace

TEST_CASE("Structured STEP import creates shared nested definitions and equivalent exports",
          "[geometry][structured-step-import]") {
  const auto mode = GENERATE(StepImportMode::Reference, StepImportMode::EditableBody);
  Fixture fixture;
  fixture.nested();
  fixture.write();
  const auto source = fixture.source(mode);
  const auto original = bytes(fixture.path());
  const auto candidate = fixture.create(mode);
  const auto& project = candidate.project;
  REQUIRE(project.part_documents().size() == 2);
  REQUIRE(project.child_assembly_documents().size() == 2);
  CHECK(project.assembly().subassembly_instances().size() == 1);
  CHECK(project.step_import_sources() == std::vector{source});
  CHECK(project.imported_topology_catalogs().empty());
  CHECK(candidate.affected_owners.size() == 6);
  CHECK(checked(AssemblyLeafOccurrenceResolver{}.resolve(project)).size() == 5);
  for (const auto& part : project.part_documents()) {
    CHECK(part.step_import_sources() == std::vector{source});
    CHECK(part.imported_topology_catalogs().size() == 1);
    CHECK(part.is_reference_part() == (mode == StepImportMode::Reference));
    CHECK(part.imported_body_features().size() == (mode == StepImportMode::EditableBody ? 1 : 0));
    CHECK((part.name() == "Rotor" || part.name() == "Pin"));
  }
  const auto module = std::find_if(
      project.child_assembly_documents().begin(), project.child_assembly_documents().end(),
      [](const auto& assembly) { return assembly.name() == "Module"; });
  const auto machine = std::find_if(
      project.child_assembly_documents().begin(), project.child_assembly_documents().end(),
      [](const auto& assembly) { return assembly.name() == "Machine"; });
  REQUIRE(module != project.child_assembly_documents().end());
  REQUIRE(machine != project.child_assembly_documents().end());
  REQUIRE(machine->subassembly_instances().size() == 2);
  CHECK(machine->subassembly_instances()[0].referenced_assembly_document() == module->id());
  CHECK(machine->subassembly_instances()[1].referenced_assembly_document() == module->id());
  CHECK(machine->constraints().empty());
  CHECK(machine->joints().empty());
  const auto json = checked(serialize_project_to_json(project));
  CHECK(json.find("definition_key") == std::string::npos);
  CHECK(json.find("product_definition") == std::string::npos);
  CHECK(json.find("ISO-10303-21") == std::string::npos);
  CHECK(checked(serialize_project_to_json(fixture.create(mode).project)) == json);
  auto reopened = checked(deserialize_project_from_json(json));
  CHECK(checked(serialize_project_to_json(reopened)) == json);
  const auto oracle = source_solids(source, fixture.root);
  REQUIRE(oracle.size() == 5);
  same_solids(project_solids(project, fixture.root), oracle);
  same_solids(project_solids(reopened, fixture.root), oracle);
  StepImportTopologyContext context(fixture.root);
  REQUIRE(
      AssemblyStepExporter{&context}.write_step(project, (fixture.root / "flat.step").string()));
  REQUIRE(AssemblyStructuredStepExporter{&context}.write_step(
      project, (fixture.root / "structured.step").string()));
  for (const auto* filename : {"flat.step", "structured.step"}) {
    auto exported = checked(StepImportSource::create(
        StepImportSourceId(filename), "Reexport", filename,
        checked(step_import_sha256(bytes(fixture.root / filename))), StepImportMode::Reference));
    same_solids(source_solids(exported, fixture.root), oracle);
  }
  CHECK(bytes(fixture.path()) == original);
}

TEST_CASE("Structured STEP deterministic ids do not depend on definition insertion order",
          "[geometry][structured-step-import]") {
  Fixture first, second;
  first.nested(false);
  second.nested(true);
  first.write();
  second.write();
  const auto a = first.create();
  const auto b = second.create();
  CHECK(a.affected_owners == b.affected_owners);
  CHECK(
      without_capture_hashes(
          nlohmann::json::parse(checked(serialize_project_to_json(a.project)))) ==
      without_capture_hashes(nlohmann::json::parse(checked(serialize_project_to_json(b.project)))));
  same_solids(project_solids(a.project, first.root), project_solids(b.project, second.root));
}

TEST_CASE("Structured STEP preserves distinct equal-geometry Assembly definitions canonically",
          "[geometry][structured-step-import]") {
  const bool equal_names = GENERATE(false, true);
  Fixture first, second;
  const auto write = [equal_names](Fixture& fixture, bool reverse) {
    const auto part = fixture.part(BRepPrimAPI_MakeBox(2, 3, 5).Shape(), "Shared");
    TDF_Label left, right;
    const auto add_left = [&] {
      left = fixture.assembly(equal_names ? "Carrier" : "Left carrier");
    };
    const auto add_right = [&] {
      right = fixture.assembly(equal_names ? "Carrier" : "Right carrier");
    };
    if (reverse) {
      add_right();
      add_left();
    } else {
      add_left();
      add_right();
    }
    fixture.occurrence(left, part, "Seat", pose({5, 0, 0}));
    fixture.occurrence(right, part, "Seat", pose({5, 0, 0}));
    const auto root = fixture.assembly("Root");
    const auto add_left_occurrence = [&] {
      fixture.occurrence(root, left, "Left mount", pose({20, 0, 0}, {0, 0, 15}));
    };
    const auto add_right_occurrence = [&] {
      fixture.occurrence(root, right, "Right mount", pose({40, 0, 0}, {0, 0, -30}));
    };
    if (reverse) {
      add_right_occurrence();
      add_left_occurrence();
    } else {
      add_left_occurrence();
      add_right_occurrence();
    }
    fixture.write();
  };
  write(first, false);
  write(second, true);
  const auto first_bytes = bytes(first.path()), second_bytes = bytes(second.path());
  const auto a = first.create(), b = second.create();
  REQUIRE(a.project.part_documents().size() == 1);
  REQUIRE(a.project.child_assembly_documents().size() == 3);
  const auto root = std::find_if(a.project.child_assembly_documents().begin(),
                                 a.project.child_assembly_documents().end(),
                                 [](const auto& assembly) { return assembly.name() == "Root"; });
  REQUIRE(root != a.project.child_assembly_documents().end());
  REQUIRE(root->subassembly_instances().size() == 2);
  CHECK(root->subassembly_instances()[0].referenced_assembly_document() !=
        root->subassembly_instances()[1].referenced_assembly_document());
  CHECK(checked(AssemblyLeafOccurrenceResolver{}.resolve(a.project)).size() == 2);
  CHECK(
      without_capture_hashes(
          nlohmann::json::parse(checked(serialize_project_to_json(a.project)))) ==
      without_capture_hashes(nlohmann::json::parse(checked(serialize_project_to_json(b.project)))));
  same_solids(project_solids(a.project, first.root), source_solids(first.source(), first.root));
  same_solids(project_solids(a.project, first.root), project_solids(b.project, second.root));
  CHECK(bytes(first.path()) == first_bytes);
  CHECK(bytes(second.path()) == second_bytes);
}

TEST_CASE("Public structured STEP export imports every Assembly definition and shared Part",
          "[geometry][structured-step-import]") {
  Fixture fixture;
  const auto supplier = supplier_project();
  const auto written =
      checked(AssemblyStructuredStepExporter{}.write_step(supplier, fixture.path().string()));
  CHECK(written.part_product_definition_count == 1);
  CHECK(written.exported_component_occurrence_count == 5);
  const auto source = fixture.source();
  const auto original = bytes(fixture.path());
  const auto imported = checked(StepImportReader{}.read(source, fixture.root));
  CHECK(std::count_if(imported.products.begin(), imported.products.end(),
                      [](const auto& product) { return product.is_assembly; }) == 5);
  const auto candidate = fixture.create();
  REQUIRE(candidate.project.child_assembly_documents().size() == 5);
  REQUIRE(candidate.project.part_documents().size() == 1);
  const auto occurrences = checked(AssemblyLeafOccurrenceResolver{}.resolve(candidate.project));
  REQUIRE(occurrences.size() == 5);
  for (const auto& occurrence : occurrences)
    CHECK(occurrence.referenced_part_document == candidate.project.part_documents().front().id());
  std::multiset<std::string> source_names, imported_names;
  for (const auto& product : imported.products)
    if (product.is_assembly)
      source_names.insert(product.name);
  for (const auto& assembly : candidate.project.child_assembly_documents())
    imported_names.insert(assembly.name());
  CHECK(imported_names == source_names);
  const auto json = checked(serialize_project_to_json(candidate.project));
  CHECK(checked(serialize_project_to_json(fixture.create().project)) == json);
  const auto reopened = checked(deserialize_project_from_json(json));
  CHECK(checked(serialize_project_to_json(reopened)) == json);
  const auto oracle = project_solids(supplier, fixture.root);
  REQUIRE(oracle.size() == 5);
  same_solids(source_solids(source, fixture.root), oracle);
  same_solids(project_solids(candidate.project, fixture.root), oracle);
  same_solids(project_solids(reopened, fixture.root), oracle);
  CHECK(bytes(fixture.path()) == original);
}

TEST_CASE(
    "Structured Editable import retains authored hierarchy through modeling and explicit refresh",
    "[geometry][structured-step-import][integration][step-import-mvp]") {
  Fixture fixture;
  fixture.nested();
  fixture.write();
  const auto source = fixture.source(StepImportMode::EditableBody);
  auto project = fixture.create(StepImportMode::EditableBody).project;
  const auto hierarchy = [](const Project& value) {
    const auto json = nlohmann::json::parse(checked(serialize_project_to_json(value)));
    return nlohmann::json{{"assembly", json.at("assembly")}, {"assemblies", json.at("assemblies")}};
  };
  const auto initial_hierarchy = hierarchy(project);
  const auto rotor = std::find_if(project.part_documents().begin(), project.part_documents().end(),
                                  [](const auto& part) { return part.name() == "Rotor"; });
  const auto pin = std::find_if(project.part_documents().begin(), project.part_documents().end(),
                                [](const auto& part) { return part.name() == "Pin"; });
  REQUIRE(rotor != project.part_documents().end());
  REQUIRE(pin != project.part_documents().end());
  const auto rotor_id = rotor->id();
  const auto body = rotor->imported_body_features().front().definition().outputs().front().body;
  constexpr Vector3 offset{2, -1, 0.5};
  REQUIRE(rotor->add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("authored.move.rotor"), body, BodyTransformCoordinateSpace::World,
      std::nullopt, offset, false, false))));
  // Genuine imported Point intent is authored after import. The repeated Pin
  // occurrences share their definition, but retain distinct rooted endpoints.
  const auto& catalog = pin->imported_topology_catalogs().front();
  const auto point =
      std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
        return entry.descriptor.kind == ImportedTopologyKind::Vertex;
      });
  REQUIRE(point != catalog.entries().end());
  const auto semantic =
      checked(make_imported_topology_target_spelling(checked(ImportedTopologyReference::create(
          catalog.id(), point->id, ImportedTopologyCapability::Point))));
  std::vector<AssemblyLeafOccurrenceDescriptor> pins;
  for (const auto& occurrence : checked(AssemblyLeafOccurrenceResolver{}.resolve(project)))
    if (occurrence.referenced_part_document == pin->id())
      pins.push_back(occurrence);
  REQUIRE(pins.size() == 2);
  const auto endpoint = [&](std::size_t index) {
    return checked(AssemblyHierarchyConstraintEndpoint::create(
        pins[index].subassembly_path, pins[index].component_instance, semantic));
  };
  REQUIRE(project.add_cross_hierarchy_constraint(checked(AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("authored.point.relationship"), "Imported Pin centers",
      AssemblyConstraintType::Coincident, endpoint(0), endpoint(1)))));
  REQUIRE(project.add_cross_hierarchy_joint(checked(AssemblyHierarchyJoint::create(
      AssemblyJointId("authored.passive.spherical"), "Imported Point joint",
      AssemblyJointType::Spherical, endpoint(0), endpoint(1), AssemblyJointState::Active,
      std::vector<AssemblyJointCoordinateSlot>{}))));
  CHECK(hierarchy(project) == initial_hierarchy);
  StepImportTopologyContext before_context(fixture.root);
  REQUIRE(validate_imported_assembly_inputs(project, before_context));
  const auto relationship_before =
      checked(AssemblyHierarchyConstraintEquationBuilder{&before_context}.build(
          project, checked(AssemblyHierarchyConstraintQuery::create(
                       project.cross_hierarchy_constraints().front()))));
  const auto joint_before =
      checked(AssemblyHierarchySphericalJointEquationBuilder{&before_context}.build(
          project, project.cross_hierarchy_joints().front()));
  CHECK(joint_before.target_a.selected_capability == AssemblyGeometricTargetCapability::Point);
  CHECK(joint_before.target_b.selected_capability == AssemblyGeometricTargetCapability::Point);
  auto shapes = checked(ShapeCache::create(ShapeCacheId("structured.authored")));
  REQUIRE(GeometryRecomputeExecutor{&before_context}.execute_document(*rotor, shapes));
  CHECK(shapes.find_body_result(body)->source_feature_id == FeatureId("authored.move.rotor"));
  CHECK(RectangleExtrusionAdapter{}.summarize(*shapes.find_body_shape(body)).volume_mm3 ==
        Approx(30));
  const auto before_json = checked(serialize_project_to_json(project));

  // The supplier changes actual geometry, retaining the complete bottom face
  // as a unique Body witness. Existing BLCAD structure remains authored intent.
  REQUIRE(!fixture.nested_rotor.IsNull());
  fixture.tool->SetShape(fixture.nested_rotor, BRepPrimAPI_MakeBox(2, 3, 7).Shape());
  fixture.write();
  const auto revised_bytes = bytes(fixture.path());
  CHECK_FALSE(AssemblyStepExporter{&before_context}.build_posed_shape(project));
  CHECK(checked(serialize_project_to_json(project)) == before_json);
  const auto refreshed =
      checked(StepEditableCommands{}.refresh(project, rotor_id, source.id(), fixture.root));
  CHECK(refreshed.affected_owners.size() == 3);
  CHECK(checked(serialize_project_to_json(project)) == before_json);
  CHECK(hierarchy(refreshed.project) == initial_hierarchy);
  CHECK(refreshed.project.cross_hierarchy_constraints().size() == 1);
  CHECK(refreshed.project.cross_hierarchy_joints() == project.cross_hierarchy_joints());
  for (const auto& old_part : project.part_documents()) {
    const auto* current = refreshed.project.find_part_document(old_part.id());
    REQUIRE(current);
    CHECK(current->name() == old_part.name());
    CHECK(current->imported_topology_catalogs() == old_part.imported_topology_catalogs());
    CHECK(current->imported_body_features() == old_part.imported_body_features());
    CHECK(current->step_import_sources().front().sha256_digest() != source.sha256_digest());
  }
  StepImportTopologyContext refreshed_context(fixture.root);
  const auto relationship_after =
      checked(AssemblyHierarchyConstraintEquationBuilder{&refreshed_context}.build(
          refreshed.project, checked(AssemblyHierarchyConstraintQuery::create(
                                 refreshed.project.cross_hierarchy_constraints().front()))));
  const auto joint_after =
      checked(AssemblyHierarchySphericalJointEquationBuilder{&refreshed_context}.build(
          refreshed.project, refreshed.project.cross_hierarchy_joints().front()));
  CHECK(relationship_after.residual == relationship_before.residual);
  CHECK(joint_after.residual.center_offset_mm == joint_before.residual.center_offset_mm);
  auto refreshed_shapes = checked(ShapeCache::create(ShapeCacheId("structured.refreshed")));
  REQUIRE(GeometryRecomputeExecutor{&refreshed_context}.execute_document(
      *refreshed.project.find_part_document(rotor_id), refreshed_shapes));
  CHECK(refreshed_shapes.find_body_result(body)->source_feature_id ==
        FeatureId("authored.move.rotor"));
  CHECK(RectangleExtrusionAdapter{}.summarize(*refreshed_shapes.find_body_shape(body)).volume_mm3 ==
        Approx(42));
  const auto json = checked(serialize_project_to_json(refreshed.project));
  const auto reopened = checked(deserialize_project_from_json(json));
  CHECK(checked(serialize_project_to_json(reopened)) == json);
  CHECK(hierarchy(reopened) == initial_hierarchy);
  CHECK_FALSE(validate_structured_step_import_candidate(reopened, source.id()));
  REQUIRE(reopened.validate_assembly_structure());

  // Independent geometry oracle: apply only the authored product-local edit
  // to the freshly verified source shapes, then compose original direct poses.
  const auto& current_source = *refreshed.project.find_step_import_source(source.id());
  auto oracle_geometry = checked(StepImportReader{}.read(current_source, fixture.root));
  for (auto& product : oracle_geometry.products)
    if (product.name == "Rotor")
      for (auto& local_body : product.bodies)
        local_body.shape = checked(BodyTransformAdapter{}.translate(local_body.shape, offset));
  const auto oracle = solids(checked(StepImportReader{}.preview_shapes(oracle_geometry)));
  REQUIRE(oracle.size() == 5);
  same_solids(project_solids(refreshed.project, fixture.root), oracle);
  same_solids(project_solids(reopened, fixture.root), oracle);
  REQUIRE(AssemblyStepExporter{&refreshed_context}.write_step(
      reopened, (fixture.root / "authored-flat.step").string()));
  REQUIRE(AssemblyStructuredStepExporter{&refreshed_context}.write_step(
      reopened, (fixture.root / "authored-structured.step").string()));
  for (const auto* name : {"authored-flat.step", "authored-structured.step"}) {
    const auto exported = checked(StepImportSource::create(
        StepImportSourceId(name), "Authored export", name,
        checked(step_import_sha256(bytes(fixture.root / name))), StepImportMode::Reference));
    same_solids(source_solids(exported, fixture.root), oracle);
  }
  CHECK(bytes(fixture.path()) == revised_bytes);
}

TEST_CASE("Structured STEP placed roots preserve gimbal and near-gimbal direct transforms in mm",
          "[geometry][structured-step-import][structured-step-import-transforms]") {
  const double pitch = GENERATE(90.0, -90.0, 89.99999999, -89.99999999, 27.0);
  const auto* unit = GENERATE("MM", "INCH", "M");
  Fixture fixture;
  const auto transform = pose({25, -3, 4}, {31, pitch, -27});
  fixture.part(BRepPrimAPI_MakeBox(2, 3, 5).Shape().Moved(TopLoc_Location(transform)),
               "Located root");
  fixture.write(unit);
  const auto source = fixture.source();
  const auto imported = checked(StepImportReader{}.read(source, fixture.root));
  const auto candidate = fixture.create();
  CHECK(candidate.project.part_documents().size() == 1);
  const auto oracle = source_solids(source, fixture.root);
  same_solids(project_solids(candidate.project, fixture.root), oracle);
  REQUIRE(oracle.size() == 1);
  CHECK(oracle.front()[6] == Approx(30));
  // OCCT may wrap a free located shape in an auxiliary assembly. Preserve every
  // resulting direct level instead of baking or applying this placement twice.
  CHECK(candidate.project.child_assembly_documents().size() ==
        static_cast<std::size_t>(std::count_if(imported.products.begin(), imported.products.end(),
                                               [](const auto& p) { return p.is_assembly; })));
}

TEST_CASE("Structured STEP multiple roots and coincident shared occurrences retain multiplicity",
          "[geometry][structured-step-import]") {
  Fixture fixture;
  SECTION("multiple free roots") {
    fixture.part(BRepPrimAPI_MakeBox(2, 3, 5).Shape(), "First root");
    fixture.part(BRepPrimAPI_MakeBox(gp_Pnt(50, 0, 0), 3, 4, 6).Shape(), "Second root");
    fixture.write();
    const auto candidate = fixture.create();
    CHECK(candidate.project.part_documents().size() == 2);
    CHECK(candidate.project.assembly().component_instances().size() == 2);
    CHECK(candidate.project.child_assembly_documents().empty());
    same_solids(project_solids(candidate.project, fixture.root),
                source_solids(fixture.source(), fixture.root));
  }
  SECTION("identical shared occurrence values") {
    const auto part = fixture.part(BRepPrimAPI_MakeBox(2, 3, 5).Shape(), "Shared");
    const auto root = fixture.assembly("Root");
    fixture.occurrence(root, part, "Repeated", pose({7, 0, 0}));
    fixture.occurrence(root, part, "Repeated", pose({7, 0, 0}));
    fixture.write();
    const auto candidate = fixture.create();
    REQUIRE(candidate.project.child_assembly_documents().size() == 1);
    const auto& occurrences =
        candidate.project.child_assembly_documents().front().component_instances();
    REQUIRE(occurrences.size() == 2);
    CHECK(occurrences[0].id() != occurrences[1].id());
    CHECK(occurrences[0].referenced_part_document() == occurrences[1].referenced_part_document());
    CHECK(occurrences[0].transform() == occurrences[1].transform());
    CHECK(checked(AssemblyLeafOccurrenceResolver{}.resolve(candidate.project)).size() == 2);
  }
}

TEST_CASE("Structured STEP preserves ordered Solid and Surface outputs of one shared product",
          "[geometry][structured-step-import]") {
  const auto mode = GENERATE(StepImportMode::Reference, StepImportMode::EditableBody);
  Fixture fixture;
  TopoDS_Compound shape;
  BRep_Builder builder;
  builder.MakeCompound(shape);
  builder.Add(shape, BRepPrimAPI_MakeBox(2, 3, 5).Shape());
  builder.Add(
      shape, BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(40, 0, 0), gp_Dir(0, 0, 1)), 0, 7, 0, 9).Face());
  fixture.part(shape, "Mixed bodies");
  fixture.write();
  const auto candidate = fixture.create(mode);
  REQUIRE(candidate.project.part_documents().size() == 1);
  const auto& part = candidate.project.part_documents().front();
  REQUIRE(part.bodies().size() == 2);
  auto cache = checked(ShapeCache::create(ShapeCacheId("mixed")));
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(part, cache));
  double volume = 0, area = 0;
  for (const auto& body : part.bodies()) {
    const auto* geometry = cache.find_body_shape(body.id());
    REQUIRE(geometry);
    if (body.kind() == BodyKind::Solid)
      volume += RectangleExtrusionAdapter{}.summarize(*geometry).volume_mm3;
    else {
      GProp_GProps properties;
      BRepGProp::SurfaceProperties(*OcctShapeView::get(*geometry), properties);
      area += properties.Mass();
    }
  }
  CHECK(volume == Approx(30));
  CHECK(area == Approx(63));
}

TEST_CASE("Structured STEP rejects ambiguous definitions and stale or unavailable source snapshots",
          "[geometry][structured-step-import][structured-step-import-rejection]") {
  Fixture fixture;
  const auto a = fixture.part(BRepPrimAPI_MakeBox(2, 3, 5).Shape(), "A");
  const auto root = fixture.assembly("Root");
  fixture.occurrence(root, a, "A occurrence", pose({5, 0, 0}));
  SECTION("distinct equal-geometry definitions retain ambiguity despite different metadata names") {
    const auto b = fixture.part(BRepPrimAPI_MakeBox(2, 3, 5).Shape(), "B");
    fixture.occurrence(root, b, "B occurrence", pose({50, 0, 0}));
    fixture.write();
    const auto source = fixture.source();
    const auto result = StepStructuredImportCommands{}.create_project(source, DocumentId("new"),
                                                                      "New", fixture.root);
    REQUIRE_FALSE(result);
    CHECK(result.error().message().find("Indistinguishable") != std::string::npos);
  }
  SECTION("changed bytes are never implicitly accepted") {
    fixture.write();
    const auto source = fixture.source();
    std::ofstream stream(fixture.path(), std::ios::app);
    stream << "\n/* supplier revision */\n";
    stream.close();
    CHECK_FALSE(StepStructuredImportCommands{}.create_project(source, DocumentId("new"), "New",
                                                              fixture.root));
  }
  SECTION("missing source") {
    fixture.write();
    const auto source = fixture.source();
    REQUIRE(std::filesystem::remove(fixture.path()));
    CHECK_FALSE(StepStructuredImportCommands{}.create_project(source, DocumentId("new"), "New",
                                                              fixture.root));
  }
  SECTION("external STEP references") {
    fixture.write("MM", {}, true);
    const auto result = StepStructuredImportCommands{}.create_project(
        fixture.source(), DocumentId("new"), "New", fixture.root);
    REQUIRE_FALSE(result);
    CHECK(result.error().message().find("external STEP") != std::string::npos);
  }
}

TEST_CASE("Raw STEP malformed usage graphs reject before XDE transfer",
          "[geometry][structured-step-import][structured-step-import-malformed]") {
  const int variant = GENERATE(0, 1, 2);
  Fixture fixture;
  fixture.nested();
  fixture.write("MM", [variant](const Handle(StepData_StepModel) & model) {
    std::vector<Handle(StepRepr_AssemblyComponentUsage)> usages;
    for (int index = 1; index <= model->NbEntities(); ++index) {
      const auto usage = Handle(StepRepr_AssemblyComponentUsage)::DownCast(model->Value(index));
      if (!usage.IsNull())
        usages.push_back(usage);
    }
    REQUIRE(usages.size() >= 3);
    if (variant == 0)
      usages.front()->SetRelatedProductDefinition(usages.front()->RelatingProductDefinition());
    else if (variant == 1) {
      bool changed = false;
      for (const auto& first : usages)
        for (const auto& second : usages)
          if (!changed &&
              first->RelatedProductDefinition() == second->RelatingProductDefinition()) {
            second->SetRelatedProductDefinition(first->RelatingProductDefinition());
            changed = true;
          }
      REQUIRE(changed);
    }
  });
  if (variant == 2) {
    auto content = bytes(fixture.path());
    // Serialize a valid supplier model first: OCCT correctly refuses to write
    // a null mandatory child handle. This is valid STEP syntax with a missing
    // definition, so the reader's own pre-transfer model check must reject it.
    const std::regex usage(
        R"((NEXT_ASSEMBLY_USAGE_OCCURRENCE\s*\(\s*'[^']*'\s*,\s*'[^']*'\s*,\s*(?:'[^']*'|\$)\s*,\s*#[0-9]+\s*,\s*)(#[0-9]+))");
    std::smatch match;
    REQUIRE(std::regex_search(content, match, usage));
    content.replace(static_cast<std::size_t>(match.position(2)),
                    static_cast<std::size_t>(match.length(2)), "$");
    std::ofstream stream(fixture.path(), std::ios::binary | std::ios::trunc);
    stream << content;
    REQUIRE(stream.good());
  }
  const auto original = bytes(fixture.path());
  const auto source = fixture.source();
  const auto result =
      StepStructuredImportCommands{}.create_project(source, DocumentId("new"), "New", fixture.root);
  REQUIRE_FALSE(result);
  INFO(result.error().message());
  if (variant == 2) {
    // The mandatory missing field can fail strict entity validation before the
    // usage graph is inspected. Both paths reject it before CAF transfer.
    CHECK((result.error().message() == "STEP contains syntax or entity validation errors" ||
           result.error().message().find("missing product definition") != std::string::npos));
  } else {
    CHECK(result.error().message().find("cycle") != std::string::npos);
  }
  CHECK(bytes(fixture.path()) == original);
}

TEST_CASE("Raw STEP deep DAG rejects by complete path depth even after descendant memoization",
          "[geometry][structured-step-import][structured-step-import-malformed]") {
  Fixture fixture;
  fixture.part(BRepPrimAPI_MakeBox(2, 3, 5).Shape(), "Body");
  fixture.write("MM", [](const Handle(StepData_StepModel) & model) {
    Handle(StepBasic_ProductDefinition) original;
    for (int index = 1; index <= model->NbEntities() && original.IsNull(); ++index)
      original = Handle(StepBasic_ProductDefinition)::DownCast(model->Value(index));
    REQUIRE(!original.IsNull());
    std::vector<Handle(StepBasic_ProductDefinition)> products;
    for (std::size_t index = 0; index < 513; ++index) {
      Handle(StepBasic_ProductDefinition) product = new StepBasic_ProductDefinition;
      product->Init(new TCollection_HAsciiString("depth"), new TCollection_HAsciiString(""),
                    original->Formation(), original->FrameOfReference());
      model->AddWithRefs(product);
      products.push_back(product);
    }
    // Point toward earlier pointer-order nodes: a DFS may encounter each child
    // as already completed, so its current stack is shorter than the full path.
    std::sort(products.begin(), products.end(), [](const auto& first, const auto& second) {
      return std::less<const StepBasic_ProductDefinition*>{}(first.get(), second.get());
    });
    for (std::size_t index = 1; index < products.size(); ++index) {
      Handle(StepRepr_NextAssemblyUsageOccurrence) usage = new StepRepr_NextAssemblyUsageOccurrence;
      usage->Init(new TCollection_HAsciiString("depth"), new TCollection_HAsciiString("depth"),
                  false, {}, products[index], products[index - 1], false, {});
      model->AddWithRefs(usage);
    }
  });
  const auto original = bytes(fixture.path());
  const auto result = StepStructuredImportCommands{}.create_project(
      fixture.source(), DocumentId("new"), "New", fixture.root);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("depth exceeds") != std::string::npos);
  CHECK(bytes(fixture.path()) == original);
}
