#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
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

Parameter angle_parameter(const char* id, double value) {
  auto quantity = Quantity::angle_deg(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_angle(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body body(const char* id) {
  auto value = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(value);
  return value.value();
}

EdgeReference linear_edge(PartFeatureInputRole role, SemanticEdge edge) {
  auto semantic = SemanticEdgeReference::create(FeatureId("feature.base"), edge);
  REQUIRE(semantic);
  auto value = EdgeReference::create_linear(role, semantic.value());
  REQUIRE(value);
  return value.value();
}

EdgeReference circular_edge(PartFeatureInputRole role, SemanticCircularEdge edge) {
  auto semantic = SemanticCircularEdgeReference::create(FeatureId("feature.hole"),
                                                        ProfileId("profile.hole"), edge);
  REQUIRE(semantic);
  auto value = EdgeReference::create_circular(role, semantic.value());
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext new_body_context() {
  auto value = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                BodyId("body.base"));
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext cut_context() {
  auto value = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("body.base"),
                                                std::nullopt);
  REQUIRE(value);
  return value.value();
}

PartDocument base_document(bool with_hole = false) {
  auto document = PartDocument::create(DocumentId("part.edge_treatment"), "EdgeTreatment");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 12.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 8.0)));
  REQUIRE(document.value().add_parameter(length_parameter("fillet.radius", 2.0)));
  REQUIRE(document.value().add_parameter(length_parameter("chamfer.first", 1.0)));
  REQUIRE(document.value().add_parameter(length_parameter("chamfer.second", 1.5)));
  REQUIRE(document.value().add_parameter(angle_parameter("chamfer.angle", 45.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                            ParameterId("base.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  REQUIRE(document.value().add_body(body("body.base")));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto contextual = base.value().with_body_result_context(new_body_context());
  REQUIRE(contextual);
  REQUIRE(document.value().add_feature(contextual.value()));
  if (!with_hole)
    return document.value();

  REQUIRE(document.value().add_parameter(length_parameter("hole.diameter", 4.0)));
  auto hole_sketch = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy"));
  REQUIRE(hole_sketch);
  auto circle = CircleProfile::create(ProfileId("profile.hole"), ParameterId("hole.diameter"));
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.value().add_sketch(hole_sketch.value()));
  auto hole = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"), FeatureId("feature.base"));
  REQUIRE(hole);
  auto cut = hole.value().with_body_result_context(cut_context());
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));
  return document.value();
}

} // namespace

TEST_CASE("Block 68 freezes ordered Fillet and Chamfer intent", "[core][edge-treatment]") {
  const auto first = linear_edge(PartFeatureInputRole::FilletEdge, SemanticEdge::TopFront);
  const auto second = linear_edge(PartFeatureInputRole::FilletEdge, SemanticEdge::TopBack);
  auto fillet = FilletFeature::create(FeatureId("fillet.main"), "Fillet", BodyId("body.base"),
                                      {first, second}, ParameterId("fillet.radius"));
  REQUIRE(fillet);
  CHECK(fillet.value().edges().at(0).source_node_id() == "feature.base");
  CHECK(std::get<SemanticEdgeReference>(fillet.value().edges().at(0).source()).edge() ==
        SemanticEdge::TopFront);
  CHECK(std::get<SemanticEdgeReference>(fillet.value().edges().at(1).source()).edge() ==
        SemanticEdge::TopBack);
  CHECK(FilletFeature::create(FeatureId("fillet.duplicate"), "Duplicate", BodyId("body.base"),
                              {first, first}, ParameterId("fillet.radius"))
            .has_error());
  CHECK(FilletFeature::create(FeatureId("fillet.empty"), "Empty", BodyId("body.base"), {},
                              ParameterId("fillet.radius"))
            .has_error());

  const auto edge = linear_edge(PartFeatureInputRole::ChamferEdge, SemanticEdge::FrontRight);
  auto equal = ChamferFeature::create_equal_distance(FeatureId("chamfer.equal"), "Equal",
                                                     BodyId("body.base"), {edge},
                                                     ParameterId("chamfer.first"));
  auto two = ChamferFeature::create_two_distance(
      FeatureId("chamfer.two"), "Two", BodyId("body.base"), {edge}, ParameterId("chamfer.first"),
      ParameterId("chamfer.second"));
  auto angle = ChamferFeature::create_distance_angle(
      FeatureId("chamfer.angled"), "Angle", BodyId("body.base"), {edge},
      ParameterId("chamfer.first"), ParameterId("chamfer.angle"));
  REQUIRE(equal);
  REQUIRE(two);
  REQUIRE(angle);
  CHECK(equal.value().mode() == ChamferMode::EqualDistance);
  CHECK_FALSE(equal.value().second_parameter().has_value());
  CHECK(two.value().mode() == ChamferMode::TwoDistance);
  CHECK(angle.value().mode() == ChamferMode::DistanceAngle);
  CHECK(to_string(angle.value().mode()) == "distance_angle");
}

