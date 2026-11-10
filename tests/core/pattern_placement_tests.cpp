#include "../support/hole_pattern_fixture.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace blcad;
namespace f = blcad::test::hole_pattern_fixture;
using f::checked;
using json = nlohmann::json;
namespace {
void count(PartDocument& p, const char* id, unsigned n) {
  REQUIRE(p.set_parameter_value(ParameterId(id), checked(Quantity::count(n))));
}
} // namespace

TEST_CASE("Pattern coordinates retain exclusions across independent direction count edits",
          "[core][parity-b156][pattern-placement]") {
  auto p = f::part();
  auto options = f::options();
  options.skipped = {{2, 1}, {1, 1}};
  options.second_direction->sign = PatternDirectionSign::Negative;
  const auto feature = f::linear(options);
  REQUIRE(p.add_linear_pattern_feature(feature));
  auto values = checked(evaluate_pattern_placements(p, feature));
  REQUIRE(values.size() == 6);
  CHECK(values[3].index == PatternOccurrenceIndex{1, 1});
  CHECK(values[3].first_distance_mm == 12);
  CHECK(values[3].second_distance_mm == -15);
  CHECK(values[3].skipped);
  CHECK(pattern_occurrence_key(values[3].index) == "1/1");
  CHECK(values[4].index == PatternOccurrenceIndex{2, 0});
  count(p, "count", 1);
  count(p, "rows", 1);
  values = checked(evaluate_pattern_placements(p, feature));
  REQUIRE(values.size() == 1);
  CHECK_FALSE(values.front().skipped);
  CHECK(values.front().first_distance_mm == 0);
  CHECK(feature.placements()->skipped.size() == 2);
  count(p, "count", 3);
  count(p, "rows", 2);
  values = checked(evaluate_pattern_placements(p, feature));
  CHECK(values[3].index == PatternOccurrenceIndex{1, 1});
  CHECK(values[3].skipped);
  CHECK(values[5].skipped);
  const auto extent = checked(LinearPatternFeature::create(
      FeatureId("single"), "Single", {f::source()}, f::axis("axis.x"), ParameterId("count"),
      LinearPatternExtentMode::TotalExtent, ParameterId("spacing.x"),
      PatternDirectionSign::Positive, f::context(), f::options(false)));
  count(p, "count", 1);
  CHECK(checked(evaluate_pattern_placements(p, extent)).front().first_distance_mm == 0);
}

TEST_CASE("Pattern circular positions distinguish full partial and single-occurrence ranges",
          "[core][parity-b156][pattern-placement]") {
  auto p = f::part();
  count(p, "count", 5);
  auto options = f::options(false);
  options.skipped = {{3, 0}};
  auto full = checked(evaluate_pattern_placements(p, f::circular(360, options)));
  REQUIRE(full.size() == 5);
  CHECK(full.back().angle_deg == 288);
  CHECK(full[3].skipped);
  auto partial = checked(evaluate_pattern_placements(p, f::circular(120)));
  CHECK(partial[1].angle_deg == 30);
  CHECK(partial.back().angle_deg == 120);
  count(p, "count", 1);
  partial = checked(evaluate_pattern_placements(p, f::circular(120, options)));
  REQUIRE(partial.size() == 1);
  CHECK(partial.front().angle_deg == 0);
  count(p, "count", 2);
  CHECK(checked(evaluate_pattern_placements(p, f::circular(120))).back().angle_deg == 120);
}

TEST_CASE("Pattern placement Core validates cost and dependencies before publishing candidates",
          "[core][parity-b156][pattern-placement]") {
  auto p = f::part();
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  const auto saved = checked(serialize_part_document_to_json(p));
  for (const auto* id : {"axis.y", "rows", "spacing.y"})
    CHECK(p.dependency_graph().has_dependency(id, "pattern"));
  p.mark_all_clean();
  REQUIRE(p.set_parameter_value(ParameterId("spacing.y"), checked(Quantity::length_mm(20))));
  auto plan = checked(p.create_recompute_plan());
  CHECK(plan.contains("pattern"));
  CHECK(plan.contains("body:body.base"));
  auto wrong = f::options();
  wrong.second_direction->count = ParameterId("width");
  auto candidate = checked(deserialize_part_document_from_json(saved));
  CHECK_FALSE(candidate.update_linear_pattern_feature(f::linear(wrong)));
  CHECK(checked(serialize_part_document_to_json(candidate)) == saved);
  count(candidate, "count", 1024);
  CHECK_FALSE(evaluate_pattern_placements(candidate, f::linear()));
  count(candidate, "count", 1);
  count(candidate, "rows", 1);
  REQUIRE(evaluate_pattern_placements(candidate, f::linear()));
  auto overflow = candidate;
  count(overflow, "count", 1024);
  REQUIRE(
      overflow.set_parameter_value(ParameterId("spacing.x"), checked(Quantity::length_mm(1e308))));
  CHECK_FALSE(evaluate_pattern_placements(overflow, f::linear()));
}

