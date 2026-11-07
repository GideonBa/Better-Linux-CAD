#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;
namespace fixture = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
const PartHistoryExecutionEntry& row(const PartHistoryEvaluationPlan& plan, const std::string& id) {
  const auto found = std::find_if(plan.entries.begin(), plan.entries.end(),
                                  [&](const auto& entry) { return entry.feature.node_id == id; });
  REQUIRE(found != plan.entries.end());
  return *found;
}
const PartHistoryBodyAvailability& body(const PartHistoryEvaluationPlan& plan, const char* id) {
  const auto found = std::find_if(plan.bodies.begin(), plan.bodies.end(),
                                  [&](const auto& entry) { return entry.body.value() == id; });
  REQUIRE(found != plan.bodies.end());
  return *found;
}
void suppress(PartDocument& part, std::vector<std::string> ids) {
  PartFeatureHistoryIntent edit;
  for (const auto& entry : checked(part.executable_history()))
    edit.order.push_back(entry.node_id);
  edit.suppressed = std::move(ids);
  REQUIRE(part.apply_history_intent(std::move(edit)));
}
void add_fillet(PartDocument& part) {
  REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Fillet", BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
}
void add_other(PartDocument& part) {
  const auto context = fixture::new_body(part, "other");
  const auto feature = checked(Feature::create_additive_extrude(
      FeatureId("other"), "Other", SketchId("profile"), ParameterId("depth")));
  REQUIRE(part.add_feature(checked(feature.with_body_result_context(context))));
}
void move(PartDocument& part, const char* id = "move") {
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId(id), BodyId("body.base"), BodyTransformCoordinateSpace::World, std::nullopt,
      {1, 0, 0}, false, false))));
}
} // namespace

TEST_CASE(
    "Effective plan covers all families with current Body predecessors and Boolean consumption",
    "[core][parity-b147][history-evaluation-plan]") {
  const auto part = fixture::all_families();
  const auto plan = checked(part.create_history_evaluation_plan());
  REQUIRE(plan.entries.size() == 15);
  CHECK(plan.complete());
  CHECK_FALSE(plan.has_blocked_entries());
  CHECK(plan.active_ids().size() == 15);
  for (const auto& entry : plan.entries) {
    CHECK(entry.state == PartHistoryExecutionState::Active);
    CHECK(entry.issues.empty());
  }
  REQUIRE(row(plan, "translate").body_inputs.size() == 1);
  CHECK(row(plan, "translate").body_inputs[0].producer_id == "base");
  REQUIRE(row(plan, "fillet").body_inputs.size() == 1);
  CHECK(row(plan, "fillet").body_inputs[0].producer_id == "translate");
  CHECK(body(plan, "body.base").producer_id == "boolean");
  CHECK_FALSE(body(plan, "body.mirror").available);
  CHECK(body(plan, "body.mirror").unavailable_due_to == "boolean");
  for (const auto& availability : plan.bodies)
    CHECK(availability.available == (availability.body.value() != "body.mirror"));
}

TEST_CASE("Suppressed modifiers bypass only Body production without blocking downstream modifiers",
          "[core][parity-b147][history-evaluation-plan]") {
  auto part = fixture::all_families();
  suppress(part, {"fillet", "translate"});
  const auto plan = checked(part.create_history_evaluation_plan());
  CHECK(plan.complete());
  CHECK(row(plan, "fillet").state == PartHistoryExecutionState::Suppressed);
  CHECK(row(plan, "translate").state == PartHistoryExecutionState::Suppressed);
  CHECK(row(plan, "chamfer").state == PartHistoryExecutionState::Active);
  REQUIRE(row(plan, "chamfer").body_inputs.size() == 1);
  CHECK(row(plan, "chamfer").body_inputs[0].producer_id == "base");
  CHECK(body(plan, "body.base").producer_id == "boolean");
}

