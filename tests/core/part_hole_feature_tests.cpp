#include "../support/hole_feature_fixture.hpp"
#include "blcad/core/project_json.hpp"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
namespace fixture = blcad::test::hole_fixture;
namespace history = blcad::test::history_fixture;
using fixture::checked;
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

TEST_CASE("Hole records use the shared modifier graph history and executable lifetime",
          "[core][parity-b152][part-hole][hole-history]") {
  auto p = history::native_base();
  const auto generation = p.executable_generation();
  REQUIRE(p.add_hole_feature(fixture::hole()));
  REQUIRE(p.hole_feature_count() == 1);
  REQUIRE(p.find_hole_feature(FeatureId("hole")));
  CHECK(p.executable_generation() > generation);
  CHECK(p.dependency_graph().has_dependency("base", "hole"));
  CHECK(p.dependency_graph().has_dependency("radius", "hole"));
  CHECK(p.dependency_graph().has_dependency("hole", "body:body.base"));
  CHECK_FALSE(p.dependency_graph().has_dependency("base", "body:body.base"));
  const auto rows = checked(p.executable_history());
  REQUIRE(rows.size() == 2);
  CHECK(rows[1].family == PartFeatureFamily::Hole);
  REQUIRE(p.apply_history_intent({{"base", "hole"}, {"hole"}}));
  const auto suppressed = checked(p.create_history_evaluation_plan());
  REQUIRE(suppressed.complete());
  CHECK(suppressed.entries[1].state == PartHistoryExecutionState::Suppressed);
  REQUIRE(p.apply_history_intent({{"base", "hole"}, {}}));
  const auto prefix = checked(p.create_history_evaluation_plan(1));
  CHECK_FALSE(prefix.complete());
  CHECK(prefix.entries[1].state == PartHistoryExecutionState::OutsidePrefix);
  const auto before = text(p);
  const auto before_graph = graph(p);
  const auto before_generation = p.executable_generation();
  CHECK_FALSE(p.add_hole_feature(fixture::hole()));
  CHECK_FALSE(p.add_hole_feature(fixture::hole("base")));
  CHECK_FALSE(p.add_hole_feature(fixture::hole("invalid", ParameterId("count"))));
  CHECK_FALSE(p.apply_history_intent({{"hole", "base"}, {}}));
  CHECK(text(p) == before);
  CHECK(graph(p) == before_graph);
  CHECK(p.executable_generation() == before_generation);
}

TEST_CASE(
    "Hole updates preserve placement history position and restore the upstream result on removal",
    "[core][parity-b152][part-hole][hole-history]") {
  auto p = fixture::part("edited", true);
  REQUIRE(p.add_hole_feature(fixture::hole("later", ParameterId("radius"))));
  REQUIRE(p.update_hole_feature(fixture::hole("hole", ParameterId("width"))));
  CHECK(checked(p.executable_history())[1].node_id == "hole");
  CHECK(p.dependency_graph().has_dependency("hole", "later"));
  CHECK_FALSE(p.dependency_graph().has_node(engineering_resource_node_id(fixture::key())));
  const auto before = text(p);
  CHECK_FALSE(p.remove_hole_feature(FeatureId("hole")));
  CHECK_FALSE(p.update_hole_feature(fixture::hole("hole", ParameterId("count"))));
  CHECK(text(p) == before);
  REQUIRE(p.remove_hole_feature(FeatureId("later")));
  CHECK(p.dependency_graph().has_dependency("hole", "body:body.base"));
  REQUIRE(p.remove_hole_feature(FeatureId("hole")));
  CHECK(p.hole_features().empty());
  CHECK(p.dependency_graph().has_dependency("base", "body:body.base"));
  CHECK(checked(p.executable_history()).size() == 1);
  CHECK(json::parse(text(p)).at("version") == 3); // The unassigned table remains owned.
}

