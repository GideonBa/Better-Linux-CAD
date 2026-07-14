#include "blcad/core/draft_feature.hpp"

#include <set>
#include <type_traits>
#include <utility>

namespace blcad {
namespace {
[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::string face_identity(const FaceReference& face) {
  return std::visit(
      [](const auto& source) {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return source.source_feature().value() + ":" + std::string(to_string(source.face()));
        else
          return source.source_feature().value() + ":" + source.source_profile().value() + ":" +
                 std::string(to_string(source.face()));
      },
      face.source());
}
} // namespace

DraftFeature::DraftFeature(FeatureId id, std::string name, BodyId target_body,
                           std::vector<FaceReference> faces, AxisReference pull_direction,
                           PlaneReference neutral_plane, ParameterId angle_parameter)
    : id_(std::move(id)), name_(std::move(name)), target_body_(std::move(target_body)),
      faces_(std::move(faces)), pull_direction_(std::move(pull_direction)),
      neutral_plane_(std::move(neutral_plane)), angle_parameter_(std::move(angle_parameter)) {}

Result<DraftFeature> DraftFeature::create(FeatureId id, std::string name, BodyId target_body,
                                          std::vector<FaceReference> faces,
                                          AxisReference pull_direction,
                                          PlaneReference neutral_plane,
                                          ParameterId angle_parameter) {
  if (id.empty())
    return Result<DraftFeature>::failure(validation_error("draft", "feature id must not be empty"));
  if (name.empty())
    return Result<DraftFeature>::failure(
        validation_error(id.value(), "feature name must not be empty"));
  if (target_body.empty())
    return Result<DraftFeature>::failure(
        validation_error(id.value(), "draft target body must not be empty"));
  if (faces.empty())
    return Result<DraftFeature>::failure(
        validation_error(id.value(), "draft requires at least one face"));
  if (angle_parameter.empty())
    return Result<DraftFeature>::failure(
        validation_error(id.value(), "draft angle parameter must not be empty"));
  if (pull_direction.role() != PartFeatureInputRole::DraftPullDirection ||
      (pull_direction.expected_capability() != PartFeatureInputCapability::Axis &&
       pull_direction.expected_capability() != PartFeatureInputCapability::Line))
    return Result<DraftFeature>::failure(validation_error(
        id.value(), "draft pull direction requires draft_pull_direction Axis or Line intent"));
  if (neutral_plane.role() != PartFeatureInputRole::DraftNeutralPlane)
    return Result<DraftFeature>::failure(validation_error(
        id.value(), "draft neutral plane requires draft_neutral_plane Plane intent"));

  std::set<std::string> identities;
  for (const FaceReference& face : faces) {
    if (face.role() != PartFeatureInputRole::DraftFace)
      return Result<DraftFeature>::failure(
          validation_error(id.value(), "draft face role must be draft_face"));
    if (!identities.insert(face_identity(face)).second)
      return Result<DraftFeature>::failure(
          validation_error(id.value(), "draft faces must not contain duplicates"));
  }
  return Result<DraftFeature>::success(DraftFeature(
      std::move(id), std::move(name), std::move(target_body), std::move(faces),
      std::move(pull_direction), std::move(neutral_plane), std::move(angle_parameter)));
}

const FeatureId& DraftFeature::id() const noexcept {
  return id_;
}
const std::string& DraftFeature::name() const noexcept {
  return name_;
}
const BodyId& DraftFeature::target_body() const noexcept {
  return target_body_;
}
const std::vector<FaceReference>& DraftFeature::faces() const noexcept {
  return faces_;
}
const AxisReference& DraftFeature::pull_direction() const noexcept {
  return pull_direction_;
}
const PlaneReference& DraftFeature::neutral_plane() const noexcept {
  return neutral_plane_;
}
const ParameterId& DraftFeature::angle_parameter() const noexcept {
  return angle_parameter_;
}

} // namespace blcad
