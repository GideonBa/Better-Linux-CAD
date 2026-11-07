#include "../support/engineering_resource_fixture.hpp"
#include "blcad/core/engineering_resource_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
namespace fixture = blcad::test::engineering_fixture;
using json = nlohmann::json;
namespace {
template <typename T> T checked(Result<T> value) {
  INFO((value ? std::string{} : value.error().message()));
  REQUIRE(value);
  return std::move(value.value());
}
} // namespace
TEST_CASE("Resource JSON preserves every typed column and property source offline",
          "[core][parity-b149][engineering-resource-json]") {
  const auto original = fixture::catalog();
  const auto text = checked(serialize_engineering_resource_catalog_to_json(original));
  const auto loaded = checked(deserialize_engineering_resource_catalog_from_json(text));
  CHECK(loaded == original);
  CHECK(checked(serialize_engineering_resource_catalog_to_json(loaded)) == text);
  const auto data = json::parse(text);
  CHECK(data.at("schema") == "blcad.engineering_resources.mvp1");
  CHECK(data.at("version") == 1);
  CHECK(data.at("entries").at(0).at("properties").at("density").at("unit") == "kg/m3");
  CHECK(
      data.at("entries").at(2).at("rows").at(0).at("cells").at(3).at("value").is_number_integer());
  const auto* table = loaded.find_table(fixture::key("synthetic.table"));
  REQUIRE(table);
  CHECK(table->find_row("synthetic.row")->cells.size() == 9);
  EngineeringResourceCatalog incomplete;
  REQUIRE(incomplete.insert(checked(EngineeringMaterial::create(
      fixture::key("synthetic.incomplete"), "Synthetic incomplete", fixture::provenance()))));
  const auto missing = checked(deserialize_engineering_resource_catalog_from_json(
      checked(serialize_engineering_resource_catalog_to_json(incomplete))));
  REQUIRE(missing.find_material(fixture::key("synthetic.incomplete")));
  CHECK_FALSE(missing.find_material(fixture::key("synthetic.incomplete"))->properties().density);
}
TEST_CASE("Resource JSON rejects malformed versions fields units dimensions and identities",
          "[core][parity-b149][engineering-resource-json]") {
  const auto text = checked(serialize_engineering_resource_catalog_to_json(fixture::catalog()));
  auto data = json::parse(text);
  SECTION("float version") {
    data["version"] = 1.0;
  }
  SECTION("overflowing version") {
    data["version"] = 4294967297ULL;
  }
  SECTION("future version") {
    data["version"] = 2;
  }
  SECTION("unknown field") {
    data["extra"] = true;
  }
  SECTION("unknown kind") {
    data["entries"][0]["kind"] = "future";
  }
  SECTION("unknown property") {
    data["entries"][0]["properties"]["future"] = 1;
  }
  SECTION("duplicate identical key") {
    data["entries"].push_back(data["entries"][0]);
  }
  SECTION("conflicting key") {
    auto entry = data["entries"][0];
    entry["properties"]["density"]["value"] = 5000;
    data["entries"].push_back(entry);
  }
  SECTION("wrong density unit") {
    data["entries"][0]["properties"]["density"]["unit"] = "mm";
  }
  SECTION("boolean density") {
    data["entries"][0]["properties"]["density"]["value"] = true;
  }
  SECTION("null pressure") {
    data["entries"][0]["properties"]["young_modulus"] = nullptr;
  }
  SECTION("unknown origin") {
    data["entries"][0]["provenance"]["origin"] = "unknown";
  }
  SECTION("null metadata") {
    data["entries"][0]["provenance"]["reference"] = nullptr;
  }
  SECTION("unknown column kind") {
    data["entries"][2]["columns"][0]["kind"] = "future";
  }
  SECTION("wrong column unit") {
    data["entries"][2]["columns"][0]["unit"] = "Pa";
  }
  SECTION("unit on text") {
    data["entries"][2]["columns"][7]["unit"] = "mm";
  }
  SECTION("duplicate row") {
    data["entries"][2]["rows"].push_back(data["entries"][2]["rows"][0]);
  }
  SECTION("unknown cell") {
    data["entries"][2]["rows"][0]["cells"][0]["column"] = "unknown";
  }
  SECTION("numeric text") {
    data["entries"][2]["rows"][0]["cells"][7]["value"] = 42;
  }
  SECTION("numeric boolean") {
    data["entries"][2]["rows"][0]["cells"][8]["value"] = 0;
  }
  SECTION("boolean number") {
    data["entries"][2]["rows"][0]["cells"][0]["value"] = false;
  }
  SECTION("float count") {
    data["entries"][2]["rows"][0]["cells"][3]["value"] = 4.0;
  }
  SECTION("inexact count") {
    data["entries"][2]["rows"][0]["cells"][3]["value"] = 9007199254740993ULL;
  }
  SECTION("overflowing count") {
    data["entries"][2]["rows"][0]["cells"][3]["value"] = 18446744073709551615ULL;
  }
  CHECK_FALSE(deserialize_engineering_resource_catalog_from_json(data.dump()));
  CHECK(checked(serialize_engineering_resource_catalog_to_json(fixture::catalog())) == text);
}