TEST_CASE("Hole strict persistence rejects malformed variants references and versions",
          "[core][parity-b152][part-hole][hole-json]") {
  const auto original = json::parse(text(fixture::part("strict", true)));
  std::vector<json> invalid;
  const auto change = [&](auto edit) {
    auto copy = original;
    edit(copy);
    invalid.push_back(std::move(copy));
  };
  change([](auto& j) { j["version"] = 4.0; });
  change([](auto& j) { j["version"] = 3; });
  change([](auto& j) { j.erase("hole_features"); });
  change([](auto& j) { j["hole_features"] = json::array(); });
  change([](auto& j) { j["history_version"] = 99; });
  change([](auto& j) { j["feature_order"] = {"base"}; });
  change([](auto& j) { j["feature_order"] = {"base", "hole", "hole"}; });
  change([](auto& j) { j["hole_features"][0]["kind"] = "unknown"; });
  change([](auto& j) { j["hole_features"][0]["direction"] = "path"; });
  change([](auto& j) { j["hole_features"][0]["extra"] = 1; });
  change([](auto& j) { j["hole_features"][0]["target_body"] = "missing"; });
  change([](auto& j) { j["hole_features"][0]["support"]["role"] = "mirror_plane"; });
  change([](auto& j) { j["hole_features"][0]["diameter"]["row"] = "missing"; });
  change([](auto& j) { j["hole_features"][0]["diameter"]["diameter_column"] = "missing"; });
  change([](auto& j) { j["hole_features"][0]["diameter"]["table"]["revision"] = "missing"; });
  change([](auto& j) { j["hole_features"][0]["diameter"]["parameter"] = "radius"; });
  change([](auto& j) { j["hole_features"][0]["placements"][0]["position"]["x"] = true; });
  change([](auto& j) { j["hole_features"][0]["placements"][0]["position"]["y"] = nullptr; });
  change([](auto& j) {
    j["hole_features"][0]["placements"].push_back(j["hole_features"][0]["placements"][0]);
  });
  change([](auto& j) { j["hole_features"][0]["extent"]["kind"] = "unknown"; });
  change([](auto& j) { j["hole_features"][0]["extent"]["depth"] = "depth"; });
  change([](auto& j) {
    j["hole_features"][0]["tip"] = {{"kind", "conical"}, {"included_angle", "angle"}};
  });
  for (const auto& value : invalid) {
    INFO(value.dump());
    CHECK_FALSE(deserialize_part_document_from_json(value.dump()));
  }
  CHECK(text(checked(deserialize_part_document_from_json(original.dump()))) == original.dump(2));
}

TEST_CASE("Standard adoption remaps every Hole atomically and retains old immutable snapshots",
          "[core][parity-b152][part-hole][hole-resources]") {
  auto p = fixture::part("revisions", true);
  REQUIRE(p.add_hole_feature(fixture::hole("later", fixture::standard("r1", "large"))));
  const auto before = text(p);
  const auto original_graph = graph(p);
  const auto generation = p.executable_generation();
  CHECK_FALSE(p.remove_engineering_resource(fixture::key()));
  CHECK_FALSE(
      p.adopt_engineering_resource_revision(fixture::key(), fixture::table("r2", 7.1, 9.1, false)));
  CHECK(text(p) == before);
  CHECK(graph(p) == original_graph);
  CHECK(p.executable_generation() == generation);
  REQUIRE(p.adopt_engineering_resource_revision(fixture::key(), fixture::table("r2", 7.1, 9.1)));
  REQUIRE(p.engineering_resources().find_table(fixture::key()));
  for (const auto& id : {FeatureId("hole"), FeatureId("later")}) {
    const auto* hole = p.find_hole_feature(id);
    REQUIRE(hole);
    CHECK(std::get<HoleStandardSelection>(hole->diameter()).table == fixture::key("r2"));
    CHECK(p.dependency_graph().has_dependency(engineering_resource_node_id(fixture::key("r2")),
                                              id.value()));
    CHECK(checked(resolve_hole_diameter(p, hole->diameter())).diameter.millimeters() ==
          Catch::Approx(id == FeatureId("hole") ? 7.1 : 9.1));
  }
  CHECK_FALSE(p.dependency_graph().has_node(engineering_resource_node_id(fixture::key())));
  CHECK(graph(checked(deserialize_part_document_from_json(text(p)))) == graph(p));
  REQUIRE(p.remove_engineering_resource(fixture::key()));
  REQUIRE(p.apply_history_intent({{"base", "hole", "later"}, {"hole", "later"}}));
  CHECK_FALSE(
      p.remove_engineering_resource(fixture::key("r2"))); // Suppression retains authored pins.
}

