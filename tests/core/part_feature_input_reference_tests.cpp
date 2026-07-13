#include "blcad/core/part_feature_input_reference.hpp"

#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <type_traits>
#include <variant>

using namespace blcad;

namespace {

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument reference_document() {
  auto document = PartDocument::create(DocumentId("part.feature_inputs"), "FeatureInputs");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z", {}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  auto line = ConstructionLine::create_explicit(ConstructionLineId("line.axis"), "Axis", {},
                                                {0.0, 1.0, 0.0});
  REQUIRE(line);
  REQUIRE(document.value().add_construction_line(line.value()));
  auto plane =
      ConstructionPlane::create_explicit(ConstructionPlaneId("plane.mirror"), "Mirror", {},
                                         {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0});
  REQUIRE(plane);
  REQUIRE(document.value().add_construction_plane(plane.value()));
  REQUIRE(document.value().add_parameter(length_parameter("width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("height", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("depth", 5.0)));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("width"),
                                          ParameterId("height"));
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto body = Body::create(BodyId("body.base"), "Base", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.value().add_body(body.value()));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                                  SketchId("sketch.base"), ParameterId("depth"));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.base"));
  REQUIRE(context);
  auto attached = feature.value().with_body_result_context(context.value());
  REQUIRE(attached);
  REQUIRE(document.value().add_feature(attached.value()));
  return document.value();
}

SemanticFaceReference top_face() {
  auto value = SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(value);
  return value.value();
}

SemanticEdgeReference top_front_edge() {
  auto value = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(value);
  return value.value();
}

} // namespace

TEST_CASE("Part feature input roles capabilities and source kinds have stable spellings",
          "[core][part-feature-input-reference]") {
  CHECK(to_string(PartFeatureInputCapability::ProfileRegion) == "profile_region");
  CHECK(to_string(PartFeatureInputCapability::Axis) == "axis");
  CHECK(to_string(PartFeatureInputCapability::Line) == "line");
  CHECK(to_string(PartFeatureInputCapability::Plane) == "plane");
  CHECK(to_string(PartFeatureInputCapability::Face) == "face");
  CHECK(to_string(PartFeatureInputCapability::Edge) == "edge");
  CHECK(to_string(PartFeatureInputCapability::Body) == "body");
  CHECK(to_string(PartFeatureInputRole::RevolveAxis) == "revolve_axis");
  CHECK(to_string(PartFeatureInputRole::MirrorPlane) == "mirror_plane");
  CHECK(to_string(PartFeatureInputRole::FilletEdge) == "fillet_edge");
  CHECK(to_string(PartFeatureInputRole::ShellRemovalFace) == "shell_removal_face");
  CHECK(to_string(PartFeatureInputRole::DraftPullDirection) == "draft_pull_direction");
  CHECK(to_string(PartFeatureInputSourceKind::SemanticCircularEdge) == "semantic_circular_edge");

  CHECK(part_feature_input_role_accepts(PartFeatureInputRole::RevolveAxis,
                                        PartFeatureInputCapability::Axis));
  CHECK_FALSE(part_feature_input_role_accepts(PartFeatureInputRole::RevolveAxis,
                                              PartFeatureInputCapability::Line));
  CHECK(part_feature_input_role_accepts(PartFeatureInputRole::DraftPullDirection,
                                        PartFeatureInputCapability::Axis));
  CHECK(part_feature_input_role_accepts(PartFeatureInputRole::DraftPullDirection,
                                        PartFeatureInputCapability::Line));
}

