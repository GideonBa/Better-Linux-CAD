#include "../support/hole_manufacturing_fixture.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
namespace fixture = blcad::test::manufacturing_fixture;
using fixture::checked;

TEST_CASE("Manufacturing Hole variants preserve strict typed intent and legacy canonical bytes",
          "[core][parity-b154][hole-manufacturing-intent]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
  auto p = fixture::part(mode);
  const auto text = checked(serialize_part_document_to_json(p));
  auto root = nlohmann::json::parse(text);
  CHECK(root["version"] == (mode == 0 ? 4 : 5));
  CHECK(checked(serialize_part_document_to_json(
            checked(deserialize_part_document_from_json(text)))) == text);
  const auto& hole = p.hole_features().front();
  CHECK(hole.placements().front().id == HolePlacementId("center"));
  if (mode != 0) {
    auto wrong = root;
    wrong["version"] = 4;
    CHECK_FALSE(deserialize_part_document_from_json(wrong.dump()));
    wrong = root;
    wrong["hole_features"][0]["manufacturing"]["kernel_face_index"] = 1;
    CHECK_FALSE(deserialize_part_document_from_json(wrong.dump()));
  } else
    CHECK_FALSE(root["hole_features"][0].contains("manufacturing"));
  if (mode == 8)
    CHECK(p.dependency_graph().has_dependency("slot", "hole"));
  auto project =
      checked(Project::create(DocumentId("manufacturing"), "Manufacturing",
                              checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  REQUIRE(project.add_part_document(p));
  const auto saved = checked(serialize_project_to_json(project));
  CHECK(checked(serialize_project_to_json(checked(deserialize_project_from_json(saved)))) == saved);
}

TEST_CASE("Manufacturing dimensions share resource pins and atomic multi-field revision adoption",
          "[core][parity-b154][hole-manufacturing-resources]") {
  const int mode = GENERATE(1, 2, 3, 4, 5, 6, 7);
  auto p = fixture::seed(true);
  REQUIRE(p.add_engineering_resource(fixture::table()));
  REQUIRE(p.add_hole_feature(fixture::feature(mode, HoleThroughAllExtent{}, true)));
  const auto key = blcad::test::hole_fixture::key();
  CHECK(p.dependency_graph().has_dependency(engineering_resource_node_id(key), "hole"));
  CHECK_FALSE(p.remove_engineering_resource(key));
  const auto saved = checked(serialize_part_document_to_json(p));
  if (mode == 1 || mode == 3 || mode == 4) {
    CHECK_FALSE(p.adopt_engineering_resource_revision(key, fixture::table("bad", true)));
    CHECK(checked(serialize_part_document_to_json(p)) == saved);
  }
  REQUIRE(p.adopt_engineering_resource_revision(key, fixture::table("r2")));
  CHECK(p.hole_features().front().standard_dependencies() ==
        std::vector{blcad::test::hole_fixture::key("r2")});
  CHECK(p.hole_features().front().placements().front().id == HolePlacementId("center"));
  CHECK(p.dependency_graph().has_dependency(
      engineering_resource_node_id(blcad::test::hole_fixture::key("r2")), "hole"));
  auto project =
      checked(Project::create(DocumentId("project"), "Project",
                              checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  REQUIRE(project.add_part_document(p));
  const auto portable = checked(serialize_project_to_json(project));
  CHECK(nlohmann::json::parse(portable)["version"] == 2);
  CHECK(checked(serialize_project_to_json(checked(deserialize_project_from_json(portable)))) ==
        portable);
}

TEST_CASE("Manufacturing mode validation rejects incompatible extents dimensions and fit limits",
          "[core][parity-b154][hole-manufacturing-invalid]") {
  auto p = fixture::seed();
  const auto old = blcad::test::hole_fixture::hole();
  CHECK_FALSE(HoleFeature::create(old.id(), old.name(), old.kind(), old.target_body(),
                                  old.support(), old.placements(), ParameterId("diameter"),
                                  HoleSymmetricExtent{ParameterId("blind")}, old.direction(),
                                  old.tip(), fixture::variant(1)));
  CHECK_FALSE(HoleFeature::create(old.id(), old.name(), old.kind(), old.target_body(),
                                  old.support(), old.placements(), ParameterId("diameter"),
                                  old.extent(), old.direction(), old.tip(), HoleStepped{{}}));
  REQUIRE(p.set_parameter_value(ParameterId("seat.diameter"), checked(Quantity::length_mm(1))));
  CHECK_FALSE(p.add_hole_feature(fixture::feature(1)));
  CHECK_FALSE(p.add_hole_feature(fixture::feature(2)));
  REQUIRE(p.set_parameter_value(ParameterId("minimum"), checked(Quantity::length_mm(3))));
  CHECK_FALSE(p.add_hole_feature(fixture::feature(5)));
}

TEST_CASE("Manufacturing angles profiles and separate revision pins fail without defaults",
          "[core][parity-b154][hole-manufacturing-resources]") {
  auto p = fixture::seed();
  REQUIRE(p.add_engineering_resource(fixture::table()));
  REQUIRE(p.add_engineering_resource(fixture::table("r2")));
  const auto source = fixture::feature(1);
  const auto a = blcad::test::hole_fixture::key();
  const auto b = blcad::test::hole_fixture::key("r2");
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      source.id(), source.name(), source.kind(), source.target_body(), source.support(),
      source.placements(), source.diameter(), source.extent(), source.direction(), source.tip(),
      HoleCounterbore{HoleStandardSelection{a, "small", "seat.diameter"},
                      HoleStandardSelection{b, "small", "seat.depth"}}))));
  CHECK(p.hole_features().front().standard_dependencies() == std::vector{a, b});
  CHECK_FALSE(p.remove_engineering_resource(a));
  CHECK_FALSE(p.remove_engineering_resource(b));
  REQUIRE(p.adopt_engineering_resource_revision(a, fixture::table("r3")));
  CHECK(p.hole_features().front().standard_dependencies() ==
        std::vector{blcad::test::hole_fixture::key("r3"), b});
  REQUIRE(p.remove_engineering_resource(a));
  REQUIRE(p.remove_hole_feature(source.id()));
  for (double angle : {0., -1., 180., 181.}) {
    REQUIRE(p.set_parameter_value(ParameterId("angle"), checked(Quantity::angle_deg(angle))));
    CHECK_FALSE(p.add_hole_feature(fixture::feature(2)));
  }
  CHECK_FALSE(resolve_hole_dimension(p, ParameterId("seat.depth"), true));
  CHECK_FALSE(resolve_hole_dimension(p, HoleStandardSelection{b, "small", "seat.depth"}, true));
  CHECK_FALSE(resolve_hole_dimension(p, HoleStandardSelection{b, "small", "angle"}));
  CHECK_FALSE(resolve_hole_dimension(p, HoleStandardSelection{b, "missing", "angle"}, true));
  REQUIRE(resolve_hole_dimension(p, HoleStandardSelection{b, "small", "angle"}, true));
  auto slot = std::get<HoleSlot>(fixture::variant(8));
  slot.profile = checked(ProfileRegionReference::create(SketchId("slot"), ProfileId("missing")));
  CHECK_FALSE(p.add_hole_feature(
      checked(HoleFeature::create(source.id(), source.name(), source.kind(), source.target_body(),
                                  source.support(), source.placements(), source.diameter(),
                                  source.extent(), source.direction(), source.tip(), slot))));
}
