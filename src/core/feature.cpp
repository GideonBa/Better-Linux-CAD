#include "blcad/core/feature.hpp"

#include <cmath>
#include <utility>

namespace blcad {

std::string_view to_string(FeatureType type) noexcept {
  switch (type) {
  case FeatureType::AdditiveExtrude:
    return "additive_extrude";
  case FeatureType::SubtractiveExtrude:
    return "subtractive_extrude";
  }

  return "additive_extrude";
}

std::string_view to_string(ExtrudeDirection direction) noexcept {
  switch (direction) {
  case ExtrudeDirection::SketchNormal:
    return "+Z";
  case ExtrudeDirection::OppositeSketchNormal:
    return "opposite_sketch_normal";
  case ExtrudeDirection::Path:
    return "path";
  }

  return "+Z";
}

std::string_view to_string(SubtractiveExtrudeDepth depth) noexcept {
  switch (depth) {
  case SubtractiveExtrudeDepth::ThroughAll:
    return "through_all";
  }

  return "through_all";
}

std::string_view to_string(ExtrudeExtentMode mode) noexcept {
  switch (mode) {
  case ExtrudeExtentMode::Distance:
    return "distance";
  case ExtrudeExtentMode::Symmetric:
    return "symmetric";
  case ExtrudeExtentMode::TwoSided:
    return "two_sided";
  case ExtrudeExtentMode::ThroughAll:
    return "through_all";
  case ExtrudeExtentMode::ToNext:
    return "to_next";
  case ExtrudeExtentMode::ToFace:
    return "to_face";
  case ExtrudeExtentMode::Between:
    return "between";
  }
  return "distance";
}

std::string_view to_string(ExtrudeThinMode mode) noexcept {
  switch (mode) {
  case ExtrudeThinMode::OneSided:
    return "one_sided";
  case ExtrudeThinMode::TwoSided:
    return "two_sided";
  case ExtrudeThinMode::MidPlane:
    return "mid_plane";
  }
  return "one_sided";
}

std::string_view to_string(FeatureBodyOperationMode mode) noexcept {
  switch (mode) {
  case FeatureBodyOperationMode::NewBody:
    return "new_body";
  case FeatureBodyOperationMode::Join:
    return "join";
  case FeatureBodyOperationMode::Cut:
    return "cut";
  case FeatureBodyOperationMode::Intersect:
    return "intersect";
  }
  return "new_body";
}

ExtrudeExtentIntent::ExtrudeExtentIntent(ExtrudeExtentMode mode,
                                         std::optional<ParameterId> first_distance_parameter,
                                         std::optional<ParameterId> second_distance_parameter,
                                         std::optional<FaceReference> first_face,
                                         std::optional<FaceReference> second_face)
    : mode_(mode), first_distance_parameter_(std::move(first_distance_parameter)),
      second_distance_parameter_(std::move(second_distance_parameter)),
      first_face_(std::move(first_face)), second_face_(std::move(second_face)) {}

Result<ExtrudeExtentIntent> ExtrudeExtentIntent::distance(ParameterId distance_parameter) {
  if (distance_parameter.empty())
    return Result<ExtrudeExtentIntent>::failure(
        Error::validation("extrude_extent", "distance extent requires a distance parameter"));
  return Result<ExtrudeExtentIntent>::success(
      ExtrudeExtentIntent(ExtrudeExtentMode::Distance, std::move(distance_parameter), std::nullopt,
                          std::nullopt, std::nullopt));
}

Result<ExtrudeExtentIntent> ExtrudeExtentIntent::symmetric(ParameterId distance_parameter) {
  if (distance_parameter.empty())
    return Result<ExtrudeExtentIntent>::failure(
        Error::validation("extrude_extent", "symmetric extent requires a distance parameter"));
  return Result<ExtrudeExtentIntent>::success(
      ExtrudeExtentIntent(ExtrudeExtentMode::Symmetric, std::move(distance_parameter), std::nullopt,
                          std::nullopt, std::nullopt));
}

Result<ExtrudeExtentIntent> ExtrudeExtentIntent::two_sided(ParameterId first_distance_parameter,
                                                           ParameterId second_distance_parameter) {
  if (first_distance_parameter.empty() || second_distance_parameter.empty())
    return Result<ExtrudeExtentIntent>::failure(
        Error::validation("extrude_extent", "two-sided extent requires two distance parameters"));
  if (first_distance_parameter == second_distance_parameter)
    return Result<ExtrudeExtentIntent>::failure(Error::validation(
        "extrude_extent", "two-sided extent distance parameters must be distinct"));
  return Result<ExtrudeExtentIntent>::success(
      ExtrudeExtentIntent(ExtrudeExtentMode::TwoSided, std::move(first_distance_parameter),
                          std::move(second_distance_parameter), std::nullopt, std::nullopt));
}

ExtrudeExtentIntent ExtrudeExtentIntent::through_all() {
  return ExtrudeExtentIntent(ExtrudeExtentMode::ThroughAll, std::nullopt, std::nullopt,
                             std::nullopt, std::nullopt);
}

ExtrudeExtentIntent ExtrudeExtentIntent::to_next() {
  return ExtrudeExtentIntent(ExtrudeExtentMode::ToNext, std::nullopt, std::nullopt, std::nullopt,
                             std::nullopt);
}

Result<ExtrudeExtentIntent> ExtrudeExtentIntent::to_face(FaceReference face) {
  if (face.role() != PartFeatureInputRole::ExtrudeLimitFace)
    return Result<ExtrudeExtentIntent>::failure(Error::validation(
        face.source_node_id(), "to-face extent requires an ExtrudeLimitFace reference"));
  return Result<ExtrudeExtentIntent>::success(ExtrudeExtentIntent(
      ExtrudeExtentMode::ToFace, std::nullopt, std::nullopt, std::move(face), std::nullopt));
}

Result<ExtrudeExtentIntent> ExtrudeExtentIntent::between(FaceReference first_face,
                                                         FaceReference second_face) {
  if (first_face.role() != PartFeatureInputRole::ExtrudeLimitFace ||
      second_face.role() != PartFeatureInputRole::ExtrudeLimitFace)
    return Result<ExtrudeExtentIntent>::failure(Error::validation(
        "extrude_extent", "between extent requires two ExtrudeLimitFace references"));
  if (first_face.source_kind() != PartFeatureInputSourceKind::SemanticPlanarFace ||
      second_face.source_kind() != PartFeatureInputSourceKind::SemanticPlanarFace)
    return Result<ExtrudeExtentIntent>::failure(Error::validation(
        "extrude_extent", "between extent requires two semantic planar face references"));
  const auto& first_identity = std::get<SemanticFaceReference>(first_face.source());
  const auto& second_identity = std::get<SemanticFaceReference>(second_face.source());
  if (first_identity.source_feature() == second_identity.source_feature() &&
      first_identity.face() == second_identity.face())
    return Result<ExtrudeExtentIntent>::failure(Error::validation(
        "extrude_extent", "between extent requires two distinct semantic planar faces"));
  return Result<ExtrudeExtentIntent>::success(
      ExtrudeExtentIntent(ExtrudeExtentMode::Between, std::nullopt, std::nullopt,
                          std::move(first_face), std::move(second_face)));
}

ExtrudeExtentMode ExtrudeExtentIntent::mode() const noexcept {
  return mode_;
}
const std::optional<ParameterId>& ExtrudeExtentIntent::first_distance_parameter() const noexcept {
  return first_distance_parameter_;
}
const std::optional<ParameterId>& ExtrudeExtentIntent::second_distance_parameter() const noexcept {
  return second_distance_parameter_;
}
const std::optional<FaceReference>& ExtrudeExtentIntent::first_face() const noexcept {
  return first_face_;
}
const std::optional<FaceReference>& ExtrudeExtentIntent::second_face() const noexcept {
  return second_face_;
}

ExtrudeThinIntent::ExtrudeThinIntent(ExtrudeThinMode mode, ParameterId first_thickness_parameter,
                                     std::optional<ParameterId> second_thickness_parameter)
    : mode_(mode), first_thickness_parameter_(std::move(first_thickness_parameter)),
      second_thickness_parameter_(std::move(second_thickness_parameter)) {}

Result<ExtrudeThinIntent>
ExtrudeThinIntent::create(ExtrudeThinMode mode, ParameterId first_thickness_parameter,
                          std::optional<ParameterId> second_thickness_parameter) {
  if (first_thickness_parameter.empty())
    return Result<ExtrudeThinIntent>::failure(
        Error::validation("extrude_thin", "thin feature requires a first thickness parameter"));
  if (mode == ExtrudeThinMode::TwoSided) {
    if (!second_thickness_parameter.has_value() || second_thickness_parameter->empty())
      return Result<ExtrudeThinIntent>::failure(Error::validation(
          "extrude_thin", "two-sided thin feature requires a second thickness parameter"));
    if (first_thickness_parameter == *second_thickness_parameter)
      return Result<ExtrudeThinIntent>::failure(Error::validation(
          "extrude_thin", "two-sided thin thickness parameters must be distinct"));
  } else if (second_thickness_parameter.has_value()) {
    return Result<ExtrudeThinIntent>::failure(Error::validation(
        "extrude_thin", "one-sided and mid-plane thin features forbid second thickness"));
  }
  return Result<ExtrudeThinIntent>::success(ExtrudeThinIntent(
      mode, std::move(first_thickness_parameter), std::move(second_thickness_parameter)));
}

ExtrudeThinMode ExtrudeThinIntent::mode() const noexcept {
  return mode_;
}
const ParameterId& ExtrudeThinIntent::first_thickness_parameter() const noexcept {
  return first_thickness_parameter_;
}
const std::optional<ParameterId>& ExtrudeThinIntent::second_thickness_parameter() const noexcept {
  return second_thickness_parameter_;
}

ExtrudeFeatureIntent::ExtrudeFeatureIntent(ExtrudeExtentIntent extent,
                                           std::optional<double> taper_angle_deg,
                                           std::optional<ExtrudeThinIntent> thin)
    : extent_(std::move(extent)), taper_angle_deg_(taper_angle_deg), thin_(std::move(thin)) {}

Result<ExtrudeFeatureIntent> ExtrudeFeatureIntent::create(ExtrudeExtentIntent extent,
                                                          std::optional<double> taper_angle_deg,
                                                          std::optional<ExtrudeThinIntent> thin) {
  if (taper_angle_deg.has_value() &&
      (!std::isfinite(*taper_angle_deg) || *taper_angle_deg <= -90.0 || *taper_angle_deg >= 90.0))
    return Result<ExtrudeFeatureIntent>::failure(Error::validation(
        "extrude_intent", "taper angle must be finite and strictly between -90 and 90 degrees"));
  return Result<ExtrudeFeatureIntent>::success(
      ExtrudeFeatureIntent(std::move(extent), taper_angle_deg, std::move(thin)));
}

const ExtrudeExtentIntent& ExtrudeFeatureIntent::extent() const noexcept {
  return extent_;
}
const std::optional<double>& ExtrudeFeatureIntent::taper_angle_deg() const noexcept {
  return taper_angle_deg_;
}
const std::optional<ExtrudeThinIntent>& ExtrudeFeatureIntent::thin() const noexcept {
  return thin_;
}
bool ExtrudeFeatureIntent::is_historical_additive_default() const noexcept {
  return extent_.mode() == ExtrudeExtentMode::Distance && !taper_angle_deg_.has_value() &&
         !thin_.has_value();
}
bool ExtrudeFeatureIntent::is_historical_subtractive_default() const noexcept {
  return extent_.mode() == ExtrudeExtentMode::ThroughAll && !taper_angle_deg_.has_value() &&
         !thin_.has_value();
}

Result<FeatureBodyResultContext>
FeatureBodyResultContext::create(FeatureBodyOperationMode operation_mode,
                                 std::optional<BodyId> target_body,
                                 std::optional<BodyId> produced_body) {
  switch (operation_mode) {
  case FeatureBodyOperationMode::NewBody:
    if (target_body.has_value())
      return Result<FeatureBodyResultContext>::failure(Error::validation(
          "feature_body_result", "new-body feature result must not have target body"));
    if (!produced_body.has_value() || produced_body->empty())
      return Result<FeatureBodyResultContext>::failure(Error::validation(
          "feature_body_result", "new-body feature result requires produced body"));
    break;
  case FeatureBodyOperationMode::Join:
  case FeatureBodyOperationMode::Cut:
  case FeatureBodyOperationMode::Intersect:
    if (!target_body.has_value() || target_body->empty())
      return Result<FeatureBodyResultContext>::failure(Error::validation(
          "feature_body_result", "modifying feature result requires target body"));
    if (produced_body.has_value() && produced_body->empty())
      return Result<FeatureBodyResultContext>::failure(
          Error::validation("feature_body_result", "feature produced body id must not be empty"));
    break;
  default:
    return Result<FeatureBodyResultContext>::failure(
        Error::validation("feature_body_result", "unsupported feature body operation mode"));
  }
  return Result<FeatureBodyResultContext>::success(
      FeatureBodyResultContext(operation_mode, std::move(target_body), std::move(produced_body)));
}

FeatureBodyOperationMode FeatureBodyResultContext::operation_mode() const noexcept {
  return operation_mode_;
}

const std::optional<BodyId>& FeatureBodyResultContext::target_body() const noexcept {
  return target_body_;
}

const std::optional<BodyId>& FeatureBodyResultContext::produced_body() const noexcept {
  return produced_body_;
}

const BodyId& FeatureBodyResultContext::effective_produced_body() const noexcept {
  return produced_body_.has_value() ? produced_body_.value() : target_body_.value();
}

FeatureBodyResultContext::FeatureBodyResultContext(FeatureBodyOperationMode operation_mode,
                                                   std::optional<BodyId> target_body,
                                                   std::optional<BodyId> produced_body)
    : operation_mode_(operation_mode), target_body_(std::move(target_body)),
      produced_body_(std::move(produced_body)) {}

Result<Feature> Feature::create_additive_extrude(FeatureId id, std::string name,
                                                 SketchId input_sketch,
                                                 ParameterId length_parameter,
                                                 ExtrudeDirection direction) {
  const auto object_id = id.empty() ? std::string("feature") : id.value();
  if (id.empty())
    return Result<Feature>::failure(Error::validation(object_id, "feature id must not be empty"));
  if (name.empty())
    return Result<Feature>::failure(Error::validation(object_id, "feature name must not be empty"));
  if (input_sketch.empty())
    return Result<Feature>::failure(
        Error::validation(object_id, "feature input sketch id must not be empty"));
  if (length_parameter.empty())
    return Result<Feature>::failure(
        Error::validation(object_id, "additive extrude length parameter id must not be empty"));
  auto extent = ExtrudeExtentIntent::distance(std::move(length_parameter));
  if (extent.has_error())
    return Result<Feature>::failure(extent.error());
  auto intent = ExtrudeFeatureIntent::create(std::move(extent.value()));
  if (intent.has_error())
    return Result<Feature>::failure(intent.error());
  return create_additive_extrude(std::move(id), std::move(name), std::move(input_sketch),
                                 std::move(intent.value()), direction);
}

Result<Feature> Feature::create_additive_extrude(FeatureId id, std::string name,
                                                 SketchId input_sketch,
                                                 ExtrudeFeatureIntent extrude_intent,
                                                 ExtrudeDirection direction) {
  const auto object_id = id.empty() ? std::string("feature") : id.value();

  if (id.empty()) {
    return Result<Feature>::failure(Error::validation(object_id, "feature id must not be empty"));
  }

  if (name.empty()) {
    return Result<Feature>::failure(Error::validation(object_id, "feature name must not be empty"));
  }

  if (input_sketch.empty()) {
    return Result<Feature>::failure(
        Error::validation(object_id, "feature input sketch id must not be empty"));
  }
  if (direction == ExtrudeDirection::Path)
    return Result<Feature>::failure(Error::validation(
        object_id, "path direction requires create_additive_path_extrude and a PathCurveId"));

  ParameterId length_parameter;
  if (extrude_intent.extent().first_distance_parameter().has_value())
    length_parameter = *extrude_intent.extent().first_distance_parameter();
  return Result<Feature>::success(
      Feature(std::move(id), std::move(name), FeatureType::AdditiveExtrude, std::move(input_sketch),
              std::move(length_parameter), FeatureId(), direction,
              SubtractiveExtrudeDepth::ThroughAll, std::move(extrude_intent)));
}

Result<Feature> Feature::create_additive_path_extrude(FeatureId id, std::string name,
                                                      SketchId input_sketch,
                                                      PathCurveId path_curve) {
  const auto object_id = id.empty() ? std::string("feature") : id.value();
  if (id.empty() || name.empty() || input_sketch.empty() || path_curve.empty())
    return Result<Feature>::failure(Error::validation(
        object_id, "path extrude requires non-empty id, name, input sketch, and PathCurveId"));
  auto intent = ExtrudeFeatureIntent::create(ExtrudeExtentIntent::through_all());
  if (intent.has_error())
    return Result<Feature>::failure(intent.error());
  return Result<Feature>::success(Feature(
      std::move(id), std::move(name), FeatureType::AdditiveExtrude, std::move(input_sketch),
      ParameterId(), FeatureId(), ExtrudeDirection::Path, SubtractiveExtrudeDepth::ThroughAll,
      std::move(intent.value()), std::move(path_curve)));
}

Result<Feature> Feature::create_subtractive_extrude(FeatureId id, std::string name,
                                                    SketchId input_sketch, FeatureId target_feature,
                                                    SubtractiveExtrudeDepth depth,
                                                    ExtrudeDirection direction) {
  if (depth != SubtractiveExtrudeDepth::ThroughAll)
    return Result<Feature>::failure(Error::validation(
        id.empty() ? "feature" : id.value(), "unsupported historical subtractive extrude depth"));
  auto intent = ExtrudeFeatureIntent::create(ExtrudeExtentIntent::through_all());
  if (intent.has_error())
    return Result<Feature>::failure(intent.error());
  return create_subtractive_extrude(std::move(id), std::move(name), std::move(input_sketch),
                                    std::move(target_feature), std::move(intent.value()),
                                    direction);
}

Result<Feature> Feature::create_subtractive_extrude(FeatureId id, std::string name,
                                                    SketchId input_sketch, FeatureId target_feature,
                                                    ExtrudeFeatureIntent extrude_intent,
                                                    ExtrudeDirection direction) {
  const auto object_id = id.empty() ? std::string("feature") : id.value();

  if (id.empty()) {
    return Result<Feature>::failure(Error::validation(object_id, "feature id must not be empty"));
  }

  if (name.empty()) {
    return Result<Feature>::failure(Error::validation(object_id, "feature name must not be empty"));
  }

  if (input_sketch.empty()) {
    return Result<Feature>::failure(
        Error::validation(object_id, "feature input sketch id must not be empty"));
  }

  if (target_feature.empty()) {
    return Result<Feature>::failure(
        Error::validation(object_id, "subtractive extrude target feature id must not be empty"));
  }
  if (direction == ExtrudeDirection::Path)
    return Result<Feature>::failure(Error::validation(
        object_id, "path direction requires create_subtractive_path_extrude and a PathCurveId"));

  return Result<Feature>::success(Feature(
      std::move(id), std::move(name), FeatureType::SubtractiveExtrude, std::move(input_sketch),
      extrude_intent.extent().first_distance_parameter().value_or(ParameterId()),
      std::move(target_feature), direction, SubtractiveExtrudeDepth::ThroughAll,
      std::move(extrude_intent)));
}

Result<Feature> Feature::create_subtractive_path_extrude(FeatureId id, std::string name,
                                                         SketchId input_sketch,
                                                         FeatureId target_feature,
                                                         PathCurveId path_curve) {
  const auto object_id = id.empty() ? std::string("feature") : id.value();
  if (id.empty() || name.empty() || input_sketch.empty() || target_feature.empty() ||
      path_curve.empty())
    return Result<Feature>::failure(Error::validation(
        object_id,
        "path cut requires non-empty id, name, input sketch, target feature, and PathCurveId"));
  auto intent = ExtrudeFeatureIntent::create(ExtrudeExtentIntent::through_all());
  if (intent.has_error())
    return Result<Feature>::failure(intent.error());
  return Result<Feature>::success(Feature(
      std::move(id), std::move(name), FeatureType::SubtractiveExtrude, std::move(input_sketch),
      ParameterId(), std::move(target_feature), ExtrudeDirection::Path,
      SubtractiveExtrudeDepth::ThroughAll, std::move(intent.value()), std::move(path_curve)));
}

Result<Feature>
Feature::with_body_result_context(FeatureBodyResultContext body_result_context) const {
  return Result<Feature>::success(
      Feature(id_, name_, type_, input_sketch_, length_parameter_, target_feature_, direction_,
              subtractive_depth_, extrude_intent_, path_curve_, std::move(body_result_context)));
}

const FeatureId& Feature::id() const noexcept {
  return id_;
}
const std::string& Feature::name() const noexcept {
  return name_;
}
FeatureType Feature::type() const noexcept {
  return type_;
}
const SketchId& Feature::input_sketch() const noexcept {
  return input_sketch_;
}
const ParameterId& Feature::length_parameter() const noexcept {
  return length_parameter_;
}
const FeatureId& Feature::target_feature() const noexcept {
  return target_feature_;
}
ExtrudeDirection Feature::direction() const noexcept {
  return direction_;
}
SubtractiveExtrudeDepth Feature::subtractive_depth() const noexcept {
  return subtractive_depth_;
}

const std::optional<FeatureBodyResultContext>& Feature::body_result_context() const noexcept {
  return body_result_context_;
}

const ExtrudeFeatureIntent& Feature::extrude_intent() const noexcept {
  return extrude_intent_;
}

const std::optional<PathCurveId>& Feature::path_curve() const noexcept {
  return path_curve_;
}

Feature::Feature(FeatureId id, std::string name, FeatureType type, SketchId input_sketch,
                 ParameterId length_parameter, FeatureId target_feature, ExtrudeDirection direction,
                 SubtractiveExtrudeDepth subtractive_depth, ExtrudeFeatureIntent extrude_intent,
                 std::optional<PathCurveId> path_curve,
                 std::optional<FeatureBodyResultContext> body_result_context)
    : id_(std::move(id)), name_(std::move(name)), type_(type),
      input_sketch_(std::move(input_sketch)), length_parameter_(std::move(length_parameter)),
      target_feature_(std::move(target_feature)), direction_(direction),
      subtractive_depth_(subtractive_depth), extrude_intent_(std::move(extrude_intent)),
      path_curve_(std::move(path_curve)), body_result_context_(std::move(body_result_context)) {}

} // namespace blcad
