#include "blcad/core/part_document_json.hpp"
#include "blcad/core/revolve_feature.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

using namespace blcad;

namespace {

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body body(const char* id) {
  auto value = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext new_body_context(const char* id);

PartDocument revolve_document() {
  auto document = PartDocument::create(DocumentId("part.revolve"), "Revolve");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.value().add_parameter(length_parameter("width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("height", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("depth", 5.0)));
  auto sketch = Sketch::create(SketchId("sketch.profile"), "Profile", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.main"), ParameterId("width"),
                                             ParameterId("height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto line = ConstructionLine::create_explicit(ConstructionLineId("line.axis"), "Axis", {},
                                                {0.0, 0.0, 1.0});
  REQUIRE(line);
  REQUIRE(document.value().add_construction_line(line.value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z", {}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  REQUIRE(document.value().add_body(body("body.full")));
  REQUIRE(document.value().add_body(body("body.partial")));
  REQUIRE(document.value().add_body(body("body.symmetric")));
  REQUIRE(document.value().add_body(body("body.source")));
  REQUIRE(document.value().add_body(body("body.semantic_axis")));
  REQUIRE(document.value().add_body(body("body.semantic_edge")));
  auto source = Feature::create_additive_extrude(FeatureId("feature.source"), "Source",
                                                 SketchId("sketch.profile"), ParameterId("depth"));
  REQUIRE(source);
  auto source_context = source.value().with_body_result_context(
      new_body_context("body.source"));
  REQUIRE(source_context);
  REQUIRE(document.value().add_feature(source_context.value()));
  return document.value();
}

ProfileRegionReference profile() {
  auto value = ProfileRegionReference::create(SketchId("sketch.profile"),
                                              ProfileId("profile.main"),
                                              PartFeatureInputRole::RevolveProfile);
  REQUIRE(value);
  return value.value();
}

AxisReference datum_axis() {
  auto value = AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis,
                                                DatumAxisId("axis.z"));
  REQUIRE(value);
  return value.value();
}

AxisReference construction_axis() {
  auto value = AxisReference::create_construction_line(
      PartFeatureInputRole::RevolveAxis, ConstructionLineId("line.axis"),
      PartFeatureInputCapability::Axis);
  REQUIRE(value);
  return value.value();
}

AxisReference semantic_axis() {
  auto semantic = SemanticAxisReference::create(FeatureId("feature.source"));
  REQUIRE(semantic);
  auto value = AxisReference::create_semantic_axis(PartFeatureInputRole::RevolveAxis,
                                                   semantic.value());
  REQUIRE(value);
  return value.value();
}

AxisReference semantic_edge_axis() {
  auto edge = SemanticEdgeReference::create(FeatureId("feature.source"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto value = AxisReference::create_semantic_edge(
      PartFeatureInputRole::RevolveAxis, edge.value(), PartFeatureInputCapability::Axis);
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext new_body_context(const char* id) {
  auto value = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                BodyId(id));
  REQUIRE(value);
  return value.value();
}

RevolveFeature make_revolve(const char* id, AxisReference axis, RevolveAngleExtent extent,
                            const char* result_body) {
  auto value = RevolveFeature::create_revolve(FeatureId(id), id, profile(), std::move(axis),
                                              std::move(extent),
                                              new_body_context(result_body));
  REQUIRE(value);
  return value.value();
}

} // namespace

TEST_CASE("Revolve angle extent freezes full partial side and symmetric semantics",
          "[core][revolve-feature]") {
  const auto full = RevolveAngleExtent::full();
  CHECK(full.mode() == RevolveExtentMode::Full);
  CHECK_FALSE(full.angle_deg().has_value());
  CHECK_FALSE(full.side().has_value());

  const auto non_principal = RevolveAngleExtent::angle(270.0, RevolveSide::Negative);
  REQUIRE(non_principal);
  CHECK(non_principal.value().angle_deg() == 270.0);
  CHECK(non_principal.value().side() == RevolveSide::Negative);
  const auto symmetric = RevolveAngleExtent::symmetric(180.0);
  REQUIRE(symmetric);
  CHECK(symmetric.value().mode() == RevolveExtentMode::Symmetric);
  CHECK(symmetric.value().angle_deg() == 180.0);
  CHECK_FALSE(symmetric.value().side().has_value());

  CHECK(RevolveAngleExtent::angle(0.0).has_error());
  CHECK(RevolveAngleExtent::angle(360.0).has_error());
  CHECK(RevolveAngleExtent::angle(std::nan("")).has_error());
  CHECK(RevolveAngleExtent::symmetric(-1.0).has_error());
}

TEST_CASE("Revolve and RevolveCut enforce role and body-operation families",
          "[core][revolve-feature]") {
  auto cut_context = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                      BodyId("body.full"), std::nullopt);
  REQUIRE(cut_context);
  auto cut = RevolveFeature::create_revolve_cut(
      FeatureId("revolve.cut"), "Cut", profile(), datum_axis(), RevolveAngleExtent::full(),
      cut_context.value());
  REQUIRE(cut);
  CHECK(cut.value().kind() == RevolveFeatureKind::RevolveCut);
  CHECK(RevolveFeature::create_revolve(FeatureId("revolve.bad"), "Bad", profile(), datum_axis(),
                                       RevolveAngleExtent::full(), cut_context.value())
            .has_error());

  auto plain_profile = ProfileRegionReference::create(SketchId("sketch.profile"),
                                                      ProfileId("profile.main"));
  REQUIRE(plain_profile);
  CHECK(RevolveFeature::create_revolve(
            FeatureId("revolve.role"), "Role", plain_profile.value(), datum_axis(),
            RevolveAngleExtent::full(), new_body_context("body.full"))
            .has_error());
}

TEST_CASE("PartDocument owns Revolve intent dependencies and invalidation",
          "[core][revolve-feature]") {
  auto document = revolve_document();
  REQUIRE(document.add_revolve_feature(make_revolve(
      "revolve.full", datum_axis(), RevolveAngleExtent::full(), "body.full")));
  CHECK(document.revolve_feature_count() == 1U);
  REQUIRE(document.find_revolve_feature(FeatureId("revolve.full")) != nullptr);
  CHECK(document.dependency_graph().has_dependency("sketch.profile", "revolve.full"));
  CHECK(document.dependency_graph().has_dependency("axis.z", "revolve.full"));
  CHECK(document.dependency_graph().has_dependency("revolve.full", "body:body.full"));
  CHECK(document.remove_body(BodyId("body.full")).has_error());

  document.mark_all_clean();
  const auto affected = document.mark_feature_changed(FeatureId("revolve.full"));
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "revolve.full") !=
        affected.value().end());
  const auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("body:body.full"));
}

TEST_CASE("Revolve JSON roundtrips supported extent and concrete axis intent",
          "[core][revolve-feature]") {
  auto document = revolve_document();
  REQUIRE(document.add_revolve_feature(make_revolve(
      "revolve.full", datum_axis(), RevolveAngleExtent::full(), "body.full")));
  auto partial = RevolveAngleExtent::angle(270.0, RevolveSide::Negative);
  auto symmetric = RevolveAngleExtent::symmetric(120.0);
  REQUIRE(partial);
  REQUIRE(symmetric);
  REQUIRE(document.add_revolve_feature(make_revolve(
      "revolve.partial", construction_axis(), partial.value(), "body.partial")));
  REQUIRE(document.add_revolve_feature(make_revolve(
      "revolve.symmetric", datum_axis(), symmetric.value(), "body.symmetric")));
  REQUIRE(document.add_revolve_feature(make_revolve(
      "revolve.semantic_axis", semantic_axis(), RevolveAngleExtent::full(),
      "body.semantic_axis")));
  REQUIRE(document.add_revolve_feature(make_revolve(
      "revolve.semantic_edge", semantic_edge_axis(), RevolveAngleExtent::full(),
      "body.semantic_edge")));

  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  REQUIRE(root.at("revolve_features").size() == 5U);
  CHECK(root.at("revolve_features").at(0).at("extent").at("mode") == "full");
  CHECK(root.at("revolve_features").at(1).at("extent").at("angle_deg") == 270.0);
  CHECK(root.at("revolve_features").at(1).at("extent").at("side") == "negative");
  CHECK(root.at("revolve_features").at(1).at("axis").at("source_kind") ==
        "construction_line");

  const auto restored = deserialize_part_document_from_json(root.dump());
  REQUIRE(restored);
  CHECK(root.at("revolve_features").at(3).at("axis").at("source_kind") == "semantic_axis");
  CHECK(root.at("revolve_features").at(4).at("axis").at("source_kind") ==
        "semantic_linear_edge");
  REQUIRE(restored.value().revolve_feature_count() == 5U);
  CHECK(restored.value().revolve_features()[1].extent().angle_deg() == 270.0);
  CHECK(restored.value().revolve_features()[2].extent().mode() ==
        RevolveExtentMode::Symmetric);
  const auto again = serialize_part_document_to_json(restored.value());
  REQUIRE(again);
  CHECK(nlohmann::json::parse(again.value()) == root);

  auto historical = root;
  historical.erase("revolve_features");
  const auto historical_restored = deserialize_part_document_from_json(historical.dump());
  REQUIRE(historical_restored);
  CHECK(historical_restored.value().revolve_feature_count() == 0U);
}

TEST_CASE("Revolve JSON rejects ambiguous angle and operation intent",
          "[core][revolve-feature]") {
  auto document = revolve_document();
  REQUIRE(document.add_revolve_feature(make_revolve(
      "revolve.full", datum_axis(), RevolveAngleExtent::full(), "body.full")));
  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto root = nlohmann::json::parse(serialized.value());

  SECTION("full extent cannot carry angle") {
    root.at("revolve_features").at(0).at("extent")["angle_deg"] = 360.0;
    CHECK(deserialize_part_document_from_json(root.dump()).has_error());
  }
  SECTION("revolve cut requires cut mode") {
    root.at("revolve_features").at(0).at("type") = "revolve_cut";
    CHECK(deserialize_part_document_from_json(root.dump()).has_error());
  }
  SECTION("array type is strict") {
    root["revolve_features"] = nlohmann::json::object();
    CHECK(deserialize_part_document_from_json(root.dump()).has_error());
  }
}
