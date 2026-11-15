#include "../support/engineering_resource_fixture.hpp"
#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <nlohmann/json.hpp>

using namespace blcad;
using json = nlohmann::json;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace ef = blcad::test::engineering_fixture;
namespace {
PathCurve path(const char* id, std::vector<PathSegmentReference> segments) {
  return f::checked(PathCurve::create(PathCurveId(id), id, std::move(segments), PathClosure::Open,
                                      PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                                      f::checked(PathExecutionPolicy::curve_preserving(1e-6))));
}
std::string saved(const PartDocument& part) {
  return f::checked(serialize_part_document_to_json(part));
}
EdgeReference generated(PartFeatureInputRole role = PartFeatureInputRole::PathSegment) {
  return f::checked(
      EdgeReference::create_linear(role, f::checked(SemanticEdgeReference::create(
                                             FeatureId("base.feature"), SemanticEdge::TopFront))));
}
PartDocument imported() {
  const auto source =
      f::checked(StepImportSource::create(StepImportSourceId("source"), "Source", "supplier.step",
                                          std::string(64, 'a'), StepImportMode::EditableBody));
  ImportedTopologyDescriptor body;
  body.kind = ImportedTopologyKind::Body;
  body.geometry = ImportedGeometryClass::Opaque;
  body.body_kind = BodyKind::Solid;
  body.bounds_mm = {0, 0, 0, 10, 10, 10};
  body.centroid = {5, 5, 5};
  body.measure = 1000;
  body.geometry_key = std::string(64, 'b');
  ImportedTopologyDescriptor edge;
  edge.kind = ImportedTopologyKind::Edge;
  edge.geometry = ImportedGeometryClass::Line;
  edge.bounds_mm = {0, 0, 0, 10, 0, 0};
  edge.centroid = {5, 0, 0};
  edge.direction = {1, 0, 0};
  edge.measure = 10;
  edge.geometry_key = std::string(64, 'c');
  const auto catalog = f::checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId("catalog"), source.id(), source.sha256_digest(),
      std::string(64, 'd'),
      {{ImportedTopologyId("solid"), {}, body},
       {ImportedTopologyId("edge"), ImportedTopologyId("solid"), edge}}));
  const auto definition = f::checked(ImportedPartDefinition::create(
      source.id(), catalog.id(),
      {{ImportedTopologyId("solid"), BodyId("supplier.body"), BodyKind::Solid}}));
  const auto feature =
      f::checked(ImportedBodyFeature::create(FeatureId("supplier"), "Supplier", definition));
  return f::checked(PartDocument::create_editable_import(DocumentId("imported"), "Imported", source,
                                                         catalog, feature));
}
void project_roundtrip(const PartDocument& part) {
  for (bool owns_resources : {false, true}) {
    auto member_part = part;
    if (owns_resources)
      ef::annotate(member_part);
    auto assembly = f::checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
    (void)f::checked(assembly.add_member_part(part.id()));
    auto project = f::checked(Project::create(DocumentId("project"), "Project", assembly));
    (void)f::checked(project.add_part_document(member_part));
    if (!owns_resources)
      (void)f::checked(project.add_engineering_resource(ef::material()));
    const auto source = f::checked(serialize_project_to_json(project));
    const auto value = json::parse(source);
    REQUIRE(value.at("version") == 2);
    const auto& member = value.at("parts").at(0);
    CHECK(member.at("version") == 11);
    CHECK(member.contains("engineering_resource_refs"));
    CHECK_FALSE(member.contains("engineering_resources"));
    CHECK(member.at("engineering_resource_refs").size() == (owns_resources ? 3 : 0));
    const auto restored = f::checked(deserialize_project_from_json(source));
    CHECK(f::checked(serialize_project_to_json(restored)) == source);
    REQUIRE(restored.find_part_document(part.id()));
    CHECK(saved(*restored.find_part_document(part.id())) == saved(member_part));
  }
}
} // namespace

