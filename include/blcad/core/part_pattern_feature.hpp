#pragma once

#include "blcad/core/feature.hpp"
#include "blcad/core/part_feature_input_reference.hpp"

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad {

enum class PatternSourceKind { Feature, Body };
enum class LinearPatternExtentMode { Spacing, TotalExtent };
enum class PatternDirectionSign { Positive, Negative };

[[nodiscard]] std::string_view to_string(PatternSourceKind kind) noexcept;
[[nodiscard]] std::string_view to_string(LinearPatternExtentMode mode) noexcept;
[[nodiscard]] std::string_view to_string(PatternDirectionSign sign) noexcept;

using PatternSource = std::variant<FeatureId, BodyId>;

class PatternSourceReference {
public:
  [[nodiscard]] static Result<PatternSourceReference> feature(FeatureId feature);
  [[nodiscard]] static Result<PatternSourceReference> body(BodyId body);

  [[nodiscard]] PatternSourceKind kind() const noexcept;
  [[nodiscard]] const PatternSource& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

  friend bool operator==(const PatternSourceReference&, const PatternSourceReference&) = default;

private:
  explicit PatternSourceReference(PatternSource source);
  PatternSource source_;
};

class LinearPatternFeature {
public:
  [[nodiscard]] static Result<LinearPatternFeature>
  create(FeatureId id, std::string name, std::vector<PatternSourceReference> sources,
         AxisReference direction, ParameterId count_parameter, LinearPatternExtentMode extent_mode,
         ParameterId extent_parameter, PatternDirectionSign direction_sign,
         FeatureBodyResultContext body_result_context);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<PatternSourceReference>& sources() const noexcept;
  [[nodiscard]] const AxisReference& direction() const noexcept;
  [[nodiscard]] const ParameterId& count_parameter() const noexcept;
  [[nodiscard]] LinearPatternExtentMode extent_mode() const noexcept;
  [[nodiscard]] const ParameterId& extent_parameter() const noexcept;
  [[nodiscard]] PatternDirectionSign direction_sign() const noexcept;
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept;

private:
  LinearPatternFeature(FeatureId id, std::string name, std::vector<PatternSourceReference> sources,
                       AxisReference direction, ParameterId count_parameter,
                       LinearPatternExtentMode extent_mode, ParameterId extent_parameter,
                       PatternDirectionSign direction_sign,
                       FeatureBodyResultContext body_result_context);

  FeatureId id_;
  std::string name_;
  std::vector<PatternSourceReference> sources_;
  AxisReference direction_;
  ParameterId count_parameter_;
  LinearPatternExtentMode extent_mode_;
  ParameterId extent_parameter_;
  PatternDirectionSign direction_sign_;
  FeatureBodyResultContext body_result_context_;
};

class CircularPatternFeature {
public:
  [[nodiscard]] static Result<CircularPatternFeature>
  create(FeatureId id, std::string name, std::vector<PatternSourceReference> sources,
         AxisReference axis, ParameterId count_parameter, double total_angle_deg,
         bool equal_spacing, FeatureBodyResultContext body_result_context);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<PatternSourceReference>& sources() const noexcept;
  [[nodiscard]] const AxisReference& axis() const noexcept;
  [[nodiscard]] const ParameterId& count_parameter() const noexcept;
  [[nodiscard]] double total_angle_deg() const noexcept;
  [[nodiscard]] bool equal_spacing() const noexcept;
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept;

private:
  CircularPatternFeature(FeatureId id, std::string name,
                         std::vector<PatternSourceReference> sources, AxisReference axis,
                         ParameterId count_parameter, double total_angle_deg, bool equal_spacing,
                         FeatureBodyResultContext body_result_context);

  FeatureId id_;
  std::string name_;
  std::vector<PatternSourceReference> sources_;
  AxisReference axis_;
  ParameterId count_parameter_;
  double total_angle_deg_;
  bool equal_spacing_;
  FeatureBodyResultContext body_result_context_;
};

} // namespace blcad
