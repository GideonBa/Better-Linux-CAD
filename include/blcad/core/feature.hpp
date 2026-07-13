#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
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

enum class ExtrudeExtentMode { Distance, Symmetric, TwoSided, ThroughAll, ToNext, ToFace, Between };
enum class ExtrudeThinMode { OneSided, TwoSided, MidPlane };

enum class FeatureBodyOperationMode {
  NewBody,
  Join,
  Cut,
  Intersect,
};

[[nodiscard]] std::string_view to_string(FeatureType type) noexcept;
[[nodiscard]] std::string_view to_string(ExtrudeDirection direction) noexcept;
[[nodiscard]] std::string_view to_string(SubtractiveExtrudeDepth depth) noexcept;
[[nodiscard]] std::string_view to_string(ExtrudeExtentMode mode) noexcept;
[[nodiscard]] std::string_view to_string(ExtrudeThinMode mode) noexcept;
[[nodiscard]] std::string_view to_string(FeatureBodyOperationMode mode) noexcept;

class ExtrudeExtentIntent {
public:
  [[nodiscard]] static Result<ExtrudeExtentIntent> distance(ParameterId distance_parameter);
  [[nodiscard]] static Result<ExtrudeExtentIntent> symmetric(ParameterId distance_parameter);
  [[nodiscard]] static Result<ExtrudeExtentIntent> two_sided(ParameterId first_distance_parameter,
                                                             ParameterId second_distance_parameter);
  [[nodiscard]] static ExtrudeExtentIntent through_all();
  [[nodiscard]] static ExtrudeExtentIntent to_next();
  [[nodiscard]] static Result<ExtrudeExtentIntent> to_face(FaceReference face);
  [[nodiscard]] static Result<ExtrudeExtentIntent> between(FaceReference first_face,
                                                           FaceReference second_face);

  [[nodiscard]] ExtrudeExtentMode mode() const noexcept;
  [[nodiscard]] const std::optional<ParameterId>& first_distance_parameter() const noexcept;
  [[nodiscard]] const std::optional<ParameterId>& second_distance_parameter() const noexcept;
  [[nodiscard]] const std::optional<FaceReference>& first_face() const noexcept;
  [[nodiscard]] const std::optional<FaceReference>& second_face() const noexcept;

private:
  ExtrudeExtentIntent(ExtrudeExtentMode mode, std::optional<ParameterId> first_distance_parameter,
                      std::optional<ParameterId> second_distance_parameter,
                      std::optional<FaceReference> first_face,
                      std::optional<FaceReference> second_face);

  ExtrudeExtentMode mode_;
  std::optional<ParameterId> first_distance_parameter_;
  std::optional<ParameterId> second_distance_parameter_;
  std::optional<FaceReference> first_face_;
  std::optional<FaceReference> second_face_;
};

class ExtrudeThinIntent {
public:
  [[nodiscard]] static Result<ExtrudeThinIntent>
  create(ExtrudeThinMode mode, ParameterId first_thickness_parameter,
         std::optional<ParameterId> second_thickness_parameter = std::nullopt);

  [[nodiscard]] ExtrudeThinMode mode() const noexcept;
  [[nodiscard]] const ParameterId& first_thickness_parameter() const noexcept;
  [[nodiscard]] const std::optional<ParameterId>& second_thickness_parameter() const noexcept;

private:
  ExtrudeThinIntent(ExtrudeThinMode mode, ParameterId first_thickness_parameter,
                    std::optional<ParameterId> second_thickness_parameter);
  ExtrudeThinMode mode_;
  ParameterId first_thickness_parameter_;
  std::optional<ParameterId> second_thickness_parameter_;
};

class ExtrudeFeatureIntent {
public:
  [[nodiscard]] static Result<ExtrudeFeatureIntent>
  create(ExtrudeExtentIntent extent, std::optional<double> taper_angle_deg = std::nullopt,
         std::optional<ExtrudeThinIntent> thin = std::nullopt);

  [[nodiscard]] const ExtrudeExtentIntent& extent() const noexcept;
  [[nodiscard]] const std::optional<double>& taper_angle_deg() const noexcept;
  [[nodiscard]] const std::optional<ExtrudeThinIntent>& thin() const noexcept;
  [[nodiscard]] bool is_historical_additive_default() const noexcept;
  [[nodiscard]] bool is_historical_subtractive_default() const noexcept;

private:
  ExtrudeFeatureIntent(ExtrudeExtentIntent extent, std::optional<double> taper_angle_deg,
                       std::optional<ExtrudeThinIntent> thin);
  ExtrudeExtentIntent extent_;
  std::optional<double> taper_angle_deg_;
  std::optional<ExtrudeThinIntent> thin_;
};

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
  create_additive_extrude(FeatureId id, std::string name, SketchId input_sketch,
                          ExtrudeFeatureIntent extrude_intent,
                          ExtrudeDirection direction = ExtrudeDirection::SketchNormal);

  [[nodiscard]] static Result<Feature>
  create_subtractive_extrude(FeatureId id, std::string name, SketchId input_sketch,
                             FeatureId target_feature,
                             SubtractiveExtrudeDepth depth = SubtractiveExtrudeDepth::ThroughAll,
                             ExtrudeDirection direction = ExtrudeDirection::SketchNormal);

  [[nodiscard]] static Result<Feature>
  create_subtractive_extrude(FeatureId id, std::string name, SketchId input_sketch,
                             FeatureId target_feature, ExtrudeFeatureIntent extrude_intent,
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
  [[nodiscard]] const ExtrudeFeatureIntent& extrude_intent() const noexcept;

private:
  Feature(FeatureId id, std::string name, FeatureType type, SketchId input_sketch,
          ParameterId length_parameter, FeatureId target_feature, ExtrudeDirection direction,
          SubtractiveExtrudeDepth subtractive_depth, ExtrudeFeatureIntent extrude_intent,
          std::optional<FeatureBodyResultContext> body_result_context = std::nullopt);

  FeatureId id_;
  std::string name_;
  FeatureType type_;
  SketchId input_sketch_;
  ParameterId length_parameter_;
  FeatureId target_feature_;
  ExtrudeDirection direction_;
  SubtractiveExtrudeDepth subtractive_depth_;
  ExtrudeFeatureIntent extrude_intent_;
  std::optional<FeatureBodyResultContext> body_result_context_;
};

} // namespace blcad
