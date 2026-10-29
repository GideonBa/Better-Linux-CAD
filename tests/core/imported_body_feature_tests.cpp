#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
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
StepImportSource source(StepImportMode mode = StepImportMode::EditableBody) {
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
  face.origin = {0, 0, 10};
  face.direction = {0, 0, 1};
  face.measure = 100;
  face.geometry_key = std::string(64, 'c');
  ImportedTopologyDescriptor edge;
  edge.kind = ImportedTopologyKind::Edge;
  edge.geometry = ImportedGeometryClass::Line;
  edge.bounds_mm = {0, 0, 0, 10, 0, 0};
  edge.centroid = {5, 0, 0};
  edge.direction = {1, 0, 0};
  edge.measure = 10;
  edge.geometry_key = std::string(64, 'd');
  return checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId("catalog"), source().id(), source().sha256_digest(),
      std::string(64, '2'),
      {{ImportedTopologyId("solid"), std::nullopt, solid},
       {ImportedTopologyId("surface"), std::nullopt, surface},
       {ImportedTopologyId("face"), ImportedTopologyId("solid"), face},
       {ImportedTopologyId("edge"), ImportedTopologyId("solid"), edge}}));
}
std::vector<ImportedBodyOutput> outputs() {
  return {{ImportedTopologyId("surface"), BodyId("z.surface"), BodyKind::Surface},
          {ImportedTopologyId("solid"), BodyId("a.solid"), BodyKind::Solid}};
}
ImportedBodyFeature base(std::vector<ImportedBodyOutput> mapping = outputs(),
                         FeatureId id = FeatureId("base")) {
  auto definition =
      checked(ImportedPartDefinition::create(source().id(), catalog().id(), std::move(mapping)));
  return checked(ImportedBodyFeature::create(std::move(id), "Imported supplier", definition));
}
PartDocument part(bool all_outputs = true) {
  auto mapping = outputs();
  if (!all_outputs)
    mapping.erase(mapping.begin());
  return checked(PartDocument::create_editable_import(DocumentId("editable"), "Editable supplier",
                                                      source(), catalog(), base(mapping)));
}
std::string saved(const PartDocument& document) {
  return checked(serialize_part_document_to_json(document));
}
Parameter parameter(const char* id, double value = 2) {
  return checked(
      Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))));
}
ImportedTopologyReference reference(const char* entry, ImportedTopologyCapability capability) {
  return checked(
      ImportedTopologyReference::create(catalog().id(), ImportedTopologyId(entry), capability));
}
Feature in_body(Feature feature, FeatureBodyOperationMode operation,
                BodyId body = BodyId("a.solid")) {
  auto context = checked(FeatureBodyResultContext::create(operation, body, std::nullopt));
  return checked(feature.with_body_result_context(context));
}
void add_downstream(PartDocument& document) {
  for (const auto* id : {"width", "height", "depth", "diameter", "radius"})
    REQUIRE(document.add_parameter(parameter(id)));
  auto add = checked(Sketch::create(SketchId("sketch.add"), "Add", DatumPlaneId("datum.xy")));
  REQUIRE(add.add_profile(checked(RectangleProfile::create(
      ProfileId("profile.add"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(document.add_sketch(add));
  auto extrusion = checked(Feature::create_additive_extrude(FeatureId("add"), "Additive", add.id(),
                                                            ParameterId("depth")));
  REQUIRE(document.add_feature(in_body(extrusion, FeatureBodyOperationMode::Join)));
  auto hole = checked(Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy")));
  REQUIRE(hole.add_profile(
      checked(CircleProfile::create(ProfileId("profile.hole"), ParameterId("diameter")))));
  REQUIRE(document.add_sketch(hole));
  auto subtraction = checked(
      Feature::create_subtractive_extrude(FeatureId("hole"), "Hole", hole.id(), FeatureId("base")));
  REQUIRE(document.add_feature(in_body(subtraction, FeatureBodyOperationMode::Cut)));
  auto edge = checked(EdgeReference::create_imported(
      PartFeatureInputRole::FilletEdge, reference("edge", ImportedTopologyCapability::Edge)));
  REQUIRE(document.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Fillet", BodyId("a.solid"), {edge}, ParameterId("radius")))));
}
void ordered(const DependencyGraph& graph, const std::vector<std::string>& expected) {
  const auto order = checked(graph.topological_order());
  for (std::size_t index = 1; index < expected.size(); ++index) {
    INFO(expected[index - 1] << " before " << expected[index]);
    const auto previous = std::find(order.begin(), order.end(), expected[index - 1]);
    const auto current = std::find(order.begin(), order.end(), expected[index]);
    REQUIRE(previous != order.end());
    REQUIRE(current != order.end());
    CHECK(previous < current);
  }
}
auto canonical_edges(const DependencyGraph& graph) {
  auto edges = graph.dependencies();
  std::sort(edges.begin(), edges.end());
  return edges;
}
} // namespace

TEST_CASE("Editable import creates an immutable base with exact ordered Solid and Surface outputs",
          "[core][imported-body-feature]") {
  const auto document = part();
  CHECK(document.mode() == PartDocumentMode::Editable);
  CHECK_FALSE(document.reference_part_definition());
  REQUIRE(document.imported_body_features().size() == 1);
  REQUIRE(document.find_imported_body_feature(FeatureId("base")));
  CHECK(*document.find_imported_body_feature(FeatureId("base")) == base());
  CHECK(document.imported_body_features().front().definition().outputs() == outputs());
  REQUIRE(document.bodies().size() == 2);
  CHECK(document.bodies()[0].id() == BodyId("a.solid"));
  CHECK(document.bodies()[1].id() == BodyId("z.surface"));
  CHECK(document.find_body(BodyId("z.surface"))->kind() == BodyKind::Surface);
  CHECK(document.datum_plane_count() == 3);
  CHECK(document.features().empty());
  CHECK(document.parameters().empty());
  const auto& graph = document.dependency_graph();
  CHECK(graph.has_dependency(source().node_id(), catalog().node_id()));
  CHECK(graph.has_dependency(catalog().node_id(), "base"));
  CHECK(graph.has_dependency("base", "body:a.solid"));
  CHECK(graph.has_dependency("base", "body:z.surface"));
  auto loaded = checked(deserialize_part_document_from_json(saved(document)));
  CHECK(saved(loaded) == saved(document));
  CHECK(loaded.imported_body_features() == document.imported_body_features());
  CHECK(canonical_edges(loaded.dependency_graph()) == canonical_edges(graph));
}

TEST_CASE(
    "Editable import rejects foreign modes catalog selections and graph collisions atomically",
    "[core][imported-body-feature]") {
  CHECK_FALSE(PartDocument::create_editable_import(
      DocumentId("p"), "Wrong mode", source(StepImportMode::Reference), catalog(), base()));
  for (const auto* entry : {"missing", "face", "surface"}) {
    auto selected = outputs();
    selected.erase(selected.begin());
    selected.front().imported_body = ImportedTopologyId(entry);
    CHECK_FALSE(PartDocument::create_editable_import(DocumentId("p"), "Wrong selection", source(),
                                                     catalog(), base(selected)));
  }
  auto foreign = checked(
      ImportedPartDefinition::create(StepImportSourceId("foreign"), catalog().id(), outputs()));
  auto foreign_base = checked(ImportedBodyFeature::create(FeatureId("base"), "Foreign", foreign));
  CHECK_FALSE(PartDocument::create_editable_import(DocumentId("p"), "Wrong owner", source(),
                                                   catalog(), foreign_base));
  CHECK_FALSE(PartDocument::create_editable_import(DocumentId("p"), "Source collision", source(),
                                                   catalog(),
                                                   base(outputs(), FeatureId(source().node_id()))));
  auto document = checked(PartDocument::create(DocumentId("p"), "Collision"));
  REQUIRE(document.add_parameter(parameter(source().node_id().c_str())));
  const auto before = saved(document);
  CHECK_FALSE(document.add_step_import_source(source()));
  CHECK(saved(document) == before);
  CHECK(document.step_import_sources().empty());
  auto removable = checked(PartDocument::create(DocumentId("q"), "Metadata"));
  REQUIRE(removable.add_step_import_source(source()));
  CHECK(removable.dependency_graph().has_node(source().node_id()));
  REQUIRE(removable.remove_step_import_source(source()));
  CHECK_FALSE(removable.dependency_graph().has_node(source().node_id()));
  CHECK_FALSE(removable.invalidation_state().has_node(source().node_id()));
}

TEST_CASE("Editable base catalog Body mappings are unique while disjoint selections remain valid",
          "[core][imported-body-feature]") {
  auto document = part(false);
  CHECK(validate_imported_topology_reference(document,
                                             reference("face", ImportedTopologyCapability::Plane)));
  CHECK_FALSE(validate_imported_topology_reference(
      document, reference("surface", ImportedTopologyCapability::Body)));
  REQUIRE(
      document.add_body(checked(Body::create(BodyId("duplicate"), "Duplicate", BodyKind::Solid))));
  const auto before = saved(document);
  const auto duplicate = base({{ImportedTopologyId("solid"), BodyId("duplicate"), BodyKind::Solid}},
                              FeatureId("duplicate.base"));
  CHECK_FALSE(document.add_imported_body_feature(duplicate));
  CHECK(saved(document) == before);
  REQUIRE(
      document.add_body(checked(Body::create(BodyId("z.surface"), "Surface", BodyKind::Surface))));
  const auto second = base({outputs().front()}, FeatureId("surface.base"));
  REQUIRE(document.add_imported_body_feature(second));
  CHECK(validate_imported_topology_reference(
      document, reference("surface", ImportedTopologyCapability::Body)));
  const auto loaded = checked(deserialize_part_document_from_json(saved(document)));
  CHECK(saved(loaded) == saved(document));
}

TEST_CASE(
    "Imported bases order downstream additive hole and edge treatment intent through JSON replay",
    "[core][imported-body-feature]") {
  auto document = part();
  add_downstream(document);
  const std::vector<std::string> chain{
      source().node_id(), catalog().node_id(), "base", "add", "hole", "fillet", "body:a.solid"};
  ordered(document.dependency_graph(), chain);
  CHECK(document.dependency_graph().has_dependency("base", "add"));
  CHECK(document.dependency_graph().has_dependency("add", "hole"));
  CHECK(document.dependency_graph().has_dependency("hole", "fillet"));
  auto loaded = checked(deserialize_part_document_from_json(saved(document)));
  CHECK(saved(loaded) == saved(document));
  CHECK(canonical_edges(loaded.dependency_graph()) == canonical_edges(document.dependency_graph()));
  ordered(loaded.dependency_graph(), chain);
  // Updating the first in-place modifier must retain the real base predecessor,
  // not replace it with the current terminal feature or lose its Body authority.
  const auto replacement = *document.find_feature(FeatureId("add"));
  REQUIRE(document.update_feature(replacement));
  CHECK(document.dependency_graph().has_dependency("base", "add"));
  ordered(document.dependency_graph(), chain);
  CHECK(saved(document) == saved(loaded));
  document.mark_all_clean();
  const auto changed =
      checked(document.set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(4))));
  CHECK(std::find(changed.begin(), changed.end(), "add") != changed.end());
  const auto plan = checked(document.create_recompute_plan());
  CHECK(plan.contains("add"));
  CHECK(plan.contains("hole"));
  CHECK(plan.contains("fillet"));
  CHECK_FALSE(plan.contains("base"));
  CHECK_FALSE(plan.contains(catalog().node_id()));
  CHECK_FALSE(plan.contains(source().node_id()));
}

TEST_CASE(
    "Imported subtraction requires an explicit mapped Solid Body without mutation on rejection",
    "[core][imported-body-feature]") {
  auto document = part();
  auto sketch = checked(Sketch::create(SketchId("sketch"), "Cut", DatumPlaneId("datum.xy")));
  REQUIRE(document.add_sketch(sketch));
  auto cut = checked(
      Feature::create_subtractive_extrude(FeatureId("cut"), "Cut", sketch.id(), FeatureId("base")));
  const auto before = saved(document);
  CHECK_FALSE(document.add_feature(cut));
  CHECK(saved(document) == before);
  CHECK_FALSE(
      document.add_feature(in_body(cut, FeatureBodyOperationMode::Cut, BodyId("z.surface"))));
  CHECK(saved(document) == before);
  REQUIRE(document.add_feature(in_body(cut, FeatureBodyOperationMode::Cut)));
  CHECK(saved(checked(deserialize_part_document_from_json(saved(document)))) == saved(document));
}

TEST_CASE(
    "Explicit source refresh invalidates the complete imported modeling chain and preserves intent",
    "[core][imported-body-feature]") {
  auto document = part();
  add_downstream(document);
  document.mark_all_clean();
  const auto original = saved(document);
  CHECK_FALSE(document.refresh_step_import_source(source(), "invalid digest"));
  CHECK(saved(document) == original);
  CHECK(checked(document.create_recompute_plan()).empty());
  REQUIRE(document.refresh_step_import_source(source(), std::string(64, '3')));
  const auto plan = checked(document.create_recompute_plan());
  // Changed inputs are already authored; the state-driven execution plan
  // contains their Dirty dependents, including every imported output Body.
  CHECK_FALSE(plan.contains(source().node_id()));
  for (const auto& node :
       {catalog().node_id(), std::string("base"), std::string("add"), std::string("hole"),
        std::string("fillet"), std::string("body:a.solid"), std::string("body:z.surface")}) {
    CAPTURE(node);
    CHECK(plan.contains(node));
    const auto* state = document.invalidation_state().find(node);
    REQUIRE(state);
    CHECK(state->status == InvalidationStatus::Dirty);
  }
  const auto explicit_roots = checked(RecomputePlan::from_graph_and_changed_nodes(
      document.dependency_graph(), {source().node_id()}));
  CHECK(explicit_roots.contains(source().node_id()));
  for (const auto& node : plan.node_ids()) {
    CAPTURE(node);
    CHECK(explicit_roots.contains(node));
  }
  CHECK(document.invalidation_state().find(source().node_id())->status ==
        InvalidationStatus::Changed);
  CHECK(document.invalidation_state().find("base")->status == InvalidationStatus::Dirty);
  CHECK(document.imported_body_features().front() == base());
  CHECK(*document.find_imported_topology_catalog(catalog().id()) == catalog());
  const auto refreshed = saved(document);
  CHECK_FALSE(document.refresh_step_import_source(source(), std::string(64, '4')));
  CHECK_FALSE(document.remove_step_import_source(source().id()));
  CHECK_FALSE(document.remove_imported_topology_catalog(catalog().id()));
  CHECK(saved(document) == refreshed);
  CHECK(saved(checked(deserialize_part_document_from_json(refreshed))) == refreshed);
}

TEST_CASE(
    "Imported base removal rejects consumers and terminal modifier removal restores its producer",
    "[core][imported-body-feature]") {
  auto document = part();
  add_downstream(document);
  const auto before = saved(document);
  CHECK_FALSE(document.remove_imported_body_feature(FeatureId("base")));
  CHECK_FALSE(document.remove_feature(FeatureId("base")));
  CHECK_FALSE(document.update_feature(checked(Feature::create_additive_extrude(
      FeatureId("base"), "Replace immutable base", SketchId("sketch.add"), ParameterId("depth")))));
  CHECK(saved(document) == before);
  REQUIRE(document.remove_fillet_feature(FeatureId("fillet")));
  CHECK(document.dependency_graph().has_dependency("hole", "body:a.solid"));
  REQUIRE(document.remove_feature(FeatureId("hole")));
  CHECK(document.dependency_graph().has_dependency("add", "body:a.solid"));
  REQUIRE(document.remove_feature(FeatureId("add")));
  CHECK(document.dependency_graph().has_dependency("base", "body:a.solid"));
  REQUIRE(document.remove_feature(FeatureId("base")));
  CHECK_FALSE(document.find_imported_body_feature(FeatureId("base")));
  CHECK_FALSE(document.dependency_graph().has_node("base"));
  CHECK(document.body_count() == 2);
  CHECK(document.find_imported_topology_catalog(catalog().id()));
  CHECK(document.find_step_import_source(source().id()));
  CHECK(saved(checked(deserialize_part_document_from_json(saved(document)))) == saved(document));
}

TEST_CASE(
    "Imported planar face provenance binds real selected output Bodies for downstream Sketches",
    "[core][imported-body-feature]") {
  auto document = part();
  const auto face = reference("face", ImportedTopologyCapability::Face);
  const auto role = checked(make_imported_face_origin_role(face));
  CHECK(role == "imported-face/imported:catalog:face:face");
  CHECK(checked(parse_imported_face_origin_role(role)) == face);
  CHECK_FALSE(make_imported_face_origin_role(reference("face", ImportedTopologyCapability::Plane)));
  for (const auto* invalid : {"imported-face/", "imported-face/imported:catalog:face:plane",
                              "imported-face/imported:catalog:%66ace:face", "base.top",
                              "imported-face/imported-face/imported:catalog:face:face"})
    CHECK_FALSE(parse_imported_face_origin_role(invalid));
  const auto support = [&](std::string origin_role, BodyId body = BodyId("a.solid")) {
    auto face_ref = checked(ResultFaceReference::create(
        FeatureId("base"), std::move(body), {{FeatureId("base"), std::move(origin_role)}}));
    return checked(ResultFaceWorkplane::create(DatumPlaneId("support"), "Imported face", face_ref));
  };
  const auto original = saved(document);
  for (const auto* invalid :
       {"top", "imported-face/imported:catalog:edge:face",
        "imported-face/imported:foreign:face:face", "imported-face/imported:catalog:absent:face"}) {
    CHECK_FALSE(document.add_result_face_workplane(support(invalid)));
    CHECK(saved(document) == original);
  }
  REQUIRE(
      document.add_body(checked(Body::create(BodyId("unrelated"), "Unrelated", BodyKind::Solid))));
  CHECK_FALSE(document.add_result_face_workplane(support(role, BodyId("unrelated"))));
  auto sketch =
      checked(Sketch::create(SketchId("face.sketch"), "Face sketch", DatumPlaneId("support")));
  REQUIRE(document.add_face_supported_sketch(support(role), sketch));
  CHECK(document.dependency_graph().has_dependency("base", "support"));
  CHECK(document.dependency_graph().has_dependency("support", "face.sketch"));
  const auto with_sketch = saved(document);
  CHECK_FALSE(document.remove_imported_body_feature(FeatureId("base")));
  CHECK(saved(document) == with_sketch);
  CHECK(saved(checked(deserialize_part_document_from_json(with_sketch))) == with_sketch);
}

TEST_CASE(
    "Imported base JSON rejects malformed bindings duplicate mappings and invalid executable order",
    "[core][imported-body-feature]") {
  using json = nlohmann::json;
  auto document = part();
  add_downstream(document);
  const auto initial = json::parse(saved(document));
  const std::vector<std::pair<std::string, std::function<void(json&)>>> mutations{
      {"wrong source mode", [](auto& j) { j["step_import_sources"][0]["mode"] = "reference"; }},
      {"foreign source",
       [](auto& j) { j["imported_body_features"][0]["definition"]["source"] = "other"; }},
      {"unknown catalog",
       [](auto& j) { j["imported_body_features"][0]["definition"]["catalog"] = "other"; }},
      {"unknown imported Body",
       [](auto& j) {
         j["imported_body_features"][0]["definition"]["outputs"][0]["imported_body"] = "absent";
       }},
      {"wrong output kind",
       [](auto& j) {
         j["imported_body_features"][0]["definition"]["outputs"][0]["kind"] = "solid";
       }},
      {"extra runtime field", [](auto& j) { j["imported_body_features"][0]["shape"] = "payload"; }},
      {"duplicate imported selection",
       [](auto& j) {
         auto duplicate = j["imported_body_features"][0];
         duplicate["id"] = "second.base";
         duplicate["definition"]["outputs"] = {duplicate["definition"]["outputs"][1]};
         duplicate["definition"]["outputs"][0]["body"] = "other.solid";
         j["imported_body_features"].push_back(duplicate);
         auto body = j["bodies"][0];
         body["id"] = "other.solid";
         j["bodies"].push_back(body);
       }},
      {"base after dependent",
       [](auto& j) { j["feature_order"] = {"add", "base", "hole", "fillet"}; }},
      {"missing base executable", [](auto& j) { j["feature_order"] = {"add", "hole", "fillet"}; }},
  };
  for (const auto& [name, mutate] : mutations) {
    DYNAMIC_SECTION(name) {
      auto changed = initial;
      mutate(changed);
      CHECK_FALSE(deserialize_part_document_from_json(changed.dump()));
      CHECK_FALSE(deserialize_part_document_from_json(changed.dump(),
                                                      PartDocumentReadMode::ReferenceRepair));
    }
  }
}

TEST_CASE("Pattern and Mirror feature sources preserve imported base identity through JSON",
          "[core][imported-body-feature]") {
  auto document = part(false);
  REQUIRE(document.add_parameter(checked(
      Parameter::create_count(ParameterId("count"), "Count", checked(Quantity::count(2))))));
  REQUIRE(document.add_parameter(parameter("spacing", 20)));
  REQUIRE(document.add_datum_axis(
      checked(DatumAxis::create_explicit(DatumAxisId("axis"), "Axis", {}, {0, 0, 1}))));
  REQUIRE(document.add_body(checked(Body::create(BodyId("copies"), "Copies", BodyKind::Solid))));
  const auto context = checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                std::nullopt, BodyId("copies")));
  const auto axis = checked(
      AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, DatumAxisId("axis")));
  const auto source_feature = checked(PatternSourceReference::feature(FeatureId("base")));
  SECTION("Linear pattern") {
    const auto feature = checked(LinearPatternFeature::create(
        FeatureId("copies"), "Linear", {source_feature}, axis, ParameterId("count"),
        LinearPatternExtentMode::Spacing, ParameterId("spacing"), PatternDirectionSign::Positive,
        context));
    REQUIRE(document.add_linear_pattern_feature(feature));
  }
  SECTION("Circular pattern") {
    const auto feature =
        checked(CircularPatternFeature::create(FeatureId("copies"), "Circular", {source_feature},
                                               axis, ParameterId("count"), 180, true, context));
    REQUIRE(document.add_circular_pattern_feature(feature));
  }
  SECTION("Mirror") {
    const auto plane = checked(PlaneReference::create_datum_plane(PartFeatureInputRole::MirrorPlane,
                                                                  DatumPlaneId("datum.xy")));
    const auto mirror_source = checked(MirrorSourceReference::feature(FeatureId("base")));
    const auto feature = checked(
        MirrorFeature::create(FeatureId("copies"), "Mirror", {mirror_source}, plane, context));
    REQUIRE(document.add_mirror_feature(feature));
  }
  CHECK(document.dependency_graph().has_dependency("base", "copies"));
  const auto original = saved(document);
  auto loaded = checked(deserialize_part_document_from_json(original));
  CHECK(saved(loaded) == original);
  CHECK(loaded.dependency_graph().has_dependency("base", "copies"));
  CHECK_FALSE(loaded.remove_imported_body_feature(FeatureId("base")));
  CHECK(saved(loaded) == original);
}

TEST_CASE("Authored graph identities cannot alias registered imported authority in either "
          "insertion order",
          "[core][imported-body-feature]") {
  const std::vector<std::string> authorities{source().node_id(), catalog().node_id(), "base",
                                             "body:a.solid"};
  for (const auto& node : authorities) {
    DYNAMIC_SECTION("Imported authority precedes authored identity " << node) {
      auto document = part();
      REQUIRE(document.add_parameter(parameter("depth")));
      REQUIRE(document.add_sketch(
          checked(Sketch::create(SketchId("sketch"), "Sketch", DatumPlaneId("datum.xy")))));
      const auto original = saved(document);
      const auto nodes = document.dependency_graph().nodes();
      const auto edges = document.dependency_graph().dependencies();
      const auto unchanged = [&] {
        CHECK(saved(document) == original);
        CHECK(document.dependency_graph().nodes() == nodes);
        CHECK(document.dependency_graph().dependencies() == edges);
      };
      CHECK_FALSE(document.add_parameter(parameter(node.c_str())));
      unchanged();
      CHECK_FALSE(document.add_expression_parameter(ParameterId(node), "Expression",
                                                    ParameterType::Length, "depth * 2"));
      unchanged();
      CHECK_FALSE(document.add_sketch(
          checked(Sketch::create(SketchId(node), "Aliased Sketch", DatumPlaneId("datum.xy")))));
      unchanged();
      CHECK_FALSE(document.add_datum_axis(
          checked(DatumAxis::create_explicit(DatumAxisId(node), "Aliased axis", {}, {0, 0, 1}))));
      unchanged();
      const auto feature = checked(Feature::create_additive_extrude(
          FeatureId(node), "Aliased feature", SketchId("sketch"), ParameterId("depth")));
      CHECK_FALSE(document.add_feature(feature));
      unchanged();
    }
    DYNAMIC_SECTION("Authored identity precedes imported authority " << node) {
      auto document = checked(PartDocument::create(DocumentId("legacy"), "Legacy"));
      REQUIRE(document.add_parameter(parameter(node.c_str())));
      if (node == source().node_id()) {
        const auto original = saved(document);
        CHECK_FALSE(document.add_step_import_source(source()));
        CHECK(saved(document) == original);
      } else {
        REQUIRE(document.add_step_import_source(source()));
        if (node == catalog().node_id()) {
          const auto original = saved(document);
          CHECK_FALSE(document.add_imported_topology_catalog(catalog()));
          CHECK(saved(document) == original);
        } else {
          REQUIRE(document.add_imported_topology_catalog(catalog()));
          for (const auto& output : outputs())
            REQUIRE(document.add_body(
                checked(Body::create(output.body, output.body.value(), output.kind))));
          const auto original = saved(document);
          CHECK_FALSE(document.add_imported_body_feature(base()));
          CHECK(saved(document) == original);
        }
      }
    }
  }
}

TEST_CASE(
    "Imported base ResultEdge witnesses require their real output Body at finishing consumers",
    "[core][imported-body-feature]") {
  auto document = part();
  REQUIRE(document.add_parameter(parameter("radius", 1)));
  REQUIRE(
      document.add_body(checked(Body::create(BodyId("unrelated"), "Unrelated", BodyKind::Solid))));
  std::array<Point3, ResultEdgeReference::sample_count> samples;
  for (std::size_t index = 0; index < samples.size(); ++index)
    samples[index] = {10.0 * index / (samples.size() - 1), 0, 0};
  const auto witness = checked(ResultEdgeReference::create(FeatureId("base"), 0, 10, samples));
  const auto edge =
      checked(EdgeReference::create_result(PartFeatureInputRole::FilletEdge, witness));
  REQUIRE(validate_part_feature_input_reference(document, edge));
  const auto original = saved(document);
  const auto wrong = checked(FilletFeature::create(
      FeatureId("round"), "Wrong Body", BodyId("unrelated"), {edge}, ParameterId("radius")));
  CHECK_FALSE(document.add_fillet_feature(wrong));
  CHECK(saved(document) == original);
  const auto chamfer_edge =
      checked(EdgeReference::create_result(PartFeatureInputRole::ChamferEdge, witness));
  const auto wrong_chamfer = checked(
      ChamferFeature::create_equal_distance(FeatureId("bevel"), "Wrong Body", BodyId("unrelated"),
                                            {chamfer_edge}, ParameterId("radius")));
  CHECK_FALSE(document.add_chamfer_feature(wrong_chamfer));
  CHECK(saved(document) == original);
  const auto fillet = checked(FilletFeature::create(
      FeatureId("round"), "Actual output", BodyId("a.solid"), {edge}, ParameterId("radius")));
  REQUIRE(document.add_fillet_feature(fillet));
  CHECK(document.dependency_graph().has_dependency("base", "round"));
  const auto with_fillet = saved(document);
  CHECK(saved(checked(deserialize_part_document_from_json(with_fillet))) == with_fillet);
  auto malformed = nlohmann::json::parse(with_fillet);
  malformed["edge_treatments"][0]["target_body"] = "unrelated";
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
}
