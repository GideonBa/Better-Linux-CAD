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

Result<Feature>
Feature::with_body_result_context(FeatureBodyResultContext body_result_context) const {
  return Result<Feature>::success(Feature(id_, name_, type_, input_sketch_, length_parameter_,
                                          target_feature_, direction_, subtractive_depth_,
                                          std::move(body_result_context)));
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

Feature::Feature(FeatureId id, std::string name, FeatureType type, SketchId input_sketch,
                 ParameterId length_parameter, FeatureId target_feature, ExtrudeDirection direction,
                 SubtractiveExtrudeDepth subtractive_depth,
                 std::optional<FeatureBodyResultContext> body_result_context)
    : id_(std::move(id)), name_(std::move(name)), type_(type),
      input_sketch_(std::move(input_sketch)), length_parameter_(std::move(length_parameter)),
      target_feature_(std::move(target_feature)), direction_(direction),
      subtractive_depth_(subtractive_depth), body_result_context_(std::move(body_result_context)) {}

} // namespace blcad
