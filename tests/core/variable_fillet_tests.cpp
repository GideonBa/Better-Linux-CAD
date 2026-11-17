#include "../support/part_history_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <nlohmann/json.hpp>
using namespace blcad;
namespace f = blcad::test::history_fixture;
namespace {
VariableFilletIntent intent() {
  return {{{"outer",
            "Outer edge",
            {{f::edge(PartFeatureInputRole::FilletEdge), false}},
            FilletPropagation::Explicit,
            FilletRadiusInterpolation::MonotoneCubic,
            {{"start", FilletStationLocation::Start, 0, ParameterId("radius")},
             {"middle", FilletStationLocation::Fraction, .3123456789012345, ParameterId("height")},
             {"end", FilletStationLocation::End, 1, ParameterId("width")}}}}};
}
FilletFeature feature(VariableFilletIntent law = intent()) {
  return f::checked(FilletFeature::create_variable(FeatureId("variable"), "Variable round",
                                                   BodyId("body.base"), std::move(law)));
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
} // namespace
TEST_CASE(
    "Variable Fillet persists semantic groups station identities reversal and radius dependencies",
    "[core][variable-fillet][parity-b175]") {
  auto part = f::native_base();
  auto law = intent();
  auto record = feature(law);
  REQUIRE(record.advanced_intent());
  law.groups.front().stations.front().radius = ParameterId("angle");
  CHECK(record.parameter_dependencies() == std::vector<ParameterId>{ParameterId("radius"),
                                                                    ParameterId("height"),
                                                                    ParameterId("width")});
  REQUIRE(part.add_fillet_feature(record));
  for (const auto* id : {"radius", "height", "width", "base"})
    CHECK(part.dependency_graph().has_dependency(id, "variable"));
  const auto json = saved(part);
  const auto root = nlohmann::json::parse(json);
  CHECK(root.at("version") == 14);
  const auto& encoded = root.at("edge_treatments").at(0);
  CHECK(encoded.at("mode") == "variable");
  CHECK_FALSE(encoded.contains("radius_parameter"));
  CHECK_FALSE(encoded.contains("edges"));
  const auto opened = f::checked(deserialize_part_document_from_json(json));
  CHECK(saved(opened) == json);
  auto project = f::checked(
      Project::create(DocumentId("variable.project"), "Variable rounds",
                      f::checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"))));
  REQUIRE(project.add_part_document(opened));
  const auto project_json = f::checked(serialize_project_to_json(project));
  CHECK(f::checked(serialize_project_to_json(
            f::checked(deserialize_project_from_json(project_json)))) == project_json);

  const auto& group =
      std::get<VariableFilletIntent>(
          opened.find_fillet_feature(FeatureId("variable"))->advanced_intent()->definition)
          .groups.front();
  CHECK(group.stations[1].fraction == .3123456789012345);
  const auto reversed = reverse_fillet_group(group);
  CHECK(reversed.id == group.id);
  CHECK(reversed.edges.front().reversed);
  CHECK(reversed.stations.front().id == "start");
  CHECK(reversed.station_basis_reversed);
  CHECK(reversed.stations.front().location == FilletStationLocation::Start);
  CHECK(reversed.stations.front().radius == ParameterId("radius"));
  CHECK(reversed.stations[1].fraction == .3123456789012345);
  CHECK(reversed.stations[1].location == FilletStationLocation::Fraction);
  CHECK(fillet_chain_fraction(reversed, reversed.stations[1]) ==
        Catch::Approx(1 - .3123456789012345).margin(1e-16));
  CHECK(reversed.stations.back().id == "end");
  CHECK(reversed.stations.back().location == FilletStationLocation::End);
  REQUIRE(validate_variable_fillet_intent({{reversed}}));
  REQUIRE(part.update_fillet_feature(feature({{reversed}})));
  CHECK(saved(part) != json);
  REQUIRE(part.update_fillet_feature(feature({{reverse_fillet_group(reversed)}})));
  CHECK(saved(part) == json);
  REQUIRE(part.update_fillet_feature(record));
  CHECK(saved(part) == json);
  const auto before = saved(part);
  for (const auto* bad : {"missing", "angle", "count"}) {
    auto invalid = intent();
    invalid.groups[0].stations[1].radius = ParameterId(bad);
    CHECK_FALSE(part.update_fillet_feature(feature(invalid)));
    CHECK(saved(part) == before);
  }
  REQUIRE(part.remove_fillet_feature(FeatureId("variable")));
  CHECK_FALSE(part.dependency_graph().has_node("variable"));
}
TEST_CASE(
    "Variable Fillet rejects ambiguous laws duplicate membership and malformed strict envelopes",
    "[core][variable-fillet][parity-b175]") {
  const auto invalid = [](const auto& mutate) {
    auto law = intent();
    mutate(law);
    CHECK_FALSE(FilletFeature::create_variable(FeatureId("variable"), "Variable",
                                               BodyId("body.base"), law));
  };
  invalid([](auto& x) { x.groups.clear(); });
  invalid([](auto& x) { x.groups[0].edges.clear(); });
  invalid([](auto& x) { x.groups[0].stations.resize(1); });
  invalid([](auto& x) { x.groups[0].stations[1].fraction = 0; });
  invalid(
      [](auto& x) { x.groups[0].stations[1].fraction = std::numeric_limits<double>::quiet_NaN(); });
  invalid([](auto& x) { x.groups[0].stations[1].fraction = 1.1; });
  invalid([](auto& x) { x.groups[0].stations[1].id = "start"; });
  invalid([](auto& x) { x.groups[0].stations[0].fraction = .1; });
  invalid([](auto& x) { x.groups[0].stations.back().fraction = .9; });
  invalid([](auto& x) { x.groups[0].stations[1].radius = ParameterId(); });
  invalid([](auto& x) { x.groups[0].edges.push_back(x.groups[0].edges.front()); });
  invalid([](auto& x) {
    auto second = x.groups.front();
    second.id = "other";
    x.groups.push_back(second);
  });
  invalid([](auto& x) { x.groups[0].edges[0].edge = f::edge(PartFeatureInputRole::ChamferEdge); });
  invalid([](auto& x) { x.groups[0].propagation = static_cast<FilletPropagation>(99); });
  auto part = f::native_base();
  REQUIRE(part.add_fillet_feature(feature()));
  const auto json = nlohmann::json::parse(saved(part));
  for (const auto& version : {1, 2, 3, 13, 15}) {
    auto malformed = json;
    malformed["version"] = version;
    CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  }
  for (const auto* field : {"mode", "groups"}) {
    auto malformed = json;
    malformed["edge_treatments"][0].erase(field);
    CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  }
  auto unknown = json;
  unknown["edge_treatments"][0]["groups"][0]["stations"][1]["fraction"] = true;
  CHECK_FALSE(deserialize_part_document_from_json(unknown.dump()));
  auto extra = json;
  extra["edge_treatments"][0]["groups"][0]["extra"] = 1;
  CHECK_FALSE(deserialize_part_document_from_json(extra.dump()));
  auto legacy = f::native_base();
  REQUIRE(legacy.add_fillet_feature(f::checked(
      FilletFeature::create(FeatureId("constant"), "Constant", BodyId("body.base"),
                            {f::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
  const auto old = saved(legacy);
  const auto old_record = nlohmann::json::parse(old).at("edge_treatments").at(0);
  CHECK(old_record.size() == 6);
  CHECK_FALSE(old_record.contains("mode"));
  CHECK_FALSE(old_record.contains("groups"));
  CHECK(saved(f::checked(deserialize_part_document_from_json(old))) == old);
  auto needless = nlohmann::json::parse(old);
  needless["version"] = 14;
  CHECK_FALSE(deserialize_part_document_from_json(needless.dump()));
}

TEST_CASE("Variable Fillet composes formula bindings ordered group reversal and normal history",
          "[core][variable-fillet][parity-b175]") {
  auto part = f::native_base();
  REQUIRE(part.add_expression_parameter(ParameterId("middle.radius"), "middle_radius",
                                        ParameterType::Length, "radius * 2"));
  auto law = intent();
  law.groups[0].stations[1].radius = ParameterId("middle.radius");
  const auto back = f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::FilletEdge,
      f::checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopBack))));
  law.groups[0].edges.push_back({back, true});
  REQUIRE(part.add_fillet_feature(feature(law)));
  CHECK(part.dependency_graph().has_dependency("radius", "middle.radius"));
  CHECK(part.dependency_graph().has_dependency("middle.radius", "variable"));
  const auto original = saved(part);
  const auto reversed = reverse_fillet_group(law.groups[0]);
  REQUIRE(reversed.edges.size() == 2);
  CHECK(std::get<SemanticEdgeReference>(reversed.edges.front().edge.source()).edge() ==
        SemanticEdge::TopBack);
  CHECK_FALSE(reversed.edges.front().reversed);
  CHECK(reversed.edges.back().reversed);
  REQUIRE(part.update_fillet_feature(feature({{reversed}})));
  CHECK(saved(f::checked(deserialize_part_document_from_json(saved(part)))) == saved(part));
  REQUIRE(part.update_fillet_feature(feature(law)));
  CHECK(saved(part) == original);
  const auto changed = f::checked(
      part.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(2))));
  CHECK(std::find(changed.begin(), changed.end(), "variable") != changed.end());
  CHECK(part.find_parameter(ParameterId("middle.radius"))->value().millimeters() == 4);
  const auto edited = saved(part);
  CHECK(edited != original);
  CHECK(saved(f::checked(deserialize_part_document_from_json(edited))) == edited);
  CHECK_FALSE(part.remove_feature(FeatureId("base")));
  CHECK(saved(part) == edited);
  const auto suppressed =
      f::checked(part.with_history_intent({{"base", "variable"}, {"variable"}}));
  CHECK(suppressed.find_fillet_feature(FeatureId("variable"))->advanced_intent());
  const auto history = f::checked(suppressed.executable_history());
  REQUIRE(history.size() == 2);
  CHECK(history.back().state == PartFeatureState::Suppressed);
  CHECK(saved(f::checked(deserialize_part_document_from_json(saved(suppressed)))) ==
        saved(suppressed));
}

TEST_CASE(
    "Variable Fillet rejects foreign producers and repairs members without changing station IDs",
    "[core][variable-fillet][parity-b175]") {
  auto part = f::native_base();
  f::rectangle(part, "other");
  const auto context = f::new_body(part, "other");
  REQUIRE(part.add_feature(f::checked(
      f::checked(Feature::create_additive_extrude(FeatureId("other.feature"), "Other",
                                                  SketchId("other"), ParameterId("depth")))
          .with_body_result_context(context))));
  const auto original = saved(part);
  CHECK_FALSE(part.add_fillet_feature(f::checked(FilletFeature::create_variable(
      FeatureId("variable"), "Foreign seed", BodyId("body.other"), intent()))));
  CHECK(saved(part) == original);
  REQUIRE(part.add_fillet_feature(feature()));
  const auto accepted = saved(part);
  auto missing = intent();
  missing.groups[0].edges[0].edge = f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::FilletEdge,
      f::checked(SemanticEdgeReference::create(FeatureId("missing"), SemanticEdge::TopFront))));
  CHECK_FALSE(part.update_fillet_feature(feature(missing)));
  CHECK(saved(part) == accepted);
  auto repaired = intent();
  repaired.groups[0].edges[0].edge = f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::FilletEdge,
      f::checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopBack))));
  REQUIRE(part.update_fillet_feature(feature(repaired)));
  const auto& recovered =
      std::get<VariableFilletIntent>(
          part.find_fillet_feature(FeatureId("variable"))->advanced_intent()->definition)
          .groups[0];
  CHECK(recovered.id == "outer");
  CHECK(recovered.stations[0].id == "start");
  CHECK(recovered.stations[1].id == "middle");
  CHECK(recovered.stations[2].id == "end");
  CHECK(recovered.stations[1].fraction == .3123456789012345);
  CHECK(recovered.stations[1].radius == ParameterId("height"));
  const auto encoded = saved(part);
  CHECK(encoded != accepted);
  CHECK(saved(f::checked(deserialize_part_document_from_json(encoded))) == encoded);
}

