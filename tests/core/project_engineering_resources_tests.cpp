#include "../support/engineering_resource_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
namespace fixture = blcad::test::engineering_fixture;
namespace history = blcad::test::history_fixture;
using json = nlohmann::json;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? std::string{} : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
Project project() {
  return checked(
      Project::create(DocumentId("project"), "Project",
                      checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"))));
}
PartDocument empty(std::string id) {
  return checked(PartDocument::create(DocumentId(id), id));
}
PartDocument member(std::string id = "first") {
  auto data = json::parse(checked(serialize_part_document_to_json(history::native_base())));
  data["document"]["id"] = id;
  auto part = checked(deserialize_part_document_from_json(data.dump()));
  fixture::annotate(part);
  return part;
}
std::string text(const Project& value) {
  return checked(serialize_project_to_json(value));
}
EngineeringMaterial new_material() {
  return checked(
      EngineeringMaterial::create(fixture::key("synthetic.new"), "New", fixture::provenance()));
}
} // namespace

TEST_CASE("Project insertion deduplicates immutable resources and preserves member pins",
          "[core][parity-b149][project-engineering-resources]") {
  auto value = project();
  auto first = member();
  auto second = member("second");
  REQUIRE(second.set_default_material(fixture::key("synthetic.material", "r2")));
  REQUIRE(value.add_part_document(first));
  REQUIRE(value.add_part_document(second));
  CHECK(value.engineering_resources().entries().size() == 3);
  CHECK(value.part_document_count() == 2);
  CHECK(value.find_part_document(first.id())->default_material() == first.default_material());
  CHECK(value.find_part_document(second.id())->default_material() == second.default_material());
  CHECK(checked(value.synchronize_engineering_resources()) == 0);
  REQUIRE(value.validate_engineering_resources());
  const auto before = text(value);
  CHECK_FALSE(value.add_part_document(first));
  CHECK(text(value) == before);
  auto copy = value;
  REQUIRE(copy.add_engineering_resource(fixture::material("r3", 7000)));
  CHECK(copy.engineering_resources().entries().size() == 4);
  CHECK(text(value) == before);
}

TEST_CASE("A late insertion conflict cannot publish any Project or resource changes",
          "[core][parity-b149][project-engineering-resources]") {
  auto value = project();
  REQUIRE(value.add_part_document(member()));
  const auto before = text(value);
  const auto resources = value.engineering_resources();
  auto conflicting = empty("conflicting");
  REQUIRE(conflicting.add_engineering_resource(new_material()));
  REQUIRE(conflicting.add_engineering_resource(fixture::material("r1", 5000)));
  CHECK_FALSE(value.add_part_document(conflicting));
  CHECK(value.engineering_resources() == resources);
  CHECK_FALSE(value.find_part_document(conflicting.id()));
  CHECK(text(value) == before);
}

TEST_CASE("Mutable member resource edits require atomic Project synchronization",
          "[core][parity-b149][project-engineering-resources]") {
  auto value = project();
  REQUIRE(value.add_part_document(member()));
  REQUIRE(value.add_part_document(empty("last")));
  const auto owned = value.engineering_resources();
  auto* first = value.find_part_document(DocumentId("first"));
  REQUIRE(first->add_engineering_resource(new_material()));
  CHECK_FALSE(value.validate_engineering_resources());
  CHECK_FALSE(serialize_project_to_json(value));
  SECTION("successful reconciliation") {
    CHECK(checked(value.synchronize_engineering_resources()) == 1);
    REQUIRE(value.validate_engineering_resources());
    CHECK(value.engineering_resources().entries().size() == 4);
    REQUIRE(serialize_project_to_json(value));
  }
  SECTION("late conflict leaves owner and members unchanged") {
    auto* last = value.find_part_document(DocumentId("last"));
    REQUIRE(last->add_engineering_resource(fixture::material("r1", 9000)));
    const auto first_before = checked(serialize_part_document_to_json(*first));
    const auto last_before = checked(serialize_part_document_to_json(*last));
    CHECK_FALSE(value.synchronize_engineering_resources());
    CHECK(value.engineering_resources() == owned);
    CHECK(checked(serialize_part_document_to_json(*first)) == first_before);
    CHECK(checked(serialize_part_document_to_json(*last)) == last_before);
  }
}

TEST_CASE("Project version 2 stores shared values once and retains portable ordered Part subsets",
          "[core][parity-b149][project-engineering-resources]") {
  auto value = project();
  auto first = member();
  const auto face = checked(ResultFaceReference::create(FeatureId("base"), BodyId("body.base"),
                                                        {{FeatureId("base"), "cap.end"}}));
  REQUIRE(first.add_result_face_workplane(
      checked(ResultFaceWorkplane::create(DatumPlaneId("support"), "Support", face))));
  history::rectangle(first, "consumer", "support");
  REQUIRE(first.apply_history_intent({{"base"}, {}}));
  auto second = empty("second");
  REQUIRE(second.add_engineering_resource(fixture::table()));
  REQUIRE(second.add_engineering_resource(fixture::material()));
  REQUIRE(second.set_default_material(fixture::key()));
  REQUIRE(value.add_part_document(first));
  REQUIRE(value.add_part_document(second));
  REQUIRE(value.add_part_document(empty("legacy")));
  REQUIRE(value.add_engineering_resource(new_material()));
  const auto before = text(value);
  const auto data = json::parse(before);
  CHECK(data.at("version") == 2);
  CHECK(data.at("engineering_resources").at("entries").size() == 4);
  CHECK(data.at("parts").at(0).at("history_version") == 2);
  CHECK(data.at("parts").at(0).at("engineering_resource_refs").size() == 3);
  CHECK_FALSE(data.at("parts").at(0).contains("engineering_resources"));
  CHECK(data.at("parts").at(1).at("engineering_resource_refs").at(0).at("id") == "synthetic.table");
  CHECK(data.at("parts").at(2).at("version") == 1);
  CHECK_FALSE(deserialize_part_document_from_json(data.at("parts").at(0).dump()));
  const auto loaded = checked(deserialize_project_from_json(before));
  CHECK(text(loaded) == before);
  CHECK(loaded.engineering_resources() == value.engineering_resources());
  CHECK(checked(serialize_part_document_to_json(*loaded.find_part_document(first.id()))) ==
        checked(serialize_part_document_to_json(first)));
  const auto standalone =
      checked(serialize_part_document_to_json(*loaded.find_part_document(second.id())));
  const auto portable = checked(deserialize_part_document_from_json(standalone));
  CHECK(portable.engineering_resources() == second.engineering_resources());
  CHECK(portable.default_material() == second.default_material());
  CHECK(portable.engineering_resources().entries().size() == 2);
}

TEST_CASE("Project resources reject malformed versions catalogs and member scopes",
          "[core][parity-b149][project-engineering-resources]") {
  auto value = project();
  REQUIRE(value.add_part_document(member()));
  auto data = json::parse(text(value));
  SECTION("floating version") {
    data["version"] = 2.0;
  }
  SECTION("overflowing version") {
    data["version"] = 4294967298ULL;
  }
  SECTION("unknown version") {
    data["version"] = 3;
  }
  SECTION("downgraded version") {
    data["version"] = 1;
  }
  SECTION("missing catalog") {
    data.erase("engineering_resources");
  }
  SECTION("null catalog") {
    data["engineering_resources"] = nullptr;
  }
  SECTION("duplicate owned key") {
    data["engineering_resources"]["entries"].push_back(data["engineering_resources"]["entries"][0]);
  }
  SECTION("missing references") {
    data["parts"][0].erase("engineering_resource_refs");
  }
  SECTION("object references") {
    data["parts"][0]["engineering_resource_refs"] = json::object();
  }
  SECTION("duplicate reference") {
    data["parts"][0]["engineering_resource_refs"].push_back(
        data["parts"][0]["engineering_resource_refs"][0]);
  }
  SECTION("unknown reference") {
    data["parts"][0]["engineering_resource_refs"][0]["revision"] = "missing";
  }
  SECTION("unknown reference field") {
    data["parts"][0]["engineering_resource_refs"][0]["extra"] = 1;
  }
  SECTION("string reference") {
    data["parts"][0]["engineering_resource_refs"][0] = "synthetic.material";
  }
  SECTION("mixed inline values") {
    data["parts"][0]["engineering_resources"] = data["engineering_resources"];
  }
  SECTION("missing assigned revision") {
    data["parts"][0]["engineering_resource_refs"].erase(0);
  }
  SECTION("downgraded member") {
    data["parts"][0]["version"] = 2;
  }
  CHECK_FALSE(deserialize_project_from_json(data.dump()));
}

TEST_CASE("Resource-free Projects retain exact legacy bytes and normalize empty owned catalogs",
          "[core][parity-b149][project-engineering-resources]") {
  auto value = project();
  REQUIRE(value.add_part_document(empty("legacy")));
  const auto legacy = text(value);
  CHECK(json::parse(legacy).at("version") == 1);
  CHECK(text(checked(deserialize_project_from_json(legacy))) == legacy);
  auto data = json::parse(legacy);
  data["version"] = 2;
  data["engineering_resources"] = {
      {"schema", "blcad.engineering_resources.mvp1"}, {"version", 1}, {"entries", json::array()}};
  data["parts"][0]["version"] = 3;
  data["parts"][0]["history_version"] = 1;
  data["parts"][0]["engineering_resource_refs"] = json::array();
  data["parts"][0]["material_assignments"] = {{"body_overrides", json::array()}};
  CHECK(text(checked(deserialize_project_from_json(data.dump()))) == legacy);
}