TEST_CASE("Typed feature-edge paths persist explicit role direction and source dependencies",
          "[core][parity-b167][semantic-sweep-intent]") {
  auto part = f::part();
  const auto segment = f::checked(PathSegmentReference::create_feature_edge(generated(), {}, true));
  REQUIRE(part.add_path_curve(path("edge.path", {segment})));
  const auto before = saved(part);
  const auto encoded = json::parse(before);
  CHECK(encoded.at("version") == 11);
  CHECK(encoded.at("path_curves").at(0).at("segments").at(0).at("source") == "feature_edge");
  CHECK(encoded.at("path_curves").at(0).at("segments").at(0).at("source_body").is_null());
  auto restored = f::checked(deserialize_part_document_from_json(before));
  CHECK(saved(restored) == before);
  const auto& actual = restored.find_path_curve(PathCurveId("edge.path"))->segments().front();
  CHECK(actual.reversed());
  CHECK(actual.feature_edge()->role() == PartFeatureInputRole::PathSegment);
  CHECK(actual.stable_key() == segment.stable_key());
  CHECK(actual.source_node_id() == "base.feature");
  CHECK_FALSE(SpatialConstraintTarget::external_curve(segment));
  CHECK_FALSE(
      PathSegmentReference::create_feature_edge(generated(PartFeatureInputRole::FilletEdge)));
  CHECK_FALSE(PathSegmentReference::create_feature_edge(generated(), BodyId("base.body")));
  CHECK_FALSE(PathCurve::create(PathCurveId("legacy"), "Legacy", {segment}, PathClosure::Open,
                                PathOrientationRule::MinimumTwist));
  CHECK_FALSE(PathCurve::create(PathCurveId("duplicate"), "Duplicate", {segment, segment},
                                PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                                f::checked(PathExecutionPolicy::curve_preserving(1e-6))));
  CHECK_FALSE(
      part.set_path_execution_policy(PathCurveId("edge.path"), PathExecutionPolicy::legacy()));
  CHECK(saved(part) == before);
  CHECK_FALSE(part.remove_feature(FeatureId("base.feature")));
  CHECK(saved(part) == before);
  project_roundtrip(part);
}

TEST_CASE("Material-region Sweep is explicit and legacy profile snapshots remain canonical",
          "[core][parity-b167][semantic-sweep-intent]") {
  auto part = sf::straight_sweep();
  const auto legacy = saved(part);
  const auto old = *part.find_sweep_feature(FeatureId("driven.sweep"));
  REQUIRE(old.profile().region_policy() == SweepRegionPolicy::OuterBoundary);
  const auto region = std::get<ProfileRegionReference>(old.profile().source());
  const auto material = f::checked(
      SweepProfileReference::create_closed_region(region, SweepRegionPolicy::MaterialRegion));
  REQUIRE(part.update_sweep_feature(f::checked(SweepFeature::create_sweep(
      old.id(), old.name(), material, old.path(), old.body_result_context()))));
  const auto current = saved(part);
  const auto encoded = json::parse(current);
  CHECK(encoded.at("version") == 11);
  CHECK(encoded.at("sweep_features").at(0).at("profile").at("region_policy") == "material_region");
  CHECK(saved(f::checked(deserialize_part_document_from_json(current))) == current);
  project_roundtrip(part);
  REQUIRE(part.update_sweep_feature(old));
  CHECK(saved(part) == legacy);
}

TEST_CASE("Circular and producer-witness path references preserve full semantic identity",
          "[core][parity-b167][semantic-sweep-intent]") {
  auto part = f::part();
  auto sketch = f::checked(
      Sketch::create(SketchId("circle.source"), "Circle source", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      f::checked(CircleProfile::create(ProfileId("circle.profile"), ParameterId("radius")))));
  REQUIRE(part.add_sketch(sketch));
  auto cut = f::checked(Feature::create_subtractive_extrude(
      FeatureId("circle.cut"), "Circular cut", sketch.id(), FeatureId("base.feature")));
  const auto context = f::checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("base.body"), {}));
  REQUIRE(part.add_feature(f::checked(cut.with_body_result_context(context))));
  const auto circle = f::checked(EdgeReference::create_circular(
      PartFeatureInputRole::PathSegment,
      f::checked(SemanticCircularEdgeReference::create(
          FeatureId("circle.cut"), ProfileId("circle.profile"), SemanticCircularEdge::SourceRim))));
  const auto segment = f::checked(PathSegmentReference::create_feature_edge(circle));
  const auto added = part.add_path_curve(f::checked(PathCurve::create(
      PathCurveId("rim"), "Rim", {segment}, PathClosure::Closed, PathOrientationRule::MinimumTwist,
      {}, {}, 1e-7, f::checked(PathExecutionPolicy::curve_preserving(1e-6)))));
  INFO((added ? "Added" : added.error().message()));
  REQUIRE(added);
  std::array<Point3, ResultEdgeReference::sample_count> samples;
  for (std::size_t i = 0; i < samples.size(); ++i)
    samples[i] = {-4 + 8 * double(i) / (samples.size() - 1), -3, 5};
  const auto witness =
      f::checked(ResultEdgeReference::create(FeatureId("base.feature"), 0, 8, samples));
  const auto edge =
      f::checked(EdgeReference::create_result(PartFeatureInputRole::PathSegment, witness));
  const auto reference = f::checked(PathSegmentReference::create_feature_edge(edge, {}, true));
  REQUIRE(part.add_path_curve(path("witness", {reference})));
  const auto source = saved(part);
  const auto restored = f::checked(deserialize_part_document_from_json(source));
  CHECK(saved(restored) == source);
  CHECK(restored.find_path_curve(PathCurveId("rim"))->segments()[0].stable_key() ==
        segment.stable_key());
  CHECK(restored.find_path_curve(PathCurveId("witness"))->segments()[0].stable_key() ==
        reference.stable_key());
  const auto& actual = std::get<ResultEdgeReference>(
      restored.find_path_curve(PathCurveId("witness"))->segments()[0].feature_edge()->source());
  CHECK(actual.samples() == samples);
}

