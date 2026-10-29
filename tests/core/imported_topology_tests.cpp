#include "blcad/core/imported_topology.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>

using namespace blcad;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
ImportedTopologyCatalog catalog(std::string id = "catalog", std::string owner = "source") {
  std::vector<ImportedTopologyEntry> entries;
  const auto add = [&](const char* id, ImportedTopologyDescriptor descriptor) {
    entries.push_back({ImportedTopologyId(id),
                       descriptor.kind == ImportedTopologyKind::Body
                           ? std::nullopt
                           : std::optional(ImportedTopologyId("body")),
                       std::move(descriptor)});
  };
  ImportedTopologyDescriptor body;
  body.kind = ImportedTopologyKind::Body;
  body.geometry = ImportedGeometryClass::Opaque;
  body.body_kind = BodyKind::Solid;
  body.centroid = {5, 5, 5};
  body.bounds_mm = {0, 0, 0, 10, 10, 10};
  body.measure = 1000;
  body.geometry_key = std::string(64, 'a');
  add("body", body);
  ImportedTopologyDescriptor face;
  face.kind = ImportedTopologyKind::Face;
  face.geometry = ImportedGeometryClass::Plane;
  face.centroid = {5, 5, 10};
  face.bounds_mm = {0, 0, 10, 10, 10, 10};
  face.measure = 100;
  face.origin = {0, 0, 10};
  face.direction = {0, 0, 1};
  face.geometry_key = std::string(64, 'b');
  face.boundary_keys = {std::string(64, '1'), std::string(64, '2')};
  add("face", face);
  auto cylinder = face;
  cylinder.geometry = ImportedGeometryClass::Cylinder;
  cylinder.centroid = {5, 5, 5};
  cylinder.bounds_mm = {4, 4, 0, 6, 6, 10};
  cylinder.measure = 62.8;
  cylinder.origin = {5, 5, 0};
  cylinder.radius_mm = 1;
  cylinder.geometry_key = std::string(64, 'c');
  add("cylinder", cylinder);
  ImportedTopologyDescriptor line;
  line.kind = ImportedTopologyKind::Edge;
  line.geometry = ImportedGeometryClass::Line;
  line.centroid = {5, 0, 0};
  line.bounds_mm = {0, 0, 0, 10, 0, 0};
  line.measure = 10;
  line.direction = {1, 0, 0};
  line.geometry_key = std::string(64, 'd');
  line.samples = {{0, 0, 0}, {10, 0, 0}};
  add("line", line);
  auto circle = line;
  circle.geometry = ImportedGeometryClass::Circle;
  circle.centroid = {5, 5, 10};
  circle.bounds_mm = {3, 3, 10, 7, 7, 10};
  circle.origin = {5, 5, 10};
  circle.direction = {0, 0, 1};
  circle.radius_mm = 2;
  circle.measure = 12.56;
  circle.geometry_key = std::string(64, 'e');
  circle.samples.clear();
  add("circle", circle);
  ImportedTopologyDescriptor vertex;
  vertex.geometry_key = std::string(64, 'f');
  add("vertex", vertex);
  return checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId(std::move(id)), StepImportSourceId(std::move(owner)),
      std::string(64, '1'), std::string(64, '2'), std::move(entries)));
}
StepImportSource source() {
  return checked(StepImportSource::create(StepImportSourceId("source"), "Supplier",
                                          "assets/source.step", std::string(64, '1'),
                                          StepImportMode::EditableBody));
}
PartDocument part() {
  return checked(PartDocument::create(DocumentId("part"), "Imported catalog host"));
}
Project project() {
  return checked(
      Project::create(DocumentId("project"), "Catalog Project",
                      checked(AssemblyDocument::create(DocumentId("assembly"), "Root"))));
}
std::string json(const PartDocument& p) {
  return checked(serialize_part_document_to_json(p));
}
std::string json(const Project& p) {
  return checked(serialize_project_to_json(p));
}
ImportedTopologyReference ref(const char* entry, ImportedTopologyCapability capability) {
  return checked(ImportedTopologyReference::create(ImportedTopologyCatalogId("catalog"),
                                                   ImportedTopologyId(entry), capability));
}
void add_profile(PartDocument& p) {
  REQUIRE(p.add_datum_plane(checked(DatumPlane::xy())));
  for (const char* id : {"width", "height"})
    REQUIRE(p.add_parameter(
        checked(Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(10))))));
  auto sketch = checked(Sketch::create(SketchId("sketch"), "Profile", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("profile"), ParameterId("width"), ParameterId("height"), {20, 0}))));
  REQUIRE(p.add_sketch(std::move(sketch)));
}
} // namespace

