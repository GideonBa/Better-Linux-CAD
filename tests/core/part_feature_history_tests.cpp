#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/part_feature_json_commands.hpp"
#include "blcad/core/sketch_driven_intent.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

using namespace blcad;
namespace fixture = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
std::string legacy(const std::string& name) {
  std::ifstream input(std::string(BLCAD_HISTORY_FIXTURES) + "/legacy-" + name + ".json");
  REQUIRE(input.good());
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::vector<std::string> ids(const PartDocument& part) {
  std::vector<std::string> result;
  for (const auto& entry : checked(part.executable_history()))
    result.push_back(entry.node_id);
  return result;
}
std::vector<std::pair<std::string, InvalidationStatus>> invalidation(const PartDocument& part) {
  std::vector<std::pair<std::string, InvalidationStatus>> result;
  for (const auto& entry : part.invalidation_state().entries())
    result.emplace_back(entry.node_id, entry.status);
  return result;
}
} // namespace

TEST_CASE("Executable history inventory covers all typed families without mutating the Part",
          "[core][parity-b146][feature-history-inventory]") {
  const auto part = fixture::all_families();
  const auto before = checked(serialize_part_document_to_json(part));
  const auto nodes = part.dependency_graph().nodes();
  const auto edges = part.dependency_graph().dependencies();
  const auto statuses = invalidation(part);
  const auto plan = checked(part.create_recompute_plan()).node_ids();
  const auto history = checked(part.executable_history());
  const std::map<std::string, PartFeatureFamily> expected{
      {"imported", PartFeatureFamily::ImportedBody},
      {"base", PartFeatureFamily::Extrude},
      {"revolve", PartFeatureFamily::Revolve},
      {"sweep", PartFeatureFamily::Sweep},
      {"loft", PartFeatureFamily::Loft},
      {"surface", PartFeatureFamily::Surface},
      {"linear", PartFeatureFamily::LinearPattern},
      {"circular", PartFeatureFamily::CircularPattern},
      {"mirror", PartFeatureFamily::Mirror},
      {"fillet", PartFeatureFamily::Fillet},
      {"chamfer", PartFeatureFamily::Chamfer},
      {"shell", PartFeatureFamily::Shell},
      {"draft", PartFeatureFamily::Draft},
      {"boolean", PartFeatureFamily::BodyBoolean},
      {"translate", PartFeatureFamily::BodyTransform}};
  std::map<std::string, PartFeatureFamily> actual;
  for (const auto& entry : history)
    REQUIRE(actual.emplace(entry.node_id, entry.family).second);
  CHECK(actual == expected);
  CHECK(history.size() == 15);
  CHECK(ids(part) == nlohmann::json::parse(legacy("all-families"))["feature_order"]
                         .get<std::vector<std::string>>());
  for (std::size_t i = 0; i < history.size(); ++i)
    for (const auto& dependent : part.dependency_graph().transitive_dependents(history[i].node_id))
      for (std::size_t j = 0; j < history.size(); ++j)
        if (history[j].node_id == dependent)
          CHECK(i < j);
  CHECK(checked(part.executable_history()) == history);
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK(part.dependency_graph().nodes() == nodes);
  CHECK(part.dependency_graph().dependencies() == edges);
  CHECK(invalidation(part) == statuses);
  CHECK(checked(part.create_recompute_plan()).node_ids() == plan);
}

TEST_CASE("Executable history preserves canonical files captured before the history refactor",
          "[core][parity-b146][feature-history-inventory][feature-order]") {
  const std::string name = GENERATE("all-families", "base", "mixed-chain", "reference");
  CAPTURE(name);
  const auto original = legacy(name);
  const auto part = checked(deserialize_part_document_from_json(original));
  CHECK(checked(serialize_part_document_to_json(part)) == original);
  const auto authored = name == "all-families"  ? fixture::all_families()
                        : name == "base"        ? fixture::base()
                        : name == "mixed-chain" ? fixture::mixed_chain()
                                                : fixture::imported(true);
  CHECK(checked(serialize_part_document_to_json(authored)) == original);
  CHECK(ids(authored) == ids(part));
}