TEST_CASE("Pattern V7 roundtrip rejects downgraded malformed and ambiguous occurrence intent",
          "[core][parity-b156][pattern-placement][part-json]") {
  auto p = f::part(true);
  auto options = f::options();
  options.skipped = {{2, 1}};
  REQUIRE(p.add_linear_pattern_feature(f::linear(options)));
  const auto saved = checked(serialize_part_document_to_json(p));
  const auto original = json::parse(saved);
  CHECK(original.at("version") == 7);
  CHECK(checked(serialize_part_document_to_json(
            checked(deserialize_part_document_from_json(saved)))) == saved);
  for (int version = 1; version <= 6; ++version) {
    auto changed = original;
    changed["version"] = version;
    CHECK_FALSE(deserialize_part_document_from_json(changed.dump()));
  }
  for (int mode = 0; mode < 11; ++mode) {
    CAPTURE(mode);
    auto changed = original;
    auto& pattern = changed["part_patterns"][0];
    auto& layout = pattern["placements"];
    if (mode == 0)
      layout["skipped"][0]["first"] = -1;
    if (mode == 1)
      layout["skipped"][0]["first"] = 1.5;
    if (mode == 2)
      layout["skipped"][0]["first"] = std::numeric_limits<std::uint64_t>::max();
    if (mode == 3)
      layout["skipped"].push_back(layout["skipped"][0]);
    if (mode == 4)
      layout["skipped"] = {{{"first", 0}, {"second", 0}}};
    if (mode == 5)
      layout["unknown"] = true;
    if (mode == 6)
      layout["second_direction"]["extent"]["mode"] = "guess";
    if (mode == 7)
      layout["second_direction"]["count_parameter"] = "width";
    if (mode == 8)
      pattern["surprise"] = true;
    if (mode == 9)
      pattern.erase("placements");
    if (mode == 10)
      layout["second_direction"]["extent"]["unknown"] = 0;
    CHECK_FALSE(deserialize_part_document_from_json(changed.dump()));
  }
}

TEST_CASE("Hole endpoints retain placement and occurrence identity without topology ordinals",
          "[core][parity-b156][pattern-placement][hole-instance-reference]") {
  const auto ref = checked(HoleInstanceReference::create(
      FeatureId("holes./:%ö"), HolePlacementId("first./:%"), HoleReferenceRole::Step,
      HolePatternOccurrence{FeatureId("pattern./:%"), {2, 1}}, "step./:%"));
  const auto text = checked(make_hole_target_spelling(ref));
  CHECK(text.find('.') == std::string::npos);
  CHECK(checked(parse_hole_target_spelling(text)) == ref);
  for (const auto* bad :
       {"hole:a:b:axis:::01:0", "hole:a:b:axis:::0:1", "hole:a:b:axis::p:-1:0",
        "hole:a:b:axis::p:1024:0", "hole:%61:b:axis:::0:0", "hole:a:b:seat:step::0:0"})
    CHECK_FALSE(parse_hole_target_spelling(bad));
  auto p = f::part();
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  auto target = checked(HoleInstanceReference::create(
      FeatureId("holes"), HolePlacementId("first"), HoleReferenceRole::Seat,
      HolePatternOccurrence{FeatureId("pattern"), {2, 1}}));
  CHECK(validate_hole_instance_reference(p, target));
  count(p, "count", 2);
  CHECK_FALSE(validate_hole_instance_reference(p, target));
  count(p, "count", 3);
  CHECK(validate_hole_instance_reference(p, target));
  auto options = f::options();
  options.skipped = {{2, 1}};
  REQUIRE(p.update_linear_pattern_feature(f::linear(options)));
  CHECK_FALSE(validate_hole_instance_reference(p, target));
}

