#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <utility>

using namespace blcad;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
StepImportSource source(StepImportMode mode = StepImportMode::Reference) {
  return checked(StepImportSource::create(StepImportSourceId("source"), "Supplier",
                                          "assets/supplier.step", std::string(64, '1'), mode));
}
ImportedTopologyCatalog catalog() {
  ImportedTopologyDescriptor solid;
  solid.kind = ImportedTopologyKind::Body;
  solid.geometry = ImportedGeometryClass::Opaque;
  solid.body_kind = BodyKind::Solid;
  solid.bounds_mm = {0, 0, 0, 10, 10, 10};
  solid.centroid = {5, 5, 5};
  solid.measure = 1000;
  solid.geometry_key = std::string(64, 'a');
  auto surface = solid;
  surface.body_kind = BodyKind::Surface;
  surface.bounds_mm = {0, 0, 0, 10, 10, 0};
  surface.centroid = {5, 5, 0};
  surface.measure = 100;
  surface.geometry_key = std::string(64, 'b');
  ImportedTopologyDescriptor face;
  face.kind = ImportedTopologyKind::Face;
  face.geometry = ImportedGeometryClass::Plane;
  face.bounds_mm = {0, 0, 10, 10, 10, 10};
  face.centroid = {5, 5, 10};
  face.measure = 100;
  face.origin = {0, 0, 10};
  face.direction = {0, 0, 1};
  face.geometry_key = std::string(64, 'c');
  return checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId("catalog"), StepImportSourceId("source"), std::string(64, '1'),
      std::string(64, '2'),
      {{ImportedTopologyId("solid"), std::nullopt, solid},
       {ImportedTopologyId("surface"), std::nullopt, surface},
       {ImportedTopologyId("face"), ImportedTopologyId("solid"), face}}));
}
ImportedPartDefinition definition() {
  return checked(ImportedPartDefinition::create(
      StepImportSourceId("source"), ImportedTopologyCatalogId("catalog"),
      {{ImportedTopologyId("surface"), BodyId("z.surface"), BodyKind::Surface},
       {ImportedTopologyId("solid"), BodyId("a.solid"), BodyKind::Solid}}));
}
PartDocument reference_part() {
  return checked(PartDocument::create_reference(DocumentId("reference"), "Supplier Part", source(),
                                                catalog(), definition()));
}
std::string serialized(const PartDocument& part) {
  return checked(serialize_part_document_to_json(part));
}
std::string serialized(const Project& project) {
  return checked(serialize_project_to_json(project));
}
Parameter parameter(ParameterScope scope = ParameterScope::Part) {
  return checked(Parameter::create_length(ParameterId("width"), "Width",
                                          checked(Quantity::length_mm(10)), scope));
}
template <typename T> void read_only(Result<T> result) {
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("read-only") != std::string::npos);
}
} // namespace