TEST_CASE(
    "Suppressed creators block exact consumers while independent Bodies and authored flags remain",
    "[core][parity-b147][history-evaluation-plan]") {
  auto part = fixture::all_families();
  suppress(part, {"base"});
  const auto before = checked(serialize_part_document_to_json(part));
  const auto plan = checked(part.create_history_evaluation_plan());
  CHECK_FALSE(plan.complete());
  CHECK(plan.has_blocked_entries());
  CHECK(row(plan, "translate").state == PartHistoryExecutionState::Blocked);
  CHECK(row(plan, "linear").state == PartHistoryExecutionState::Blocked);
  CHECK(row(plan, "linear").feature.state == PartFeatureState::Active);
  CHECK(row(plan, "revolve").state == PartHistoryExecutionState::Active);
  CHECK(row(plan, "sweep").state == PartHistoryExecutionState::Active);
  CHECK(row(plan, "loft").state == PartHistoryExecutionState::Active);
  CHECK(row(plan, "surface").state == PartHistoryExecutionState::Active);
  CHECK(body(plan, "body.revolve").available);
  CHECK(body(plan, "body.surface").available);
  CHECK(body(plan, "imported.body").available);
  CHECK_FALSE(body(plan, "body.base").available);
  REQUIRE_FALSE(row(plan, "translate").issues.empty());
  CHECK(row(plan, "translate").issues[0].code == PartHistoryInputIssueCode::SuppressedProducer);
  CHECK(row(plan, "translate").issues[0].producer_id == "base");
  CHECK(row(plan, "translate").issues[0].consumer_id == "translate");
  CHECK(row(plan, "translate").issues[0].input_node_id == "body:body.base");
  CHECK(checked(serialize_part_document_to_json(part)) == before);
}

TEST_CASE("Semantic resources cannot consume a suppressed modifier even when its Body bypasses",
          "[core][parity-b147][history-evaluation-plan]") {
  auto part = fixture::native_base();
  add_fillet(part);
  const auto reference = checked(ResultFaceReference::create(
      FeatureId("fillet"), BodyId("body.base"), {{FeatureId("base"), "cap.end"}}));
  REQUIRE(part.add_result_face_workplane(
      checked(ResultFaceWorkplane::create(DatumPlaneId("support"), "Support", reference))));
  fixture::rectangle(part, "consumer", "support");
  const auto context = fixture::new_body(part, "consumer");
  REQUIRE(part.add_revolve_feature(checked(RevolveFeature::create_revolve(
      FeatureId("consumer.feature"), "Consumer",
      fixture::profile(PartFeatureInputRole::RevolveProfile, "consumer"),
      fixture::axis(PartFeatureInputRole::RevolveAxis), RevolveAngleExtent::full(), context))));
  move(part);
  suppress(part, {"fillet"});
  const auto plan = checked(part.create_history_evaluation_plan());
  CHECK(row(plan, "move").state == PartHistoryExecutionState::Active);
  CHECK(body(plan, "body.base").available);
  CHECK(row(plan, "consumer.feature").state == PartHistoryExecutionState::Blocked);
  CHECK_FALSE(body(plan, "body.consumer").available);
  REQUIRE(std::any_of(row(plan, "consumer.feature").issues.begin(),
                      row(plan, "consumer.feature").issues.end(), [](const auto& issue) {
                        return issue.code == PartHistoryInputIssueCode::SuppressedProducer &&
                               issue.producer_id == "fillet" && issue.input_node_id == "consumer";
                      }));
  suppress(part, {"fillet", "consumer.feature"});
  const auto closure = checked(part.create_history_evaluation_plan());
  CHECK(closure.complete());
  CHECK(row(closure, "consumer.feature").state == PartHistoryExecutionState::Suppressed);
  CHECK(row(closure, "move").state == PartHistoryExecutionState::Active);
}

