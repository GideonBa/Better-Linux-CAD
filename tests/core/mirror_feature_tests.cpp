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

Body body(const char* id) {
  auto value = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext new_body_context(const char* id) {
  auto value =
      FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, BodyId(id));
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext modifying_context(const char* id) {
  auto value =
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, BodyId(id), std::nullopt);
  REQUIRE(value);
  return value.value();
}

MirrorSourceReference feature_source(const char* id) {
  auto value = MirrorSourceReference::feature(FeatureId(id));
  REQUIRE(value);
  return value.value();
}

MirrorSourceReference body_source(const char* id) {
  auto value = MirrorSourceReference::body(BodyId(id));
  REQUIRE(value);
  return value.value();
}

PlaneReference datum_plane() {
  auto value = PlaneReference::create_datum_plane(PartFeatureInputRole::MirrorPlane,
                                                  DatumPlaneId("datum.xy"));
  REQUIRE(value);
  return value.value();
}

PlaneReference construction_plane() {
  auto value = PlaneReference::create_construction_plane(PartFeatureInputRole::MirrorPlane,
                                                         ConstructionPlaneId("plane.mirror"));
  REQUIRE(value);
  return value.value();
}

PlaneReference semantic_plane() {
  auto face = SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(face);
  auto value =
      PlaneReference::create_semantic_face(PartFeatureInputRole::MirrorPlane, face.value());
  REQUIRE(value);
  return value.value();
}

PartDocument base_document() {
  auto document = PartDocument::create(DocumentId("part.mirror"), "Mirror");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 5.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto plane = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.mirror"), "Mirror",
                                                  {5.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0},
                                                  {1.0, 0.0, 0.0});
  REQUIRE(plane);
  REQUIRE(document.value().add_construction_plane(plane.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                          ParameterId("base.height"));
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  REQUIRE(document.value().add_body(body("body.base")));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto with_context = base.value().with_body_result_context(new_body_context("body.base"));
  REQUIRE(with_context);
  REQUIRE(document.value().add_feature(with_context.value()));
  return document.value();
}

MirrorFeature mirror(const char* id, std::vector<MirrorSourceReference> sources,
                     PlaneReference plane, const char* result_body) {
  auto value = MirrorFeature::create(FeatureId(id), id, std::move(sources), std::move(plane),
                                     new_body_context(result_body));
  REQUIRE(value);
  return value.value();
}

} // namespace

TEST_CASE("Block 66 freezes ordered Mirror Feature and Body sources", "[core][mirror-feature]") {
  const auto feature = feature_source("feature.base");
  const auto source_body = body_source("body.base");
  CHECK(feature.kind() == MirrorSourceKind::Feature);
  CHECK(feature.source_node_id() == "feature.base");
  CHECK(source_body.kind() == MirrorSourceKind::Body);
  CHECK(source_body.source_node_id() == "body:body.base");
  CHECK(MirrorSourceReference::feature(FeatureId()).has_error());
  CHECK(MirrorSourceReference::body(BodyId()).has_error());

  auto valid = MirrorFeature::create(FeatureId("mirror.valid"), "Mirror", {feature, source_body},
                                     datum_plane(), new_body_context("body.result"));
  REQUIRE(valid);
  CHECK(valid.value().sources().at(0) == feature);
  CHECK(valid.value().sources().at(1) == source_body);
  CHECK(valid.value().mirror_plane().role() == PartFeatureInputRole::MirrorPlane);
  CHECK(MirrorFeature::create(FeatureId("mirror.empty"), "Empty", {}, datum_plane(),
                              new_body_context("body.result"))
            .has_error());
  CHECK(MirrorFeature::create(FeatureId("mirror.duplicate"), "Duplicate", {feature, feature},
                              datum_plane(), new_body_context("body.result"))
            .has_error());
  auto wrong_plane = PlaneReference::create_datum_plane(PartFeatureInputRole::DraftNeutralPlane,
                                                        DatumPlaneId("datum.xy"));
  REQUIRE(wrong_plane);
  CHECK(MirrorFeature::create(FeatureId("mirror.role"), "Role", {feature}, wrong_plane.value(),
                              new_body_context("body.result"))
            .has_error());
}

TEST_CASE("Block 66 integrates Mirror planes sources bodies and invalidation",
          "[core][mirror-feature]") {
  auto document = base_document();
  REQUIRE(document.add_body(body("body.datum")));
  REQUIRE(document.add_body(body("body.construction")));
  REQUIRE(document.add_body(body("body.semantic")));
  REQUIRE(document.add_mirror_feature(
      mirror("mirror.datum", {feature_source("feature.base")}, datum_plane(), "body.datum")));
  REQUIRE(document.add_mirror_feature(mirror("mirror.construction", {body_source("body.base")},
                                             construction_plane(), "body.construction")));
  REQUIRE(document.add_mirror_feature(mirror("mirror.semantic", {feature_source("mirror.datum")},
                                             semantic_plane(), "body.semantic")));

  CHECK(document.mirror_feature_count() == 3U);
  CHECK(document.dependency_graph().has_dependency("feature.base", "mirror.datum"));
  CHECK(document.dependency_graph().has_dependency("datum.xy", "mirror.datum"));
  CHECK(document.dependency_graph().has_dependency("body:body.base", "mirror.construction"));
  CHECK(document.dependency_graph().has_dependency("plane.mirror", "mirror.construction"));
  CHECK(document.dependency_graph().has_dependency("mirror.datum", "mirror.semantic"));
  CHECK(document.dependency_graph().has_dependency("feature.base", "mirror.semantic"));
  CHECK(document.dependency_graph().has_dependency("mirror.semantic", "body:body.semantic"));
  CHECK(document.remove_body(BodyId("body.base")).has_error());

  document.mark_all_clean();
  const auto affected = document.mark_feature_changed(FeatureId("feature.base"));
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "mirror.datum") !=
        affected.value().end());
  CHECK(std::find(affected.value().begin(), affected.value().end(), "mirror.semantic") !=
        affected.value().end());
  REQUIRE(document.invalidation_state().find("body:body.semantic") != nullptr);
  CHECK(document.invalidation_state().find("body:body.semantic")->status ==
        InvalidationStatus::Dirty);
}