TEST_CASE("Project Hole resource updates affect only selected definitions and stay portable",
          "[core][parity-b152][part-hole][hole-resources][hole-json]") {
  auto project = fixture::project();
  const auto untouched = text(*project.find_part_document(DocumentId("second")));
  const auto before = checked(serialize_project_to_json(project));
  CHECK_FALSE(project.adopt_engineering_resource_revision(
      fixture::key(), fixture::table("r2", 7.1), {DocumentId("first"), DocumentId("missing")}));
  CHECK(checked(serialize_project_to_json(project)) == before);
  REQUIRE(project.adopt_engineering_resource_revision(fixture::key(), fixture::table("r2", 7.1),
                                                      {DocumentId("first")}));
  CHECK(text(*project.find_part_document(DocumentId("second"))) == untouched);
  const auto encoded = checked(serialize_project_to_json(project));
  const auto data = json::parse(encoded);
  CHECK(data.at("version") == 2);
  CHECK(data.at("parts")[0].at("version") == 4);
  CHECK(data.at("parts")[0].contains("engineering_resource_refs"));
  CHECK_FALSE(data.at("parts")[0].contains("engineering_resources"));
  auto reopened = checked(deserialize_project_from_json(encoded));
  CHECK(checked(serialize_project_to_json(reopened)) == encoded);
  CHECK(text(*reopened.find_part_document(DocumentId("second"))) == untouched);
  const auto standalone = text(*reopened.find_part_document(DocumentId("first")));
  auto extracted = checked(deserialize_part_document_from_json(standalone));
  CHECK(checked(resolve_hole_diameter(extracted, extracted.hole_features().front().diameter()))
            .diameter.millimeters() == Catch::Approx(7.1));
  CHECK(graph(extracted) == graph(*project.find_part_document(DocumentId("first"))));
}

TEST_CASE(
    "Derived resource identities cannot alias authored parameter nodes or erase unrelated intent",
    "[core][parity-b152][part-hole][hole-resources]") {
  const auto a = checked(EngineeringResourceKey::create(EngineeringResourceId("a:bc"), "d"));
  const auto b = checked(EngineeringResourceKey::create(EngineeringResourceId("a"), "bc:d"));
  CHECK(engineering_resource_node_id(a) != engineering_resource_node_id(b));
  auto p = history::native_base();
  REQUIRE(p.add_engineering_resource(fixture::table()));
  const auto node = engineering_resource_node_id(fixture::key());
  REQUIRE(p.add_parameter(checked(Parameter::create_length(ParameterId(node), "Collision witness",
                                                           checked(Quantity::length_mm(2))))));
  const auto before = text(p);
  CHECK_FALSE(p.add_hole_feature(fixture::hole("standard", fixture::standard())));
  CHECK(text(p) == before);
  REQUIRE(p.add_hole_feature(fixture::hole()));
  REQUIRE(p.update_hole_feature(fixture::hole("hole", ParameterId("width"))));
  REQUIRE(p.find_parameter(ParameterId(node)));
  CHECK(p.dependency_graph().has_node(node));
  REQUIRE(p.remove_hole_feature(FeatureId("hole")));
  CHECK(p.dependency_graph().has_node(node));
  REQUIRE(p.remove_engineering_resource(fixture::key()));
  REQUIRE(p.find_parameter(ParameterId(node)));
  CHECK(p.dependency_graph().has_node(node));
}