TEST_CASE("A blocked active modifier is never silently bypassed by a later Body consumer",
          "[core][parity-b147][history-evaluation-plan]") {
  auto part = fixture::native_base();
  add_other(part);
  const auto side = checked(FaceReference::create_planar(
      PartFeatureInputRole::DraftFace,
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Front))));
  const auto neutral = checked(PlaneReference::create_semantic_face(
      PartFeatureInputRole::DraftNeutralPlane,
      checked(SemanticFaceReference::create(FeatureId("other"), SemanticFace::Top))));
  REQUIRE(part.add_draft_feature(checked(DraftFeature::create(
      FeatureId("draft"), "Draft", BodyId("body.base"), {side},
      fixture::axis(PartFeatureInputRole::DraftPullDirection), neutral, ParameterId("angle")))));
  move(part);
  suppress(part, {"other"});
  const auto plan = checked(part.create_history_evaluation_plan());
  CHECK(row(plan, "base").state == PartHistoryExecutionState::Active);
  CHECK(row(plan, "draft").state == PartHistoryExecutionState::Blocked);
  CHECK(row(plan, "move").state == PartHistoryExecutionState::Blocked);
  CHECK_FALSE(body(plan, "body.base").available);
  REQUIRE(std::any_of(row(plan, "move").issues.begin(), row(plan, "move").issues.end(),
                      [](const auto& issue) {
                        return issue.code == PartHistoryInputIssueCode::BlockedProducer &&
                               issue.producer_id == "draft";
                      }));
  suppress(part, {"other", "draft"});
  const auto closure = checked(part.create_history_evaluation_plan());
  CHECK(closure.complete());
  CHECK(row(closure, "move").state == PartHistoryExecutionState::Active);
}

TEST_CASE("Every authored prefix has exact active rows and remains explicitly partial",
          "[core][parity-b147][history-evaluation-plan]") {
  const auto part = fixture::all_families(true);
  std::vector<std::string> ids;
  for (const auto& entry : checked(part.executable_history()))
    ids.push_back(entry.node_id);
  const auto position = [&](const char* id) {
    return std::find(ids.begin(), ids.end(), id) - ids.begin();
  };
  for (std::size_t count = 0; count <= ids.size(); ++count) {
    CAPTURE(count);
    const auto plan = checked(part.create_history_evaluation_plan(count));
    CHECK_FALSE(plan.complete());
    CHECK_FALSE(plan.has_blocked_entries());
    CHECK(plan.active_ids() ==
          std::vector<std::string>(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(count)));
    CHECK(body(plan, "body.base").available ==
          (position("base") < static_cast<std::ptrdiff_t>(count)));
    CHECK(body(plan, "body.mirror").available ==
          (position("mirror") < static_cast<std::ptrdiff_t>(count) &&
           position("boolean") >= static_cast<std::ptrdiff_t>(count)));
    CHECK(body(plan, "imported.body").available ==
          (position("imported") < static_cast<std::ptrdiff_t>(count)));
    for (std::size_t row_index = count; row_index < ids.size(); ++row_index)
      CHECK(plan.entries[row_index].state == PartHistoryExecutionState::OutsidePrefix);
  }
  CHECK_FALSE(part.create_history_evaluation_plan(ids.size() + 1));
}

TEST_CASE("Prefix rows count suppressed entries and never mutate source graph invalidation or JSON",
          "[core][parity-b147][history-evaluation-plan]") {
  auto part = fixture::native_base();
  add_fillet(part);
  move(part);
  suppress(part, {"fillet"});
  const auto before = checked(serialize_part_document_to_json(part));
  const auto graph = part.dependency_graph().dependencies();
  std::vector<std::pair<std::string, InvalidationStatus>> statuses;
  for (const auto& entry : part.invalidation_state().entries())
    statuses.emplace_back(entry.node_id, entry.status);
  const auto plan = checked(part.create_history_evaluation_plan(2));
  CHECK(plan.entries.size() == 3);
  CHECK(plan.active_ids() == std::vector<std::string>{"base"});
  CHECK(row(plan, "fillet").state == PartHistoryExecutionState::Suppressed);
  CHECK(row(plan, "move").state == PartHistoryExecutionState::OutsidePrefix);
  CHECK(body(plan, "body.base").producer_id == "base");
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK(part.dependency_graph().dependencies() == graph);
  std::vector<std::pair<std::string, InvalidationStatus>> after;
  for (const auto& entry : part.invalidation_state().entries())
    after.emplace_back(entry.node_id, entry.status);
  CHECK(after == statuses);
}

