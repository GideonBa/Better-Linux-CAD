#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <utility>

using namespace blcad;

namespace {

Body make_body(const char* id, const char* name, BodyKind kind,
               BodyVisibility visibility = BodyVisibility::Visible) {
  auto body = Body::create(BodyId(id), name, kind, visibility);
  REQUIRE(body);
  return body.value();
}

PartDocument make_body_document() {
  auto document = PartDocument::create(DocumentId("part.body_json"), "BodyJson");
  REQUIRE(document);
  REQUIRE(document.value().add_body(
      make_body("body.z_surface", "Surface", BodyKind::Surface, BodyVisibility::Hidden)));
  REQUIRE(document.value().add_body(make_body("body.a_solid", "Solid", BodyKind::Solid)));
  return document.value();
}

nlohmann::json serialized_body_document() {
  const auto serialized = serialize_part_document_to_json(make_body_document());
  REQUIRE(serialized);
  return nlohmann::json::parse(serialized.value());
}

} // namespace

TEST_CASE("Body intent survives deterministic PartDocument JSON roundtrip",
          "[core][part-body-json][integration][part-construction-mvp]") {
  const auto serialized = serialize_part_document_to_json(make_body_document());
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());

  REQUIRE(root.at("bodies").is_array());
  REQUIRE(root.at("bodies").size() == 2U);
  CHECK(root.at("bodies").at(0) == nlohmann::json{{"id", "body.a_solid"},
                                                  {"name", "Solid"},
                                                  {"kind", "solid"},
                                                  {"visibility", "visible"}});
  CHECK(root.at("bodies").at(1) == nlohmann::json{{"id", "body.z_surface"},
                                                  {"name", "Surface"},
                                                  {"kind", "surface"},
                                                  {"visibility", "hidden"}});

  const auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().body_count() == 2U);
  CHECK(restored.value().bodies()[0].id().value() == "body.a_solid");
  CHECK(restored.value().bodies()[0].kind() == BodyKind::Solid);
  CHECK(restored.value().bodies()[0].visibility() == BodyVisibility::Visible);
  CHECK(restored.value().bodies()[1].id().value() == "body.z_surface");
  CHECK(restored.value().bodies()[1].kind() == BodyKind::Surface);
  CHECK(restored.value().bodies()[1].visibility() == BodyVisibility::Hidden);

  const auto serialized_again = serialize_part_document_to_json(restored.value());
  REQUIRE(serialized_again);
  CHECK(serialized_again.value() == serialized.value());
}

TEST_CASE("PartDocument JSON without bodies follows the historical zero-body path",
          "[core][part-body-json]") {
  auto root = serialized_body_document();
  root.erase("bodies");

  const auto restored = deserialize_part_document_from_json(root.dump());
  REQUIRE(restored);
  CHECK(restored.value().body_count() == 0U);
  CHECK(restored.value().bodies().empty());

  auto empty_root = serialized_body_document();
  empty_root["bodies"] = nlohmann::json::array();
  const auto explicitly_empty = deserialize_part_document_from_json(empty_root.dump());
  REQUIRE(explicitly_empty);
  CHECK(explicitly_empty.value().body_count() == 0U);
}

TEST_CASE("PartDocument JSON canonicalizes authored body order", "[core][part-body-json]") {
  auto root = serialized_body_document();
  std::swap(root.at("bodies").at(0), root.at("bodies").at(1));

  const auto restored = deserialize_part_document_from_json(root.dump());
  REQUIRE(restored);
  REQUIRE(restored.value().body_count() == 2U);
  CHECK(restored.value().bodies()[0].id().value() == "body.a_solid");
  CHECK(restored.value().bodies()[1].id().value() == "body.z_surface");

  const auto canonical = serialize_part_document_to_json(restored.value());
  REQUIRE(canonical);
  const auto canonical_root = nlohmann::json::parse(canonical.value());
  CHECK(canonical_root.at("bodies").at(0).at("id") == "body.a_solid");
  CHECK(canonical_root.at("bodies").at(1).at("id") == "body.z_surface");
}

TEST_CASE("PartDocument JSON validates body structure and values", "[core][part-body-json]") {
  SECTION("bodies must be an array") {
    auto root = serialized_body_document();
    root["bodies"] = nlohmann::json::object();
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "bodies must be an array in part document json");
  }

  SECTION("entries must be objects") {
    auto root = serialized_body_document();
    root["bodies"] = nlohmann::json::array({"body.a"});
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "body entry must be an object in part document json");
  }

  SECTION("all fields are required") {
    auto root = serialized_body_document();
    root.at("bodies").at(0).erase("visibility");
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "body entry must contain id, name, kind, and visibility");
  }

  SECTION("all fields are strings") {
    auto root = serialized_body_document();
    root.at("bodies").at(0).at("name") = 17;
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "body id, name, kind, and visibility must be strings");
  }

  SECTION("unsupported kinds fail closed") {
    auto root = serialized_body_document();
    root.at("bodies").at(0).at("kind") = "construction_only";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "unsupported body kind in part document json");
  }

  SECTION("unsupported visibility fails closed") {
    auto root = serialized_body_document();
    root.at("bodies").at(0).at("visibility") = "suppressed";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "unsupported body visibility in part document json");
  }

  SECTION("empty persistent fields fail closed") {
    auto root = serialized_body_document();
    root.at("bodies").at(0).at("id") = "";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "body id must not be empty");
  }

  SECTION("empty names fail closed") {
    auto root = serialized_body_document();
    root.at("bodies").at(0).at("name") = "";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "body name must not be empty");
  }

  SECTION("duplicate ids fail closed") {
    auto root = serialized_body_document();
    root.at("bodies").push_back(root.at("bodies").at(0));
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "body id must be unique within part document");
  }
}