TEST_CASE("Every Hole extent and blind conical tip roundtrips through the shared ordered reader",
          "[core][parity-b152][part-hole][hole-json]") {
  auto seed = history::native_base();
  const auto context = history::new_body(seed, "limit");
  REQUIRE(seed.add_feature(
      checked(checked(Feature::create_additive_extrude(FeatureId("limit"), "Limit",
                                                       SketchId("profile"), ParameterId("depth")))
                  .with_body_result_context(context))));
  const auto face = checked(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      checked(SemanticFaceReference::create(FeatureId("limit"), SemanticFace::Top))));
  const std::vector<HoleExtent> extents{HoleThroughAllExtent{},
                                        HoleBlindExtent{ParameterId("depth")},
                                        HoleToNextExtent{},
                                        HoleToFaceExtent{face},
                                        HoleUpToBodyExtent{BodyId("body.limit")},
                                        HoleSymmetricExtent{ParameterId("depth")}};
  for (const auto& extent : extents) {
    auto p = seed;
    REQUIRE(p.add_hole_feature(fixture::hole("hole", ParameterId("radius"), extent)));
    const auto serialized = text(p);
    const auto copy = checked(deserialize_part_document_from_json(serialized));
    CHECK(text(copy) == serialized);
    CHECK(graph(copy) == graph(p));
    CHECK(copy.hole_features().front().extent().index() == extent.index());
    CHECK(checked(copy.create_history_evaluation_plan()).complete());
  }
  auto p = seed;
  REQUIRE(p.add_hole_feature(fixture::hole("hole", ParameterId("radius"),
                                           HoleBlindExtent{ParameterId("depth")},
                                           HoleConicalTip{ParameterId("angle")})));
  const auto copy = checked(deserialize_part_document_from_json(text(p)));
  CHECK(std::get<HoleConicalTip>(copy.hole_features().front().tip()).included_angle ==
        ParameterId("angle"));
  CHECK(text(copy) == text(p));
}

TEST_CASE(
    "Hole Sketch point placements retain their source identity across source edits and recovery",
    "[core][parity-b152][part-hole][hole-history][hole-json]") {
  auto p = history::native_base();
  const auto make_sketch = [](Point2 position, bool include_point = true) {
    auto s = checked(Sketch::create(SketchId("centers"), "Centers", DatumPlaneId("datum.xy")));
    if (include_point)
      fixture::accept(
          s.add_entity(checked(SketchPoint::create(SketchEntityId("center"), position))));
    return s;
  };
  REQUIRE(p.add_sketch(make_sketch({1, -2})));
  const auto base = fixture::hole();
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      base.id(), base.name(), base.kind(), base.target_body(), base.support(),
      {{HolePlacementId("stable"),
        HoleSketchPointReference{SketchId("centers"), SketchEntityId("center")}}},
      base.diameter(), base.extent(), base.direction(), base.tip()))));
  CHECK(p.dependency_graph().has_dependency("centers", "hole"));
  const auto hole_json = json::parse(text(p)).at("hole_features");
  REQUIRE(p.update_sketch(make_sketch({-4, 3})));
  CHECK(json::parse(text(p)).at("hole_features") == hole_json);
  const auto restored = checked(deserialize_part_document_from_json(text(p)));
  CHECK(
      restored.find_sketch(SketchId("centers"))->find_point(SketchEntityId("center"))->position() ==
      Point2{-4, 3});
  CHECK(graph(restored) == graph(p));
  REQUIRE(p.update_sketch(make_sketch({}, false)));
  CHECK_FALSE(p.validate_reference_sources());
  CHECK_FALSE(deserialize_part_document_from_json(text(p)));
  REQUIRE(p.update_sketch(make_sketch({-4, 3})));
  REQUIRE(p.validate_reference_sources());
  CHECK(text(p) == text(restored));
}

