#pragma once

#include "blcad/core/part_feature_input_reference.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class ShellDirection { Inward, Outward };

[[nodiscard]] std::string_view to_string(ShellDirection direction) noexcept;

class ShellFeature {
public:
  [[nodiscard]] static Result<ShellFeature> create(FeatureId id, std::string name,
                                                   BodyId target_body,
                                                   std::vector<FaceReference> removed_faces,
                                                   ParameterId thickness_parameter,
                                                   ShellDirection direction);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const BodyId& target_body() const noexcept;
  [[nodiscard]] const std::vector<FaceReference>& removed_faces() const noexcept;
  [[nodiscard]] const ParameterId& thickness_parameter() const noexcept;
  [[nodiscard]] ShellDirection direction() const noexcept;

private:
  ShellFeature(FeatureId id, std::string name, BodyId target_body,
               std::vector<FaceReference> removed_faces, ParameterId thickness_parameter,
               ShellDirection direction);

  FeatureId id_;
  std::string name_;
  BodyId target_body_;
  std::vector<FaceReference> removed_faces_;
  ParameterId thickness_parameter_;
  ShellDirection direction_;
};

} // namespace blcad
