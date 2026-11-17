#include "../support/engineering_resource_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/model_state.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
namespace history = blcad::test::history_fixture;
namespace resources = blcad::test::engineering_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? std::string{} : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
ModelState named(std::string id = "large", std::string name = "Large") {
  return {ModelStateId(std::move(id)), std::move(name), {}, {}, {}, {}, {}};
}
std::string text(const PartDocument& part) {
  return checked(serialize_part_document_to_json(part));
}
double mm(const PartDocument& part, std::string id) {
  const auto* parameter = part.find_parameter(ParameterId(std::move(id)));
  REQUIRE(parameter);
  return parameter->value().millimeters();
}
PartDocument parameter_part() {
  auto part = history::native_base();
  REQUIRE(part.add_expression_parameter(ParameterId("double"), "Double", ParameterType::Length,
                                        "width * 2"));
  return part;
}
} // namespace

TEST_CASE("Model states keep Primary and sibling parameter intent isolated",
          "[core][model-state][parity-b189][parity-b190]") {
  auto part = parameter_part();
  const auto original = text(part);
  CHECK(nlohmann::json::parse(original).count("model_states") == 0);
  auto large = named();
  large.parameters = {{ParameterId("width"), checked(Quantity::length_mm(40))},
                      {ParameterId("count"), checked(Quantity::count(7))},
                      {ParameterId("angle"), checked(Quantity::angle_deg(-30))},
                      {ParameterId("double"), std::string("width * 3")}};
  auto identical = large;
  identical.id = ModelStateId("same-size");
  identical.name = "Same size";
  REQUIRE(part.replace_model_states({large, identical}));
  const auto source = text(part);
  CHECK(mm(part, "width") == 20);
  CHECK(mm(part, "double") == 40);
  const auto evaluated = checked(part.evaluate_model_state(large.id));
  CHECK(mm(evaluated, "width") == 40);
  CHECK(mm(evaluated, "double") == 120);
  CHECK(evaluated.find_parameter(ParameterId("count"))->value().count_value() == 7);
  CHECK(evaluated.find_parameter(ParameterId("angle"))->value().degrees() == -30);
  CHECK(evaluated.effective_model_state() == large.id);
  CHECK(evaluated.model_states().empty());
  CHECK_FALSE(evaluated.evaluate_model_state(primary_model_state_id()));
  CHECK_FALSE(model_state_source_key(evaluated, primary_model_state_id()));
  CHECK(evaluated.find_parameter(ParameterId("double"))->formula() == "width * 3");
  CHECK(text(part) == source);
  CHECK(checked(model_state_source_key(part, large.id)) !=
        checked(model_state_source_key(part, identical.id)));
  CHECK_FALSE(model_state_source_key(part, ModelStateId("missing")));
  CHECK(checked(part.evaluate_model_state(primary_model_state_id())).effective_model_state() ==
        primary_model_state_id());
  auto root = nlohmann::json::parse(source);
  REQUIRE(root.at("version") == 17);
  REQUIRE(root.at("model_states").size() == 2);
  CHECK_FALSE(root.contains("active_model_state"));
  const auto reopened = checked(deserialize_part_document_from_json(source));
  CHECK(text(reopened) == source);
  CHECK(mm(checked(reopened.evaluate_model_state(large.id)), "double") == 120);
  REQUIRE(part.replace_model_states({}));
  CHECK(text(part) == original);
}

