#include "blcad/core/edge_treatment_feature.hpp"

#include <set>
#include <type_traits>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::string edge_identity(const EdgeReference& edge) {
  return std::visit(
      [](const auto& source) {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, SemanticEdgeReference>)
          return source.node_id();
        else
          return source.source_feature().value() + ":" + source.source_profile().value() + ":" +
                 std::string(to_string(source.edge()));
      },
      edge.source());
}

[[nodiscard]] Result<std::size_t> validate_common(const FeatureId& id, const std::string& name,
                                                  const BodyId& target_body,
                                                  const std::vector<EdgeReference>& edges,
                                                  PartFeatureInputRole required_role) {
  if (id.empty())
    return Result<std::size_t>::failure(
        validation_error("edge_treatment", "feature id must not be empty"));
  if (name.empty())
    return Result<std::size_t>::failure(
        validation_error(id.value(), "feature name must not be empty"));
  if (target_body.empty())
    return Result<std::size_t>::failure(
        validation_error(id.value(), "target body must not be empty"));
  if (edges.empty())
    return Result<std::size_t>::failure(
        validation_error(id.value(), "edge treatment requires at least one edge"));
  std::set<std::string> identities;
  for (const auto& edge : edges) {
    if (edge.role() != required_role)
      return Result<std::size_t>::failure(
          validation_error(id.value(), "edge reference role does not match edge treatment"));
    if (!identities.insert(edge_identity(edge)).second)
      return Result<std::size_t>::failure(
          validation_error(id.value(), "edge treatment edges must not contain duplicates"));
  }
  return Result<std::size_t>::success(edges.size());
}

} // namespace

std::string_view to_string(ChamferMode mode) noexcept {
  switch (mode) {
  case ChamferMode::EqualDistance:
    return "equal_distance";
  case ChamferMode::TwoDistance:
    return "two_distance";
  case ChamferMode::DistanceAngle:
    return "distance_angle";
  }
  return "equal_distance";
}

FilletFeature::FilletFeature(FeatureId id, std::string name, BodyId target_body,
                             std::vector<EdgeReference> edges, ParameterId radius_parameter)
    : id_(std::move(id)), name_(std::move(name)), target_body_(std::move(target_body)),
      edges_(std::move(edges)), radius_parameter_(std::move(radius_parameter)) {}

Result<FilletFeature> FilletFeature::create(FeatureId id, std::string name, BodyId target_body,
                                            std::vector<EdgeReference> edges,
                                            ParameterId radius_parameter) {
  auto valid = validate_common(id, name, target_body, edges, PartFeatureInputRole::FilletEdge);
  if (valid.has_error())
    return Result<FilletFeature>::failure(valid.error());
  if (radius_parameter.empty())
    return Result<FilletFeature>::failure(
        validation_error(id.value(), "fillet radius parameter must not be empty"));
  return Result<FilletFeature>::success(FilletFeature(std::move(id), std::move(name),
                                                      std::move(target_body), std::move(edges),
                                                      std::move(radius_parameter)));
}

const FeatureId& FilletFeature::id() const noexcept {
  return id_;
}
const std::string& FilletFeature::name() const noexcept {
  return name_;
}
const BodyId& FilletFeature::target_body() const noexcept {
  return target_body_;
}
const std::vector<EdgeReference>& FilletFeature::edges() const noexcept {
  return edges_;
}
const ParameterId& FilletFeature::radius_parameter() const noexcept {
  return radius_parameter_;
}

ChamferFeature::ChamferFeature(FeatureId id, std::string name, BodyId target_body,
                               std::vector<EdgeReference> edges, ChamferMode mode,
                               ParameterId first_parameter,
                               std::optional<ParameterId> second_parameter)
    : id_(std::move(id)), name_(std::move(name)), target_body_(std::move(target_body)),
      edges_(std::move(edges)), mode_(mode), first_parameter_(std::move(first_parameter)),
      second_parameter_(std::move(second_parameter)) {}

Result<ChamferFeature> ChamferFeature::create_validated(FeatureId id, std::string name,
                                                        BodyId target_body,
                                                        std::vector<EdgeReference> edges,
                                                        ChamferMode mode, ParameterId first,
                                                        std::optional<ParameterId> second) {
  auto valid = validate_common(id, name, target_body, edges, PartFeatureInputRole::ChamferEdge);
  if (valid.has_error())
    return Result<ChamferFeature>::failure(valid.error());
  if (first.empty() ||
      (mode != ChamferMode::EqualDistance && (!second.has_value() || second->empty())) ||
      (mode == ChamferMode::EqualDistance && second.has_value()))
    return Result<ChamferFeature>::failure(
        validation_error(id.value(), "chamfer parameter combination is invalid"));
  return Result<ChamferFeature>::success(ChamferFeature(std::move(id), std::move(name),
                                                        std::move(target_body), std::move(edges),
                                                        mode, std::move(first), std::move(second)));
}

Result<ChamferFeature> ChamferFeature::create_equal_distance(FeatureId id, std::string name,
                                                             BodyId target_body,
                                                             std::vector<EdgeReference> edges,
                                                             ParameterId distance_parameter) {
  return create_validated(std::move(id), std::move(name), std::move(target_body), std::move(edges),
                          ChamferMode::EqualDistance, std::move(distance_parameter), std::nullopt);
}
Result<ChamferFeature> ChamferFeature::create_two_distance(FeatureId id, std::string name,
                                                           BodyId target_body,
                                                           std::vector<EdgeReference> edges,
                                                           ParameterId first_distance_parameter,
                                                           ParameterId second_distance_parameter) {
  return create_validated(std::move(id), std::move(name), std::move(target_body), std::move(edges),
                          ChamferMode::TwoDistance, std::move(first_distance_parameter),
                          std::move(second_distance_parameter));
}
Result<ChamferFeature> ChamferFeature::create_distance_angle(FeatureId id, std::string name,
                                                             BodyId target_body,
                                                             std::vector<EdgeReference> edges,
                                                             ParameterId distance_parameter,
                                                             ParameterId angle_parameter) {
  return create_validated(std::move(id), std::move(name), std::move(target_body), std::move(edges),
                          ChamferMode::DistanceAngle, std::move(distance_parameter),
                          std::move(angle_parameter));
}
const FeatureId& ChamferFeature::id() const noexcept {
  return id_;
}
const std::string& ChamferFeature::name() const noexcept {
  return name_;
}
const BodyId& ChamferFeature::target_body() const noexcept {
  return target_body_;
}
const std::vector<EdgeReference>& ChamferFeature::edges() const noexcept {
  return edges_;
}
ChamferMode ChamferFeature::mode() const noexcept {
  return mode_;
}
const ParameterId& ChamferFeature::first_parameter() const noexcept {
  return first_parameter_;
}
const std::optional<ParameterId>& ChamferFeature::second_parameter() const noexcept {
  return second_parameter_;
}

} // namespace blcad
