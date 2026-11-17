#pragma once

#include "blcad/core/part_feature_input_reference.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

struct AdvancedFilletIntent;
struct VariableFilletIntent;
struct FaceBlendIntent;
struct FullRoundIntent;

enum class ChamferMode { EqualDistance, TwoDistance, DistanceAngle };

[[nodiscard]] std::string_view to_string(ChamferMode mode) noexcept;

class FilletFeature {
public:
  [[nodiscard]] static Result<FilletFeature> create(FeatureId id, std::string name,
                                                    BodyId target_body,
                                                    std::vector<EdgeReference> edges,
                                                    ParameterId radius_parameter);

  [[nodiscard]] static Result<FilletFeature> create_variable(FeatureId, std::string name, BodyId,
                                                             VariableFilletIntent);
  [[nodiscard]] static Result<FilletFeature> create_face_blend(FeatureId, std::string, BodyId,
                                                               FaceBlendIntent);
  [[nodiscard]] static Result<FilletFeature> create_full_round(FeatureId, std::string, BodyId,
                                                               FullRoundIntent);
  [[nodiscard]] std::vector<FaceReference> faces() const;
  [[nodiscard]] std::vector<PathCurveId> hold_lines() const;
  [[nodiscard]] const AdvancedFilletIntent* advanced_intent() const noexcept;
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;
  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const BodyId& target_body() const noexcept;
  [[nodiscard]] const std::vector<EdgeReference>& edges() const noexcept;
  [[nodiscard]] const ParameterId& radius_parameter() const noexcept;

private:
  FilletFeature(FeatureId id, std::string name, BodyId target_body,
                std::vector<EdgeReference> edges, ParameterId radius_parameter);

  FeatureId id_;
  std::string name_;
  BodyId target_body_;
  std::vector<EdgeReference> edges_;
  ParameterId radius_parameter_;
  std::shared_ptr<const AdvancedFilletIntent> advanced_;
};

class ChamferFeature {
public:
  [[nodiscard]] static Result<ChamferFeature>
  create_equal_distance(FeatureId id, std::string name, BodyId target_body,
                        std::vector<EdgeReference> edges, ParameterId distance_parameter);
  [[nodiscard]] static Result<ChamferFeature>
  create_two_distance(FeatureId id, std::string name, BodyId target_body,
                      std::vector<EdgeReference> edges, ParameterId first_distance_parameter,
                      ParameterId second_distance_parameter);
  [[nodiscard]] static Result<ChamferFeature>
  create_distance_angle(FeatureId id, std::string name, BodyId target_body,
                        std::vector<EdgeReference> edges, ParameterId distance_parameter,
                        ParameterId angle_parameter);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const BodyId& target_body() const noexcept;
  [[nodiscard]] const std::vector<EdgeReference>& edges() const noexcept;
  [[nodiscard]] ChamferMode mode() const noexcept;
  [[nodiscard]] const ParameterId& first_parameter() const noexcept;
  [[nodiscard]] const std::optional<ParameterId>& second_parameter() const noexcept;

private:
  [[nodiscard]] static Result<ChamferFeature>
  create_validated(FeatureId id, std::string name, BodyId target_body,
                   std::vector<EdgeReference> edges, ChamferMode mode, ParameterId first_parameter,
                   std::optional<ParameterId> second_parameter);
  ChamferFeature(FeatureId id, std::string name, BodyId target_body,
                 std::vector<EdgeReference> edges, ChamferMode mode, ParameterId first_parameter,
                 std::optional<ParameterId> second_parameter);

  FeatureId id_;
  std::string name_;
  BodyId target_body_;
  std::vector<EdgeReference> edges_;
  ChamferMode mode_;
  ParameterId first_parameter_;
  std::optional<ParameterId> second_parameter_;
};

} // namespace blcad