TEST_CASE("Block 68 integrates body ownership dependencies and invalidation",
          "[core][edge-treatment]") {
  auto document = base_document();
  auto fillet =
      FilletFeature::create(FeatureId("fillet.main"), "Fillet", BodyId("body.base"),
                            {linear_edge(PartFeatureInputRole::FilletEdge, SemanticEdge::TopFront)},
                            ParameterId("fillet.radius"));
  REQUIRE(fillet);
  REQUIRE(document.add_fillet_feature(fillet.value()));
  auto chamfer = ChamferFeature::create_distance_angle(
      FeatureId("chamfer.main"), "Chamfer", BodyId("body.base"),
      {linear_edge(PartFeatureInputRole::ChamferEdge, SemanticEdge::TopBack)},
      ParameterId("chamfer.first"), ParameterId("chamfer.angle"));
  REQUIRE(chamfer);
  REQUIRE(document.add_chamfer_feature(chamfer.value()));

  CHECK(document.fillet_feature_count() == 1U);
  CHECK(document.chamfer_feature_count() == 1U);
  CHECK(document.find_fillet_feature(FeatureId("fillet.main")) != nullptr);
  CHECK(document.dependency_graph().has_dependency("feature.base", "fillet.main"));
  CHECK(document.dependency_graph().has_dependency("fillet.radius", "fillet.main"));
  CHECK(document.dependency_graph().has_dependency("fillet.main", "chamfer.main"));
  CHECK(document.dependency_graph().has_dependency("chamfer.first", "chamfer.main"));
  CHECK(document.dependency_graph().has_dependency("chamfer.angle", "chamfer.main"));
  CHECK(document.dependency_graph().has_dependency("chamfer.main", "body:body.base"));
  CHECK_FALSE(document.dependency_graph().has_cycle());
  CHECK(document.remove_body(BodyId("body.base")).has_error());

  document.mark_all_clean();
  auto affected = document.mark_parameter_changed(ParameterId("fillet.radius"));
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "fillet.main") !=
        affected.value().end());
  CHECK(std::find(affected.value().begin(), affected.value().end(), "chamfer.main") !=
        affected.value().end());
  CHECK(document.invalidation_state().find("body:body.base")->status == InvalidationStatus::Dirty);
}

TEST_CASE("Block 68 validates semantic circular edges and parameter kinds transactionally",
          "[core][edge-treatment]") {
  auto document = base_document(true);
  const std::size_t nodes = document.dependency_graph().node_count();
  auto chamfer = ChamferFeature::create_two_distance(
      FeatureId("chamfer.rims"), "Rims", BodyId("body.base"),
      {circular_edge(PartFeatureInputRole::ChamferEdge, SemanticCircularEdge::SourceRim),
       circular_edge(PartFeatureInputRole::ChamferEdge, SemanticCircularEdge::OppositeRim)},
      ParameterId("chamfer.first"), ParameterId("chamfer.second"));
  REQUIRE(chamfer);
  REQUIRE(document.add_chamfer_feature(chamfer.value()));
  CHECK(document.dependency_graph().has_dependency("feature.hole", "chamfer.rims"));

  auto invalid = base_document();
  auto wrong_angle = ChamferFeature::create_distance_angle(
      FeatureId("chamfer.wrong"), "Wrong", BodyId("body.base"),
      {linear_edge(PartFeatureInputRole::ChamferEdge, SemanticEdge::TopFront)},
      ParameterId("chamfer.first"), ParameterId("chamfer.second"));
  REQUIRE(wrong_angle);
  CHECK(invalid.add_chamfer_feature(wrong_angle.value()).has_error());
  auto missing_semantic =
      SemanticEdgeReference::create(FeatureId("feature.missing"), SemanticEdge::TopFront);
  REQUIRE(missing_semantic);
  auto missing_edge =
      EdgeReference::create_linear(PartFeatureInputRole::FilletEdge, missing_semantic.value());
  REQUIRE(missing_edge);
  auto missing_fillet =
      FilletFeature::create(FeatureId("fillet.missing"), "Missing", BodyId("body.base"),
                            {missing_edge.value()}, ParameterId("fillet.radius"));
  REQUIRE(missing_fillet);
  CHECK(invalid.add_fillet_feature(missing_fillet.value()).has_error());
  CHECK(invalid.chamfer_feature_count() == 0U);
  CHECK(invalid.fillet_feature_count() == 0U);
  CHECK(invalid.dependency_graph().node_count() < nodes + 2U);
}

