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

Body body() {
  auto value = Body::create(BodyId("body.base"), "Base", BodyKind::Solid);
  REQUIRE(value);
  return value.value();
}

FaceReference planar_face(SemanticFace face, FeatureId source = FeatureId("feature.base")) {
  auto semantic = SemanticFaceReference::create(std::move(source), face);
  REQUIRE(semantic);
  auto reference =
      FaceReference::create_planar(PartFeatureInputRole::ShellRemovalFace, semantic.value());
  REQUIRE(reference);
  return reference.value();
}

FaceReference cylindrical_face() {
  auto semantic = SemanticCylindricalFaceReference::create(FeatureId("feature.hole"),
                                                           ProfileId("profile.hole"));
  REQUIRE(semantic);
  auto reference =
      FaceReference::create_cylindrical(PartFeatureInputRole::ShellRemovalFace, semantic.value());
  REQUIRE(reference);
  return reference.value();
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
  auto document = PartDocument::create(DocumentId("part.shell"), "Shell");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 12.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 8.0)));
  REQUIRE(document.value().add_parameter(length_parameter("shell.thickness", 1.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                            ParameterId("base.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  REQUIRE(document.value().add_body(body()));
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

ShellFeature shell(const char* id, std::vector<FaceReference> faces,
                   ShellDirection direction = ShellDirection::Inward,
                   const char* thickness = "shell.thickness") {
  auto value = ShellFeature::create(FeatureId(id), id, BodyId("body.base"), std::move(faces),
                                    ParameterId(thickness), direction);
  REQUIRE(value);
  return value.value();
}

} // namespace

TEST_CASE("Block 71 freezes ordered ShellFeature intent and thickness direction semantics",
          "[core][shell-feature]") {
  const auto top = planar_face(SemanticFace::Top);
  const auto front = planar_face(SemanticFace::Front);
  auto feature =
      ShellFeature::create(FeatureId("shell.main"), "Shell", BodyId("body.base"), {top, front},
                           ParameterId("shell.thickness"), ShellDirection::Outward);
  REQUIRE(feature);
  CHECK(feature.value().removed_faces().size() == 2U);
  CHECK(std::get<SemanticFaceReference>(feature.value().removed_faces()[0].source()).face() ==
        SemanticFace::Top);
  CHECK(std::get<SemanticFaceReference>(feature.value().removed_faces()[1].source()).face() ==
        SemanticFace::Front);
  CHECK(feature.value().direction() == ShellDirection::Outward);
  CHECK(to_string(ShellDirection::Inward) == "inward");
  CHECK(to_string(ShellDirection::Outward) == "outward");

  CHECK(ShellFeature::create(FeatureId("shell.empty"), "Shell", BodyId("body.base"), {},
                             ParameterId("shell.thickness"), ShellDirection::Inward)
            .has_error());
  CHECK(ShellFeature::create(FeatureId("shell.duplicate"), "Shell", BodyId("body.base"), {top, top},
                             ParameterId("shell.thickness"), ShellDirection::Inward)
            .has_error());
  auto wrong_semantic = SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(wrong_semantic);
  auto wrong_role =
      FaceReference::create_planar(PartFeatureInputRole::DraftFace, wrong_semantic.value());
  REQUIRE(wrong_role);
  CHECK(ShellFeature::create(FeatureId("shell.role"), "Shell", BodyId("body.base"),
                             {wrong_role.value()}, ParameterId("shell.thickness"),
                             ShellDirection::Inward)
            .has_error());
}

TEST_CASE("Block 71 integrates ShellFeature body history dependencies and invalidation",
          "[core][shell-feature]") {
  auto document = base_document();
  REQUIRE(document.add_shell_feature(
      shell("shell.main", {planar_face(SemanticFace::Top), planar_face(SemanticFace::Front)})));
  CHECK(document.shell_feature_count() == 1U);
  CHECK(document.find_shell_feature(FeatureId("shell.main")) != nullptr);
  CHECK(document.dependency_graph().has_dependency("feature.base", "shell.main"));
  CHECK(document.dependency_graph().has_dependency("shell.thickness", "shell.main"));
  CHECK(document.dependency_graph().has_dependency("shell.main", "body:body.base"));
  CHECK(document.remove_body(BodyId("body.base")).has_error());

  document.mark_all_clean();
  auto thickness = Quantity::length_mm(2.0, "shell.thickness");
  REQUIRE(thickness);
  auto affected = document.set_parameter_value(ParameterId("shell.thickness"), thickness.value());
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "shell.main") !=
        affected.value().end());
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(std::any_of(plan.value().steps().begin(), plan.value().steps().end(),
                    [](const auto& step) { return step.node_id == "shell.main"; }));
}

TEST_CASE("Block 71 validates planar cylindrical body and parameter references transactionally",
          "[core][shell-feature]") {
  auto document = base_document(true);
  REQUIRE(document.add_shell_feature(
      shell("shell.hole", {planar_face(SemanticFace::Top), cylindrical_face()})));
  CHECK(document.dependency_graph().has_dependency("feature.hole", "shell.hole"));

  auto missing =
      shell("shell.missing", {planar_face(SemanticFace::Top, FeatureId("feature.missing"))});
  CHECK(document.add_shell_feature(missing).has_error());
  CHECK(document.shell_feature_count() == 1U);
  CHECK(!document.dependency_graph().has_node("shell.missing"));

  auto invalid = base_document();
  auto count_quantity = Quantity::count(2.0, "shell.count");
  REQUIRE(count_quantity);
  auto count =
      Parameter::create_count(ParameterId("shell.count"), "shell.count", count_quantity.value());
  REQUIRE(count);
  REQUIRE(invalid.add_parameter(count.value()));
  CHECK(invalid
            .add_shell_feature(shell("shell.wrong-unit", {planar_face(SemanticFace::Top)},
                                     ShellDirection::Inward, "shell.count"))
            .has_error());
  CHECK(invalid.shell_feature_count() == 0U);
}

TEST_CASE("Block 71 ShellFeature JSON preserves order direction and body history",
          "[core][shell-feature][integration][part-construction-mvp]") {
  auto document = base_document();
  REQUIRE(document.add_shell_feature(
      shell("shell.first", {planar_face(SemanticFace::Top), planar_face(SemanticFace::Front)},
            ShellDirection::Inward)));
  REQUIRE(document.add_shell_feature(
      shell("shell.second", {planar_face(SemanticFace::Back)}, ShellDirection::Outward)));
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto json = nlohmann::json::parse(serialized.value());
  REQUIRE(json.at("shell_features").size() == 2U);
  CHECK(json.at("shell_features")[0].at("id") == "shell.first");
  CHECK(json.at("shell_features")[0].at("removed_faces")[0].at("face") == "top");
  CHECK(json.at("shell_features")[0].at("removed_faces")[1].at("face") == "front");
  CHECK(json.at("shell_features")[1].at("direction") == "outward");

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().shell_feature_count() == 2U);
  CHECK(restored.value().shell_features()[0].id().value() == "shell.first");
  CHECK(restored.value().shell_features()[1].direction() == ShellDirection::Outward);
  CHECK(restored.value().dependency_graph().has_dependency("shell.first", "shell.second"));
  auto repeated = serialize_part_document_to_json(restored.value());
  REQUIRE(repeated);
  CHECK(repeated.value() == serialized.value());
}

TEST_CASE("Block 71 ShellFeature JSON is strict and backward compatible", "[core][shell-feature]") {
  auto document = base_document();
  REQUIRE(document.add_shell_feature(
      shell("shell.main", {planar_face(SemanticFace::Top)}, ShellDirection::Inward)));
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto json = nlohmann::json::parse(serialized.value());

  auto historical = json;
  historical.erase("shell_features");
  auto historical_result = deserialize_part_document_from_json(historical.dump());
  REQUIRE(historical_result);
  CHECK(historical_result.value().shell_feature_count() == 0U);

  auto unknown = json;
  unknown["shell_features"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(unknown.dump()).has_error());
  auto wrong_direction = json;
  wrong_direction["shell_features"][0]["direction"] = "sideways";
  CHECK(deserialize_part_document_from_json(wrong_direction.dump()).has_error());
  auto wrong_role = json;
  wrong_role["shell_features"][0]["removed_faces"][0]["role"] = "draft_face";
  CHECK(deserialize_part_document_from_json(wrong_role.dump()).has_error());
  auto not_an_array = json;
  not_an_array["shell_features"] = nlohmann::json::object();
  CHECK(deserialize_part_document_from_json(not_an_array.dump()).has_error());
}
