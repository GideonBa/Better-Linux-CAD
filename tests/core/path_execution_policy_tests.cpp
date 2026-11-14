#include "../support/engineering_resource_fixture.hpp"
#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <limits>
#include <nlohmann/json.hpp>
using namespace blcad;
using json = nlohmann::json;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace ef = blcad::test::engineering_fixture;
namespace {
PartDocument simple() {
  auto p = f::part();
  (void)f::checked(p.add_path_curve(
      f::checked(PathCurve::create(PathCurveId("path"), "Reversed line",
                                   {f::checked(PathSegmentReference::create_planar(
                                       SketchId("reference.planar"), SketchEntityId("source.line"),
                                       PlanarPathCurveKind::Line, true))},
                                   PathClosure::Open, PathOrientationRule::FixedUpVector,
                                   Vector3{0, 0, 1}, PathContinuityHint::G1, 2e-6))));
  return p;
}
std::string snapshot(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
PartDocument reference_part() {
  const auto source =
      f::checked(StepImportSource::create(StepImportSourceId("source"), "Source", "supplier.step",
                                          std::string(64, 'a'), StepImportMode::Reference));
  ImportedTopologyDescriptor body;
  body.kind = ImportedTopologyKind::Body;
  body.geometry = ImportedGeometryClass::Opaque;
  body.body_kind = BodyKind::Solid;
  body.bounds_mm = {0, 0, 0, 1, 1, 1};
  body.centroid = {.5, .5, .5};
  body.measure = 1;
  body.geometry_key = std::string(64, 'b');
  const auto catalog = f::checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId("catalog"), source.id(), source.sha256_digest(),
      std::string(64, 'c'), {{ImportedTopologyId("solid"), {}, body}}));
  const auto definition = f::checked(ImportedPartDefinition::create(
      source.id(), catalog.id(), {{ImportedTopologyId("solid"), BodyId("body"), BodyKind::Solid}}));
  return f::checked(PartDocument::create_reference(DocumentId("reference"), "Reference", source,
                                                   catalog, definition));
}
} // namespace
TEST_CASE("Path execution policy upgrades only explicit intent and retains canonical legacy bytes",
          "[core][parity-b166][path-execution-policy]") {
  auto p = simple();
  const auto original = snapshot(p);
  const auto legacy = json::parse(original);
  CHECK(legacy.at("version") == 1);
  CHECK_FALSE(legacy.at("path_curves").at(0).contains("execution_policy"));
  REQUIRE(p.find_path_curve(PathCurveId("path")));
  const auto old = *p.find_path_curve(PathCurveId("path"));
  CHECK(old.execution_policy().kind() == PathGeometryPolicyKind::LegacyFixed);
  CHECK_FALSE(old.execution_policy().tolerance_mm());
  CHECK(snapshot(f::checked(deserialize_part_document_from_json(original))) == original);
  REQUIRE(p.set_path_execution_policy(old.id(),
                                      f::checked(PathExecutionPolicy::curve_preserving(.00025))));
  const auto saved = snapshot(p);
  const auto encoded = json::parse(saved);
  CHECK(encoded.at("version") == 10);
  CHECK(encoded.at("path_curves").at(0).at("execution_policy") ==
        json{{"kind", "curve_preserving"}, {"tolerance_mm", .00025}});
  const auto loaded = f::checked(deserialize_part_document_from_json(saved));
  CHECK(snapshot(loaded) == saved);
  const auto& path = *loaded.find_path_curve(old.id());
  CHECK(path.segments().front().stable_key() == old.segments().front().stable_key());
  CHECK(path.segments().front().reversed());
  CHECK(path.fixed_up_vector() == old.fixed_up_vector());
  CHECK(path.continuity_hint() == old.continuity_hint());
  CHECK(path.connection_tolerance_mm() == 2e-6);
  CHECK(path.execution_policy().tolerance_mm() == .00025);
  auto assembly =
      f::checked(AssemblyDocument::create(DocumentId("policy.assembly"), "Policy assembly"));
  (void)f::checked(assembly.add_member_part(loaded.id()));
  auto project =
      f::checked(Project::create(DocumentId("policy.project"), "Policy project", assembly));
  (void)f::checked(project.add_part_document(loaded));
  const auto project_json = f::checked(serialize_project_to_json(project));
  CHECK(f::checked(serialize_project_to_json(
            f::checked(deserialize_project_from_json(project_json)))) == project_json);
  REQUIRE(p.set_path_execution_policy(old.id(), PathExecutionPolicy::legacy()));
  CHECK(snapshot(p) == original);
}
TEST_CASE("Path policy invalidates dependent features without changing source or graph ownership",
          "[core][parity-b166][path-execution-policy]") {
  auto p = sf::straight_sweep();
  const auto old = snapshot(p);
  const auto edges = p.dependency_graph().dependencies();
  const auto generation = p.executable_generation();
  p.mark_all_clean();
  auto changed = p.set_path_execution_policy(
      PathCurveId("driven.path"), f::checked(PathExecutionPolicy::curve_preserving(1e-4)));
  REQUIRE(changed);
  CHECK(std::find(changed.value().begin(), changed.value().end(), "driven.sweep") !=
        changed.value().end());
  CHECK(p.invalidation_state().find("driven.path")->status == InvalidationStatus::Changed);
  CHECK(p.invalidation_state().find("driven.sweep")->status == InvalidationStatus::Dirty);
  CHECK(p.invalidation_state().find("driven")->status == InvalidationStatus::Clean);
  CHECK(p.dependency_graph().dependencies() == edges);
  CHECK(p.executable_generation() == generation);
  CHECK(json::parse(snapshot(p)).at("sketches_3d") == json::parse(old).at("sketches_3d"));
  CHECK(snapshot(f::checked(deserialize_part_document_from_json(snapshot(p)))) == snapshot(p));
  p.mark_all_clean();
  REQUIRE(p.set_path_execution_policy(PathCurveId("driven.path"),
                                      f::checked(PathExecutionPolicy::curve_preserving(1e-4))));
  CHECK(p.invalidation_state().nodes_with_status(InvalidationStatus::Dirty).empty());
  CHECK(p.invalidation_state().nodes_with_status(InvalidationStatus::Changed).empty());
  const auto before = snapshot(p);
  CHECK_FALSE(p.set_path_execution_policy(PathCurveId("missing"), PathExecutionPolicy::legacy()));
  CHECK_FALSE(p.set_path_execution_policy(PathCurveId{}, PathExecutionPolicy::legacy()));
  CHECK(snapshot(p) == before);
  REQUIRE(p.set_path_execution_policy(PathCurveId("driven.path"), PathExecutionPolicy::legacy()));
  CHECK(snapshot(p) == old);
  auto immutable = reference_part();
  const auto ref = snapshot(immutable);
  CHECK_FALSE(
      immutable.set_path_execution_policy(PathCurveId("missing"), PathExecutionPolicy::legacy()));
  CHECK(snapshot(immutable) == ref);
}
TEST_CASE("Path policy rejects malformed tolerance modes and unnecessary version envelopes",
          "[core][parity-b166][path-execution-policy]") {
  for (double value : {0., -1., 1e-10, 1.01, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()})
    CHECK_FALSE(PathExecutionPolicy::curve_preserving(value));
  CHECK(PathExecutionPolicy::curve_preserving(1e-9));
  CHECK(PathExecutionPolicy::curve_preserving(1));
  auto p = simple();
  const auto original = snapshot(p);
  REQUIRE(p.set_path_execution_policy(PathCurveId("path"),
                                      f::checked(PathExecutionPolicy::curve_preserving(1e-4))));
  const auto good = json::parse(snapshot(p));
  const std::vector<std::function<void(json&)>> edits{
      [](json& j) { j["version"] = 9; },
      [](json& j) { j["version"] = 11; },
      [](json& j) { j["path_curves"][0].erase("execution_policy"); },
      [](json& j) { j["path_curves"][0]["execution_policy"] = nullptr; },
      [](json& j) { j["path_curves"][0]["execution_policy"]["kind"] = "legacy_fixed"; },
      [](json& j) { j["path_curves"][0]["execution_policy"]["kind"] = false; },
      [](json& j) { j["path_curves"][0]["execution_policy"].erase("tolerance_mm"); },
      [](json& j) { j["path_curves"][0]["execution_policy"].erase("kind"); },
      [](json& j) { j["path_curves"][0]["execution_policy"]["samples"] = 32; },
      [](json& j) { j["path_curves"][0]["execution_policy"]["tolerance_mm"] = "0.001"; },
      [](json& j) { j["path_curves"][0]["execution_policy"]["tolerance_mm"] = 0; },
      [](json& j) { j["path_curves"][0]["execution_policy"]["tolerance_mm"] = -1; },
      [](json& j) { j["path_curves"][0]["execution_policy"]["tolerance_mm"] = 1e-10; },
      [](json& j) { j["path_curves"][0]["execution_policy"]["tolerance_mm"] = 2; },
  };
  for (const auto& edit : edits) {
    auto bad = good;
    edit(bad);
    CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  }
  auto unnecessary = json::parse(original);
  unnecessary["version"] = 10;
  CHECK_FALSE(deserialize_part_document_from_json(unnecessary.dump()));
  CHECK(snapshot(f::checked(deserialize_part_document_from_json(original))) == original);
}