TEST_CASE("A state parameter table evaluates its complete final formula graph atomically",
          "[core][model-state][parity-b189]") {
  auto part = parameter_part();
  REQUIRE(part.add_expression_parameter(ParameterId("chain"), "Chain", ParameterType::Length,
                                        "Double + 1 mm"));
  auto rewired = named();
  // Sequentially replacing Double first would temporarily form a cycle.
  rewired.parameters = {{ParameterId("double"), std::string("Chain + 2 mm")},
                        {ParameterId("chain"), std::string("width * 4")}};
  REQUIRE(part.replace_model_states({rewired}));
  auto effective = checked(part.evaluate_model_state(rewired.id));
  CHECK(mm(effective, "chain") == 80);
  CHECK(mm(effective, "double") == 82);
  for (std::size_t i = 0; i < part.parameter_count(); ++i) {
    CHECK(effective.parameters()[i].id() == part.parameters()[i].id());
    CHECK(effective.parameters()[i].type() == part.parameters()[i].type());
    CHECK(effective.parameters()[i].scope() == part.parameters()[i].scope());
  }
  const auto before = text(part);
  auto cycle = rewired;
  cycle.parameters.back().value = std::string("Double + 1 mm");
  CHECK_FALSE(part.replace_model_states({cycle}));
  CHECK(text(part) == before);
  for (const auto& bad : std::vector<ModelStateParameterOverride>{
           {ParameterId("double"), checked(Quantity::length_mm(5))},
           {ParameterId("width"), std::string("1 mm")},
           {ParameterId("width"), checked(Quantity::count(3))},
           {ParameterId("missing"), checked(Quantity::length_mm(5))}}) {
    auto state = named();
    state.parameters = {bad};
    CHECK_FALSE(part.replace_model_states({state}));
    CHECK(text(part) == before);
  }
}

TEST_CASE("Model states validate reserved identities duplicate targets and explicit materials",
          "[core][model-state][parity-b189]") {
  auto part = parameter_part();
  resources::annotate(part);
  auto state = named();
  state.features = {{"base", true}};
  state.materials = {{std::nullopt, std::nullopt},
                     {BodyId("body.base"), resources::key("synthetic.material", "r2")}};
  REQUIRE(part.replace_model_states({state}));
  auto effective = checked(part.evaluate_model_state(state.id));
  CHECK_FALSE(effective.default_material());
  REQUIRE(effective.effective_material_for_body(BodyId("body.base")));
  CHECK(effective.effective_material_for_body(BodyId("body.base"))->key() ==
        resources::key("synthetic.material", "r2"));
  const auto plan = checked(effective.create_history_evaluation_plan());
  REQUIRE(plan.entries.size() == 1);
  CHECK(plan.entries.front().state == PartHistoryExecutionState::Suppressed);
  CHECK(part.default_material() == resources::key());
  const auto before = text(part);
  SECTION("reserved Primary identity") {
    state.id = primary_model_state_id();
  }
  SECTION("reserved Primary name") {
    state.name = "Primary";
  }
  SECTION("blank name") {
    state.name = " \t";
  }
  SECTION("missing executable") {
    state.features.front().executable = "missing";
  }
  SECTION("missing Body") {
    state.materials.back().body = BodyId("missing");
  }
  SECTION("missing resource") {
    state.materials.back().material = resources::key("missing");
  }
  SECTION("duplicate target") {
    state.features.push_back(state.features.front());
  }
  SECTION("wrong owner") {
    state.components = {{ComponentInstanceId("bad"), true, {}}};
  }
  CHECK_FALSE(part.replace_model_states({state}));
  CHECK(text(part) == before);
  auto reference = history::imported(true);
  CHECK_FALSE(reference.replace_model_states({named()}));
  CHECK(reference.evaluate_model_state(primary_model_state_id()));
}

TEST_CASE("State JSON uses strict necessary versions and typed override records",
          "[core][model-state][parity-b189]") {
  auto part = parameter_part();
  auto state = named();
  state.parameters = {{ParameterId("count"), checked(Quantity::count(5))}};
  REQUIRE(part.replace_model_states({state}));
  auto root = nlohmann::json::parse(text(part));
  SECTION("downgrade") {
    root["version"] = 15;
  }
  SECTION("unnecessary version") {
    root["model_states"] = nlohmann::json::array();
  }
  SECTION("unknown operation version") {
    root["model_states"][0]["version"] = 2;
  }
  SECTION("unknown field") {
    root["model_states"][0]["parent_state"] = "primary";
  }
  SECTION("wrong literal type") {
    root["model_states"][0]["parameters"][0]["value"] = true;
  }
  SECTION("wrong operation") {
    root["model_states"][0]["parameters"][0]["kind"] = "evaluated";
  }
  SECTION("unknown unit") {
    root["model_states"][0]["parameters"][0]["unit"] = "pixels";
  }
  SECTION("empty id") {
    root["model_states"][0]["id"] = "";
  }
  SECTION("duplicate id") {
    root["model_states"].push_back(root["model_states"][0]);
  }
  CHECK_FALSE(deserialize_part_document_from_json(root.dump()));
}