TEST_CASE("References retain source identity capability and feature-specific role separately",
          "[core][part-feature-input-reference]") {
  auto profile = ProfileRegionReference::create(SketchId("sketch.base"), ProfileId("profile.base"),
                                                PartFeatureInputRole::RevolveProfile);
  REQUIRE(profile);
  CHECK(profile.value().source_kind() == PartFeatureInputSourceKind::SketchProfileRegion);
  CHECK(profile.value().expected_capability() == PartFeatureInputCapability::ProfileRegion);
  CHECK(profile.value().role() == PartFeatureInputRole::RevolveProfile);
  CHECK(profile.value().source_node_id() == "sketch.base");

  auto mirror_plane =
      PlaneReference::create_semantic_face(PartFeatureInputRole::MirrorPlane, top_face());
  auto removal_face =
      FaceReference::create_planar(PartFeatureInputRole::ShellRemovalFace, top_face());
  REQUIRE(mirror_plane);
  REQUIRE(removal_face);
  CHECK(mirror_plane.value().source_kind() == PartFeatureInputSourceKind::SemanticPlanarFace);
  CHECK(removal_face.value().source_kind() == PartFeatureInputSourceKind::SemanticPlanarFace);
  CHECK(mirror_plane.value().expected_capability() == PartFeatureInputCapability::Plane);
  CHECK(removal_face.value().expected_capability() == PartFeatureInputCapability::Face);
  CHECK(mirror_plane.value().source_node_id() == "feature.base");
  CHECK(removal_face.value().source_node_id() == "feature.base");

  auto fillet = EdgeReference::create_linear(PartFeatureInputRole::FilletEdge, top_front_edge());
  REQUIRE(fillet);
  CHECK(fillet.value().expected_capability() == PartFeatureInputCapability::Edge);
  CHECK(fillet.value().source_kind() == PartFeatureInputSourceKind::SemanticLinearEdge);
  CHECK(std::holds_alternative<SemanticEdgeReference>(fillet.value().source()));

  auto cylindrical_identity = SemanticCylindricalFaceReference::create(FeatureId("feature.hole"),
                                                                       ProfileId("profile.hole"));
  auto circular_identity = SemanticCircularEdgeReference::create(
      FeatureId("feature.hole"), ProfileId("profile.hole"), SemanticCircularEdge::OppositeRim);
  REQUIRE(cylindrical_identity);
  REQUIRE(circular_identity);
  auto draft_face = FaceReference::create_cylindrical(PartFeatureInputRole::DraftFace,
                                                      cylindrical_identity.value());
  auto chamfer_edge =
      EdgeReference::create_circular(PartFeatureInputRole::ChamferEdge, circular_identity.value());
  REQUIRE(draft_face);
  REQUIRE(chamfer_edge);
  CHECK(draft_face.value().source_kind() == PartFeatureInputSourceKind::SemanticCylindricalFace);
  CHECK(chamfer_edge.value().source_kind() == PartFeatureInputSourceKind::SemanticCircularEdge);
  CHECK(std::get<SemanticCylindricalFaceReference>(draft_face.value().source()).source_profile() ==
        ProfileId("profile.hole"));
  CHECK(std::get<SemanticCircularEdgeReference>(chamfer_edge.value().source()).edge() ==
        SemanticCircularEdge::OppositeRim);
}

TEST_CASE("AxisReference distinguishes Axis and Line capability over semantic typed sources",
          "[core][part-feature-input-reference]") {
  auto revolve = AxisReference::create_construction_line(PartFeatureInputRole::RevolveAxis,
                                                         ConstructionLineId("line.axis"),
                                                         PartFeatureInputCapability::Axis);
  auto draft = AxisReference::create_construction_line(PartFeatureInputRole::DraftPullDirection,
                                                       ConstructionLineId("line.axis"),
                                                       PartFeatureInputCapability::Line);
  REQUIRE(revolve);
  REQUIRE(draft);
  CHECK(revolve.value().expected_capability() == PartFeatureInputCapability::Axis);
  CHECK(draft.value().expected_capability() == PartFeatureInputCapability::Line);
  CHECK(revolve.value().source_kind() == PartFeatureInputSourceKind::ConstructionLine);
  CHECK(draft.value().source_kind() == PartFeatureInputSourceKind::ConstructionLine);
  CHECK(revolve.value().source_node_id() == "line.axis");

  auto semantic_axis = SemanticAxisReference::create(FeatureId("feature.base"));
  REQUIRE(semantic_axis);
  auto pattern =
      AxisReference::create_semantic_axis(PartFeatureInputRole::PatternAxis, semantic_axis.value());
  REQUIRE(pattern);
  CHECK(pattern.value().source_kind() == PartFeatureInputSourceKind::SemanticAxis);
  CHECK(pattern.value().source_node_id() == "feature.base");
}