TEST_CASE("Hole version four preserves typed intent graph and legacy history modes",
          "[core][parity-b152][part-hole][hole-json]") {
  for (const bool authored : {false, true})
    for (const bool standard : {false, true}) {
      auto p = fixture::part("portable", authored, standard);
      const auto source = text(p);
      const auto value = json::parse(source);
      CHECK(value.at("version") == 4);
      CHECK(value.at("history_version") == (authored ? 2 : 1));
      CHECK(value.at("feature_order") == json::array({"base", "hole"}));
      CHECK(value.at("hole_features").size() == 1);
      CHECK(value.contains("engineering_resources"));
      auto reopened = checked(deserialize_part_document_from_json(source));
      CHECK(text(reopened) == source);
      CHECK(graph(reopened) == graph(p));
      CHECK(reopened.authored_history_intent().has_value() == authored);
      const auto* hole = reopened.find_hole_feature(FeatureId("hole"));
      REQUIRE(hole);
      CHECK(hole->placements().front().id == HolePlacementId("center"));
      CHECK(checked(resolve_hole_diameter(reopened, hole->diameter())).diameter.millimeters() ==
            Catch::Approx(standard ? 6.6 : 1));
    }
  const auto legacy = text(history::native_base());
  CHECK(json::parse(legacy).at("version") == 1);
  CHECK_FALSE(json::parse(legacy).contains("hole_features"));
  CHECK(text(checked(deserialize_part_document_from_json(legacy))) == legacy);
}
} // namespace

TEST_CASE("Projected Hole placements preserve their construction and Sketch dependencies",
          "[core][parity-b152][hole-combined-acceptance]") {
  auto p = history::native_base();
  REQUIRE(p.add_construction_point(checked(
      ConstructionPoint::create_explicit(ConstructionPointId("anchor"), "Anchor", {2, -3, 0}))));
  auto sketch =
      checked(Sketch::create(SketchId("centers"), "Projected centers", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_reference(checked(ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("projected"), ConstructionPointId("anchor")))));
  REQUIRE(p.add_sketch(std::move(sketch)));
  const auto support = checked(PlaneReference::create_datum_plane(
      PartFeatureInputRole::HoleSupportPlane, DatumPlaneId("datum.xy")));
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("hole"), "Projected Hole", HoleKind::Plain, BodyId("body.base"), support,
      {{HolePlacementId("stable"),
        HoleSketchPointReference{SketchId("centers"), SketchEntityId("projected")}}},
      ParameterId("radius"), HoleThroughAllExtent{}, HoleDirection::SupportNormal))));
  CHECK(p.dependency_graph().has_dependency("anchor", "centers"));
  CHECK(p.dependency_graph().has_dependency("centers", "hole"));
  const auto hole = json::parse(text(p)).at("hole_features");
  REQUIRE(p.update_construction_point(checked(
      ConstructionPoint::create_explicit(ConstructionPointId("anchor"), "Anchor", {4, 1, 0}))));
  REQUIRE(p.validate_reference_sources());
  CHECK(json::parse(text(p)).at("hole_features") == hole);
  const auto reopened = checked(deserialize_part_document_from_json(text(p)));
  CHECK(text(reopened) == text(p));
  CHECK(graph(reopened) == graph(p));
  const auto& placement = reopened.hole_features().front().placements().front();
  CHECK(placement.id == HolePlacementId("stable"));
  CHECK(std::get<HoleSketchPointReference>(placement.source).point == SketchEntityId("projected"));
  const auto repair =
      checked(deserialize_part_document_from_json(text(p), PartDocumentReadMode::ReferenceRepair));
  CHECK(text(repair) == text(p));
  CHECK(graph(repair) == graph(p));
}