TEST_CASE("Repeated Assembly occurrences select independent child model states",
          "[core][model-state][parity-b189][parity-b190]") {
  auto part = parameter_part();
  resources::annotate(part);
  auto large = named();
  large.parameters = {{ParameterId("width"), checked(Quantity::length_mm(40))}};
  REQUIRE(part.replace_model_states({large}));
  auto child = checked(AssemblyDocument::create(DocumentId("child"), "Cylinder"));
  REQUIRE(child.add_member_part(part.id()));
  REQUIRE(child.add_component_instance(
      checked(ComponentInstance::create(ComponentInstanceId("piston"), "Piston", part.id()))));
  auto expanded = named("expanded", "Expanded");
  expanded.components = {{ComponentInstanceId("piston"), {}, large.id}};
  REQUIRE(child.replace_model_states({expanded}));
  auto assembly = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(assembly.add_subassembly_instance(
      checked(SubassemblyInstance::create(SubassemblyInstanceId("first"), "First", child.id()))));
  REQUIRE(assembly.add_subassembly_instance(checked(SubassemblyInstance::create(
      SubassemblyInstanceId("second"), "Second", child.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, identity_rigid_transform(), expanded.id))));
  auto project = checked(Project::create(DocumentId("project"), "Variants", assembly));
  REQUIRE(project.add_part_document(part));
  REQUIRE(project.add_child_assembly_document(child));
  REQUIRE(validate_project_model_states(project));
  const auto before = checked(serialize_project_to_json(project));
  auto leaves = checked(AssemblyLeafOccurrenceResolver{}.resolve(project));
  REQUIRE(leaves.size() == 2);
  CHECK(leaves[0].selected_model_state == primary_model_state_id());
  CHECK(leaves[1].selected_model_state == large.id);
  CHECK(leaves[0].referenced_part_document == leaves[1].referenced_part_document);
  auto roundtrip = checked(deserialize_project_from_json(before));
  CHECK(checked(serialize_project_to_json(roundtrip)) == before);
  CHECK(checked(AssemblyLeafOccurrenceResolver{}.resolve(roundtrip)) == leaves);
  // Removing the selected state from its definition makes the complete Project
  // invalid; suppression never hides a dangling persistent state reference.
  REQUIRE(project.find_part_document(part.id())->replace_model_states({}));
  CHECK_FALSE(validate_project_model_states(project));
  CHECK_FALSE(project.validate_assembly_structure());
}

TEST_CASE("Assembly state suppression differs from display and positional intent",
          "[core][model-state][parity-b189]") {
  auto part = parameter_part();
  REQUIRE(part.replace_model_states({named()}));
  auto assembly = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(assembly.add_member_part(part.id()));
  auto component =
      checked(ComponentInstance::create(ComponentInstanceId("part"), "Part", part.id()));
  component = checked(component.with_model_state(ModelStateId("large")));
  component = checked(component.with_transform({{10, 20, 30}, {20, 30, 40}}));
  component = checked(component.with_visibility(ComponentVisibility::Hidden));
  component = checked(component.with_grounding_state(ComponentGroundingState::Grounded));
  CHECK(component.selected_model_state() == ModelStateId("large"));
  REQUIRE(assembly.add_component_instance(component));
  auto suppressed = named("suppressed", "Suppressed");
  suppressed.components = {{component.id(), true, {}}};
  REQUIRE(assembly.replace_model_states({suppressed}));
  auto project = checked(Project::create(DocumentId("project"), "Project", assembly));
  REQUIRE(project.add_part_document(part));
  CHECK(checked(AssemblyLeafOccurrenceResolver{}.resolve(project)).empty());
  CHECK(checked(AssemblyLeafOccurrenceResolver{}.resolve(project,
                                                         AssemblyLeafParticipation::PhysicalActive))
            .size() == 1);
  CHECK(checked(AssemblyLeafOccurrenceResolver{}.resolve(
                    project, AssemblyLeafParticipation::PhysicalActive, suppressed.id))
            .empty());
  CHECK(project.assembly().find_component_instance(component.id())->transform() ==
        component.transform());
  auto root = nlohmann::json::parse(checked(serialize_assembly_document_to_json(assembly)));
  REQUIRE(root.at("version") == 2);
  root["version"] = 1;
  CHECK_FALSE(deserialize_assembly_document_from_json(root.dump()));
}