TEST_CASE("Executable history excludes resources and immutable reference import outputs",
          "[core][parity-b146][feature-history-inventory]") {
  auto empty = checked(PartDocument::create(DocumentId("empty"), "Empty"));
  CHECK(ids(empty).empty());
  REQUIRE(empty.add_datum_plane(checked(DatumPlane::xy())));
  REQUIRE(empty.add_body(checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  CHECK(ids(empty).empty());
  const auto reference = fixture::imported(true);
  CHECK(reference.body_count() == 1);
  CHECK(ids(reference).empty());
  CHECK(ids(fixture::imported()) == std::vector<std::string>{"imported"});
}

TEST_CASE("Shared history replay keeps interleaved predecessors and nonfeature consumers",
          "[core][parity-b146][feature-history-inventory][feature-order]") {
  const auto original = legacy("mixed-chain");
  const auto part = checked(deserialize_part_document_from_json(original));
  CHECK(ids(part) ==
        std::vector<std::string>{"imported", "base", "tool", "before", "boolean", "after"});
  CHECK(part.dependency_graph().has_dependency("base", "before"));
  CHECK(part.dependency_graph().has_dependency("before", "boolean"));
  CHECK(part.dependency_graph().has_dependency("boolean", "after"));
  CHECK(part.dependency_graph().has_dependency("base", "consumer.plane"));
  CHECK(part.dependency_graph().has_dependency("consumer.plane", "consumer.sketch"));
  REQUIRE(part.invalidation_state().has_node("consumer.plane"));
  REQUIRE(part.invalidation_state().has_node("consumer.sketch"));
  CHECK_FALSE(part.dependency_graph().has_cycle());

  // Replay order and family-array order have distinct existing contracts.
  auto root = nlohmann::json::parse(original);
  std::reverse(root["features"].begin(), root["features"].end());
  std::reverse(root["body_transforms"].begin(), root["body_transforms"].end());
  const auto reversed = checked(deserialize_part_document_from_json(root.dump()));
  REQUIRE(reversed.features().size() == 2);
  REQUIRE(reversed.body_transforms().size() == 2);
  CHECK(reversed.features()[0].id().value() == "tool");
  CHECK(reversed.features()[1].id().value() == "base");
  CHECK(reversed.body_transforms()[0].id().value() == "after");
  CHECK(reversed.body_transforms()[1].id().value() == "before");
  CHECK(reversed.dependency_graph().has_dependency("before", "boolean"));
  CHECK(reversed.dependency_graph().has_dependency("boolean", "after"));
  CHECK(reversed.dependency_graph().has_dependency("base", "consumer.plane"));
}

TEST_CASE("Shared history replay rejects incomplete corrupt and backward producer orders",
          "[core][parity-b146][feature-history-inventory][feature-order]") {
  auto root = nlohmann::json::parse(legacy("all-families"));
  bool invalid_metadata = true;
  SECTION("missing executable") {
    root["feature_order"].erase(0);
  }
  SECTION("duplicate executable") {
    root["feature_order"][0] = root["feature_order"][1];
  }
  SECTION("unknown executable") {
    root["feature_order"][0] = "missing";
  }
  SECTION("resource node is not an executable") {
    root["feature_order"][0] = "profile";
  }
  SECTION("body alias is not an executable") {
    root["feature_order"][0] = "body:body.base";
  }
  SECTION("nonstring element") {
    root["feature_order"][0] = 42;
  }
  SECTION("nonarray order") {
    root["feature_order"] = nlohmann::json::object();
  }
  SECTION("producer after consumer") {
    invalid_metadata = false;
    auto& order = root["feature_order"];
    std::swap(order[1], order.back());
  }
  const auto rejected = deserialize_part_document_from_json(root.dump());
  REQUIRE_FALSE(rejected);
  CHECK_FALSE(rejected.error().message().empty());
  if (invalid_metadata) {
    CHECK(rejected.error().category() == ErrorCategory::Validation);
    CHECK(rejected.error().object_id() == "part_document_json");
  }
  // A rejected load does not poison the next valid load.
  const auto valid = legacy("all-families");
  CHECK(checked(serialize_part_document_to_json(
            checked(deserialize_part_document_from_json(valid)))) == valid);
}

TEST_CASE("Canonical Pattern and Mirror record updates rebind later Direct Edit and Emboss sources",
          "[core][feature-update-command][generic-source-rebind][parity-b187]") {
  const auto family =
      GENERATE(std::string("linear"), std::string("circular"), std::string("mirror"));
  CAPTURE(family);
  auto part = fixture::native_base();
  for (const auto& [id, value] : {std::pair{"direct.distance", 1.}, {"label.depth", .5}})
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  REQUIRE(part.add_direct_edit_feature(checked(DirectEditFeature::create(
      FeatureId("direct"), "Direct top", BodyId("body.base"), FeatureId("base"),
      {{fixture::face(PartFeatureInputRole::SurfaceSourceFace), DirectEditFaceCapability::Planar}},
      DirectFaceOffset{ParameterId("direct.distance")}))));
  auto sketch =
      checked(Sketch::create(SketchId("label.sketch"), "Label", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("label.profile"), ParameterId("radius"), ParameterId("radius")))));
  REQUIRE(part.add_sketch(sketch));
  const auto face = checked(ResultFaceReference::create(FeatureId("direct"), BodyId("body.base"),
                                                        {{FeatureId("direct"), "face/0"}}));
  const EmbossIntent label{
      checked(ProfileRegionReference::create(sketch.id(), ProfileId("label.profile"))),
      {checked(FaceReference::create_result(PartFeatureInputRole::SurfaceSourceFace, face))},
      EmbossOperation::Raise,
      ParameterId("label.depth"),
      EmbossMapping::Planar,
      {},
      0,
      {}};
  REQUIRE(part.add_sketch_driven_feature(checked(SketchDrivenFeature::create(
      FeatureId("label"), "Label", BodyId("body.base"), SketchDrivenDefinition{label}))));
  const auto context = fixture::new_body(part, "pattern");
  const auto source = checked(PatternSourceReference::feature(FeatureId("base")));
  if (family == "linear")
    REQUIRE(part.add_linear_pattern_feature(checked(LinearPatternFeature::create(
        FeatureId("source.pattern"), "Source pattern", {source},
        fixture::axis(PartFeatureInputRole::PatternAxis), ParameterId("count"),
        LinearPatternExtentMode::Spacing, ParameterId("depth"), PatternDirectionSign::Positive,
        context))));
  else if (family == "circular")
    REQUIRE(part.add_circular_pattern_feature(checked(
        CircularPatternFeature::create(FeatureId("source.pattern"), "Source pattern", {source},
                                       fixture::axis(PartFeatureInputRole::PatternAxis),
                                       ParameterId("count"), 360, true, context))));
  else
    REQUIRE(part.add_mirror_feature(checked(
        MirrorFeature::create(FeatureId("source.pattern"), "Source mirror",
                              {checked(MirrorSourceReference::feature(FeatureId("base")))},
                              fixture::plane(PartFeatureInputRole::MirrorPlane), context))));
  const auto original = checked(serialize_part_document_to_json(part));
  const auto root = nlohmann::json::parse(original);
  const std::string collection = family == "mirror" ? "mirror_features" : "part_patterns";
  const auto replace = [&](const PartDocument& document, const nlohmann::json& record) {
    return family == "mirror" ? replace_part_mirror_from_json(document, record.dump())
                              : replace_part_pattern_from_json(document, record.dump());
  };
  CHECK(ids(part) == std::vector<std::string>{"base", "direct", "source.pattern", "label"});
  for (const char* target : {"direct", "label"}) {
    CAPTURE(target);
    auto record = root.at(collection).front();
    record["sources"][0]["feature"] = target;
    const auto updated =
        checked(replace(checked(deserialize_part_document_from_json(original)), record));
    CHECK(updated.dependency_graph().has_dependency(target, "source.pattern"));
    CHECK_FALSE(updated.dependency_graph().has_dependency("base", "source.pattern"));
    const auto encoded = checked(serialize_part_document_to_json(updated));
    CHECK(checked(serialize_part_document_to_json(
              checked(deserialize_part_document_from_json(encoded)))) == encoded);
    CHECK(checked(serialize_part_document_to_json(part)) == original);
    if (std::string(target) == "label") {
      CHECK(ids(updated) == std::vector<std::string>{"base", "direct", "label", "source.pattern"});
      auto corrupt = root;
      corrupt[collection][0] = record;
      CHECK_FALSE(deserialize_part_document_from_json(corrupt.dump()));
      auto authored = part;
      REQUIRE(authored.apply_history_intent({ids(authored), {}}));
      const auto authored_before = checked(serialize_part_document_to_json(authored));
      CHECK_FALSE(replace(authored, record));
      CHECK(checked(serialize_part_document_to_json(authored)) == authored_before);
    }
  }
  for (const auto& target :
       {nlohmann::json("source.pattern"), nlohmann::json("missing"), nlohmann::json(42)}) {
    auto record = root.at(collection).front();
    record["sources"][0]["feature"] = target;
    CHECK_FALSE(replace(part, record));
    CHECK(checked(serialize_part_document_to_json(part)) == original);
  }
  CHECK_FALSE((family == "mirror" ? replace_part_mirror_from_json(part, "{")
                                  : replace_part_pattern_from_json(part, "{")));
}