TEST_CASE("Reference Part owns exact ordered catalog Body outputs without a feature producer",
          "[core][step-reference-part]") {
  const auto part = reference_part();
  CHECK(part.mode() == PartDocumentMode::Reference);
  REQUIRE(part.reference_part_definition());
  CHECK(*part.reference_part_definition() == definition());
  REQUIRE(part.body_count() == 2);
  CHECK(part.bodies()[0].id() == BodyId("a.solid"));
  CHECK(part.reference_part_definition()->outputs()[0].body == BodyId("z.surface"));
  CHECK(part.find_body(BodyId("z.surface"))->kind() == BodyKind::Surface);
  CHECK(part.imported_body_features().empty());
  CHECK(part.features().empty());
  CHECK(part.dependency_graph().has_dependency("imported-catalog:catalog", "body:a.solid"));
  CHECK(part.dependency_graph().has_dependency("imported-catalog:catalog", "body:z.surface"));
  auto loaded = checked(deserialize_part_document_from_json(serialized(part)));
  CHECK(loaded.is_reference_part());
  CHECK(serialized(loaded) == serialized(part));
  CHECK(*loaded.reference_part_definition() == definition());

  auto one_body = checked(ImportedPartDefinition::create(
      StepImportSourceId("source"), ImportedTopologyCatalogId("catalog"),
      {{ImportedTopologyId("solid"), BodyId("selected"), BodyKind::Solid}}));
  CHECK(PartDocument::create_reference(DocumentId("single"), "Selected Body", source(), catalog(),
                                       one_body));
  auto only_surface = checked(ImportedPartDefinition::create(
      StepImportSourceId("source"), ImportedTopologyCatalogId("catalog"),
      {{ImportedTopologyId("surface"), BodyId("surface"), BodyKind::Surface}}));
  auto subset = checked(PartDocument::create_reference(DocumentId("subset"), "Surface only",
                                                       source(), catalog(), only_surface));
  CHECK(validate_imported_topology_reference(
      subset, checked(ImportedTopologyReference::create(ImportedTopologyCatalogId("catalog"),
                                                        ImportedTopologyId("surface"),
                                                        ImportedTopologyCapability::Body))));
  CHECK_FALSE(validate_imported_topology_reference(
      subset, checked(ImportedTopologyReference::create(ImportedTopologyCatalogId("catalog"),
                                                        ImportedTopologyId("face"),
                                                        ImportedTopologyCapability::Face))));
}

TEST_CASE("Imported Part factories reject inconsistent ownership, selection and output mapping",
          "[core][step-reference-part]") {
  CHECK_FALSE(ImportedPartDefinition::create(StepImportSourceId{}, ImportedTopologyCatalogId("c"),
                                             definition().outputs()));
  CHECK_FALSE(ImportedPartDefinition::create(StepImportSourceId("s"), ImportedTopologyCatalogId{},
                                             definition().outputs()));
  CHECK_FALSE(
      ImportedPartDefinition::create(StepImportSourceId("s"), ImportedTopologyCatalogId("c"), {}));
  for (const auto duplicate_body : {false, true}) {
    auto outputs = definition().outputs();
    if (duplicate_body)
      outputs[1].body = outputs[0].body;
    else
      outputs[1].imported_body = outputs[0].imported_body;
    CHECK_FALSE(ImportedPartDefinition::create(StepImportSourceId("s"),
                                               ImportedTopologyCatalogId("c"), outputs));
  }
  CHECK_FALSE(PartDocument::create_reference(DocumentId("p"), "Wrong mode",
                                             source(StepImportMode::EditableBody), catalog(),
                                             definition()));
  for (const auto* invalid_entry : {"missing", "face", "surface"}) {
    auto outputs = definition().outputs();
    outputs[1].imported_body = ImportedTopologyId(invalid_entry);
    if (std::string(invalid_entry) == "surface")
      outputs.erase(outputs.begin());
    auto invalid = checked(ImportedPartDefinition::create(
        StepImportSourceId("source"), ImportedTopologyCatalogId("catalog"), outputs));
    CHECK_FALSE(PartDocument::create_reference(DocumentId("p"), "Wrong selection", source(),
                                               catalog(), invalid));
  }
  auto foreign = checked(ImportedPartDefinition::create(
      StepImportSourceId("foreign"), ImportedTopologyCatalogId("catalog"), definition().outputs()));
  CHECK_FALSE(
      PartDocument::create_reference(DocumentId("p"), "Wrong owner", source(), catalog(), foreign));
}

