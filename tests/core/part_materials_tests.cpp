#include "../support/engineering_resource_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
namespace fixture = blcad::test::engineering_fixture;
namespace history = blcad::test::history_fixture;
using json = nlohmann::json;
namespace {
template <typename T> T checked(Result<T> value) {
  INFO((value ? std::string{} : value.error().message()));
  REQUIRE(value);
  return std::move(value.value());
}
std::string text(const PartDocument& part) {
  return checked(serialize_part_document_to_json(part));
}
PartDocument part() {
  auto p = history::native_base();
  fixture::annotate(p);
  return p;
}
} // namespace
TEST_CASE("Part material default and Body overrides pin explicit revisions without geometry edits",
          "[core][parity-b149][part-materials]") {
  auto p = history::native_base();
  const auto graph = p.dependency_graph().dependencies();
  const auto generation = p.executable_generation();
  fixture::annotate(p);
  REQUIRE(p.effective_material_for_body(BodyId("body.base")));
  CHECK(p.effective_material_for_body(BodyId("body.base"))->key() == fixture::key());
  REQUIRE(p.set_body_material(BodyId("body.base"), fixture::key("synthetic.material", "r2")));
  CHECK(p.effective_material_for_body(BodyId("body.base"))->key().revision() == "r2");
  REQUIRE(p.set_body_material(BodyId("body.base"), std::nullopt));
  CHECK(p.effective_material_for_body(BodyId("body.base"))->key().revision() == "r1");
  const auto before = text(p);
  CHECK_FALSE(p.set_body_material(BodyId("missing"), fixture::key()));
  CHECK_FALSE(p.set_default_material(fixture::key("synthetic.table")));
  CHECK_FALSE(p.set_default_material(fixture::key("missing")));
  CHECK(text(p) == before);
  CHECK(p.dependency_graph().dependencies() == graph);
  CHECK(p.executable_generation() == generation);
  REQUIRE(p.set_default_material(std::nullopt));
  CHECK_FALSE(p.effective_material_for_body(BodyId("body.base")));
  CHECK_FALSE(p.effective_material_for_body(BodyId("missing")));
}
TEST_CASE("A removed unreferenced Body cannot regain a material override by reusing its id",
          "[core][parity-b149][part-materials]") {
  auto p = part();
  const BodyId id("temporary");
  REQUIRE(p.add_body(checked(Body::create(id, "Temporary", BodyKind::Solid))));
  REQUIRE(p.set_body_material(id, fixture::key("synthetic.material", "r2")));
  REQUIRE(p.remove_body(id));
  CHECK(p.body_materials().empty());
  REQUIRE(p.add_body(checked(Body::create(id, "Replacement", BodyKind::Solid))));
  CHECK(p.effective_material_for_body(id)->key() == fixture::key());
  REQUIRE(p.set_body_material(BodyId("body.base"), fixture::key("synthetic.material", "r2")));
  const auto before = text(p);
  CHECK_FALSE(p.remove_body(BodyId("body.base")));
  CHECK(text(p) == before);
}
TEST_CASE("Part version 3 preserves pins resources and both history modes through support preludes",
          "[core][parity-b149][part-materials][resource-history]") {
  const bool authored = GENERATE(false, true);
  auto p = part();
  const auto reference = checked(ResultFaceReference::create(FeatureId("base"), BodyId("body.base"),
                                                             {{FeatureId("base"), "cap.end"}}));
  REQUIRE(p.add_result_face_workplane(
      checked(ResultFaceWorkplane::create(DatumPlaneId("support"), "Support", reference))));
  history::rectangle(p, "consumer", "support");
  if (authored)
    REQUIRE(p.apply_history_intent({{"base"}, {}}));
  const auto before = text(p);
  const auto data = json::parse(before);
  CHECK(data.at("version") == 3);
  CHECK(data.at("history_version") == (authored ? 2 : 1));
  const auto loaded = checked(deserialize_part_document_from_json(before));
  CHECK(text(loaded) == before);
  CHECK(bool(loaded.authored_history_intent()) == authored);
  CHECK(loaded.default_material() == p.default_material());
  CHECK(loaded.engineering_resources() == p.engineering_resources());
  CHECK(loaded.dependency_graph().has_dependency("base", "support"));
  if (authored) {
    auto intent = *loaded.authored_history_intent();
    intent.suppressed = {"base"};
    const auto suppressed = checked(loaded.with_history_intent(intent));
    CHECK(suppressed.default_material() == p.default_material());
    CHECK(suppressed.engineering_resources() == p.engineering_resources());
  }
}
TEST_CASE("Part resource persistence rejects malformed scope pins units versions and downgrades",
          "[core][parity-b149][part-materials]") {
  auto data = json::parse(text(part()));
  SECTION("missing history version") {
    data.erase("history_version");
  }
  SECTION("unknown history version") {
    data["history_version"] = 3;
  }
  SECTION("float version") {
    data["version"] = 3.0;
  }
  SECTION("overflowing version") {
    data["version"] = 4294967299ULL;
  }
  SECTION("downgraded document") {
    data["version"] = 1;
  }
  SECTION("missing catalog") {
    data.erase("engineering_resources");
  }
  SECTION("missing assignments") {
    data.erase("material_assignments");
  }
  SECTION("null default") {
    data["material_assignments"]["default"] = nullptr;
  }
  SECTION("unknown default") {
    data["material_assignments"]["default"]["revision"] = "missing";
  }
  SECTION("table default") {
    data["material_assignments"]["default"]["id"] = "synthetic.table";
  }
  SECTION("unknown assignment field") {
    data["material_assignments"]["future"] = true;
  }
  SECTION("unknown Body") {
    data["material_assignments"]["body_overrides"] = {
        {{"body", "missing"}, {"material", data["material_assignments"]["default"]}}};
  }
  SECTION("duplicate Body") {
    auto entry = json{{"body", "body.base"}, {"material", data["material_assignments"]["default"]}};
    data["material_assignments"]["body_overrides"] = {entry, entry};
  }
  SECTION("wrong unit") {
    data["engineering_resources"]["entries"][0]["properties"]["density"]["unit"] = "mm";
  }
  SECTION("unresolved Project scope") {
    data["engineering_resource_refs"] = json::array();
  }
  SECTION("authored mode requires arrays") {
    data["history_version"] = 2;
  }
  CHECK_FALSE(deserialize_part_document_from_json(data.dump()));
}
TEST_CASE("Legacy resource-free bytes and normalized empty version 3 retain their history meaning",
          "[core][parity-b149][part-materials]") {
  auto p = history::native_base();
  const auto legacy = text(p);
  CHECK(json::parse(legacy).at("version") == 1);
  auto empty = json::parse(text(part()));
  empty["engineering_resources"]["entries"] = json::array();
  empty["material_assignments"] = {{"body_overrides", json::array()}};
  CHECK(text(checked(deserialize_part_document_from_json(empty.dump()))) == legacy);
  REQUIRE(p.apply_history_intent({{"base"}, {}}));
  const auto authored = text(p);
  CHECK(json::parse(authored).at("version") == 2);
  CHECK(text(checked(deserialize_part_document_from_json(authored))) == authored);
  auto ref = history::imported(true);
  const auto before = text(ref);
  CHECK_FALSE(ref.add_engineering_resource(fixture::material()));
  CHECK_FALSE(ref.set_default_material(fixture::key()));
  CHECK(text(ref) == before);
}
TEST_CASE(
    "Project version 1 rejects resource-bearing Part snapshots until Project ownership exists",
    "[core][parity-b149][part-materials]") {
  auto assembly = checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
  auto project = checked(Project::create(DocumentId("project"), "Project", assembly));
  SECTION("writer rejects resource-bearing members") {
    REQUIRE(project.add_part_document(part()));
    CHECK_FALSE(serialize_project_to_json(project));
  }
  SECTION("reader rejects a reserved Project resource catalog") {
    auto data = json::parse(checked(serialize_project_to_json(project)));
    data["engineering_resources"] = json::parse(text(part())).at("engineering_resources");
    CHECK_FALSE(deserialize_project_from_json(data.dump()));
  }
  SECTION("reader rejects independently embedded Part resources") {
    auto data = json::parse(checked(serialize_project_to_json(project)));
    data["parts"].push_back(json::parse(text(part())));
    CHECK_FALSE(deserialize_project_from_json(data.dump()));
  }
}
