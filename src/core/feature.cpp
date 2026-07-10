#include "blcad/core/feature.hpp"

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

Result<Feature> Feature::create_additive_extrude(FeatureId id, std::string name,
                                                 SketchId input_sketch,
                                                 ParameterId length_parameter,
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

  if (length_parameter.empty()) {
    return Result<Feature>::failure(
        Error::validation(object_id, "additive extrude length parameter id must not be empty"));
  }

  return Result<Feature>::success(Feature(
      std::move(id), std::move(name), FeatureType::AdditiveExtrude, std::move(input_sketch),
      std::move(length_parameter), FeatureId(), direction, SubtractiveExtrudeDepth::ThroughAll));
}

Result<Feature> Feature::create_subtractive_extrude(FeatureId id, std::string name,
                                                    SketchId input_sketch, FeatureId target_feature,
                                                    SubtractiveExtrudeDepth depth,
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

  return Result<Feature>::success(
      Feature(std::move(id), std::move(name), FeatureType::SubtractiveExtrude,
              std::move(input_sketch), ParameterId(), std::move(target_feature), direction, depth));
}

const FeatureId& Feature::id() const noexcept { return id_; }
const std::string& Feature::name() const noexcept { return name_; }
FeatureType Feature::type() const noexcept { return type_; }
const SketchId& Feature::input_sketch() const noexcept { return input_sketch_; }
const ParameterId& Feature::length_parameter() const noexcept { return length_parameter_; }
const FeatureId& Feature::target_feature() const noexcept { return target_feature_; }
ExtrudeDirection Feature::direction() const noexcept { return direction_; }
SubtractiveExtrudeDepth Feature::subtractive_depth() const noexcept { return subtractive_depth_; }

Feature::Feature(FeatureId id, std::string name, FeatureType type, SketchId input_sketch,
                 ParameterId length_parameter, FeatureId target_feature, ExtrudeDirection direction,
                 SubtractiveExtrudeDepth subtractive_depth)
    : id_(std::move(id)), name_(std::move(name)), type_(type),
      input_sketch_(std::move(input_sketch)), length_parameter_(std::move(length_parameter)),
      target_feature_(std::move(target_feature)), direction_(direction),
      subtractive_depth_(subtractive_depth) {}

} // namespace blcad