TEST_CASE(
    "Reference Parts retain exact immutable BLCAD origin frames independently of imported geometry",
    "[core][step-reference-part]") {
  auto part = reference_part();
  REQUIRE(part.datum_planes().size() == 3);
  std::size_t index = 0;
  for (const auto& expected :
       {checked(DatumPlane::xy()), checked(DatumPlane::xz()), checked(DatumPlane::yz())}) {
    const auto& actual = part.datum_planes()[index++];
    CHECK(actual.id() == expected.id());
    CHECK(actual.name() == expected.name());
    CHECK(actual.origin() == expected.origin());
    CHECK(actual.x_axis() == expected.x_axis());
    CHECK(actual.y_axis() == expected.y_axis());
    CHECK(actual.normal() == expected.normal());
  }
  const auto original = serialized(part);
  read_only(part.add_datum_plane(checked(DatumPlane::xy(DatumPlaneId("custom"), "Custom"))));
  CHECK(serialized(part) == original);
  auto converted = checked(
      part.convert_reference_to_editable(DocumentId("editable.frames"), "Editable frames",
                                         FeatureId("base"), {BodyId("surface"), BodyId("solid")}));
  REQUIRE(converted.datum_planes().size() == 3);
  const auto reference_json = nlohmann::json::parse(original);
  const auto converted_json = nlohmann::json::parse(serialized(converted));
  CHECK(converted_json.at("datum_planes") == reference_json.at("datum_planes"));
  CHECK(converted.add_datum_plane(checked(DatumPlane::xy(DatumPlaneId("custom"), "Custom"))));
  const auto imported_catalog = catalog();
  for (const auto& entry : imported_catalog.entries())
    for (const auto capability : imported_topology_capabilities(entry.descriptor))
      CHECK(to_string(capability) != "frame");
}