TEST_CASE("Block 66 advances an in-place Mirror Body producer without a self cycle",
          "[core][mirror-feature]") {
  auto document = base_document();
  auto feature =
      MirrorFeature::create(FeatureId("mirror.in_place"), "InPlace", {body_source("body.base")},
                            datum_plane(), modifying_context("body.base"));
  REQUIRE(feature);
  REQUIRE(document.add_mirror_feature(feature.value()));
  CHECK(document.dependency_graph().has_dependency("feature.base", "mirror.in_place"));
  CHECK_FALSE(document.dependency_graph().has_dependency("body:body.base", "mirror.in_place"));
  CHECK(document.dependency_graph().has_dependency("mirror.in_place", "body:body.base"));
  CHECK_FALSE(document.dependency_graph().has_cycle());
}

TEST_CASE("Block 66 rejects missing Mirror sources and planes transactionally",
          "[core][mirror-feature]") {
  auto document = base_document();
  REQUIRE(document.add_body(body("body.mirror")));
  const std::size_t graph_nodes = document.dependency_graph().node_count();
  auto missing_feature = mirror("mirror.missing_feature", {feature_source("feature.missing")},
                                datum_plane(), "body.mirror");
  CHECK(document.add_mirror_feature(std::move(missing_feature)).has_error());
  auto missing_body =
      mirror("mirror.missing_body", {body_source("body.missing")}, datum_plane(), "body.mirror");
  CHECK(document.add_mirror_feature(std::move(missing_body)).has_error());
  auto absent_plane = PlaneReference::create_datum_plane(PartFeatureInputRole::MirrorPlane,
                                                         DatumPlaneId("datum.missing"));
  REQUIRE(absent_plane);
  auto missing_plane = mirror("mirror.missing_plane", {feature_source("feature.base")},
                              absent_plane.value(), "body.mirror");
  CHECK(document.add_mirror_feature(std::move(missing_plane)).has_error());
  CHECK(document.mirror_feature_count() == 0U);
  CHECK(document.dependency_graph().node_count() == graph_nodes);
}

TEST_CASE("Block 66 Mirror JSON roundtrips all plane kinds in dependency order",
          "[core][mirror-feature]") {
  auto document = base_document();
  REQUIRE(document.add_body(body("body.datum")));
  REQUIRE(document.add_body(body("body.construction")));
  REQUIRE(document.add_body(body("body.semantic")));
  REQUIRE(document.add_mirror_feature(
      mirror("mirror.datum", {feature_source("feature.base")}, datum_plane(), "body.datum")));
  REQUIRE(document.add_mirror_feature(mirror("mirror.construction", {body_source("body.base")},
                                             construction_plane(), "body.construction")));
  REQUIRE(document.add_mirror_feature(mirror("mirror.semantic", {feature_source("mirror.datum")},
                                             semantic_plane(), "body.semantic")));

  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  REQUIRE(root.at("mirror_features").size() == 3U);
  CHECK(root.at("mirror_features").at(0).at("id") == "mirror.datum");
  CHECK(root.at("mirror_features").at(1).at("id") == "mirror.construction");
  CHECK(root.at("mirror_features").at(2).at("id") == "mirror.semantic");
  CHECK(root.at("mirror_features").at(0).at("mirror_plane").at("source_kind") == "datum_plane");
  CHECK(root.at("mirror_features").at(1).at("mirror_plane").at("source_kind") ==
        "construction_plane");
  CHECK(root.at("mirror_features").at(2).at("mirror_plane").at("source_kind") ==
        "semantic_planar_face");

  const auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().mirror_feature_count() == 3U);
  const auto* semantic = restored.value().find_mirror_feature(FeatureId("mirror.semantic"));
  REQUIRE(semantic != nullptr);
  CHECK(semantic->sources().front().source_node_id() == "mirror.datum");
  CHECK(semantic->mirror_plane().source_kind() == PartFeatureInputSourceKind::SemanticPlanarFace);
  CHECK(semantic->body_result_context().effective_produced_body() == BodyId("body.semantic"));
}

TEST_CASE("Block 66 Mirror JSON is additive strict and backward compatible",
          "[core][mirror-feature]") {
  auto document = base_document();
  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto historical = nlohmann::json::parse(serialized.value());
  historical.erase("mirror_features");
  const auto restored = deserialize_part_document_from_json(historical.dump());
  REQUIRE(restored);
  CHECK(restored.value().mirror_feature_count() == 0U);
  const auto rewritten = serialize_part_document_to_json(restored.value());
  REQUIRE(rewritten);
  CHECK(nlohmann::json::parse(rewritten.value()).at("mirror_features").empty());

  REQUIRE(document.add_body(body("body.mirror")));
  REQUIRE(document.add_mirror_feature(
      mirror("mirror.main", {feature_source("feature.base")}, datum_plane(), "body.mirror")));
  const auto with_mirror = serialize_part_document_to_json(document);
  REQUIRE(with_mirror);
  auto malformed = nlohmann::json::parse(with_mirror.value());
  malformed["mirror_features"][0]["mirror_plane"]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  malformed = nlohmann::json::parse(with_mirror.value());
  malformed["mirror_features"][0]["sources"][0]["kind"] = "edge";
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  malformed = nlohmann::json::parse(with_mirror.value());
  malformed["mirror_features"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
}
