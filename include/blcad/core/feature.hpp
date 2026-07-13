#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace blcad {

enum class FeatureType {
  AdditiveExtrude,
  SubtractiveExtrude,
};

enum class ExtrudeDirection {
  SketchNormal,
  OppositeSketchNormal,
};

enum class SubtractiveExtrudeDepth {
  ThroughAll,
};

enum class FeatureBodyOperationMode {
  NewBody,
  Join,
  Cut,
  Intersect,
};

[[nodiscard]] std::string_view to_string(FeatureType type) noexcept;
[[nodiscard]] std::string_view to_string(ExtrudeDirection direction) noexcept;
[[nodiscard]] std::string_view to_string(SubtractiveExtrudeDepth depth) noexcept;
[[nodiscard]] std::string_view to_string(FeatureBodyOperationMode mode) noexcept;

class FeatureBodyResultContext {
public:
  [[nodiscard]] static Result<FeatureBodyResultContext>
  create(FeatureBodyOperationMode operation_mode, std::optional<BodyId> target_body,
         std::optional<BodyId> produced_body);

  [[nodiscard]] FeatureBodyOperationMode operation_mode() const noexcept;
  [[nodiscard]] const std::optional<BodyId>& target_body() const noexcept;
  [[nodiscard]] const std::optional<BodyId>& produced_body() const noexcept;
  // NewBody always returns produced_body. Modifying modes return an explicit
  // produced_body when present and otherwise preserve target_body identity.
  [[nodiscard]] const BodyId& effective_produced_body() const noexcept;

  friend bool operator==(const FeatureBodyResultContext&,
                         const FeatureBodyResultContext&) = default;

private:
  FeatureBodyResultContext(FeatureBodyOperationMode operation_mode,
                           std::optional<BodyId> target_body, std::optional<BodyId> produced_body);

  FeatureBodyOperationMode operation_mode_;
  std::optional<BodyId> target_body_;
  std::optional<BodyId> produced_body_;
};

class Feature {
public:
  [[nodiscard]] static Result<Feature>
  create_additive_extrude(FeatureId id, std::string name, SketchId input_sketch,
                          ParameterId length_parameter,
                          ExtrudeDirection direction = ExtrudeDirection::SketchNormal);

  [[nodiscard]] static Result<Feature>
  create_subtractive_extrude(FeatureId id, std::string name, SketchId input_sketch,
                             FeatureId target_feature,
                             SubtractiveExtrudeDepth depth = SubtractiveExtrudeDepth::ThroughAll,
                             ExtrudeDirection direction = ExtrudeDirection::SketchNormal);

  [[nodiscard]] Result<Feature>
  with_body_result_context(FeatureBodyResultContext body_result_context) const;

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] FeatureType type() const noexcept;
  [[nodiscard]] const SketchId& input_sketch() const noexcept;
  [[nodiscard]] const ParameterId& length_parameter() const noexcept;
  [[nodiscard]] const FeatureId& target_feature() const noexcept;
  [[nodiscard]] ExtrudeDirection direction() const noexcept;
  [[nodiscard]] SubtractiveExtrudeDepth subtractive_depth() const noexcept;
  [[nodiscard]] const std::optional<FeatureBodyResultContext>& body_result_context() const noexcept;

private:
  Feature(FeatureId id, std::string name, FeatureType type, SketchId input_sketch,
          ParameterId length_parameter, FeatureId target_feature, ExtrudeDirection direction,
          SubtractiveExtrudeDepth subtractive_depth,
          std::optional<FeatureBodyResultContext> body_result_context = std::nullopt);

  FeatureId id_;
  std::string name_;
  FeatureType type_;
  SketchId input_sketch_;
  ParameterId length_parameter_;
  FeatureId target_feature_;
  ExtrudeDirection direction_;
  SubtractiveExtrudeDepth subtractive_depth_;
  std::optional<FeatureBodyResultContext> body_result_context_;
};

} // namespace blcad
