#include "blcad/core/body.hpp"
#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

Body make_body(const char* id, const char* name, BodyKind kind,
               BodyVisibility visibility = BodyVisibility::Visible) {
  auto body = Body::create(BodyId(id), name, kind, visibility);
  REQUIRE(body);
  return body.value();
}

PartDocument make_part() {
  auto part = PartDocument::create(DocumentId("part.multi_body"), "MultiBody");
  REQUIRE(part);
  return part.value();
}

} // namespace

TEST_CASE("Body freezes identity kind and visibility intent", "[core][part-body]") {
  const auto solid = Body::create(BodyId("body.solid"), "Solid", BodyKind::Solid);
  REQUIRE(solid);
  CHECK(solid.value().id().value() == "body.solid");
  CHECK(solid.value().name() == "Solid");
  CHECK(solid.value().kind() == BodyKind::Solid);
  CHECK(solid.value().visibility() == BodyVisibility::Visible);
  CHECK(to_string(solid.value().kind()) == "solid");
  CHECK(to_string(solid.value().visibility()) == "visible");

  auto hidden = solid.value().with_visibility(BodyVisibility::Hidden);
  REQUIRE(hidden);
  CHECK(hidden.value().visibility() == BodyVisibility::Hidden);
  CHECK(to_string(hidden.value().visibility()) == "hidden");
  CHECK(solid.value().visibility() == BodyVisibility::Visible);

  const auto surface =
      Body::create(BodyId("body.surface"), "Surface", BodyKind::Surface, BodyVisibility::Hidden);
  REQUIRE(surface);
  CHECK(surface.value().kind() == BodyKind::Surface);
  CHECK(to_string(surface.value().kind()) == "surface");
}

TEST_CASE("Body rejects empty persistent identity fields", "[core][part-body]") {
  const auto empty_id = Body::create(BodyId(), "Body", BodyKind::Solid);
  REQUIRE(empty_id.has_error());
  CHECK(empty_id.error().object_id() == "body");
  CHECK(empty_id.error().message() == "body id must not be empty");

  const auto empty_name = Body::create(BodyId("body.empty_name"), "", BodyKind::Surface);
  REQUIRE(empty_name.has_error());
  CHECK(empty_name.error().object_id() == "body.empty_name");
  CHECK(empty_name.error().message() == "body name must not be empty");
}

TEST_CASE("PartDocument owns bodies in canonical lexicographic id order", "[core][part-body]") {
  auto part = make_part();
  CHECK(part.body_count() == 0U);
  CHECK(part.bodies().empty());

  auto z = part.add_body(make_body("body.z", "SharedName", BodyKind::Surface));
  auto a = part.add_body(make_body("body.a", "SharedName", BodyKind::Solid));
  auto middle =
      part.add_body(make_body("body.middle", "Middle", BodyKind::Solid, BodyVisibility::Hidden));
  REQUIRE(z);
  REQUIRE(a);
  REQUIRE(middle);
  CHECK(z.value() == 0U);
  CHECK(a.value() == 0U);
  CHECK(middle.value() == 1U);
  REQUIRE(part.body_count() == 3U);
  CHECK(part.bodies()[0].id().value() == "body.a");
  CHECK(part.bodies()[1].id().value() == "body.middle");
  CHECK(part.bodies()[2].id().value() == "body.z");

  const Body* found = part.find_body(BodyId("body.middle"));
  REQUIRE(found);
  CHECK(found->kind() == BodyKind::Solid);
  CHECK(found->visibility() == BodyVisibility::Hidden);
  CHECK(part.find_body(BodyId("body.missing")) == nullptr);

  const auto duplicate = part.add_body(make_body("body.a", "Duplicate", BodyKind::Surface));
  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().message() == "body id must be unique within part document");
  CHECK(part.body_count() == 3U);
}

TEST_CASE("PartDocument updates visibility and removes bodies explicitly", "[core][part-body]") {
  auto part = make_part();
  REQUIRE(part.add_body(make_body("body.a", "A", BodyKind::Solid)));
  REQUIRE(part.add_body(make_body("body.b", "B", BodyKind::Surface)));

  auto updated = part.set_body_visibility(BodyId("body.b"), BodyVisibility::Hidden);
  REQUIRE(updated);
  CHECK(updated.value() == 1U);
  REQUIRE(part.find_body(BodyId("body.b")));
  CHECK(part.find_body(BodyId("body.b"))->visibility() == BodyVisibility::Hidden);

  CHECK(part.set_body_visibility(BodyId(), BodyVisibility::Visible).has_error());
  CHECK(part.set_body_visibility(BodyId("body.missing"), BodyVisibility::Visible).has_error());
  CHECK(part.remove_body(BodyId()).has_error());
  CHECK(part.remove_body(BodyId("body.missing")).has_error());

  auto removed = part.remove_body(BodyId("body.a"));
  REQUIRE(removed);
  CHECK(removed.value() == 1U);
  CHECK(part.find_body(BodyId("body.a")) == nullptr);
  REQUIRE(part.body_count() == 1U);
  CHECK(part.bodies().front().id().value() == "body.b");
}

TEST_CASE("Historical single-final-shape Parts do not gain implicit Body intent",
          "[core][part-body]") {
  auto part = PartDocument::create(DocumentId("part.historical"), "Historical");
  REQUIRE(part);
  CHECK(part.value().body_count() == 0U);
  CHECK(part.value().bodies().empty());
  CHECK(part.value().dependency_graph().node_count() == 0U);
  CHECK(part.value().invalidation_state().node_count() == 0U);
}
