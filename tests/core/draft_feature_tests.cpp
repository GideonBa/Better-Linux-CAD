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

FaceReference face(SemanticFace value, FeatureId source = FeatureId("feature.base")) {
  auto semantic = SemanticFaceReference::create(std::move(source), value);
  REQUIRE(semantic);
  auto reference = FaceReference::create_planar(PartFeatureInputRole::DraftFace, semantic.value());
  REQUIRE(reference);
  return reference.value();
}

AxisReference datum_pull() {
  auto value = AxisReference::create_datum_axis(PartFeatureInputRole::DraftPullDirection,
                                                DatumAxisId("axis.pull"));
  REQUIRE(value);
  return value.value();
}

AxisReference line_pull(PartFeatureInputCapability capability = PartFeatureInputCapability::Line) {
  auto value = AxisReference::create_construction_line(PartFeatureInputRole::DraftPullDirection,
                                                       ConstructionLineId("line.pull"), capability);
  REQUIRE(value);
  return value.value();
}

PlaneReference datum_neutral() {
  auto value = PlaneReference::create_datum_plane(PartFeatureInputRole::DraftNeutralPlane,
                                                  DatumPlaneId("datum.xy"));
  REQUIRE(value);
  return value.value();
}

PlaneReference construction_neutral() {
  auto value = PlaneReference::create_construction_plane(PartFeatureInputRole::DraftNeutralPlane,
                                                         ConstructionPlaneId("plane.neutral"));
  REQUIRE(value);
  return value.value();
}

PartDocument base_document(double angle = 5.0) {
  auto document = PartDocument::create(DocumentId("part.draft"), "Draft");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 12.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 8.0)));
  REQUIRE(document.value().add_parameter(angle_parameter("draft.angle", angle)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.pull"), "Pull", {0.0, 0.0, 0.0},
                                         {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  auto line = ConstructionLine::create_explicit(
      ConstructionLineId("line.pull"), "Pull line", {0.0, 0.0, 0.0},
      {0.408248290463863, 0.408248290463863, 0.816496580927726});
  REQUIRE(line);
  REQUIRE(document.value().add_construction_line(line.value()));
  auto plane = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.neutral"), "Neutral",
                                                  {0.0, 0.0, 2.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
                                                  {0.0, 0.0, 1.0});
  REQUIRE(plane);
  REQUIRE(document.value().add_construction_plane(plane.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                            ParameterId("base.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto body = Body::create(BodyId("body.base"), "Base", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.value().add_body(body.value()));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.base"));
  REQUIRE(context);
  auto contextual = base.value().with_body_result_context(context.value());
  REQUIRE(contextual);
  REQUIRE(document.value().add_feature(contextual.value()));
  return document.value();
}

DraftFeature draft(const char* id, std::vector<FaceReference> faces, AxisReference pull,
                   PlaneReference neutral, const char* angle = "draft.angle") {
  auto value = DraftFeature::create(FeatureId(id), id, BodyId("body.base"), std::move(faces),
                                    std::move(pull), std::move(neutral), ParameterId(angle));
  REQUIRE(value);
  return value.value();
}
} // namespace

TEST_CASE("Block 73 freezes ordered DraftFeature references and signed angle authority",
          "[core][draft-feature]") {
  auto value =
      DraftFeature::create(FeatureId("draft.main"), "Draft", BodyId("body.base"),
                           {face(SemanticFace::Front), face(SemanticFace::Right)}, line_pull(),
                           construction_neutral(), ParameterId("draft.angle"));
  REQUIRE(value);
  CHECK(value.value().faces().size() == 2U);
  CHECK(value.value().pull_direction().expected_capability() == PartFeatureInputCapability::Line);
  CHECK(value.value().neutral_plane().source_kind() ==
        PartFeatureInputSourceKind::ConstructionPlane);
  CHECK(value.value().angle_parameter() == ParameterId("draft.angle"));

  CHECK(DraftFeature::create(FeatureId("draft.empty"), "Draft", BodyId("body.base"), {},
                             datum_pull(), datum_neutral(), ParameterId("draft.angle"))
            .has_error());
  const auto front = face(SemanticFace::Front);
  CHECK(DraftFeature::create(FeatureId("draft.duplicate"), "Draft", BodyId("body.base"),
                             {front, front}, datum_pull(), datum_neutral(),
                             ParameterId("draft.angle"))
            .has_error());
}

TEST_CASE("Block 73 integrates DraftFeature body history dependencies and invalidation",
          "[core][draft-feature]") {
  auto document = base_document();
  REQUIRE(document.add_draft_feature(
      draft("draft.main", {face(SemanticFace::Front)}, datum_pull(), construction_neutral())));
  CHECK(document.draft_feature_count() == 1U);
  CHECK(document.find_draft_feature(FeatureId("draft.main")) != nullptr);
  CHECK(document.dependency_graph().has_dependency("feature.base", "draft.main"));
  CHECK(document.dependency_graph().has_dependency("axis.pull", "draft.main"));
  CHECK(document.dependency_graph().has_dependency("plane.neutral", "draft.main"));
  CHECK(document.dependency_graph().has_dependency("draft.angle", "draft.main"));
  CHECK(document.dependency_graph().has_dependency("draft.main", "body:body.base"));
  CHECK(document.remove_body(BodyId("body.base")).has_error());

  document.mark_all_clean();
  auto negative = Quantity::angle_deg(-7.0, "draft.angle");
  REQUIRE(negative);
  auto affected = document.set_parameter_value(ParameterId("draft.angle"), negative.value());
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "draft.main") !=
        affected.value().end());
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(std::any_of(plan.value().steps().begin(), plan.value().steps().end(),
                    [](const auto& step) { return step.node_id == "draft.main"; }));
}