TEST_CASE("Imported catalogs validate stable ids body ownership and normalized descriptor domains",
          "[core][step-import-topology]") {
  const auto valid = catalog();
  CHECK(valid == catalog());
  CHECK(valid.find(ImportedTopologyId("face"))->parent_body == ImportedTopologyId("body"));
  const auto rejects = [&](auto entries) {
    CHECK_FALSE(ImportedTopologyCatalog::create(valid.id(), valid.source(),
                                                valid.captured_sha256_digest(), valid.product_key(),
                                                std::move(entries)));
  };
  auto entries = valid.entries();
  entries.push_back(entries.front());
  rejects(entries);
  entries = valid.entries();
  entries[1].parent_body = ImportedTopologyId("line");
  rejects(entries);
  entries = valid.entries();
  entries[0].parent_body = ImportedTopologyId("body");
  rejects(entries);
  entries = valid.entries();
  entries[1].parent_body.reset();
  rejects(entries);
  entries = valid.entries();
  entries[1].descriptor.direction = {0, 0, -1};
  rejects(entries);
  entries = valid.entries();
  entries[1].descriptor.direction = {0, 0, 2};
  rejects(entries);
  entries = valid.entries();
  entries[1].descriptor.origin.x = 1;
  rejects(entries);
  entries = valid.entries();
  entries[1].descriptor.bounds_mm[0] = 99;
  rejects(entries);
  entries = valid.entries();
  entries[2].descriptor.radius_mm = 0;
  rejects(entries);
  entries = valid.entries();
  entries[1].descriptor.measure = std::numeric_limits<double>::infinity();
  rejects(entries);
  entries = valid.entries();
  entries[3].descriptor.samples[0].x = std::numeric_limits<double>::quiet_NaN();
  rejects(entries);
  entries = valid.entries();
  std::reverse(entries[1].descriptor.boundary_keys.begin(),
               entries[1].descriptor.boundary_keys.end());
  rejects(entries);
  CHECK_FALSE(ImportedTopologyCatalog::create(valid.id(), valid.source(), std::string(64, 'A'),
                                              valid.product_key(), valid.entries()));
  CHECK_FALSE(ImportedTopologyCatalog::create(valid.id(), valid.source(),
                                              valid.captured_sha256_digest(), "STEP entity #17",
                                              valid.entries()));
  CHECK_FALSE(ImportedTopologyReference::create({}, ImportedTopologyId("face"),
                                                ImportedTopologyCapability::Plane));
  CHECK(to_string(ImportedTopologyRecoveryStatus::Resolved) == "resolved");
  CHECK(to_string(ImportedTopologyRecoveryStatus::Lost) == "lost");
  CHECK(to_string(ImportedTopologyRecoveryStatus::Ambiguous) == "ambiguous");
}

TEST_CASE("Imported capabilities are explicit and typed Part inputs retain catalog dependencies",
          "[core][step-import-topology]") {
  auto p = part();
  REQUIRE(p.add_step_import_source(source()));
  REQUIRE(p.add_imported_topology_catalog(catalog()));
  CHECK(imported_topology_capabilities(catalog().find(ImportedTopologyId("face"))->descriptor) ==
        std::vector{ImportedTopologyCapability::Plane, ImportedTopologyCapability::Face});
  CHECK(validate_imported_topology_reference(p, ref("cylinder", ImportedTopologyCapability::Axis)));
  CHECK(validate_imported_topology_reference(p, ref("circle", ImportedTopologyCapability::Point)));
  CHECK(validate_imported_topology_reference(p, ref("vertex", ImportedTopologyCapability::Point)));
  CHECK_FALSE(
      validate_imported_topology_reference(p, ref("line", ImportedTopologyCapability::Axis)));
  CHECK_FALSE(
      validate_imported_topology_reference(p, ref("face", ImportedTopologyCapability::Cylinder)));
  CHECK_FALSE(
      validate_imported_topology_reference(p, ref("missing", ImportedTopologyCapability::Point)));
  auto axis = checked(AxisReference::create_imported(
      PartFeatureInputRole::RevolveAxis, ref("cylinder", ImportedTopologyCapability::Axis)));
  CHECK(axis.source_kind() == PartFeatureInputSourceKind::ImportedTopology);
  CHECK(axis.source_node_id() == catalog().node_id());
  CHECK(validate_part_feature_input_reference(p, axis));
  CHECK(PlaneReference::create_imported(PartFeatureInputRole::MirrorPlane,
                                        ref("face", ImportedTopologyCapability::Plane)));
  CHECK(FaceReference::create_imported(PartFeatureInputRole::ExtrudeLimitFace,
                                       ref("face", ImportedTopologyCapability::Face)));
  CHECK(EdgeReference::create_imported(PartFeatureInputRole::FilletEdge,
                                       ref("circle", ImportedTopologyCapability::Edge)));
  CHECK_FALSE(AxisReference::create_imported(PartFeatureInputRole::RevolveAxis,
                                             ref("line", ImportedTopologyCapability::Line)));
  CHECK(AxisReference::create_imported(PartFeatureInputRole::DraftPullDirection,
                                       ref("line", ImportedTopologyCapability::Line)));
  CHECK_FALSE(PlaneReference::create_imported(PartFeatureInputRole::MirrorPlane,
                                              ref("cylinder", ImportedTopologyCapability::Axis)));
  CHECK_FALSE(FaceReference::create_imported(PartFeatureInputRole::MirrorPlane,
                                             ref("face", ImportedTopologyCapability::Face)));
  auto opaque_entries = catalog().entries();
  opaque_entries[1].descriptor.geometry = ImportedGeometryClass::Opaque;
  opaque_entries[1].descriptor.direction = {};
  opaque_entries[3].descriptor.geometry = ImportedGeometryClass::Opaque;
  opaque_entries[3].descriptor.direction = {};
  const auto opaque = checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId("opaque"), source().id(), std::string(64, '1'),
      std::string(64, '2'), std::move(opaque_entries)));
  REQUIRE(p.add_imported_topology_catalog(opaque));
  const auto opaque_reference = [&](const char* id, ImportedTopologyCapability capability) {
    return checked(
        ImportedTopologyReference::create(opaque.id(), ImportedTopologyId(id), capability));
  };
  CHECK(validate_imported_topology_reference(
      p, opaque_reference("face", ImportedTopologyCapability::Face)));
  CHECK(validate_imported_topology_reference(
      p, opaque_reference("line", ImportedTopologyCapability::Edge)));
  CHECK_FALSE(validate_imported_topology_reference(
      p, opaque_reference("face", ImportedTopologyCapability::Plane)));
  CHECK_FALSE(validate_imported_topology_reference(
      p, opaque_reference("line", ImportedTopologyCapability::Line)));
}

