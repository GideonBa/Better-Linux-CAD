#include "../support/engineering_resource_fixture.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
using namespace blcad;
namespace f = blcad::test::spatial_fixture;
namespace ef = blcad::test::engineering_fixture;
using json = nlohmann::json;
namespace {
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
PartDocument inputs() {
  auto p = f::part();
  (void)f::checked(p.add_expression_parameter(ParameterId("tolerance"), "Tolerance",
                                              ParameterType::Length, "width / 800000"));
  (void)f::checked(p.add_expression_parameter(ParameterId("deviation"), "Deviation",
                                              ParameterType::Length, "distance / 100000"));
  return p;
}
SurfaceReference plane() {
  return f::checked(SurfaceReference::create_semantic_face(
      f::checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))));
}
SurfaceOffsetParameters parameters(bool approximate = false) {
  return {ParameterId("distance"), ParameterId("tolerance"), SurfaceOffsetSide::AlongNormal,
          approximate ? SurfaceOffsetApproximation::VerifiedApproximation
                      : SurfaceOffsetApproximation::ExactAnalytic,
          approximate ? std::optional(ParameterId("deviation")) : std::nullopt};
}
FeatureBodyResultContext context(FeatureBodyOperationMode mode,
                                 const char* output = "surface.result") {
  return f::checked(FeatureBodyResultContext::create(
      mode,
      mode == FeatureBodyOperationMode::NewBody ? std::nullopt : std::optional(BodyId("base.body")),
      mode == FeatureBodyOperationMode::NewBody ? std::optional(BodyId(output)) : std::nullopt));
}
void project_roundtrip(const PartDocument& input) {
  for (const bool owned : {false, true}) {
    auto part = input;
    if (owned)
      ef::annotate(part);
    auto assembly = f::checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
    (void)f::checked(assembly.add_member_part(part.id()));
    auto project = f::checked(Project::create(DocumentId("project"), "Project", assembly));
    (void)f::checked(project.add_part_document(part));
    if (!owned)
      (void)f::checked(project.add_engineering_resource(ef::material()));
    const auto bytes = f::checked(serialize_project_to_json(project));
    const auto root = json::parse(bytes).at("parts").at(0);
    CHECK(root.at("version") == 13);
    CHECK_FALSE(root.contains("engineering_resources"));
    CHECK(root.at("engineering_resource_refs").size() == (owned ? 3U : 0U));
    const auto restored = f::checked(deserialize_project_from_json(bytes));
    CHECK(f::checked(serialize_project_to_json(restored)) == bytes);
    CHECK(saved(*restored.find_part_document(part.id())) == saved(part));
  }
}
} // namespace
TEST_CASE("OffsetSurface and Thicken preserve necessary V13 and all Body operation identities",
          "[core][surface-offset-intent][parity-b170]") {
  for (const auto mode : {FeatureBodyOperationMode::NewBody, FeatureBodyOperationMode::Join,
                          FeatureBodyOperationMode::Cut, FeatureBodyOperationMode::Intersect}) {
    for (const bool thicken : {false, true}) {
      if (!thicken && mode != FeatureBodyOperationMode::NewBody)
        continue;
      auto p = inputs();
      if (mode == FeatureBodyOperationMode::NewBody)
        (void)f::checked(p.add_body(f::checked(Body::create(
            BodyId("surface.result"), "Result", thicken ? BodyKind::Solid : BodyKind::Surface))));
      const auto c = context(mode);
      const auto feature = f::checked(
          thicken ? SurfaceOffsetFeature::create_thicken(FeatureId("surface.operation"), "Thicken",
                                                         plane(), parameters(true), c)
                  : SurfaceOffsetFeature::create_offset(FeatureId("surface.operation"), "Offset",
                                                        plane(), parameters(), c));
      REQUIRE(p.add_surface_feature(feature));
      CHECK(surface_feature_result_kind(feature) ==
            (thicken ? BodyKind::Solid : BodyKind::Surface));
      CHECK(surface_feature_result_body(feature) == c.effective_produced_body());
      const auto before = saved(p);
      const auto encoded = json::parse(before);
      CHECK(encoded.at("version") == 13);
      CHECK(saved(f::checked(deserialize_part_document_from_json(before))) == before);
      for (int version = 1; version < 13; ++version) {
        auto old = encoded;
        old["version"] = version;
        CHECK_FALSE(deserialize_part_document_from_json(old.dump()));
      }
      CHECK(p.dependency_graph().has_dependency("base.feature", "surface.operation"));
      CHECK(p.dependency_graph().has_dependency("distance", "surface.operation"));
      CHECK(p.dependency_graph().has_dependency("tolerance", "surface.operation"));
      auto edited_policy = feature.parameters();
      edited_policy.side = SurfaceOffsetSide::OppositeNormal;
      const auto edited =
          f::checked(thicken ? SurfaceOffsetFeature::create_thicken(feature.id(), "Edited", plane(),
                                                                    edited_policy, c)
                             : SurfaceOffsetFeature::create_offset(feature.id(), "Edited", plane(),
                                                                   edited_policy, c));
      REQUIRE(p.update_surface_feature(edited));
      CHECK(surface_feature_result_body(*p.find_surface_feature(feature.id())) ==
            c.effective_produced_body());
      const auto after_edit = saved(p);
      edited_policy.tolerance = ParameterId("turns");
      const auto invalid_edit =
          f::checked(thicken ? SurfaceOffsetFeature::create_thicken(feature.id(), "Invalid",
                                                                    plane(), edited_policy, c)
                             : SurfaceOffsetFeature::create_offset(feature.id(), "Invalid", plane(),
                                                                   edited_policy, c));
      CHECK_FALSE(p.update_surface_feature(invalid_edit));
      CHECK(saved(p) == after_edit);
      if (mode != FeatureBodyOperationMode::NewBody) {
        CHECK_FALSE(p.dependency_graph().has_cycle());
        REQUIRE(p.remove_surface_feature(feature.id()));
        CHECK(p.dependency_graph().has_dependency("base.feature", "body:base.body"));
      } else
        project_roundtrip(p);
    }
  }
}
TEST_CASE("Surface selection owns typed faces and protects source Body and semantic producers",
          "[core][surface-offset-intent][parity-b170]") {
  auto p = inputs();
  (void)f::checked(
      p.add_body(f::checked(Body::create(BodyId("surface.result"), "Result", BodyKind::Surface))));
  const auto face = f::checked(FaceReference::create_planar(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))));
  CHECK_FALSE(SurfaceReference::create_faces(BodyId("base.body"), {face, face}));
  const auto wrong_role = f::checked(FaceReference::create_planar(
      PartFeatureInputRole::LoftBoundarySupport,
      f::checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))));
  CHECK_FALSE(SurfaceReference::create_faces(BodyId("base.body"), {wrong_role}));
  const auto target = f::checked(SurfaceReference::create_faces(BodyId("base.body"), {face}));
  const auto feature = f::checked(
      SurfaceOffsetFeature::create_offset(FeatureId("offset"), "Offset", target, parameters(),
                                          context(FeatureBodyOperationMode::NewBody)));
  REQUIRE(p.add_surface_feature(feature));
  CHECK(p.dependency_graph().has_dependency("body:base.body", "offset"));
  CHECK_FALSE(p.remove_body(BodyId("base.body")));
  CHECK_FALSE(p.remove_feature(FeatureId("base.feature")));
  const auto before = saved(p);
  CHECK(saved(f::checked(deserialize_part_document_from_json(before))) == before);
  PartFeatureHistoryIntent history;
  for (const auto& row : f::checked(p.executable_history()))
    history.order.push_back(row.node_id);
  history.suppressed = {"base.feature"};
  REQUIRE(p.apply_history_intent(history));
  const auto plan = f::checked(p.create_history_evaluation_plan());
  const auto blocked = std::find_if(plan.entries.begin(), plan.entries.end(), [](const auto& row) {
    return row.feature.node_id == "offset";
  });
  REQUIRE(blocked != plan.entries.end());
  CHECK(blocked->state == PartHistoryExecutionState::Blocked);
  auto invalid = json::parse(before);
  invalid["surface_features"][0]["target"]["body"] = "surface.result";
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
}
TEST_CASE("Offset policy bounds and dependent formulas reject atomically without inactive budgets",
          "[core][surface-offset-intent][parity-b170]") {
  auto p = inputs();
  (void)f::checked(
      p.add_body(f::checked(Body::create(BodyId("surface.result"), "Result", BodyKind::Surface))));
  auto policy = parameters(true);
  policy.side = SurfaceOffsetSide::Symmetric;
  const auto feature = f::checked(SurfaceOffsetFeature::create_offset(
      FeatureId("offset"), "Offset", plane(), policy, context(FeatureBodyOperationMode::NewBody)));
  REQUIRE(p.add_surface_feature(feature));
  REQUIRE(p.set_parameter_formula(ParameterId("tolerance"), "width / 1000000"));
  const auto before = saved(p);
  CHECK_FALSE(
      p.set_parameter_value(ParameterId("width"), f::checked(Quantity::length_mm(2000000))));
  CHECK(saved(p) == before);
  CHECK_FALSE(
      p.set_parameter_value(ParameterId("distance"), f::checked(Quantity::length_mm(1e-7))));
  CHECK(saved(p) == before);
  CHECK_FALSE(p.set_parameter_formula(ParameterId("deviation"), "distance"));
  CHECK(saved(p) == before);
  auto inactive = parameters();
  inactive.maximum_deviation = ParameterId("deviation");
  CHECK_FALSE(SurfaceOffsetFeature::create_offset(FeatureId("bad"), "Bad", plane(), inactive,
                                                  context(FeatureBodyOperationMode::NewBody)));
  CHECK_FALSE(SurfaceOffsetFeature::create_offset(FeatureId("bad"), "Bad", plane(), parameters(),
                                                  context(FeatureBodyOperationMode::Join)));
  for (int mutation = 0; mutation < 7; ++mutation) {
    auto invalid = json::parse(before);
    auto& record = invalid["surface_features"][0];
    if (mutation == 0)
      record["parameters"]["side"] = "guess";
    if (mutation == 1)
      record["parameters"]["approximation"] = "best_effort";
    if (mutation == 2)
      record["parameters"]["maximum_deviation"] = nullptr;
    if (mutation == 3)
      record["parameters"]["sampled_offsets"] = json::array();
    if (mutation == 4)
      record.erase("operation_mode");
    if (mutation == 5)
      record["parameters"]["tolerance"] = "turns";
    if (mutation == 6)
      record["result_body"] = "surface.result";
    CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  }
}
