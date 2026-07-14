#pragma once

#include "blcad/core/part_feature_input_reference.hpp"

#include <string>
#include <vector>

namespace blcad {

class DraftFeature {
public:
  [[nodiscard]] static Result<DraftFeature>
  create(FeatureId id, std::string name, BodyId target_body, std::vector<FaceReference> faces,
         AxisReference pull_direction, PlaneReference neutral_plane, ParameterId angle_parameter);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const BodyId& target_body() const noexcept;
  [[nodiscard]] const std::vector<FaceReference>& faces() const noexcept;
  [[nodiscard]] const AxisReference& pull_direction() const noexcept;
  [[nodiscard]] const PlaneReference& neutral_plane() const noexcept;
  [[nodiscard]] const ParameterId& angle_parameter() const noexcept;

private:
  DraftFeature(FeatureId id, std::string name, BodyId target_body, std::vector<FaceReference> faces,
               AxisReference pull_direction, PlaneReference neutral_plane,
               ParameterId angle_parameter);

  FeatureId id_;
  std::string name_;
  BodyId target_body_;
  std::vector<FaceReference> faces_;
  AxisReference pull_direction_;
  PlaneReference neutral_plane_;
  ParameterId angle_parameter_;
};

} // namespace blcad