TEST_CASE("Reference Part rejects authored edits atomically and retains explicit refresh",
          "[core][step-reference-part]") {
  auto part = reference_part();
  const auto original = serialized(part);
  const auto assert_unchanged = [&](auto result) {
    read_only(std::move(result));
    CHECK(serialized(part) == original);
  };
  assert_unchanged(part.add_parameter(parameter()));
  assert_unchanged(part.add_expression_parameter(ParameterId("computed"), "Computed",
                                                 ParameterType::Length, "1 mm"));
  assert_unchanged(
      part.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(30))));
  assert_unchanged(part.set_parameter_formula(ParameterId("width"), "3 mm"));
  assert_unchanged(part.add_datum_plane(checked(DatumPlane::xy())));
  auto axis = checked(DatumAxis::create_explicit(DatumAxisId("axis"), "Axis", {}, {0, 0, 1}));
  assert_unchanged(part.add_datum_axis(axis));
  assert_unchanged(part.update_datum_axis(axis));
  auto sketch = checked(Sketch::create(SketchId("sketch"), "Sketch", DatumPlaneId("datum.xy")));
  assert_unchanged(part.add_sketch(sketch));
  assert_unchanged(part.update_sketch(sketch));
  auto sketch3d = checked(Sketch3D::create(Sketch3DId("sketch3d"), "Sketch 3D"));
  assert_unchanged(part.add_sketch_3d(sketch3d));
  assert_unchanged(part.update_sketch_3d(sketch3d));
  assert_unchanged(part.remove_sketch_3d(sketch3d.id()));
  auto extrude = checked(Feature::create_additive_extrude(
      FeatureId("extrude"), "Extrude", SketchId("sketch"), ParameterId("width")));
  assert_unchanged(part.add_feature(extrude));
  assert_unchanged(part.update_feature(extrude));
  auto boolean = checked(BodyBooleanFeature::create(
      FeatureId("boolean"), BodyBooleanOperation::Add, BodyId("a.solid"), {BodyId("tool")},
      BodyBooleanResultMode::ModifyTarget, std::nullopt, true));
  assert_unchanged(part.add_body_boolean_feature(boolean));
  assert_unchanged(part.update_body_boolean_feature(boolean));
  assert_unchanged(
      part.add_body(checked(Body::create(BodyId("new"), "New Body", BodyKind::Solid))));
  assert_unchanged(part.remove_body(BodyId("a.solid")));
  assert_unchanged(part.set_body_visibility(BodyId("a.solid"), BodyVisibility::Hidden));
  assert_unchanged(part.add_sketch_ownership(checked(SketchOwnership::create(
      SketchId("sketch"), BodyId("a.solid"), SketchAssociation::ReferenceOnly))));
  auto transform = checked(BodyTransform::create_translate(
      BodyTransformId("move"), BodyId("a.solid"), BodyTransformCoordinateSpace::World, std::nullopt,
      {1, 2, 3}, false, false));
  assert_unchanged(part.add_body_transform(transform));
  assert_unchanged(part.add_step_import_source(source()));
  assert_unchanged(part.remove_step_import_source(StepImportSourceId("source")));
  assert_unchanged(part.remove_step_import_source(source()));
  assert_unchanged(part.add_imported_topology_catalog(catalog()));
  assert_unchanged(part.remove_imported_topology_catalog(ImportedTopologyCatalogId("catalog")));
  auto base = checked(ImportedBodyFeature::create(FeatureId("base"), "Base", definition()));
  assert_unchanged(part.add_imported_body_feature(base));
  assert_unchanged(part.mark_parameter_changed(ParameterId("width")));
  assert_unchanged(part.mark_feature_changed(FeatureId("base")));
  assert_unchanged(part.mark_body_changed(BodyId("a.solid")));
  assert_unchanged(part.mark_body_transform_changed(BodyTransformId("move")));
  using Remove = Result<std::size_t> (PartDocument::*)(FeatureId);
  for (const auto remove :
       {static_cast<Remove>(&PartDocument::remove_feature), &PartDocument::remove_revolve_feature,
        &PartDocument::remove_sweep_feature, &PartDocument::remove_loft_feature,
        &PartDocument::remove_surface_feature, &PartDocument::remove_linear_pattern_feature,
        &PartDocument::remove_circular_pattern_feature, &PartDocument::remove_mirror_feature,
        &PartDocument::remove_fillet_feature, &PartDocument::remove_chamfer_feature,
        &PartDocument::remove_shell_feature, &PartDocument::remove_draft_feature,
        &PartDocument::remove_body_boolean_feature})
    assert_unchanged((part.*remove)(FeatureId("absent")));

  auto accepted = *part.find_step_import_source(StepImportSourceId("source"));
  CHECK(part.refresh_step_import_source(accepted, std::string(64, '3')));
  CHECK(*part.reference_part_definition() == definition());
  CHECK(part.find_imported_topology_catalog(ImportedTopologyCatalogId("catalog"))
            ->captured_sha256_digest() == std::string(64, '1'));
  CHECK(part.invalidation_state().find("body:a.solid")->status == InvalidationStatus::Dirty);
  const auto refreshed = serialized(part);
  CHECK_FALSE(part.refresh_step_import_source(accepted, std::string(64, '4')));
  CHECK(serialized(part) == refreshed);
  CHECK(part.is_reference_part());
  auto converted = checked(
      part.convert_reference_to_editable(DocumentId("refreshed.editable"), "Refreshed editable",
                                         FeatureId("base"), {BodyId("surface"), BodyId("solid")}));
  CHECK(converted.find_step_import_source(StepImportSourceId("source"))->sha256_digest() ==
        std::string(64, '3'));
  CHECK(converted.find_imported_topology_catalog(ImportedTopologyCatalogId("catalog"))
            ->captured_sha256_digest() == std::string(64, '1'));
  part.mark_all_clean();
  CHECK(serialized(part) == refreshed);
  CHECK(part.create_recompute_plan());
}