TEST_CASE("Reference factories reject empty identity and role-capability mismatches",
          "[core][part-feature-input-reference]") {
  CHECK(ProfileRegionReference::create(SketchId(), ProfileId("profile.base")).has_error());
  CHECK(ProfileRegionReference::create(SketchId("sketch.base"), ProfileId()).has_error());
  CHECK(ProfileRegionReference::create(SketchId("sketch.base"), ProfileId("profile.base"),
                                       PartFeatureInputRole::FilletEdge)
            .has_error());
  CHECK(AxisReference::create_datum_axis(PartFeatureInputRole::FilletEdge, DatumAxisId("axis.z"))
            .has_error());
  CHECK(AxisReference::create_construction_line(PartFeatureInputRole::DraftPullDirection,
                                                ConstructionLineId("line.axis"),
                                                PartFeatureInputCapability::Face)
            .has_error());
  CHECK(PlaneReference::create_datum_plane(PartFeatureInputRole::ShellRemovalFace,
                                           DatumPlaneId("datum.xy"))
            .has_error());
  CHECK(EdgeReference::create_linear(PartFeatureInputRole::MirrorPlane, top_front_edge())
            .has_error());
  CHECK(BodyReference::create(PartFeatureInputRole::TargetBody, BodyId()).has_error());

  static_assert(!std::is_constructible_v<AxisReference, std::string>);
  static_assert(!std::is_constructible_v<FaceReference, std::string>);
  static_assert(!std::is_constructible_v<EdgeReference, std::string>);
}

TEST_CASE("Part feature input validation resolves typed document and generated topology sources",
          "[core][part-feature-input-reference]") {
  const PartDocument document = reference_document();
  auto profile = ProfileRegionReference::create(SketchId("sketch.base"), ProfileId("profile.base"));
  auto axis =
      AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis, DatumAxisId("axis.z"));
  auto line = AxisReference::create_construction_line(PartFeatureInputRole::DraftPullDirection,
                                                      ConstructionLineId("line.axis"),
                                                      PartFeatureInputCapability::Line);
  auto plane = PlaneReference::create_construction_plane(PartFeatureInputRole::MirrorPlane,
                                                         ConstructionPlaneId("plane.mirror"));
  auto face = FaceReference::create_planar(PartFeatureInputRole::ShellRemovalFace, top_face());
  auto edge = EdgeReference::create_linear(PartFeatureInputRole::FilletEdge, top_front_edge());
  auto body = BodyReference::create(PartFeatureInputRole::TargetBody, BodyId("body.base"));
  REQUIRE(profile);
  REQUIRE(axis);
  REQUIRE(line);
  REQUIRE(plane);
  REQUIRE(face);
  REQUIRE(edge);
  REQUIRE(body);
  REQUIRE(validate_part_feature_input_reference(document, profile.value()));
  REQUIRE(validate_part_feature_input_reference(document, axis.value()));
  REQUIRE(validate_part_feature_input_reference(document, line.value()));
  REQUIRE(validate_part_feature_input_reference(document, plane.value()));
  REQUIRE(validate_part_feature_input_reference(document, face.value()));
  REQUIRE(validate_part_feature_input_reference(document, edge.value()));
  REQUIRE(validate_part_feature_input_reference(document, body.value()));

  auto missing_body =
      BodyReference::create(PartFeatureInputRole::TargetBody, BodyId("body.missing"));
  auto missing_profile =
      ProfileRegionReference::create(SketchId("sketch.base"), ProfileId("profile.missing"));
  REQUIRE(missing_body);
  REQUIRE(missing_profile);
  CHECK(validate_part_feature_input_reference(document, missing_body.value()).has_error());
  CHECK(validate_part_feature_input_reference(document, missing_profile.value()).has_error());
}