TEST_CASE("Reversing a variable Fillet twice retains tiny literal station fractions exactly",
          "[core][variable-fillet][parity-b175]") {
  auto law = intent();
  law.groups[0].stations[1].fraction = 1e-20;
  auto part = f::native_base();
  REQUIRE(part.add_fillet_feature(feature(law)));
  const auto before = saved(part);
  const auto reversed = reverse_fillet_group(law.groups[0]);
  REQUIRE(validate_variable_fillet_intent({{reversed}}));
  CHECK(reversed.stations[1].fraction == 1e-20);
  REQUIRE(part.update_fillet_feature(feature({{reverse_fillet_group(reversed)}})));
  CHECK(saved(part) == before);
}

TEST_CASE("Variable radius value and formula failures roll back the complete parameter intent",
          "[core][variable-fillet][parity-b175]") {
  auto part = f::native_base();
  REQUIRE(part.add_expression_parameter(ParameterId("middle.radius"), "middle_radius",
                                        ParameterType::Length, "radius * 2 - 1 mm"));
  auto law = intent();
  law.groups[0].stations[1].radius = ParameterId("middle.radius");
  REQUIRE(part.add_fillet_feature(feature(law)));
  const auto before = saved(part);
  const auto graph = part.dependency_graph().dependencies();
  CHECK_FALSE(part.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(.5))));
  CHECK(saved(part) == before);
  CHECK(part.dependency_graph().dependencies() == graph);
  CHECK(part.find_parameter(ParameterId("middle.radius"))->value().millimeters() == 1);
  CHECK_FALSE(part.set_parameter_formula(ParameterId("middle.radius"), "radius * 0"));
  CHECK(saved(part) == before);
  CHECK(part.dependency_graph().dependencies() == graph);
  CHECK_FALSE(part.set_parameter_formula(ParameterId("middle.radius"), "radius / 0"));
  CHECK(saved(part) == before);
  CHECK(part.dependency_graph().dependencies() == graph);
}