TEST_CASE("Reference conversion creates a new editable base intent without changing assemblies",
          "[core][step-reference-part]") {
  const auto reference = reference_part();
  const auto original = serialized(reference);
  const auto converted = checked(reference.convert_reference_to_editable(
      DocumentId("editable"), "Editable supplier", FeatureId("base"),
      {BodyId("edited.surface"), BodyId("edited.solid")}));
  CHECK(serialized(reference) == original);
  CHECK(converted.mode() == PartDocumentMode::Editable);
  CHECK_FALSE(converted.reference_part_definition());
  REQUIRE(converted.imported_body_features().size() == 1);
  const auto& base = converted.imported_body_features()[0];
  CHECK(base.id() == FeatureId("base"));
  CHECK((base.definition().outputs()[0] == ImportedBodyOutput{ImportedTopologyId("surface"),
                                                              BodyId("edited.surface"),
                                                              BodyKind::Surface}));
  CHECK(base.definition().outputs()[1].body == BodyId("edited.solid"));
  CHECK(converted.find_step_import_source(StepImportSourceId("source"))->mode() ==
        StepImportMode::EditableBody);
  CHECK(converted.find_step_import_source(StepImportSourceId("source"))
            ->project_relative_asset_path() == source().project_relative_asset_path());
  CHECK(*converted.find_imported_topology_catalog(ImportedTopologyCatalogId("catalog")) ==
        catalog());
  CHECK(converted.dependency_graph().has_dependency("imported-catalog:catalog", "base"));
  CHECK(converted.dependency_graph().has_dependency("base", "body:edited.surface"));
  CHECK(serialized(checked(deserialize_part_document_from_json(serialized(converted)))) ==
        serialized(converted));
  auto editable = converted;
  CHECK(editable.add_parameter(parameter()));
  CHECK_FALSE(editable.remove_body(BodyId("edited.solid")));
  CHECK_FALSE(editable.add_imported_body_feature(base));

  auto assembly = checked(AssemblyDocument::create(DocumentId("root"), "Assembly"));
  REQUIRE(assembly.add_member_part(reference.id()));
  REQUIRE(assembly.add_component_instance(
      checked(ComponentInstance::create(ComponentInstanceId("one"), "One", reference.id()))));
  auto project = checked(Project::create(DocumentId("project"), "Project", assembly));
  REQUIRE(project.add_part_document(reference));
  REQUIRE(project.add_part_document(converted));
  CHECK(project.assembly().component_instances()[0].referenced_part_document() == reference.id());
  CHECK(project.assembly().member_part_count() == 1);
  CHECK(serialized(checked(deserialize_project_from_json(serialized(project)))) ==
        serialized(project));

  CHECK_FALSE(reference.convert_reference_to_editable(reference.id(), "Bad", FeatureId("base"),
                                                      {BodyId("a"), BodyId("b")}));
  CHECK_FALSE(reference.convert_reference_to_editable(DocumentId("e"), "Bad", FeatureId("base"),
                                                      {BodyId("a")}));
  CHECK_FALSE(reference.convert_reference_to_editable(DocumentId("e"), "Bad", FeatureId("base"),
                                                      {BodyId("a"), BodyId("a")}));
  CHECK_FALSE(reference.convert_reference_to_editable(DocumentId("e"), "Bad", FeatureId{},
                                                      {BodyId("a"), BodyId("b")}));
  CHECK_FALSE(converted.convert_reference_to_editable(DocumentId("other"), "Bad", FeatureId("base"),
                                                      {BodyId("a"), BodyId("b")}));
  CHECK(serialized(reference) == original);
}

