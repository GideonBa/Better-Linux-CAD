#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <limits>
#include <optional>
#include <string>

using namespace blcad;

namespace {

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument transform_document() {
  auto document = PartDocument::create(DocumentId("part.transforms"), "Transforms");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_parameter(length_parameter("part.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("part.height", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("part.depth", 5.0)));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"),
                                             ParameterId("part.width"),
                                             ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto body = Body::create(BodyId("body.base"), "Base", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.value().add_body(body.value()));
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.base"));
  REQUIRE(context);
  auto attached = feature.value().with_body_result_context(context.value());
  REQUIRE(attached);
  REQUIRE(document.value().add_feature(attached.value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z", Point3{},
                                         Vector3{0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  return document.value();
}

BodyTransform translate(const char* id, bool apply_sketches = false) {
  auto transform = BodyTransform::create_translate(
      BodyTransformId(id), BodyId("body.base"), BodyTransformCoordinateSpace::World, std::nullopt,
      Vector3{10.0, -2.0, 3.0}, apply_sketches, false);
  REQUIRE(transform);
  return transform.value();
}

BodyTransform rotate(const char* id, bool apply_sketches = false) {
  auto axis = BodyTransformRotationAxis::create_datum_axis(DatumAxisId("axis.z"));
  REQUIRE(axis);
  auto transform = BodyTransform::create_rotate(
      BodyTransformId(id), BodyId("body.base"), BodyTransformCoordinateSpace::BodyLocal,
      std::nullopt, axis.value(), 45.0, apply_sketches, true);
  REQUIRE(transform);
  return transform.value();
}

BodyTransform scale(const char* id) {
  auto transform = BodyTransform::create_uniform_scale(
      BodyTransformId(id), BodyId("body.base"), BodyTransformCoordinateSpace::SketchLocal,
      std::string("sketch.base"), 1.5, Point3{1.0, 2.0, 3.0}, true, false);
  REQUIRE(transform);
  return transform.value();
}

SketchOwnership ownership(SketchAssociation association = SketchAssociation::DrivesBody) {
  auto value = SketchOwnership::create(SketchId("sketch.base"), BodyId("body.base"), association);
  REQUIRE(value);
  return value.value();
}

} // namespace

TEST_CASE("BodyTransform freezes kinds spaces axis identities and direct parameters",
          "[core][body-transform]") {
  CHECK(to_string(BodyTransformKind::Translate) == "translate");
  CHECK(to_string(BodyTransformKind::Rotate) == "rotate");
  CHECK(to_string(BodyTransformKind::UniformScale) == "uniform_scale");
  CHECK(to_string(BodyTransformCoordinateSpace::World) == "world");
  CHECK(to_string(BodyTransformCoordinateSpace::BodyLocal) == "body_local");
  CHECK(to_string(BodyTransformCoordinateSpace::SketchLocal) == "sketch_local");
  CHECK(to_string(BodyTransformCoordinateSpace::ConstructionReference) ==
        "construction_reference");
  CHECK(to_string(BodyTransformRotationAxisKind::Explicit) == "explicit");
  CHECK(to_string(BodyTransformRotationAxisKind::DatumAxis) == "datum_axis");
  CHECK(to_string(BodyTransformRotationAxisKind::ConstructionLine) == "construction_line");
  CHECK(to_string(BodyTransformRotationAxisKind::SemanticEdge) == "semantic_edge");

  const BodyTransform translation = translate("transform.translate", true);
  CHECK(translation.kind() == BodyTransformKind::Translate);
  CHECK(translation.translation_mm() == Vector3{10.0, -2.0, 3.0});
  CHECK(translation.apply_to_owned_sketches());
  CHECK_FALSE(translation.apply_to_owned_construction_geometry());

  const BodyTransform rotation = rotate("transform.rotate");
  CHECK(rotation.kind() == BodyTransformKind::Rotate);
  REQUIRE(rotation.rotation_axis().has_value());
  CHECK(rotation.rotation_axis()->kind() == BodyTransformRotationAxisKind::DatumAxis);
  CHECK(rotation.rotation_axis()->datum_axis().value() == "axis.z");
  CHECK(rotation.angle_deg() == 45.0);

  const BodyTransform scaling = scale("transform.scale");
  CHECK(scaling.kind() == BodyTransformKind::UniformScale);
  CHECK(scaling.coordinate_reference() == std::optional<std::string>("sketch.base"));
  CHECK(scaling.scale_factor() == 1.5);
  CHECK(scaling.scale_center() == Point3{1.0, 2.0, 3.0});

  auto explicit_axis = BodyTransformRotationAxis::create_explicit(
      Point3{1.0, 2.0, 3.0}, Vector3{0.0, 1.0, 0.0});
  REQUIRE(explicit_axis);
  CHECK(explicit_axis.value().kind() == BodyTransformRotationAxisKind::Explicit);
  auto semantic = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(semantic);
  REQUIRE(BodyTransformRotationAxis::create_semantic_edge(semantic.value()));
  REQUIRE(BodyTransformRotationAxis::create_construction_line(
      ConstructionLineId("construction.line")));
}

TEST_CASE("BodyTransform and SketchOwnership reject incomplete or unsafe intent",
          "[core][body-transform][core][sketch-ownership]") {
  CHECK(BodyTransform::create_translate(
            BodyTransformId(), BodyId("body.base"), BodyTransformCoordinateSpace::World,
            std::nullopt, {}, false, false)
            .has_error());
  CHECK(BodyTransform::create_translate(
            BodyTransformId("transform.missing_body"), BodyId(),
            BodyTransformCoordinateSpace::World, std::nullopt, {}, false, false)
            .has_error());
  CHECK(BodyTransform::create_translate(
            BodyTransformId("transform.world_ref"), BodyId("body.base"),
            BodyTransformCoordinateSpace::World, std::string("sketch.base"), {}, false, false)
            .has_error());
  CHECK(BodyTransform::create_translate(
            BodyTransformId("transform.sketch_missing"), BodyId("body.base"),
            BodyTransformCoordinateSpace::SketchLocal, std::nullopt, {}, false, false)
            .has_error());
  CHECK(BodyTransform::create_translate(
            BodyTransformId("transform.nan"), BodyId("body.base"),
            BodyTransformCoordinateSpace::World, std::nullopt,
            Vector3{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0}, false, false)
            .has_error());
  CHECK(BodyTransformRotationAxis::create_explicit({}, {}).has_error());
  CHECK(BodyTransformRotationAxis::create_datum_axis(DatumAxisId()).has_error());
  CHECK(BodyTransform::create_uniform_scale(
            BodyTransformId("transform.scale"), BodyId("body.base"),
            BodyTransformCoordinateSpace::World, std::nullopt, 0.0, {}, false, false)
            .has_error());
  CHECK(SketchOwnership::create(SketchId(), BodyId("body.base"),
                                SketchAssociation::DrivesBody)
            .has_error());
  CHECK(SketchOwnership::create(SketchId("sketch.base"), BodyId(),
                                SketchAssociation::DrivesBody)
            .has_error());
}

TEST_CASE("PartDocument preserves transform stack order dependencies invalidation and removal",
          "[core][body-transform][core][sketch-ownership]") {
  PartDocument document = transform_document();
  REQUIRE(document.add_sketch_ownership(ownership()));
  REQUIRE(document.add_body_transform(translate("transform.1", true)));
  REQUIRE(document.add_body_transform(rotate("transform.2", true)));
  REQUIRE(document.add_body_transform(scale("transform.3")));

  REQUIRE(document.body_transform_count() == 3U);
  CHECK(document.body_transforms()[0].id().value() == "transform.1");
  CHECK(document.body_transforms()[1].id().value() == "transform.2");
  CHECK(document.body_transforms()[2].id().value() == "transform.3");
  CHECK(document.sketch_ownership_count() == 1U);
  CHECK(document.dependency_graph().has_dependency("feature.base", "transform.1"));
  CHECK(document.dependency_graph().has_dependency("transform.1", "transform.2"));
  CHECK(document.dependency_graph().has_dependency("transform.2", "transform.3"));
  CHECK(document.dependency_graph().has_dependency("transform.3", "body:body.base"));
  CHECK(document.dependency_graph().has_dependency("axis.z", "transform.2"));
  CHECK(document.dependency_graph().has_dependency("sketch.base", "transform.3"));
  CHECK(document.dependency_graph().has_dependency("sketch_ownership:sketch.base",
                                                   "transform.1"));

  document.mark_all_clean();
  auto changed = document.mark_body_transform_changed(BodyTransformId("transform.1"));
  REQUIRE(changed);
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("transform.2"));
  CHECK(plan.value().contains("transform.3"));
  CHECK(plan.value().contains("body:body.base"));
  CHECK(document.remove_body(BodyId("body.base")).has_error());
}

TEST_CASE("PartDocument validates transform and ownership references transactionally",
          "[core][body-transform][core][sketch-ownership]") {
  PartDocument document = transform_document();
  REQUIRE(document.add_sketch_ownership(ownership(SketchAssociation::ConsumedByBody)));
  CHECK(document.add_sketch_ownership(ownership()).has_error());

  auto missing_sketch = BodyTransform::create_uniform_scale(
      BodyTransformId("transform.missing_sketch"), BodyId("body.base"),
      BodyTransformCoordinateSpace::SketchLocal, std::string("sketch.missing"), 2.0, {}, false,
      false);
  REQUIRE(missing_sketch);
  CHECK(document.add_body_transform(missing_sketch.value()).has_error());

  auto missing_axis = BodyTransformRotationAxis::create_datum_axis(DatumAxisId("axis.missing"));
  REQUIRE(missing_axis);
  auto invalid_rotate = BodyTransform::create_rotate(
      BodyTransformId("transform.missing_axis"), BodyId("body.base"),
      BodyTransformCoordinateSpace::World, std::nullopt, missing_axis.value(), 10.0, false, false);
  REQUIRE(invalid_rotate);
  CHECK(document.add_body_transform(invalid_rotate.value()).has_error());

  REQUIRE(document.add_body_transform(translate("transform.unique")));
  CHECK(document.add_body_transform(translate("transform.unique")).has_error());
  CHECK(document.body_transform_count() == 1U);
}

TEST_CASE("PartDocument connects construction and semantic transform references",
          "[core][body-transform]") {
  PartDocument document = transform_document();
  auto line = ConstructionLine::create_explicit(ConstructionLineId("line.axis"), "Axis",
                                                Point3{}, Vector3{0.0, 1.0, 0.0});
  REQUIRE(line);
  REQUIRE(document.add_construction_line(line.value()));

  auto construction_translate = BodyTransform::create_translate(
      BodyTransformId("transform.construction"), BodyId("body.base"),
      BodyTransformCoordinateSpace::ConstructionReference, std::string("line.axis"),
      Vector3{0.0, 5.0, 0.0}, false, true);
  REQUIRE(construction_translate);
  REQUIRE(document.add_body_transform(construction_translate.value()));
  CHECK(document.dependency_graph().has_dependency("line.axis", "transform.construction"));

  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto edge_axis = BodyTransformRotationAxis::create_semantic_edge(edge.value());
  REQUIRE(edge_axis);
  auto semantic_rotate = BodyTransform::create_rotate(
      BodyTransformId("transform.semantic"), BodyId("body.base"),
      BodyTransformCoordinateSpace::World, std::nullopt, edge_axis.value(), 20.0, false, false);
  REQUIRE(semantic_rotate);
  REQUIRE(document.add_body_transform(semantic_rotate.value()));
  CHECK(document.dependency_graph().has_dependency("feature.base", "transform.semantic"));
  CHECK(document.dependency_graph().has_dependency("transform.construction",
                                                   "transform.semantic"));
}

TEST_CASE("BodyTransform and SketchOwnership JSON are additive exact and ordered",
          "[core][body-transform][core][sketch-ownership]") {
  PartDocument document = transform_document();
  REQUIRE(document.add_sketch_ownership(ownership(SketchAssociation::ReferenceOnly)));
  REQUIRE(document.add_body_transform(translate("transform.translate", true)));
  REQUIRE(document.add_body_transform(rotate("transform.rotate", true)));
  REQUIRE(document.add_body_transform(scale("transform.scale")));

  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto root = nlohmann::json::parse(serialized.value());
  REQUIRE(root.at("sketch_ownerships").size() == 1U);
  CHECK(root.at("sketch_ownerships").at(0).at("association") == "reference_only");
  REQUIRE(root.at("body_transforms").size() == 3U);
  CHECK(root.at("body_transforms").at(0).at("id") == "transform.translate");
  CHECK(root.at("body_transforms").at(0).at("translation_mm") ==
        nlohmann::json{{"x", 10.0}, {"y", -2.0}, {"z", 3.0}});
  CHECK(root.at("body_transforms").at(1).at("rotation_axis").at("kind") == "datum_axis");
  CHECK(root.at("body_transforms").at(1).at("rotation_axis").at("datum_axis") == "axis.z");
  CHECK(root.at("body_transforms").at(2).at("coordinate_reference") == "sketch.base");
  CHECK(root.at("body_transforms").at(2).at("scale_factor") == 1.5);

  const auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().sketch_ownerships() == document.sketch_ownerships());
  REQUIRE(restored.value().body_transform_count() == 3U);
  CHECK(restored.value().body_transforms()[0].id().value() == "transform.translate");
  CHECK(restored.value().body_transforms()[1].rotation_axis()->datum_axis().value() == "axis.z");
  CHECK(restored.value().body_transforms()[2].scale_factor() == 1.5);

  auto historical = root;
  historical.erase("sketch_ownerships");
  historical.erase("body_transforms");
  const auto compatible = deserialize_part_document_from_json(historical.dump());
  REQUIRE(compatible);
  CHECK(compatible.value().sketch_ownership_count() == 0U);
  CHECK(compatible.value().body_transform_count() == 0U);
}

TEST_CASE("BodyTransform and SketchOwnership JSON reject malformed intent",
          "[core][body-transform][core][sketch-ownership]") {
  PartDocument document = transform_document();
  const auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const auto base = nlohmann::json::parse(serialized.value());

  auto wrong_container = base;
  wrong_container["body_transforms"] = nlohmann::json::object();
  CHECK(deserialize_part_document_from_json(wrong_container.dump()).has_error());

  auto bad_kind = base;
  bad_kind["body_transforms"] = nlohmann::json::array(
      {{{"id", "transform.bad"},
        {"body", "body.base"},
        {"kind", "matrix"},
        {"coordinate_space", "world"},
        {"apply_to_owned_sketches", false},
        {"apply_to_owned_construction_geometry", false}}});
  CHECK(deserialize_part_document_from_json(bad_kind.dump()).has_error());

  auto missing_reference = base;
  missing_reference["body_transforms"] = nlohmann::json::array(
      {{{"id", "transform.bad"},
        {"body", "body.base"},
        {"kind", "uniform_scale"},
        {"coordinate_space", "sketch_local"},
        {"scale_factor", 2.0},
        {"center", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}}},
        {"apply_to_owned_sketches", false},
        {"apply_to_owned_construction_geometry", false}}});
  CHECK(deserialize_part_document_from_json(missing_reference.dump()).has_error());

  auto bad_association = base;
  bad_association["sketch_ownerships"] = nlohmann::json::array(
      {{{"sketch", "sketch.base"},
        {"owning_body", "body.base"},
        {"association", "automatic"}}});
  CHECK(deserialize_part_document_from_json(bad_association.dump()).has_error());
}