TEST_CASE("Pattern placement snapshots preserve shared Project ownership and legacy bytes",
          "[core][parity-b156][pattern-placement][project-json]") {
  auto p = f::part();
  const auto legacy = checked(serialize_part_document_to_json(p));
  CHECK(json::parse(legacy).at("version") == 5);
  CHECK(checked(serialize_part_document_to_json(
            checked(deserialize_part_document_from_json(legacy)))) == legacy);
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  auto project =
      checked(Project::create(DocumentId("project"), "Project",
                              checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  REQUIRE(project.add_part_document(p));
  const auto offline = checked(serialize_project_to_json(project));
  CHECK(json::parse(offline).at("version") == 1);
  CHECK(checked(serialize_project_to_json(checked(deserialize_project_from_json(offline)))) ==
        offline);
  REQUIRE(project.add_engineering_resource(blcad::test::hole_fixture::table()));
  const auto shared = checked(serialize_project_to_json(project));
  const auto envelope = json::parse(shared);
  CHECK(envelope.at("version") == 2);
  CHECK(envelope.at("parts")[0].at("engineering_resource_refs").empty());
  CHECK(checked(serialize_project_to_json(checked(deserialize_project_from_json(shared)))) ==
        shared);
}

TEST_CASE("Resource-free general Pattern V7 uses the same ordered replay and Project envelope",
          "[core][parity-b156][pattern-placement][project-json]") {
  auto p = blcad::test::history_fixture::native_base();
  const auto context = blcad::test::history_fixture::new_body(p, "copy");
  REQUIRE(p.add_linear_pattern_feature(checked(LinearPatternFeature::create(
      FeatureId("pattern"), "Copy", {f::source("base")}, f::axis("axis"), ParameterId("count"),
      LinearPatternExtentMode::TotalExtent, ParameterId("depth"), PatternDirectionSign::Positive,
      context, PatternPlacementOptions{}))));
  const auto saved = checked(serialize_part_document_to_json(p));
  CHECK(json::parse(saved).at("version") == 7);
  CHECK(checked(serialize_part_document_to_json(
            checked(deserialize_part_document_from_json(saved)))) == saved);
  auto project =
      checked(Project::create(DocumentId("project"), "Project",
                              checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  REQUIRE(project.add_part_document(p));
  REQUIRE(project.add_engineering_resource(blcad::test::hole_fixture::table()));
  const auto shared = checked(serialize_project_to_json(project));
  CHECK(json::parse(shared).at("parts")[0].at("engineering_resource_refs").empty());
  CHECK(checked(serialize_project_to_json(checked(deserialize_project_from_json(shared)))) ==
        shared);
}

TEST_CASE("Hole Pattern intent rejects mixed operations and ambiguous exclusion coordinates",
          "[core][parity-b156][pattern-placement]") {
  auto p = f::part();
  const auto saved = checked(serialize_part_document_to_json(p));
  const auto seed = f::source();
  const auto create = [&](const PatternPlacementOptions& options,
                          std::vector<PatternSourceReference> sources,
                          FeatureBodyResultContext context) {
    return LinearPatternFeature::create(
        FeatureId("pattern"), "Pattern", std::move(sources), f::axis("axis.x"),
        ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("spacing.x"),
        PatternDirectionSign::Positive, std::move(context), options);
  };
  for (int mode = 0; mode < 6; ++mode) {
    auto options = f::options();
    if (mode == 0)
      options.skipped = {{0, 0}};
    if (mode == 1)
      options.skipped = {{1, 0}, {1, 0}};
    if (mode == 2)
      options.skipped = {{1024, 0}};
    if (mode == 3) {
      options.second_direction.reset();
      options.skipped = {{1, 1}};
    }
    if (mode == 4)
      options.second_direction->sign = static_cast<PatternDirectionSign>(77);
    if (mode == 5)
      options.second_direction->extent_mode = static_cast<LinearPatternExtentMode>(77);
    CHECK_FALSE(create(options, {seed}, f::context()));
  }
  auto mixed = checked(create(f::options(), {seed, f::source("base")}, f::context()));
  CHECK_FALSE(p.add_linear_pattern_feature(mixed));
  auto join = checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::Join,
                                                       BodyId("body.base"), std::nullopt));
  CHECK_FALSE(p.add_linear_pattern_feature(checked(create(f::options(), {seed}, join))));
  CHECK(checked(serialize_part_document_to_json(p)) == saved);
  auto options = f::options();
  CHECK_FALSE(CircularPatternFeature::create(FeatureId("pattern"), "Pattern", {seed},
                                             f::axis("axis"), ParameterId("count"), 360, true,
                                             f::context(), options));
  CHECK_FALSE(LinearPatternFeature::create(
      FeatureId("bad"), "Bad", {seed}, f::axis("axis.x"), ParameterId("count"),
      static_cast<LinearPatternExtentMode>(77), ParameterId("spacing.x"),
      PatternDirectionSign::Positive, f::context(), options));
  CHECK_FALSE(LinearPatternFeature::create(
      FeatureId("bad"), "Bad", {seed}, f::axis("axis.x"), ParameterId("count"),
      LinearPatternExtentMode::Spacing, ParameterId("spacing.x"),
      static_cast<PatternDirectionSign>(77), f::context(), options));
}

TEST_CASE(
    "Legacy large Pattern snapshots remain readable and nested Hole operations reject atomically",
    "[core][parity-b156][pattern-placement]") {
  auto legacy = blcad::test::history_fixture::native_base();
  count(legacy, "count", 2048);
  const auto context = blcad::test::history_fixture::new_body(legacy, "copies");
  REQUIRE(legacy.add_linear_pattern_feature(checked(LinearPatternFeature::create(
      FeatureId("large"), "Legacy large Pattern", {f::source("base")}, f::axis("axis"),
      ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("depth"),
      PatternDirectionSign::Positive, context))));
  const auto saved = checked(serialize_part_document_to_json(legacy));
  CHECK(json::parse(saved).at("version") == 1);
  CHECK(checked(serialize_part_document_to_json(
            checked(deserialize_part_document_from_json(saved)))) == saved);
  auto p = f::part();
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  const auto before = checked(serialize_part_document_to_json(p));
  const auto nested = checked(LinearPatternFeature::create(
      FeatureId("nested"), "Nested negative operation", {f::source("pattern")}, f::axis("axis.x"),
      ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("spacing.x"),
      PatternDirectionSign::Positive, f::context(), PatternPlacementOptions{}));
  CHECK_FALSE(p.add_linear_pattern_feature(nested));
  CHECK(checked(serialize_part_document_to_json(p)) == before);
}