TEST_CASE("Resource-aware Projects retain V10 path intent and exact member revision subsets",
          "[core][parity-b166][path-execution-policy][project-engineering-resources]") {
  for (bool owns_resources : {true, false}) {
    CAPTURE(owns_resources);
    auto part = simple();
    if (owns_resources) {
      ef::annotate(part);
      REQUIRE(part.set_body_material(BodyId("base.body"), ef::key("synthetic.material", "r2")));
    }
    auto assembly =
        f::checked(AssemblyDocument::create(DocumentId("resources.assembly"), "Assembly"));
    (void)f::checked(assembly.add_member_part(part.id()));
    auto project =
        f::checked(Project::create(DocumentId("resources.project"), "Project", assembly));
    REQUIRE(project.add_part_document(part));
    if (!owns_resources)
      REQUIRE(project.add_engineering_resource(ef::material()));
    const auto legacy = f::checked(serialize_project_to_json(project));
    REQUIRE(project.find_part_document(part.id())->set_path_execution_policy(
        PathCurveId("path"), f::checked(PathExecutionPolicy::curve_preserving(1e-5))));
    const auto saved = f::checked(serialize_project_to_json(project));
    const auto data = json::parse(saved);
    CHECK(data.at("version") == 2);
    const auto& member = data.at("parts").at(0);
    CHECK(member.at("version") == 10);
    CHECK_FALSE(member.contains("engineering_resources"));
    REQUIRE(member.contains("engineering_resource_refs"));
    CHECK(member.at("engineering_resource_refs").size() == (owns_resources ? 3 : 0));
    CHECK_FALSE(deserialize_part_document_from_json(member.dump()));
    auto loaded = f::checked(deserialize_project_from_json(saved));
    CHECK(f::checked(serialize_project_to_json(loaded)) == saved);
    CHECK(loaded.engineering_resources() == project.engineering_resources());
    const auto* restored = loaded.find_part_document(part.id());
    REQUIRE(restored);
    CHECK(restored->engineering_resources() == part.engineering_resources());
    CHECK(restored->default_material() == part.default_material());
    CHECK(restored->body_materials() == part.body_materials());
    const auto portable = f::checked(serialize_part_document_to_json(*restored));
    CHECK(snapshot(f::checked(deserialize_part_document_from_json(portable))) == portable);
    auto malformed = data;
    malformed["parts"][0]["engineering_resource_refs"] =
        json::array({json{{"id", "missing"}, {"revision", "r1"}}});
    CHECK_FALSE(deserialize_project_from_json(malformed.dump()));
    malformed = data;
    malformed["parts"][0]["engineering_resources"] = data.at("engineering_resources");
    CHECK_FALSE(deserialize_project_from_json(malformed.dump()));
    REQUIRE(loaded.find_part_document(part.id())->set_path_execution_policy(
        PathCurveId("path"), PathExecutionPolicy::legacy()));
    CHECK(f::checked(serialize_project_to_json(loaded)) == legacy);
  }
}