TEST_CASE("Reference Part JSON rejects mixed authored intent and malformed imported authority",
          "[core][step-reference-part]") {
  using json = nlohmann::json;
  const auto saved = json::parse(serialized(reference_part()));
  const std::vector<std::pair<std::string, std::function<void(json&)>>> invalid{
      {"null binding", [](auto& j) { j["reference_part"] = nullptr; }},
      {"extra binding field", [](auto& j) { j["reference_part"]["runtime_product"] = 0; }},
      {"missing catalog", [](auto& j) { j["reference_part"]["catalog"] = "absent"; }},
      {"foreign source", [](auto& j) { j["reference_part"]["source"] = "foreign"; }},
      {"wrong source mode", [](auto& j) { j["step_import_sources"][0]["mode"] = "editable_body"; }},
      {"unknown selected entry",
       [](auto& j) { j["reference_part"]["outputs"][0]["imported_body"] = "absent"; }},
      {"Face selected as Body",
       [](auto& j) { j["reference_part"]["outputs"][0]["imported_body"] = "face"; }},
      {"kind mismatch", [](auto& j) { j["reference_part"]["outputs"][0]["kind"] = "solid"; }},
      {"unknown kind", [](auto& j) { j["reference_part"]["outputs"][0]["kind"] = "wire"; }},
      {"extra output field", [](auto& j) { j["reference_part"]["outputs"][0]["index"] = 1; }},
      {"empty output mapping", [](auto& j) { j["reference_part"]["outputs"] = json::array(); }},
      {"duplicate output mapping",
       [](auto& j) { j["reference_part"]["outputs"][1] = j["reference_part"]["outputs"][0]; }},
      {"missing output Body", [](auto& j) { j["bodies"].erase(j["bodies"].begin()); }},
      {"extra output Body",
       [](auto& j) {
         auto b = j["bodies"][0];
         b["id"] = "extra";
         j["bodies"].push_back(b);
       }},
      {"authored datum", [](auto& j) { j["datum_planes"] = {json::object()}; }},
      {"missing origin datum", [](auto& j) { j["datum_planes"].erase(j["datum_planes"].begin()); }},
      {"extra origin datum", [](auto& j) { j["datum_planes"].push_back(j["datum_planes"][0]); }},
      {"edited origin datum", [](auto& j) { j["datum_planes"][0]["origin"]["x"] = 1; }},
      {"edited datum axis", [](auto& j) { j["datum_planes"][1]["x_axis"]["x"] = -1; }},
      {"renamed origin datum", [](auto& j) { j["datum_planes"][0]["name"] = "Authored"; }},
      {"reidentified origin datum", [](auto& j) { j["datum_planes"][0]["id"] = "custom"; }},
      {"extra datum field", [](auto& j) { j["datum_planes"][0]["source_face"] = "face"; }},
      {"malformed datum vector", [](auto& j) { j["datum_planes"][0]["normal"] = "axis"; }},
      {"reordered origin datums",
       [](auto& j) { std::swap(j["datum_planes"][0], j["datum_planes"][1]); }},
      {"authored feature", [](auto& j) { j["features"] = {json::object()}; }},
      {"mixed imported feature", [](auto& j) { j["imported_body_features"] = {json::object()}; }},
      {"replay bypass",
       [](auto& j) {
         j["result_face_workplanes"] = {json::object()};
         j["feature_order"] = {"fake"};
       }},
      {"malformed base array", [](auto& j) { j["imported_body_features"] = false; }},
  };
  for (const auto& [name, mutate] : invalid) {
    DYNAMIC_SECTION(name) {
      auto changed = saved;
      mutate(changed);
      CHECK_FALSE(deserialize_part_document_from_json(changed.dump()));
      CHECK_FALSE(deserialize_part_document_from_json(changed.dump(),
                                                      PartDocumentReadMode::ReferenceRepair));
    }
  }
  auto legacy = checked(PartDocument::create(DocumentId("old"), "Old authored Part"));
  const auto old = json::parse(serialized(legacy));
  CHECK_FALSE(old.contains("reference_part"));
  CHECK_FALSE(old.contains("imported_body_features"));
  CHECK(serialized(checked(deserialize_part_document_from_json(old.dump()))) == serialized(legacy));
}

TEST_CASE("Reference-target parameter binding failure leaves the entire Project unchanged",
          "[core][step-reference-part]") {
  auto reference = reference_part();
  auto authored = checked(PartDocument::create(DocumentId("authored"), "Authored"));
  REQUIRE(authored.add_parameter(parameter()));
  auto assembly = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(assembly.add_parameter(parameter(ParameterScope::Assembly)));
  REQUIRE(assembly.add_member_part(authored.id()));
  REQUIRE(assembly.add_member_part(reference.id()));
  for (const auto* part : {&authored, &reference})
    REQUIRE(assembly.add_binding(
        checked(ParameterBinding::create(ParameterBindingId("binding." + part->id().value()),
                                         part->id(), ParameterId("width"), ParameterId("width")))));
  auto project = checked(Project::create(DocumentId("project"), "Project", assembly));
  REQUIRE(project.add_part_document(authored));
  REQUIRE(project.add_part_document(reference));
  const auto original = serialized(project);
  auto changed =
      project.set_assembly_parameter_value(ParameterId("width"), checked(Quantity::length_mm(25)));
  read_only(std::move(changed));
  CHECK(serialized(project) == original);
  CHECK(project.find_part_document(authored.id())
            ->find_parameter(ParameterId("width"))
            ->value()
            .millimeters() == 10);
  CHECK(project.assembly().find_parameter(ParameterId("width"))->value().millimeters() == 10);
}