TEST_CASE("Imported feature-edge paths retain an explicit catalog Body owner",
          "[core][parity-b167][semantic-sweep-intent]") {
  auto part = imported();
  const auto reference = f::checked(ImportedTopologyReference::create(
      ImportedTopologyCatalogId("catalog"), ImportedTopologyId("edge"),
      ImportedTopologyCapability::Edge));
  const auto edge =
      f::checked(EdgeReference::create_imported(PartFeatureInputRole::PathSegment, reference));
  CHECK_FALSE(PathSegmentReference::create_feature_edge(edge));
  const auto segment =
      f::checked(PathSegmentReference::create_feature_edge(edge, BodyId("supplier.body"), true));
  REQUIRE(part.add_path_curve(path("imported.path", {segment})));
  CHECK(part.dependency_graph().has_dependency("body:supplier.body", "imported.path"));
  const auto current = saved(part);
  CHECK(saved(f::checked(deserialize_part_document_from_json(current))) == current);
  REQUIRE(
      part.add_body(f::checked(Body::create(BodyId("unrelated"), "Unrelated", BodyKind::Solid))));
  const auto before = saved(part);
  const auto wrong =
      f::checked(PathSegmentReference::create_feature_edge(edge, BodyId("unrelated")));
  CHECK_FALSE(part.add_path_curve(path("wrong.owner", {wrong})));
  CHECK(saved(part) == before);
  CHECK_FALSE(part.remove_body(BodyId("supplier.body")));
  CHECK(saved(part) == before);
  project_roundtrip(part);
}

TEST_CASE("Part V11 rejects downgraded noncanonical malformed and unnecessary Sweep envelopes",
          "[core][parity-b167][semantic-sweep-intent]") {
  auto part = f::part();
  REQUIRE(part.add_path_curve(
      path("typed", {f::checked(PathSegmentReference::create_feature_edge(generated()))})));
  const auto valid = json::parse(saved(part));
  for (const auto& mutate : std::vector<std::function<void(json&)>>{
           [](auto& v) { v["version"] = 10; }, [](auto& v) { v["version"] = 9; },
           [](auto& v) { v["path_curves"][0]["segments"][0]["extra"] = 1; },
           [](auto& v) { v["path_curves"][0]["segments"][0].erase("source_body"); },
           [](auto& v) { v["path_curves"][0]["segments"][0]["source_body"] = "base.body"; },
           [](auto& v) { v["path_curves"][0]["segments"][0]["edge_reference"]["curve_type"] = -1; },
           [](auto& v) { v["path_curves"][0]["execution_policy"]["kind"] = "legacy_fixed"; },
           [](auto& v) { v["path_curves"][0].erase("execution_policy"); }}) {
    auto invalid = valid;
    mutate(invalid);
    CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  }
  auto unnecessary = json::parse(saved(f::part()));
  unnecessary["version"] = 11;
  CHECK_FALSE(deserialize_part_document_from_json(unnecessary.dump()));
}
TEST_CASE("V11 replays declared mixed native and imported order without creating authored history",
          "[core][parity-b167][semantic-sweep-intent]") {
  auto part = f::part();
  const auto supplier = imported();
  for (const auto& value : supplier.step_import_sources())
    REQUIRE(part.add_step_import_source(value));
  for (const auto& value : supplier.imported_topology_catalogs())
    REQUIRE(part.add_imported_topology_catalog(value));
  for (const auto& value : supplier.bodies())
    REQUIRE(part.add_body(value));
  for (const auto& value : supplier.imported_body_features())
    REQUIRE(part.add_imported_body_feature(value));
  REQUIRE(part.add_path_curve(
      path("mixed.order", {f::checked(PathSegmentReference::create_feature_edge(generated()))})));
  CHECK_FALSE(part.authored_history_intent());
  const auto bytes = saved(part);
  const auto encoded = json::parse(bytes);
  REQUIRE(encoded.at("history_version") == 1);
  CHECK_FALSE(encoded.contains("suppressed_features"));
  const auto restored = f::checked(deserialize_part_document_from_json(bytes));
  CHECK_FALSE(restored.authored_history_intent());
  CHECK(json::parse(saved(restored)).at("feature_order") == encoded.at("feature_order"));
  CHECK(saved(restored) == bytes);
  project_roundtrip(part);
}