TEST_CASE("Block 68 JSON roundtrips deterministic edge-treatment order and all modes",
          "[core][edge-treatment]") {
  auto document = base_document();
  auto fillet =
      FilletFeature::create(FeatureId("fillet.main"), "Fillet", BodyId("body.base"),
                            {linear_edge(PartFeatureInputRole::FilletEdge, SemanticEdge::TopFront),
                             linear_edge(PartFeatureInputRole::FilletEdge, SemanticEdge::TopBack)},
                            ParameterId("fillet.radius"));
  REQUIRE(fillet);
  REQUIRE(document.add_fillet_feature(fillet.value()));
  const auto add_chamfer = [&document](ChamferFeature feature) {
    REQUIRE(document.add_chamfer_feature(std::move(feature)));
  };
  auto equal = ChamferFeature::create_equal_distance(
      FeatureId("chamfer.equal"), "Equal", BodyId("body.base"),
      {linear_edge(PartFeatureInputRole::ChamferEdge, SemanticEdge::FrontRight)},
      ParameterId("chamfer.first"));
  REQUIRE(equal);
  add_chamfer(equal.value());
  auto two = ChamferFeature::create_two_distance(
      FeatureId("chamfer.two"), "Two", BodyId("body.base"),
      {linear_edge(PartFeatureInputRole::ChamferEdge, SemanticEdge::FrontLeft)},
      ParameterId("chamfer.first"), ParameterId("chamfer.second"));
  REQUIRE(two);
  add_chamfer(two.value());
  auto angle = ChamferFeature::create_distance_angle(
      FeatureId("chamfer.angled"), "Angle", BodyId("body.base"),
      {linear_edge(PartFeatureInputRole::ChamferEdge, SemanticEdge::BackRight)},
      ParameterId("chamfer.first"), ParameterId("chamfer.angle"));
  REQUIRE(angle);
  add_chamfer(angle.value());

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  REQUIRE(root.at("edge_treatments").size() == 4U);
  CHECK(root.at("edge_treatments").at(0).at("id") == "fillet.main");
  CHECK(root.at("edge_treatments").at(1).at("mode") == "equal_distance");
  CHECK(root.at("edge_treatments").at(2).at("mode") == "two_distance");
  CHECK(root.at("edge_treatments").at(3).at("mode") == "distance_angle");
  CHECK(root.at("edge_treatments").at(3).at("edges").at(0).at("source_kind") ==
        "semantic_linear_edge");
  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().fillet_feature_count() == 1U);
  CHECK(restored.value().chamfer_feature_count() == 3U);
  CHECK(restored.value().find_chamfer_feature(FeatureId("chamfer.angled"))->second_parameter() ==
        std::optional<ParameterId>(ParameterId("chamfer.angle")));
}

TEST_CASE("Block 68 JSON is strict and backward compatible", "[core][edge-treatment]") {
  auto document = base_document();
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto historical = nlohmann::json::parse(serialized.value());
  historical.erase("edge_treatments");
  auto restored = deserialize_part_document_from_json(historical.dump());
  REQUIRE(restored);
  CHECK(restored.value().fillet_feature_count() == 0U);
  CHECK(nlohmann::json::parse(serialize_part_document_to_json(restored.value()).value())
            .at("edge_treatments")
            .empty());

  auto fillet =
      FilletFeature::create(FeatureId("fillet.main"), "Fillet", BodyId("body.base"),
                            {linear_edge(PartFeatureInputRole::FilletEdge, SemanticEdge::TopFront)},
                            ParameterId("fillet.radius"));
  REQUIRE(fillet);
  REQUIRE(document.add_fillet_feature(fillet.value()));
  serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto malformed = nlohmann::json::parse(serialized.value());
  malformed["edge_treatments"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  malformed = nlohmann::json::parse(serialized.value());
  malformed["edge_treatments"][0]["edges"][0]["edge"] = "missing";
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  malformed = nlohmann::json::parse(serialized.value());
  malformed["edge_treatments"] = nlohmann::json::object();
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
}
