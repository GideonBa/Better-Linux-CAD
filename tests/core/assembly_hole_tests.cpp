#include "../support/assembly_hole_fixture.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
using namespace blcad;
namespace a = blcad::test::assembly_hole;
namespace h = blcad::test::history_fixture;
namespace {
std::string saved(const Project& p) {
  return h::checked(serialize_project_to_json(p));
}
AssemblyHoleFeature changed(const AssemblyHoleFeature& f, AssemblyHoleLayout layout,
                            std::vector<AssemblyHoleParticipant> participants) {
  return h::checked(
      AssemblyHoleFeature::create(f.id(), f.name(), std::move(layout), std::move(participants)));
}
} // namespace
TEST_CASE("Assembly Hole owns one shared layout and explicit revision-pinned participant recipes",
          "[core][assembly-hole][parity-b196][A03]") {
  auto p = a::project();
  const auto baseline = saved(p);
  const auto feature = a::feature();
  REQUIRE(p.replace_assembly_hole_features({feature}));
  REQUIRE(validate_assembly_hole_features(p));
  CHECK(feature.layout().source == a::occurrence("cover", "cover.part"));
  CHECK(feature.layout().pattern == std::optional(FeatureId("layout.pattern")));
  CHECK(feature.participants()[0].recipe.placements().size() == 1);
  REQUIRE(feature.participants()[1].recipe.thread());
  CHECK(
      std::get<ThreadStandardProfile>(feature.participants()[1].recipe.thread()->profile()).table ==
      a::pin());
  const auto owned = saved(p);
  CHECK(nlohmann::json::parse(owned)["version"] == 5);
  auto opened = h::checked(deserialize_project_from_json(owned));
  CHECK(saved(opened) == owned);
  CHECK(opened.find_part_document(DocumentId("base.part"))
            ->find_hole_feature(FeatureId("mounting.hole")) == nullptr);
  CHECK_FALSE(p.replace_assembly_hole_features({feature, feature}));
  CHECK(saved(p) == owned);
  REQUIRE(p.replace_assembly_hole_features({}));
  CHECK(saved(p) == baseline);
}
TEST_CASE(
    "Assembly Hole rejects duplicate occurrences definitions roles and accidental shared edits",
    "[core][assembly-hole][parity-b196][A03]") {
  const auto feature = a::feature();
  auto participants = feature.participants();
  participants.push_back(participants.front());
  CHECK_FALSE(
      AssemblyHoleFeature::create(feature.id(), feature.name(), feature.layout(), participants));
  participants = feature.participants();
  participants[1].id = participants[0].id;
  CHECK_FALSE(
      AssemblyHoleFeature::create(feature.id(), feature.name(), feature.layout(), participants));
  participants = feature.participants();
  participants[1].role = AssemblyHoleParticipantRole::Seat;
  CHECK_FALSE(
      AssemblyHoleFeature::create(feature.id(), feature.name(), feature.layout(), participants));
  participants = feature.participants();
  participants[0].role = AssemblyHoleParticipantRole::Threaded;
  CHECK_FALSE(
      AssemblyHoleFeature::create(feature.id(), feature.name(), feature.layout(), participants));
  participants = feature.participants();
  participants[1].definition_policy = static_cast<AssemblyHoleDefinitionPolicy>(19);
  CHECK_FALSE(
      AssemblyHoleFeature::create(feature.id(), feature.name(), feature.layout(), participants));
  auto repeated = a::project(true, true);
  const auto before = saved(repeated);
  CHECK_FALSE(repeated.replace_assembly_hole_features({feature}));
  CHECK(saved(repeated) == before);
  REQUIRE(repeated.replace_assembly_hole_features(
      {a::feature(true, AssemblyHoleDefinitionPolicy::AffectSharedDefinition)}));
  participants =
      a::feature(true, AssemblyHoleDefinitionPolicy::AffectSharedDefinition).participants();
  auto duplicate = participants[1];
  duplicate.id = "second-base";
  duplicate.occurrence.component = ComponentInstanceId("base.other");
  participants.push_back(duplicate);
  CHECK_FALSE(
      AssemblyHoleFeature::create(feature.id(), feature.name(), feature.layout(), participants));
  auto reference = a::project(true, false, true);
  CHECK_FALSE(reference.replace_assembly_hole_features({a::feature(
      true, AssemblyHoleDefinitionPolicy::SingleOccurrence, primary_model_state_id(), true)}));
}
TEST_CASE("Assembly Hole captures active occurrence state and rejects suppressed or unrelated "
          "layout producers",
          "[core][assembly-hole][parity-b196][A03]") {
  auto p = a::project();
  const auto feature = a::feature();
  auto invalid = feature.layout();
  invalid.source.part = DocumentId("missing");
  CHECK_FALSE(
      p.replace_assembly_hole_features({changed(feature, invalid, feature.participants())}));
  // Structural authoring may temporarily hold an unresolved member. A matching
  // leaf path does not imply that the source Part pointer exists.
  auto root = h::checked(AssemblyDocument::create(DocumentId("incomplete"), "Incomplete"));
  REQUIRE(root.add_member_part(DocumentId("missing")));
  REQUIRE(root.add_member_part(DocumentId("base.part")));
  REQUIRE(root.add_component_instance(h::checked(ComponentInstance::create(
      ComponentInstanceId("cover"), "Missing source", DocumentId("missing")))));
  REQUIRE(root.add_component_instance(h::checked(
      ComponentInstance::create(ComponentInstanceId("base"), "Base", DocumentId("base.part")))));
  auto incomplete =
      h::checked(Project::create(DocumentId("incomplete.project"), "Incomplete", root));
  REQUIRE(incomplete.add_part_document(a::base()));
  const auto missing = h::checked(
      AssemblyHoleFeature::create(FeatureId("missing-source"), "Missing source",
                                  {{{}, ComponentInstanceId("cover"), DocumentId("missing")},
                                   primary_model_state_id(),
                                   FeatureId("layout.hole"),
                                   std::nullopt},
                                  {{"base",
                                    {{}, ComponentInstanceId("base"), DocumentId("base.part")},
                                    primary_model_state_id(),
                                    AssemblyHoleParticipantRole::Threaded,
                                    AssemblyHoleDefinitionPolicy::SingleOccurrence,
                                    a::threaded()}}));
  CHECK_FALSE(incomplete.replace_assembly_hole_features({missing}));
  invalid = feature.layout();
  invalid.pattern = FeatureId("base");
  CHECK_FALSE(
      p.replace_assembly_hole_features({changed(feature, invalid, feature.participants())}));
  auto* cover_part = p.find_part_document(DocumentId("cover.part"));
  REQUIRE(cover_part);
  REQUIRE(cover_part->add_body(
      h::checked(Body::create(BodyId("wrong.body"), "Wrong pattern", BodyKind::Solid))));
  REQUIRE(cover_part->add_linear_pattern_feature(h::checked(LinearPatternFeature::create(
      FeatureId("wrong.pattern"), "Different producer",
      {h::checked(PatternSourceReference::feature(FeatureId("base")))},
      h::checked(AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis,
                                                  DatumAxisId("layout.axis"))),
      ParameterId("layout.count"), LinearPatternExtentMode::Spacing, ParameterId("layout.spacing"),
      PatternDirectionSign::Positive,
      h::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("wrong.body")))))));
  invalid = feature.layout();
  invalid.pattern = FeatureId("wrong.pattern");
  CHECK_FALSE(
      p.replace_assembly_hole_features({changed(feature, invalid, feature.participants())}));
  auto* child = p.find_assembly_document(DocumentId("mounting.child"));
  REQUIRE(child);
  REQUIRE(child->set_component_instance_suppression_state(ComponentInstanceId("base"),
                                                          ComponentSuppressionState::Suppressed));
  CHECK_FALSE(p.replace_assembly_hole_features({feature}));
  REQUIRE(child->set_component_instance_suppression_state(ComponentInstanceId("base"),
                                                          ComponentSuppressionState::Active));
  auto* base = p.find_part_document(DocumentId("base.part"));
  REQUIRE(base);
  ModelState short_state{ModelStateId("short"),
                         "Short blind thread",
                         {{ParameterId("blind.depth"), h::checked(Quantity::length_mm(3))}},
                         {},
                         {},
                         {},
                         {}};
  REQUIRE(base->replace_model_states({short_state}));
  CHECK_FALSE(p.replace_assembly_hole_features(
      {a::feature(true, AssemblyHoleDefinitionPolicy::SingleOccurrence, ModelStateId("short"))}));
  REQUIRE(child->set_component_model_state(ComponentInstanceId("base"), ModelStateId("short")));
  REQUIRE(p.replace_assembly_hole_features(
      {a::feature(true, AssemblyHoleDefinitionPolicy::SingleOccurrence, ModelStateId("short"))}));
  auto suppressed = a::project();
  auto* cover = suppressed.find_part_document(DocumentId("cover.part"));
  REQUIRE(cover);
  REQUIRE(
      cover->apply_history_intent({{"base", "layout.hole", "layout.pattern"}, {"layout.pattern"}}));
  CHECK_FALSE(suppressed.replace_assembly_hole_features({a::feature()}));
  REQUIRE(
      cover->apply_history_intent({{"base", "layout.hole", "layout.pattern"}, {"layout.hole"}}));
  CHECK_FALSE(suppressed.replace_assembly_hole_features({a::feature()}));
}
TEST_CASE("Assembly Hole dependency cycles include coordinated and adaptive edges",
          "[core][assembly-hole][parity-b196][A03]") {
  auto p = a::project();
  auto* base = p.find_part_document(DocumentId("base.part"));
  REQUIRE(base);
  REQUIRE(base->add_hole_feature(a::threaded()));
  const auto feature = a::feature();
  auto reverse_recipe = a::clearance();
  reverse_recipe = h::checked(HoleFeature::create(
      FeatureId("reverse.hole"), "Reverse", reverse_recipe.kind(), reverse_recipe.target_body(),
      reverse_recipe.support(), reverse_recipe.placements(), reverse_recipe.diameter(),
      reverse_recipe.extent(), reverse_recipe.direction(), reverse_recipe.tip()));
  auto reverse = h::checked(AssemblyHoleFeature::create(
      FeatureId("reverse"), "Reverse dependency",
      {a::occurrence("base", "base.part"), primary_model_state_id(), FeatureId("mounting.hole"),
       std::nullopt},
      {{"cover", a::occurrence("cover", "cover.part"), primary_model_state_id(),
        AssemblyHoleParticipantRole::Clearance, AssemblyHoleDefinitionPolicy::SingleOccurrence,
        reverse_recipe}}));
  const auto before = saved(p);
  CHECK_FALSE(p.replace_assembly_hole_features({feature, reverse}));
  CHECK(saved(p) == before);
  auto* cover = p.find_part_document(DocumentId("cover.part"));
  REQUIRE(cover);
  REQUIRE(cover->add_construction_plane(h::checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("adaptive.plane"), "Adaptive plane",
                                         {}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  auto reference = h::checked(ProjectDesignReference::create(
      ProjectDesignReferenceId("base-to-cover"), "Base drives cover",
      a::occurrence("base", "base.part"), primary_model_state_id(),
      ProjectDesignSemanticSource{"base.top"}, a::occurrence("cover", "cover.part"),
      ConstructionPlaneId("adaptive.plane")));
  REQUIRE(p.set_design_reference(reference));
  CHECK_FALSE(p.replace_assembly_hole_features({feature}));
}
TEST_CASE(
    "Project V5 Assembly Hole records reject malformed versions recipes pins and foreign contexts",
    "[core][assembly-hole][parity-b196][A03]") {
  auto p = a::project();
  REQUIRE(p.replace_assembly_hole_features({a::feature()}));
  auto root = nlohmann::json::parse(saved(p));
  for (int mutation = 0; mutation < 12; ++mutation) {
    auto value = root;
    auto& record = value["assembly_hole_features"][0];
    if (mutation == 0)
      value["version"] = 4;
    if (mutation == 1)
      record["version"] = 2;
    if (mutation == 2)
      record["version"] = 1.0;
    if (mutation == 3)
      record["participants"].push_back(record["participants"][0]);
    if (mutation == 4)
      record["participants"][1]["definition_policy"] = "guess";
    if (mutation == 5)
      record["participants"][1]["state"] = "missing";
    if (mutation == 6)
      record["layout"]["occurrence"]["path"] = {"missing"};
    if (mutation == 7)
      record["participants"][1]["recipe"]["unknown"] = true;
    if (mutation == 8)
      record["layout"]["world_axes"] = {0, 0, 1};
    if (mutation == 9)
      value["assembly_hole_features"] = nlohmann::json::array();
    if (mutation == 10)
      record["participants"][0]["role"] = "threaded";
    if (mutation == 11)
      record["participants"][1]["recipe"]["diameter"]["table"]["revision"] = "missing";
    CAPTURE(mutation);
    CHECK_FALSE(deserialize_project_from_json(value.dump()));
  }
  CHECK(saved(h::checked(deserialize_project_from_json(root.dump()))) == saved(p));
}