TEST_CASE("Mixed legacy custom and standard Hole members retain portable Project formats",
          "[core][parity-b152][hole-combined-acceptance]") {
  auto p = checked(Project::create(DocumentId("mixed"), "Mixed",
                                   checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  REQUIRE(p.add_part_document(fixture::part("custom", false, false)));
  auto source = checked(serialize_project_to_json(p));
  CHECK(json::parse(source).at("version") == 1);
  CHECK(checked(serialize_project_to_json(checked(deserialize_project_from_json(source)))) ==
        source);
  REQUIRE(p.add_part_document(fixture::part("standard", true)));
  REQUIRE(p.add_part_document(history::native_base()));
  source = checked(serialize_project_to_json(p));
  const auto encoded = json::parse(source);
  CHECK(encoded.at("version") == 2);
  CHECK(encoded.at("parts")[0].at("version") == 4);
  CHECK(encoded.at("parts")[0].at("engineering_resource_refs").empty());
  CHECK(encoded.at("parts")[1].at("engineering_resource_refs").size() == 1);
  CHECK(encoded.at("parts")[2].at("version") == 1);
  const auto reopened = checked(deserialize_project_from_json(source));
  CHECK(checked(serialize_project_to_json(reopened)) == source);
  for (const auto& part : reopened.part_documents())
    CHECK(text(checked(deserialize_part_document_from_json(text(part)))) == text(part));
}

TEST_CASE("Hole result faces share typed input persistence and dependency authority",
          "[core][parity-b153][hole-result-reference]") {
  auto p = fixture::part("references", true, false);
  const auto reference = checked(ResultFaceReference::create(
      FeatureId("hole"), BodyId("body.base"), {{FeatureId("hole"), "hole/6:center/wall"}}));
  const auto face =
      checked(FaceReference::create_result(PartFeatureInputRole::DraftFace, reference));
  CHECK(face.source_kind() == PartFeatureInputSourceKind::ResultFace);
  CHECK(face.source_node_id() == "hole");
  REQUIRE(validate_part_feature_input_reference(p, face));
  CHECK_FALSE(FaceReference::create_result(PartFeatureInputRole::FilletEdge, reference));
  REQUIRE(p.add_draft_feature(checked(DraftFeature::create(
      FeatureId("draft"), "Draft bore", BodyId("body.base"), {face},
      history::axis(PartFeatureInputRole::DraftPullDirection),
      history::plane(PartFeatureInputRole::DraftNeutralPlane), ParameterId("angle")))));
  CHECK(p.dependency_graph().has_dependency("hole", "draft"));
  const auto encoded = text(p);
  const auto root = json::parse(encoded);
  CHECK(root.at("version") == 4);
  CHECK(root.at("draft_features")[0]["faces"][0]["source_kind"] == "result_face");
  CHECK(text(checked(deserialize_part_document_from_json(encoded))) == encoded);
  for (int mutation = 0; mutation != 5; ++mutation) {
    auto broken = root;
    auto& ref = broken["draft_features"][0]["faces"][0]["reference"];
    if (mutation == 0)
      ref["producer"] = "missing";
    if (mutation == 1)
      ref["body"] = "missing";
    if (mutation == 2)
      ref["origins"][0]["feature"] = "missing";
    if (mutation == 3)
      ref["origins"].push_back(ref["origins"][0]);
    if (mutation == 4)
      ref["kernel_index"] = 2;
    CHECK_FALSE(deserialize_part_document_from_json(broken.dump()));
  }
  CHECK_FALSE(p.remove_hole_feature(FeatureId("hole")));
  CHECK(text(p) == encoded);
}

TEST_CASE(
    "Atomic Extrude to Hole replacement preserves Body consumers and canonical schema envelopes",
    "[core][parity-b157][hole-family-replacement]") {
  const int mode = GENERATE(0, 1, 2);
  const bool authored = GENERATE(false, true);
  auto part = history::native_base();
  auto sketch =
      checked(Sketch::create(SketchId("legacy.sketch"), "Legacy circle", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      checked(CircleProfile::create(ProfileId("legacy.circle"), ParameterId("radius")))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  auto cut = checked(Feature::create_subtractive_extrude(
      FeatureId("legacy"), "Legacy cut", SketchId("legacy.sketch"), FeatureId("base")));
  auto context = checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                          BodyId("body.base"), std::nullopt));
  REQUIRE(part.add_feature(checked(cut.with_body_result_context(context))));
  const auto copies = history::new_body(part, "copies");
  REQUIRE(part.add_linear_pattern_feature(checked(
      LinearPatternFeature::create(FeatureId("copies"), "Body copies",
                                   {checked(PatternSourceReference::body(BodyId("body.base")))},
                                   history::axis(PartFeatureInputRole::PatternAxis),
                                   ParameterId("count"), LinearPatternExtentMode::Spacing,
                                   ParameterId("width"), PatternDirectionSign::Positive, copies))));
  if (authored)
    REQUIRE(part.apply_history_intent({{"base", "legacy", "copies"}, {}}));
  const auto length = [&](const char* id, double value) {
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  };
  HoleManufacturing manufacturing;
  std::optional<ThreadDefinition> thread;
  if (mode == 1) {
    length("seat.diameter", 3);
    length("seat.depth", 1);
    manufacturing = HoleCounterbore{ParameterId("seat.diameter"), ParameterId("seat.depth")};
  } else if (mode == 2) {
    length("thread.major", 2);
    length("thread.pitch", 1);
    REQUIRE(part.add_parameter(checked(Parameter::create_angle(
        ParameterId("thread.angle"), "Thread angle", checked(Quantity::angle_deg(60))))));
    thread = checked(ThreadDefinition::create(
        ThreadParameterProfile{"Custom", "2 x 1", "Authored", ParameterId("thread.major"),
                               ParameterId("radius"), ParameterId("thread.pitch"),
                               ParameterId("thread.angle"), std::nullopt},
        ParameterId("depth")));
  }
  auto basic = fixture::hole("legacy");
  auto hole = checked(HoleFeature::create(basic.id(), basic.name(), basic.kind(),
                                          basic.target_body(), basic.support(), basic.placements(),
                                          basic.diameter(), basic.extent(), basic.direction(),
                                          basic.tip(), manufacturing, thread));
  const auto before = text(part);
  const auto before_graph = graph(part);
  auto converted = replace_part_extrude_with_hole(part, hole);
  INFO((converted ? "ok" : converted.error().message()));
  REQUIRE(converted);
  CHECK(text(part) == before);
  CHECK(graph(part) == before_graph);
  CHECK_FALSE(converted.value().find_feature(FeatureId("legacy")));
  REQUIRE(converted.value().find_hole_feature(FeatureId("legacy")));
  CHECK_FALSE(converted.value().dependency_graph().has_dependency("legacy.sketch", "legacy"));
  CHECK(converted.value().dependency_graph().has_dependency("legacy", "body:body.base"));
  CHECK(checked(converted.value().executable_history())[1].node_id == "legacy");
  CHECK(checked(converted.value().executable_history())[2].node_id == "copies");
  CHECK(bool(converted.value().authored_history_intent()) == authored);
  CHECK(json::parse(text(converted.value())).at("version") == (mode == 2 ? 6 : mode == 1 ? 5 : 4));
  CHECK(text(checked(deserialize_part_document_from_json(text(converted.value())))) ==
        text(converted.value()));
  CHECK_FALSE(replace_part_extrude_with_hole(part, fixture::hole("base")));
  CHECK_FALSE(replace_part_extrude_with_hole(part, fixture::hole("missing")));
  CHECK_FALSE(
      replace_part_extrude_with_hole(part, fixture::hole("legacy", ParameterId("missing"))));
  CHECK(text(part) == before);
}