TEST_CASE("Converted base feature JSON validates authority and retains explicit execution order",
          "[core][step-reference-part]") {
  using json = nlohmann::json;
  auto converted = checked(reference_part().convert_reference_to_editable(
      DocumentId("editable"), "Editable", FeatureId("base"), {BodyId("surface"), BodyId("solid")}));
  auto saved = json::parse(serialized(converted));
  saved["feature_order"] = {"base"};
  auto replayed = checked(deserialize_part_document_from_json(saved.dump()));
  CHECK(serialized(replayed) == serialized(converted));
  CHECK(replayed.imported_body_features() == converted.imported_body_features());
  const std::vector<std::pair<std::string, std::function<void(json&)>>> invalid{
      {"wrong feature fields", [](auto& j) { j["imported_body_features"][0]["shape"] = "opaque"; }},
      {"duplicate feature id",
       [](auto& j) { j["imported_body_features"].push_back(j["imported_body_features"][0]); }},
      {"empty feature id", [](auto& j) { j["imported_body_features"][0]["id"] = ""; }},
      {"reference source", [](auto& j) { j["step_import_sources"][0]["mode"] = "reference"; }},
      {"foreign source",
       [](auto& j) { j["imported_body_features"][0]["definition"]["source"] = "foreign"; }},
      {"foreign catalog",
       [](auto& j) { j["imported_body_features"][0]["definition"]["catalog"] = "foreign"; }},
      {"wrong output kind",
       [](auto& j) {
         j["imported_body_features"][0]["definition"]["outputs"][0]["kind"] = "solid";
       }},
      {"missing output", [](auto& j) { j["bodies"].erase(j["bodies"].begin()); }},
      {"missing execution", [](auto& j) { j["feature_order"] = json::array(); }},
      {"duplicate execution", [](auto& j) { j["feature_order"] = {"base", "base"}; }},
  };
  for (const auto& [name, mutate] : invalid) {
    DYNAMIC_SECTION(name) {
      auto changed = saved;
      mutate(changed);
      CHECK_FALSE(deserialize_part_document_from_json(changed.dump()));
    }
  }
}

TEST_CASE("Project shares one Reference definition across repeated nested occurrences",
          "[core][step-reference-part]") {
  const auto part = reference_part();
  auto child = checked(AssemblyDocument::create(DocumentId("child"), "Child"));
  REQUIRE(child.add_member_part(part.id()));
  REQUIRE(child.add_component_instance(checked(
      ComponentInstance::create(ComponentInstanceId("child.part"), "Nested part", part.id()))));
  auto root = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(root.add_member_part(part.id()));
  for (const auto* id : {"first", "second"}) {
    REQUIRE(root.add_component_instance(
        checked(ComponentInstance::create(ComponentInstanceId(id), id, part.id()))));
    REQUIRE(root.add_subassembly_instance(checked(SubassemblyInstance::create(
        SubassemblyInstanceId(std::string("nested.") + id), id, child.id()))));
  }
  auto project = checked(Project::create(DocumentId("project"), "Shared definitions", root));
  REQUIRE(project.add_part_document(part));
  REQUIRE(project.add_child_assembly_document(child));
  CHECK(project.validate_assembly_structure());
  auto loaded = checked(deserialize_project_from_json(serialized(project)));
  CHECK(loaded.part_document_count() == 1);
  CHECK(loaded.find_part_document(part.id())->is_reference_part());
  CHECK(loaded.assembly().component_instance_count() == 2);
  CHECK(loaded.assembly().subassembly_instance_count() == 2);
  CHECK(loaded.child_assembly_documents()[0].component_instance_count() == 1);
  CHECK(serialized(loaded) == serialized(project));
}
