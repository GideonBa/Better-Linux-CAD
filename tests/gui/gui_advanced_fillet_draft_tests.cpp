#include "../support/part_history_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_fillet_draft.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::history_fixture;
namespace {
EdgeReference edge() {
  return f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::FilletEdge,
      f::checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopFront))));
}
FaceReference face(SemanticFace side,
                   PartFeatureInputRole role = PartFeatureInputRole::FilletSupportFace) {
  return f::checked(FaceReference::create_planar(
      role, f::checked(SemanticFaceReference::create(FeatureId("base"), side))));
}
VariableFilletIntent law(ParameterId radius = ParameterId("radius")) {
  return {{{"group",
            "Outer group",
            {{edge(), false}},
            FilletPropagation::Explicit,
            FilletRadiusInterpolation::Linear,
            {{"start", FilletStationLocation::Start, 0, radius},
             {"end", FilletStationLocation::End, 1, radius}}}}};
}
FilletFeature variable(VariableFilletIntent intent = law()) {
  return f::checked(FilletFeature::create_variable(FeatureId("finish"), "Variable finish",
                                                   BodyId("body.base"), intent));
}
FilletFeature round() {
  FullRoundIntent intent{
      {"first", "Front", {face(SemanticFace::Front)}},
      {"second", "Back", {face(SemanticFace::Back)}},
      {"middle", "Top", {face(SemanticFace::Top, PartFeatureInputRole::FilletMiddleFace)}},
      FilletContinuity::Curvature,
      {}};
  return f::checked(FilletFeature::create_full_round(FeatureId("finish"), "Full round",
                                                     BodyId("body.base"), intent));
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
void setup(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("history"), "History"));
  REQUIRE(session.commit_part_transaction("Fixture", [](PartDocument& p) {
    p = f::native_base();
    return Result<std::size_t>::success(1);
  }));
}
std::shared_ptr<GuiFilletDraft> draft(FilletFeature record) {
  return std::make_shared<GuiFilletDraft>(
      GuiFilletDraft{std::move(record), false, false, {}, {}, {}, {}});
}
} // namespace
TEST_CASE("Typed finishing draft preserves tiny untouched stations and explicit group reversal",
          "[gui][advanced-fillet-draft][parity-b179]") {
  auto p = f::native_base();
  auto intent = law();
  intent.groups[0].stations.insert(
      intent.groups[0].stations.begin() + 1,
      {"tiny", FilletStationLocation::Fraction, 1e-20, ParameterId("radius")});
  REQUIRE(p.add_fillet_feature(variable(intent)));
  const auto before = saved(p);
  auto state = draft(*p.find_fillet_feature(FeatureId("finish")));
  state->editing = true;
  auto candidate = p;
  REQUIRE(state->apply(candidate));
  CHECK(saved(candidate) == before);
  intent.groups[0] = reverse_fillet_group(intent.groups[0]);
  intent.groups[0] = reverse_fillet_group(intent.groups[0]);
  state->record = variable(intent);
  candidate = p;
  REQUIRE(state->apply(candidate));
  CHECK(saved(candidate) == before);
  CHECK(saved(p) == before);
}
TEST_CASE("Full-round controller uses no radius and commits one normal undoable transaction",
          "[gui][advanced-fillet-draft][parity-b179]") {
  GuiDocumentSession session;
  setup(session);
  const auto before = saved(*session.part_document());
  GuiInteractiveFinishingController controller;
  auto state = draft(round());
  REQUIRE(controller.begin_draft(session, state));
  CHECK(controller.handles().empty());
  REQUIRE(controller.preview(session));
  REQUIRE(controller.preview_shape(session));
  CHECK(saved(*session.part_document()) == before);
  REQUIRE(controller.apply(session));
  const auto after = saved(*session.part_document());
  REQUIRE(session.part_document()->find_fillet_feature(FeatureId("finish")));
  CHECK(session.part_document()
            ->find_fillet_feature(FeatureId("finish"))
            ->parameter_dependencies()
            .empty());
  REQUIRE(session.undo());
  CHECK(saved(*session.part_document()) == before);
  REQUIRE(session.redo());
  CHECK(saved(*session.part_document()) == after);
}
TEST_CASE("Station handles stage their exact binding and reject failed geometry atomically",
          "[gui][advanced-fillet-draft][parity-b179]") {
  GuiDocumentSession session;
  setup(session);
  const auto before = saved(*session.part_document());
  auto state = draft(variable());
  for (const auto* id : {"station.start", "station.end"}) {
    GuiViewportManipulatorHandle handle;
    handle.id = id;
    handle.reference_value = 1;
    state->handles.push_back(handle);
    state->handle_bindings.emplace(id, ParameterId("radius"));
  }
  GuiInteractiveFinishingController controller;
  REQUIRE(controller.begin_draft(session, state));
  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = "station.end";
  candidate.scalar_value = 40;
  REQUIRE(controller.apply_manipulator(candidate));
  CHECK(controller.handle_value("station.start") == 40);
  CHECK(controller.handle_value("station.end") == 40);
  CHECK_FALSE(controller.preview_shape(session));
  CHECK_FALSE(controller.apply(session));
  CHECK(saved(*session.part_document()) == before);
  CHECK(controller.active());
  candidate.scalar_value = 2;
  REQUIRE(controller.apply_manipulator(candidate));
  REQUIRE(controller.preview_shape(session));
  REQUIRE(controller.apply(session));
  CHECK(session.part_document()->find_parameter(ParameterId("radius"))->value().millimeters() == 2);
  REQUIRE(session.undo());
  CHECK(saved(*session.part_document()) == before);
}
TEST_CASE("Explicit independent values preserve formulas while rebinding all linked stations",
          "[gui][advanced-fillet-draft][parity-b179]") {
  auto p = f::native_base();
  REQUIRE(p.add_expression_parameter(ParameterId("computed.radius"), "Computed radius",
                                     ParameterType::Length, "radius * 1.2"));
  auto state = draft(variable(law(ParameterId("computed.radius"))));
  state->values["computed.radius"] = 2.;
  auto candidate = p;
  CHECK_FALSE(state->apply(candidate));
  state->independent_values = true;
  candidate = p;
  REQUIRE(state->apply(candidate));
  const auto* record = candidate.find_fillet_feature(FeatureId("finish"));
  REQUIRE(record);
  const auto& stored =
      std::get<VariableFilletIntent>(record->advanced_intent()->definition).groups[0];
  CHECK(stored.stations[0].radius == stored.stations[1].radius);
  CHECK(stored.stations[0].radius != ParameterId("computed.radius"));
  CHECK(candidate.find_parameter(ParameterId("computed.radius"))->formula() ==
        p.find_parameter(ParameterId("computed.radius"))->formula());
  CHECK(candidate.find_parameter(stored.stations[0].radius)->value().millimeters() == 2);
  CHECK(stored.stations[0].id == "start");
  CHECK(stored.stations[1].id == "end");
}
TEST_CASE("Typed mode boundaries reject unrelated pending values and cancellation preserves intent",
          "[gui][advanced-fillet-draft][parity-b179]") {
  GuiDocumentSession session;
  setup(session);
  const auto before = saved(*session.part_document());
  auto wrong = draft(round());
  wrong->pending.push_back(f::checked(Parameter::create_length(
      ParameterId("old.radius"), "Old mode radius", f::checked(Quantity::length_mm(2)))));
  GuiInteractiveFinishingController controller;
  CHECK_FALSE(controller.begin_draft(session, wrong));
  CHECK(saved(*session.part_document()) == before);
  auto state = draft(variable());
  REQUIRE(controller.begin_draft(session, state));
  controller.set_first_value(2);
  REQUIRE(controller.preview(session));
  controller.cancel();
  CHECK_FALSE(controller.active());
  CHECK(saved(*session.part_document()) == before);
}