TEST_CASE("Imported catalog JSON preserves old bytes and independent Part and Project ownership",
          "[core][step-import-topology]") {
  auto p = part();
  auto assembly = project();
  const auto old_part = json(p), old_project = json(assembly);
  CHECK_FALSE(nlohmann::json::parse(old_part).contains("imported_topology_catalogs"));
  CHECK_FALSE(nlohmann::json::parse(old_project).contains("imported_topology_catalogs"));
  REQUIRE(p.add_step_import_source(source()));
  REQUIRE(p.add_imported_topology_catalog(catalog()));
  REQUIRE(assembly.add_step_import_source(source()));
  REQUIRE(assembly.add_imported_topology_catalog(catalog()));
  REQUIRE(assembly.add_part_document(p));
  const auto encoded = json(assembly);
  const auto restored = checked(deserialize_project_from_json(encoded));
  CHECK(restored.imported_topology_catalogs() == assembly.imported_topology_catalogs());
  CHECK(restored.part_documents().front().imported_topology_catalogs() ==
        p.imported_topology_catalogs());
  CHECK(json(restored) == encoded);
  CHECK(json(checked(deserialize_part_document_from_json(json(p)))) == json(p));
  REQUIRE(p.remove_imported_topology_catalog(ImportedTopologyCatalogId("catalog")));
  REQUIRE(p.remove_step_import_source(source()));
  CHECK(json(p) == old_part);
  auto empty = nlohmann::json::parse(old_project);
  empty["imported_topology_catalogs"] = nlohmann::json::array();
  CHECK(json(checked(deserialize_project_from_json(empty.dump()))) == old_project);
}