TEST_CASE("Imported Body modification bypasses to the immutable source owner",
          "[core][parity-b147][history-evaluation-plan]") {
  auto part = fixture::base();
  const auto cut = checked(Feature::create_subtractive_extrude(
      FeatureId("cut"), "Cut", SketchId("profile"), FeatureId("imported")));
  const auto context = checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::Cut, BodyId("imported.body"), BodyId("imported.body")));
  REQUIRE(part.add_feature(checked(cut.with_body_result_context(context))));
  suppress(part, {"cut"});
  const auto plan = checked(part.create_history_evaluation_plan());
  CHECK(plan.complete());
  CHECK(body(plan, "imported.body").available);
  CHECK(body(plan, "imported.body").producer_id == "imported");
  CHECK(body(plan, "body.base").producer_id == "base");
  CHECK(row(plan, "cut").state == PartHistoryExecutionState::Suppressed);
}

TEST_CASE("Empty and immutable reference plans retain their distinct source owned basis",
          "[core][parity-b147][history-evaluation-plan]") {
  const auto empty = checked(PartDocument::create(DocumentId("empty"), "Empty"));
  const auto plan = checked(empty.create_history_evaluation_plan());
  CHECK(plan.complete());
  CHECK(plan.entries.empty());
  CHECK(plan.bodies.empty());
  CHECK_FALSE(checked(empty.create_history_evaluation_plan(0)).complete());
  const auto reference = fixture::imported(true);
  const auto imported = checked(reference.create_history_evaluation_plan());
  CHECK(imported.complete());
  CHECK(imported.entries.empty());
  REQUIRE(imported.bodies.size() == 1);
  CHECK(imported.bodies[0].available);
  CHECK_FALSE(imported.bodies[0].producer_id);
}

TEST_CASE("Resource IDs resembling Body aliases remain typed resources",
          "[core][parity-b147][history-evaluation-plan]") {
  auto part = fixture::native_base();
  REQUIRE(part.add_parameter(checked(Parameter::create_length(ParameterId("body:depth"), "Depth",
                                                              checked(Quantity::length_mm(8))))));
  fixture::rectangle(part, "body:profile");
  const auto feature = checked(Feature::create_additive_extrude(
      FeatureId("base"), "Base", SketchId("body:profile"), ParameterId("body:depth")));
  const auto context = *part.find_feature(FeatureId("base"))->body_result_context();
  REQUIRE(part.update_feature(checked(feature.with_body_result_context(context))));
  const auto plan = checked(part.create_history_evaluation_plan());
  CHECK(plan.complete());
  CHECK(row(plan, "base").state == PartHistoryExecutionState::Active);
  CHECK(row(plan, "base").body_inputs.empty());
  CHECK(body(plan, "body.base").producer_id == "base");
}

TEST_CASE("A declared Body without an executable creator cannot satisfy a modifier input",
          "[core][parity-b147][history-evaluation-plan]") {
  auto part = checked(PartDocument::create(DocumentId("unproduced"), "Unproduced"));
  REQUIRE(part.add_body(checked(Body::create(BodyId("body.base"), "Body", BodyKind::Solid))));
  move(part);
  const auto plan = checked(part.create_history_evaluation_plan());
  CHECK_FALSE(plan.complete());
  CHECK(row(plan, "move").state == PartHistoryExecutionState::Blocked);
  REQUIRE(row(plan, "move").issues.size() == 1);
  CHECK(row(plan, "move").issues[0].code == PartHistoryInputIssueCode::MissingBodyCreator);
  CHECK(row(plan, "move").issues[0].producer_id.empty());
  CHECK_FALSE(body(plan, "body.base").available);
}
