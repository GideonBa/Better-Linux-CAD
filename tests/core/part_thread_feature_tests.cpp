#include "../support/thread_feature_fixture.hpp"
#include "blcad/core/project_json.hpp"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
namespace f = blcad::test::thread_fixture;
namespace h = blcad::test::history_fixture;
using f::checked;
using json = nlohmann::json;
namespace {
std::string text(const PartDocument& p) {
  return checked(serialize_part_document_to_json(p));
}
auto graph(const PartDocument& p) {
  auto nodes = p.dependency_graph().nodes();
  auto edges = p.dependency_graph().dependencies();
  std::sort(nodes.begin(), nodes.end());
  std::sort(edges.begin(), edges.end());
  return std::pair{nodes, edges};
}
std::vector<std::string> order(const PartDocument& p) {
  std::vector<std::string> result;
  for (const auto& row : checked(p.executable_history()))
    result.push_back(row.node_id);
  return result;
}
EngineeringStandardTable dual_table(std::string revision, bool second = true) {
  auto table = f::table(std::move(revision), 2.1);
  auto rows = table.rows();
  if (second) {
    auto other = rows.front();
    other.key = "later";
    rows.push_back(std::move(other));
  }
  return checked(EngineeringStandardTable::create(table.key(), table.name(), table.provenance(),
                                                  table.columns(), std::move(rows)));
}
Project project() {
  return checked(Project::create(DocumentId("thread.project"), "Thread project",
                                 checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
}
} // namespace

TEST_CASE("Internal and external Threads share Body modifier history and guarded identities",
          "[core][parity-b155][thread-history]") {
  auto p = f::pair_part();
  const auto generation = p.executable_generation();
  REQUIRE(p.add_hole_feature(f::internal()));
  REQUIRE(p.add_thread_feature(f::external()));
  REQUIRE(p.thread_feature_count() == 1);
  REQUIRE(p.find_thread_feature(FeatureId("external")));
  CHECK(p.executable_generation() > generation);
  CHECK(p.dependency_graph().has_dependency("base", "internal"));
  CHECK(p.dependency_graph().has_dependency("shaft", "external"));
  CHECK(p.dependency_graph().has_dependency("thread.pitch", "external"));
  CHECK(p.dependency_graph().has_dependency("thread.pitch", "internal"));
  CHECK(p.dependency_graph().has_dependency("external", "body:body.shaft"));
  CHECK_FALSE(p.dependency_graph().has_dependency("shaft", "body:body.shaft"));
  auto rows = checked(p.executable_history());
  CHECK(std::count_if(rows.begin(), rows.end(),
                      [](const auto& r) { return r.family == PartFeatureFamily::Thread; }) == 1);
  const auto before = text(p);
  const auto before_graph = graph(p);
  const auto before_generation = p.executable_generation();
  CHECK_FALSE(p.add_thread_feature(f::external()));
  CHECK_FALSE(p.add_thread_feature(f::external(f::definition(), "internal")));
  CHECK_FALSE(p.add_hole_feature(f::internal(f::definition(), "external")));
  const auto normal = f::external();
  const auto wrong_body =
      checked(ThreadFeature::create(FeatureId("wrong.body"), "Wrong Body", BodyId("body.base"),
                                    normal.support(), normal.thread()));
  CHECK_FALSE(p.add_thread_feature(wrong_body));
  CHECK_FALSE(p.remove_body(BodyId("body.shaft")));
  auto reversed = order(p);
  std::reverse(reversed.begin(), reversed.end());
  CHECK_FALSE(p.apply_history_intent({reversed, {}}));
  CHECK(text(p) == before);
  CHECK(graph(p) == before_graph);
  CHECK(p.executable_generation() == before_generation);
  const auto affected = checked(
      p.set_parameter_value(ParameterId("thread.pitch"), checked(Quantity::length_mm(1.8))));
  CHECK(std::find(affected.begin(), affected.end(), "internal") != affected.end());
  CHECK(std::find(affected.begin(), affected.end(), "external") != affected.end());
}

TEST_CASE("Thread suppression prefix update and removal retain shared authority and legacy bytes",
          "[core][parity-b155][thread-history]") {
  auto p = f::pair_part();
  const auto original = text(p);
  REQUIRE(p.add_hole_feature(f::internal(f::definition(f::selected()))));
  REQUIRE(p.add_thread_feature(f::external(f::definition(f::selected()))));
  const auto authored = order(p);
  REQUIRE(p.apply_history_intent({authored, {"external", "internal"}}));
  const auto plan = checked(p.create_history_evaluation_plan());
  REQUIRE(plan.complete());
  for (const auto& entry : plan.entries)
    if (entry.feature.node_id == "external" || entry.feature.node_id == "internal")
      CHECK(entry.state == PartHistoryExecutionState::Suppressed);
  CHECK_FALSE(p.remove_engineering_resource(f::selected().table));
  const auto prefix = checked(p.create_history_evaluation_plan(0));
  CHECK_FALSE(prefix.complete());
  CHECK(prefix.active_ids().empty());
  REQUIRE(p.update_thread_feature(f::external(
      f::definition(f::custom(), true, ThreadHandedness::Left, ThreadRepresentation::Modeled))));
  CHECK(order(p) == authored);
  CHECK(p.find_thread_feature(FeatureId("external"))->thread().handedness() ==
        ThreadHandedness::Left);
  REQUIRE(p.remove_thread_feature(FeatureId("external")));
  CHECK(p.dependency_graph().has_dependency("shaft", "body:body.shaft"));
  REQUIRE(p.remove_hole_feature(FeatureId("internal")));
  CHECK_FALSE(p.dependency_graph().has_node(engineering_resource_node_id(f::selected().table)));
  // Authored history remains intentionally authored. A separate untouched-history Part
  // proves the old byte representation after adding/removing both new consumers.
  auto legacy = f::pair_part();
  REQUIRE(legacy.add_thread_feature(f::external()));
  REQUIRE(legacy.add_hole_feature(f::internal()));
  REQUIRE(legacy.remove_thread_feature(FeatureId("external")));
  REQUIRE(legacy.remove_hole_feature(FeatureId("internal")));
  CHECK(text(legacy) == original);
}

TEST_CASE(
    "Canonical Thread V6 persists both consumers and exact offline data without numeric caches",
    "[core][parity-b155][thread-json]") {
  for (const auto mode : {ThreadRepresentation::Cosmetic, ThreadRepresentation::Modeled}) {
    auto p = f::pair_part();
    REQUIRE(p.add_thread_feature(
        f::external(f::definition(f::selected("r1", true), true, ThreadHandedness::Left, mode))));
    REQUIRE(p.add_hole_feature(
        f::internal(f::definition(f::custom(), true, ThreadHandedness::Right, mode))));
    REQUIRE(p.apply_history_intent({order(p), {"external"}}));
    const auto encoded = text(p);
    const auto j = json::parse(encoded);
    CHECK(j.at("version") == 6);
    CHECK(j.at("history_version") == 2);
    CHECK(j.at("thread_features")[0].at("thread").at("profile").at("table").at("revision") == "r1");
    CHECK(j.at("hole_features")[0].at("thread").at("profile").at("minor_diameter") ==
          "thread.minor");
    CHECK(encoded.find("TopoDS") == std::string::npos);
    CHECK(encoded.find("major_diameter_mm") == std::string::npos);
    auto copy = checked(deserialize_part_document_from_json(encoded));
    CHECK(text(copy) == encoded);
    CHECK(graph(copy) == graph(p));
    CHECK(order(copy) == order(p));
    const auto resolved =
        checked(resolve_thread_definition(copy, copy.thread_features().front().thread()));
    CHECK(resolved.representation == mode);
    CHECK(resolved.handedness == ThreadHandedness::Left);
    CHECK(resolved.taper_angle_deg == -2);
    CHECK(resolved.provenance.back().provenance.source() == "Synthetic r1/taper");
    CHECK(copy.hole_features().front().thread()->start() == ParameterId("thread.start"));
  }
}

TEST_CASE("Thread V6 rejects malformed profiles modes selectors envelopes and stale sources",
          "[core][parity-b155][thread-json]") {
  auto p = f::pair_part();
  REQUIRE(p.add_thread_feature(f::external()));
  REQUIRE(p.add_hole_feature(f::internal()));
  const auto original = json::parse(text(p));
  const auto reject = [&](auto change) {
    auto j = original;
    change(j);
    CHECK_FALSE(deserialize_part_document_from_json(j.dump()));
  };
  reject([](auto& j) { j["version"] = 5; });
  reject([](auto& j) { j["version"] = 6.0; });
  reject([](auto& j) { j["thread_features"] = json::array(); });
  reject([](auto& j) {
    j.erase("thread_features");
    j["hole_features"][0].erase("thread");
  });
  reject([](auto& j) { j["thread_features"][0]["direction"] = "guess"; });
  reject([](auto& j) { j["thread_features"][0]["extra"] = true; });
  reject([](auto& j) { j["thread_features"][0]["thread"]["handedness"] = "both"; });
  reject([](auto& j) { j["thread_features"][0]["thread"]["representation"] = "auto"; });
  reject([](auto& j) { j["thread_features"][0]["thread"]["start"] = nullptr; });
  reject([](auto& j) { j["thread_features"][0]["thread"]["length"] = "missing"; });
  reject([](auto& j) { j["thread_features"][0]["thread"]["extra"] = 1; });
  reject([](auto& j) { j["thread_features"][0]["thread"]["profile"]["kind"] = "guess"; });
  reject([](auto& j) { j["thread_features"][0]["thread"]["profile"].erase("pitch"); });
  reject([](auto& j) { j["thread_features"][0]["thread"]["profile"]["minor_diameter"] = "count"; });
  reject([](auto& j) { j["hole_features"][0]["thread"] = nullptr; });
  reject([](auto& j) { j["hole_features"][0]["thread"]["profile"]["extra"] = 1; });
  auto sourced = f::pair_part();
  REQUIRE(sourced.add_thread_feature(f::external(f::definition(f::selected()))));
  auto j = json::parse(text(sourced));
  j["thread_features"][0]["thread"]["profile"]["table"]["revision"] = "unavailable";
  CHECK_FALSE(deserialize_part_document_from_json(j.dump()));
}

TEST_CASE("Internal Thread intervals cannot consume the blind tip or conflict with pilot intent",
          "[core][parity-b155][thread-inputs]") {
  auto p = f::pair_part();
  REQUIRE(p.add_hole_feature(f::internal(f::definition(f::custom(), true))));
  const auto before = text(p);
  const auto generation = p.executable_generation();
  const auto too_deep = checked(
      ThreadDefinition::create(f::custom(), ParameterId("depth"), ParameterId("thread.start")));
  CHECK_FALSE(p.update_hole_feature(f::internal(too_deep)));
  auto mismatched = f::custom();
  mismatched.minor_diameter = ParameterId("radius");
  CHECK_FALSE(p.update_hole_feature(f::internal(f::definition(mismatched))));
  CHECK(text(p) == before);
  CHECK(p.executable_generation() == generation);
  auto prototype = f::internal();
  const auto create = [&](HoleKind kind, HoleManufacturing manufacturing = {},
                          HoleExtent extent = HoleThroughAllExtent{}) {
    return HoleFeature::create(prototype.id(), prototype.name(), kind, prototype.target_body(),
                               prototype.support(), prototype.placements(), prototype.diameter(),
                               std::move(extent), prototype.direction(), prototype.tip(),
                               std::move(manufacturing), prototype.thread());
  };
  CHECK_FALSE(create(HoleKind::Clearance));
  CHECK_FALSE(create(HoleKind::Plain,
                     HoleFitted{"Fit", ParameterId("thread.minor"), ParameterId("thread.major")}));
  CHECK_FALSE(create(HoleKind::Plain,
                     HoleCenter{ParameterId("thread.major"), ParameterId("thread.angle")}));
  CHECK_FALSE(create(HoleKind::Plain, {}, HoleSymmetricExtent{ParameterId("depth")}));
  const auto seated = checked(create(
      HoleKind::Plain, HoleCounterbore{ParameterId("thread.major"), ParameterId("thread.start")}));
  auto candidate = f::pair_part();
  REQUIRE(candidate.add_hole_feature(seated));
  CHECK(checked(deserialize_part_document_from_json(text(candidate)))
            .hole_features()
            .front()
            .thread()
            .has_value());
}

TEST_CASE(
    "Thread revision adoption rolls back a late external consumer including catalog insertion",
    "[core][parity-b155][thread-resources]") {
  auto p = f::pair_part();
  REQUIRE(p.add_engineering_resource(dual_table("shared")));
  auto normal = f::selected("shared"), later = normal;
  later.row = "later";
  REQUIRE(p.add_hole_feature(f::internal(f::definition(normal))));
  REQUIRE(p.add_thread_feature(f::external(f::definition(later))));
  REQUIRE(p.apply_history_intent({order(p), {"external"}}));
  const auto before = text(p);
  const auto before_graph = graph(p);
  const auto generation = p.executable_generation();
  CHECK_FALSE(p.remove_engineering_resource(normal.table));
  CHECK_FALSE(p.adopt_engineering_resource_revision(normal.table, dual_table("broken", false)));
  CHECK(text(p) == before);
  CHECK(graph(p) == before_graph);
  CHECK(p.executable_generation() == generation);
  CHECK_FALSE(p.engineering_resources().find(f::selected("broken").table));
  REQUIRE(p.adopt_engineering_resource_revision(normal.table, dual_table("next")));
  CHECK(p.hole_features().front().thread()->standard_dependencies() ==
        std::vector{f::selected("next").table});
  CHECK(p.thread_features().front().standard_dependencies() ==
        std::vector{f::selected("next").table});
  CHECK_FALSE(p.dependency_graph().has_node(engineering_resource_node_id(normal.table)));
  REQUIRE(p.remove_engineering_resource(normal.table));
  CHECK(text(checked(deserialize_part_document_from_json(text(p)))) == text(p));
}

TEST_CASE("Thread Parts retain offline Project ownership and selected-member adoption",
          "[core][parity-b155][thread-project]") {
  auto first = f::pair_part("first"), second = f::pair_part("second");
  REQUIRE(first.add_thread_feature(f::external(f::definition(f::selected()))));
  REQUIRE(first.add_hole_feature(f::internal(f::definition(f::selected()))));
  REQUIRE(second.add_thread_feature(f::external(f::definition(f::selected()))));
  auto value = project();
  REQUIRE(value.add_part_document(first));
  REQUIRE(value.add_part_document(second));
  REQUIRE(value.validate_engineering_resources());
  CHECK(value.engineering_resources().entries().size() == 2);
  const auto untouched = text(*value.find_part_document(DocumentId("second")));
  REQUIRE(value.adopt_engineering_resource_revision(f::selected().table, f::table("next", 2.4),
                                                    {DocumentId("first")}));
  CHECK(text(*value.find_part_document(DocumentId("second"))) == untouched);
  const auto encoded = checked(serialize_project_to_json(value));
  const auto j = json::parse(encoded);
  CHECK(j.at("version") == 2);
  CHECK(j.at("parts")[0].at("version") == 6);
  CHECK(j.at("parts")[0].contains("engineering_resource_refs"));
  CHECK_FALSE(j.at("parts")[0].contains("engineering_resources"));
  auto copy = checked(deserialize_project_from_json(encoded));
  CHECK(checked(serialize_project_to_json(copy)) == encoded);
  const auto* member = copy.find_part_document(DocumentId("first"));
  REQUIRE(member);
  const auto standalone = checked(deserialize_part_document_from_json(text(*member)));
  CHECK(
      checked(resolve_thread_definition(standalone, standalone.thread_features().front().thread()))
          .pitch_mm == Catch::Approx(2.4));
}

TEST_CASE(
    "Custom Thread Parts roundtrip in resource-free Projects and empty member reference lists",
    "[core][parity-b155][thread-project]") {
  for (bool project_resource : {false, true}) {
    auto p = f::pair_part("custom", false);
    REQUIRE(p.add_thread_feature(f::external()));
    auto value = project();
    REQUIRE(value.add_part_document(p));
    if (project_resource)
      REQUIRE(value.add_engineering_resource(blcad::test::engineering_fixture::material()));
    const auto encoded = checked(serialize_project_to_json(value));
    const auto copy = checked(deserialize_project_from_json(encoded));
    CHECK(checked(serialize_project_to_json(copy)) == encoded);
    CHECK(copy.find_part_document(DocumentId("custom"))->thread_feature_count() == 1);
  }
}
