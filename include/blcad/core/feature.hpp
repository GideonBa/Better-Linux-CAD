#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>

namespace blcad {

enum class FeatureType {
  AdditiveExtrude,
  SubtractiveExtrude,
};

enum class ExtrudeDirection {
  PositiveZ,
};

enum class SubtractiveExtrudeDepth {
  ThroughAll,
};

[[nodiscard]] std::string_view to_string(FeatureType type) noexcept;
[[nodiscard]] std::string_view to_string(ExtrudeDirection direction) noexcept;
[[nodiscard]] std::string_view to_string(SubtractiveExtrudeDepth depth) noexcept;

class Feature {
public:
  [[nodiscard]] static Result<Feature>
  create_additive_extrude(FeatureId id, std::string name, SketchId input_sketch,
                          ParameterId length_parameter,
                          ExtrudeDirection direction = ExtrudeDirection::PositiveZ);

  [[nodiscard]] static Result<Feature>
  create_subtractive_extrude(FeatureId id, std::string name, SketchId input_sketch,
                             FeatureId target_feature,
                             SubtractiveExtrudeDepth depth = SubtractiveExtrudeDepth::ThroughAll,
                             ExtrudeDirection direction = ExtrudeDirection::PositiveZ);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] FeatureType type() const noexcept;
  [[nodiscard]] const SketchId& input_sketch() const noexcept;
  [[nodiscard]] const ParameterId& length_parameter() const noexcept;
  [[nodiscard]] const FeatureId& target_feature() const noexcept;
  [[nodiscard]] ExtrudeDirection direction() const noexcept;
  [[nodiscard]] SubtractiveExtrudeDepth subtractive_depth() const noexcept;

private:
  Feature(FeatureId id, std::string name, FeatureType type, SketchId input_sketch,
          ParameterId length_parameter, FeatureId target_feature, ExtrudeDirection direction,
          SubtractiveExtrudeDepth subtractive_depth);

  FeatureId id_;
  std::string name_;
  FeatureType type_;
  SketchId input_sketch_;
  ParameterId length_parameter_;
  FeatureId target_feature_;
  ExtrudeDirection direction_;
  SubtractiveExtrudeDepth subtractive_depth_;
};

} // namespace blcad
