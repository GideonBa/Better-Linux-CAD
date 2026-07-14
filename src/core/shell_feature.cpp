#include "blcad/core/shell_feature.hpp"

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

std::string_view to_string(ShellDirection direction) noexcept {
  switch (direction) {
  case ShellDirection::Inward:
    return "inward";
  case ShellDirection::Outward:
    return "outward";
  }
  return "inward";
}

ShellFeature::ShellFeature(FeatureId id, std::string name, BodyId target_body,
                           std::vector<FaceReference> removed_faces,
                           ParameterId thickness_parameter, ShellDirection direction)
    : id_(std::move(id)), name_(std::move(name)), target_body_(std::move(target_body)),
      removed_faces_(std::move(removed_faces)),
      thickness_parameter_(std::move(thickness_parameter)), direction_(direction) {}

Result<ShellFeature> ShellFeature::create(FeatureId id, std::string name, BodyId target_body,
                                          std::vector<FaceReference> removed_faces,
                                          ParameterId thickness_parameter,
                                          ShellDirection direction) {
  if (id.empty())
    return Result<ShellFeature>::failure(validation_error("shell", "feature id must not be empty"));
  if (name.empty())
    return Result<ShellFeature>::failure(
        validation_error(id.value(), "feature name must not be empty"));
  if (target_body.empty())
    return Result<ShellFeature>::failure(
        validation_error(id.value(), "shell target body must not be empty"));
  if (removed_faces.empty())
    return Result<ShellFeature>::failure(
        validation_error(id.value(), "shell requires at least one removed face"));
  if (thickness_parameter.empty())
    return Result<ShellFeature>::failure(
        validation_error(id.value(), "shell thickness parameter must not be empty"));

  std::set<std::string> identities;
  for (const FaceReference& face : removed_faces) {
    if (face.role() != PartFeatureInputRole::ShellRemovalFace)
      return Result<ShellFeature>::failure(
          validation_error(id.value(), "removed face role must be shell_removal_face"));
    if (!identities.insert(face_identity(face)).second)
      return Result<ShellFeature>::failure(
          validation_error(id.value(), "shell removed faces must not contain duplicates"));
  }
  return Result<ShellFeature>::success(
      ShellFeature(std::move(id), std::move(name), std::move(target_body), std::move(removed_faces),
                   std::move(thickness_parameter), direction));
}

const FeatureId& ShellFeature::id() const noexcept {
  return id_;
}
const std::string& ShellFeature::name() const noexcept {
  return name_;
}
const BodyId& ShellFeature::target_body() const noexcept {
  return target_body_;
}
const std::vector<FaceReference>& ShellFeature::removed_faces() const noexcept {
  return removed_faces_;
}
const ParameterId& ShellFeature::thickness_parameter() const noexcept {
  return thickness_parameter_;
}
ShellDirection ShellFeature::direction() const noexcept {
  return direction_;
}

} // namespace blcad