TEST_CASE("Block 73 validates DraftFeature references and angle range transactionally",
          "[core][draft-feature]") {
  auto document = base_document();
  auto missing = draft("draft.missing", {face(SemanticFace::Front, FeatureId("feature.missing"))},
                       datum_pull(), datum_neutral());
  CHECK(document.add_draft_feature(missing).has_error());
  CHECK(document.draft_feature_count() == 0U);
  CHECK(!document.dependency_graph().has_node("draft.missing"));

  auto zero = base_document(0.0);
  CHECK(zero.add_draft_feature(
                draft("draft.zero", {face(SemanticFace::Front)}, datum_pull(), datum_neutral()))
            .has_error());
  auto excessive = base_document(-90.0);
  CHECK(excessive
            .add_draft_feature(draft("draft.excessive", {face(SemanticFace::Front)}, datum_pull(),
                                     datum_neutral()))
            .has_error());
  auto wrong_unit = base_document();
  REQUIRE(wrong_unit.add_parameter(length_parameter("draft.length", 3.0)));
  CHECK(wrong_unit
            .add_draft_feature(draft("draft.unit", {face(SemanticFace::Front)}, datum_pull(),
                                     datum_neutral(), "draft.length"))
            .has_error());
}

TEST_CASE("Block 73 DraftFeature JSON preserves order pull capability and neutral plane",
          "[core][draft-feature][integration][part-construction-mvp]") {
  auto document = base_document(-5.0);
  REQUIRE(document.add_draft_feature(draft("draft.main",
                                           {face(SemanticFace::Front), face(SemanticFace::Right)},
                                           line_pull(), construction_neutral())));
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto json = nlohmann::json::parse(serialized.value());
  REQUIRE(json.at("draft_features").size() == 1U);
  const auto& record = json.at("draft_features")[0];
  CHECK(record.at("faces")[0].at("face") == "front");
  CHECK(record.at("faces")[1].at("face") == "right");
  CHECK(record.at("pull_direction").at("capability") == "line");
  CHECK(record.at("pull_direction").at("construction_line") == "line.pull");
  CHECK(record.at("neutral_plane").at("construction_plane") == "plane.neutral");

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().draft_feature_count() == 1U);
  CHECK(restored.value().draft_features()[0].faces().size() == 2U);
  CHECK(restored.value().draft_features()[0].pull_direction().expected_capability() ==
        PartFeatureInputCapability::Line);
  CHECK(restored.value().find_parameter(ParameterId("draft.angle"))->value().degrees() == -5.0);
  CHECK(restored.value().dependency_graph().has_dependency("draft.main", "body:body.base"));
}

TEST_CASE("Block 73 DraftFeature JSON is strict and backward compatible", "[core][draft-feature]") {
  auto document = base_document();
  REQUIRE(document.add_draft_feature(
      draft("draft.main", {face(SemanticFace::Front)}, datum_pull(), datum_neutral())));
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto json = nlohmann::json::parse(serialized.value());

  auto historical = json;
  historical.erase("draft_features");
  auto restored = deserialize_part_document_from_json(historical.dump());
  REQUIRE(restored);
  CHECK(restored.value().draft_feature_count() == 0U);

  auto unknown = json;
  unknown["draft_features"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(unknown.dump()).has_error());
  auto wrong_role = json;
  wrong_role["draft_features"][0]["faces"][0]["role"] = "shell_removal_face";
  CHECK(deserialize_part_document_from_json(wrong_role.dump()).has_error());
  auto wrong_capability = json;
  wrong_capability["draft_features"][0]["pull_direction"]["capability"] = "plane";
  CHECK(deserialize_part_document_from_json(wrong_capability.dump()).has_error());
  auto not_array = json;
  not_array["draft_features"] = nlohmann::json::object();
  CHECK(deserialize_part_document_from_json(not_array.dump()).has_error());
}
