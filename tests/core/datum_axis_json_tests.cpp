#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <string>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_datum_axis_document() {
  auto document = PartDocument::create(DocumentId("part.datum_axis_json"), "DatumAxisJson");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 25.0)));

  auto line = ConstructionLine::create_explicit(ConstructionLineId("construction.center"), "Center",
                                                Point3{1.0, 2.0, 3.0}, Vector3{1.0, 0.0, 0.0});
  REQUIRE(line);
  REQUIRE(document.value().add_construction_line(std::move(line.value())));

  auto explicit_axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "Spindle",
                                                  Point3{4.0, 5.0, 6.0}, Vector3{0.0, 0.0, 1.0},
                                                  {ParameterId("part.height")});
  REQUIRE(explicit_axis);
  REQUIRE(document.value().add_datum_axis(std::move(explicit_axis.value())));

  auto derived_axis = DatumAxis::create_from_construction_line(
      DatumAxisId("datum_axis.from_line"), "FromLine", ConstructionLineId("construction.center"));
  REQUIRE(derived_axis);
  REQUIRE(document.value().add_datum_axis(std::move(derived_axis.value())));
  return document.value();
}

} // namespace

TEST_CASE("DatumAxis intent survives part document JSON roundtrip", "[core][datum-axis-json]") {
  const PartDocument document = make_datum_axis_document();

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  REQUIRE(root.at("datum_axes").size() == 2U);
  CHECK(root.at("datum_axes").at(0).at("kind") == "explicit");
  CHECK(root.at("datum_axes").at(1).at("kind") == "from_construction_line");

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().datum_axis_count() == 2U);

  const DatumAxis* explicit_axis =
      restored.value().find_datum_axis(DatumAxisId("datum_axis.spindle"));
  REQUIRE(explicit_axis != nullptr);
  CHECK(explicit_axis->name() == "Spindle");
  CHECK(explicit_axis->kind() == DatumAxisKind::Explicit);
  CHECK(explicit_axis->origin() == Point3{4.0, 5.0, 6.0});
  CHECK(explicit_axis->direction() == Vector3{0.0, 0.0, 1.0});
  REQUIRE(explicit_axis->parameter_dependencies().size() == 1U);
  CHECK(explicit_axis->parameter_dependencies().front().value() == "part.height");

  const DatumAxis* derived_axis =
      restored.value().find_datum_axis(DatumAxisId("datum_axis.from_line"));
  REQUIRE(derived_axis != nullptr);
  CHECK(derived_axis->kind() == DatumAxisKind::FromConstructionLine);
  CHECK(derived_axis->source_construction_line().value() == "construction.center");

  CHECK(restored.value().dependency_graph().has_dependency("part.height", "datum_axis.spindle"));
  CHECK(restored.value().dependency_graph().has_dependency("construction.center",
                                                           "datum_axis.from_line"));
}

TEST_CASE("Part document JSON remains backward compatible without datum axes",
          "[core][datum-axis-json]") {
  auto document = PartDocument::create(DocumentId("part.no_axes"), "NoAxes");
  REQUIRE(document);
  auto serialized = serialize_part_document_to_json(document.value());
  REQUIRE(serialized);

  auto root = nlohmann::json::parse(serialized.value());
  root.erase("datum_axes");

  auto restored = deserialize_part_document_from_json(root.dump());
  REQUIRE(restored);
  CHECK(restored.value().datum_axis_count() == 0U);
}

TEST_CASE("Part document JSON validates datum axis intent at load", "[core][datum-axis-json]") {
  const PartDocument document = make_datum_axis_document();
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);

  SECTION("unsupported kind fails closed") {
    auto root = nlohmann::json::parse(serialized.value());
    root.at("datum_axes").at(0).at("kind") = "unsupported";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "unsupported datum axis kind in part document json");
  }

  SECTION("duplicate ids fail closed") {
    auto root = nlohmann::json::parse(serialized.value());
    root.at("datum_axes").push_back(root.at("datum_axes").at(0));
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "datum axis id must be unique within part document");
  }

  SECTION("missing source construction line fails closed") {
    auto root = nlohmann::json::parse(serialized.value());
    root.at("datum_axes").at(1).at("source_construction_line") = "construction.missing";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() ==
          "construction-line-derived datum axis source line must exist in part document");
  }

  SECTION("non-unit explicit direction fails closed") {
    auto root = nlohmann::json::parse(serialized.value());
    root.at("datum_axes").at(0).at("direction").at("z") = 2.0;
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() == "datum axis direction must be unit length");
  }

  SECTION("missing declared parameter dependency fails closed") {
    auto root = nlohmann::json::parse(serialized.value());
    root.at("datum_axes").at(0).at("parameter_dependencies").at(0) = "part.missing";
    const auto restored = deserialize_part_document_from_json(root.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() ==
          "datum axis parameter dependency must exist in part document");
  }
}