TEST_CASE("Imported catalog JSON rejects malformed descriptor evidence and dangling ownership",
          "[core][step-import-topology]") {
  const bool project_host = GENERATE(false, true);
  auto p = part();
  auto assembly = project();
  REQUIRE(p.add_step_import_source(source()));
  REQUIRE(p.add_imported_topology_catalog(catalog()));
  REQUIRE(assembly.add_step_import_source(source()));
  REQUIRE(assembly.add_imported_topology_catalog(catalog()));
  const auto root = nlohmann::json::parse(project_host ? json(assembly) : json(p));
  const auto rejects = [&](auto value) {
    if (project_host)
      CHECK_FALSE(deserialize_project_from_json(value.dump()));
    else
      CHECK_FALSE(deserialize_part_document_from_json(value.dump()));
  };
  auto value = root;
  value["imported_topology_catalogs"] = nullptr;
  rejects(value);
  for (const char* field :
       {"id", "source", "captured_sha256_digest", "product_key", "descriptor_version", "entries"}) {
    value = root;
    value["imported_topology_catalogs"][0].erase(field);
    rejects(value);
  }
  value = root;
  value["imported_topology_catalogs"][0]["descriptor_version"] = 1.0;
  rejects(value);
  value = root;
  value["imported_topology_catalogs"][0]["source"] = "other";
  rejects(value);
  value = root;
  value["imported_topology_catalogs"][0]["raw_step_entity"] = 17;
  rejects(value);
  value = root;
  value["imported_topology_catalogs"].push_back(value["imported_topology_catalogs"][0]);
  rejects(value);
  value = root;
  auto& entries = value["imported_topology_catalogs"][0]["entries"];
  entries.push_back(entries[0]);
  rejects(value);
  value = root;
  value["imported_topology_catalogs"][0]["entries"][1]["parent_body"] = "missing";
  rejects(value);
  for (const auto& [field, replacement] : std::vector<std::pair<std::string, nlohmann::json>>{
           {"geometry", "cylinder"},
           {"direction", {{"x", 0}, {"y", 0}, {"z", -1}}},
           {"measure", -1},
           {"geometry_key", std::string(64, 'A')},
           {"samples", {{{"x", 0}, {"y", 0}}}},
           {"centroid", nullptr},
           {"body_kind", "solid"},
           {"radius_mm", -2},
           {"unknown", true}}) {
    value = root;
    value["imported_topology_catalogs"][0]["entries"][1]["descriptor"][field] = replacement;
    rejects(value);
  }
}

TEST_CASE("Imported assembly spellings are canonical disjoint and preserve arbitrary typed ids",
          "[core][step-import-topology]") {
  const auto reference = checked(ImportedTopologyReference::create(
      ImportedTopologyCatalogId("catalog.a/: %"), ImportedTopologyId("face:b/c.d%"),
      ImportedTopologyCapability::Plane));
  const auto spelling = checked(make_imported_topology_target_spelling(reference));
  CHECK(spelling == "imported:catalog%2Ea%2F%3A%20%25:face%3Ab%2Fc%2Ed%25:plane");
  CHECK(spelling.find('.') == std::string::npos);
  CHECK(checked(parse_imported_topology_target_spelling(spelling)) == reference);
  for (const char* invalid :
       {"imported:", "imported:a:b:bogus", "imported:a:b:plane:extra", "imported:a%2e:b:plane",
        "imported:%61:b:plane", "imported:a:b.c:plane", "topo:a:b:plane", "ref:plane:a"})
    CHECK_FALSE(parse_imported_topology_target_spelling(invalid));
}

TEST_CASE(
    "Imported catalog dependencies survive feature updates JSON and source refresh atomically",
    "[core][step-import-topology]") {
  auto p = part();
  REQUIRE(p.add_step_import_source(source()));
  REQUIRE(p.add_imported_topology_catalog(catalog()));
  add_profile(p);
  REQUIRE(p.add_body(checked(Body::create(BodyId("result"), "Authored result", BodyKind::Solid))));
  const auto feature = checked(RevolveFeature::create_revolve(
      FeatureId("revolve"), "Imported cylinder axis",
      checked(ProfileRegionReference::create(SketchId("sketch"), ProfileId("profile"),
                                             PartFeatureInputRole::RevolveProfile)),
      checked(AxisReference::create_imported(PartFeatureInputRole::RevolveAxis,
                                             ref("cylinder", ImportedTopologyCapability::Axis))),
      RevolveAngleExtent::full(),
      checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                               BodyId("result")))));
  REQUIRE(p.add_revolve_feature(feature));
  CHECK(p.dependency_graph().has_dependency(catalog().node_id(), "revolve"));
  const auto initial = json(p);
  CHECK_FALSE(p.remove_imported_topology_catalog(ImportedTopologyCatalogId("catalog")));
  CHECK_FALSE(p.remove_step_import_source(source()));
  CHECK(json(p) == initial);
  REQUIRE(p.update_revolve_feature(feature));
  CHECK(p.imported_topology_catalogs() == std::vector{catalog()});
  CHECK(json(checked(deserialize_part_document_from_json(json(p)))) == json(p));
  p.mark_all_clean();
  REQUIRE(p.refresh_step_import_source(source(), std::string(64, '3')));
  CHECK(p.imported_topology_catalogs() == std::vector{catalog()});
  CHECK(p.invalidation_state().find(catalog().node_id())->status == InvalidationStatus::Changed);
  CHECK(p.invalidation_state().find("revolve")->status == InvalidationStatus::Dirty);
  CHECK_FALSE(p.refresh_step_import_source(source(), std::string(64, '4')));
  const auto after = json(p);
  CHECK(json(checked(deserialize_part_document_from_json(after))) == after);
  REQUIRE(p.remove_revolve_feature(feature.id()));
  REQUIRE(p.remove_imported_topology_catalog(ImportedTopologyCatalogId("catalog")));
  REQUIRE(p.remove_step_import_source(StepImportSourceId("source")));
}
