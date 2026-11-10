#pragma once

#include "blcad/core/feature.hpp"
#include "blcad/core/part_feature_input_reference.hpp"

#include <compare>
#include <cstddef>
#include <optional>
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

class PartDocument;
// Coordinates remain stable when either direction's count changes. They are
// separate from source identity and HolePlacementId, never flattened ordinals.
struct PatternOccurrenceIndex {
  std::size_t first = 0;
  std::size_t second = 0;
  friend auto operator<=>(const PatternOccurrenceIndex&, const PatternOccurrenceIndex&) = default;
};
struct LinearPatternDirection {
  AxisReference direction;
  ParameterId count;
  LinearPatternExtentMode extent_mode;
  ParameterId extent;
  PatternDirectionSign sign = PatternDirectionSign::Positive;
};
struct PatternPlacementOptions {
  std::optional<LinearPatternDirection> second_direction;
  // Dormant exclusions survive count reductions. The seed (0,0) cannot be
  // excluded: an in-place operation has already authored its original result.
  std::vector<PatternOccurrenceIndex> skipped;
};
struct PatternPlacement {
  PatternOccurrenceIndex index;
  double first_distance_mm = 0;
  double second_distance_mm = 0;
  double angle_deg = 0;
  bool skipped = false;
};
inline constexpr std::size_t pattern_placement_limit = 1024;
[[nodiscard]] std::string pattern_occurrence_key(PatternOccurrenceIndex index);

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

[[nodiscard]] std::string pattern_source_occurrence_key(const PatternSourceReference& source,
                                                        PatternOccurrenceIndex index,
                                                        bool extended = true);

class LinearPatternFeature {
public:
  [[nodiscard]] static Result<LinearPatternFeature>
  create(FeatureId id, std::string name, std::vector<PatternSourceReference> sources,
         AxisReference direction, ParameterId count_parameter, LinearPatternExtentMode extent_mode,
         ParameterId extent_parameter, PatternDirectionSign direction_sign,
         FeatureBodyResultContext body_result_context,
         std::optional<PatternPlacementOptions> placements = {});

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<PatternSourceReference>& sources() const noexcept;
  [[nodiscard]] const AxisReference& direction() const noexcept;
  [[nodiscard]] const ParameterId& count_parameter() const noexcept;
  [[nodiscard]] LinearPatternExtentMode extent_mode() const noexcept;
  [[nodiscard]] const ParameterId& extent_parameter() const noexcept;
  [[nodiscard]] PatternDirectionSign direction_sign() const noexcept;
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept;
  [[nodiscard]] const std::optional<PatternPlacementOptions>& placements() const noexcept;

private:
  LinearPatternFeature(FeatureId id, std::string name, std::vector<PatternSourceReference> sources,
                       AxisReference direction, ParameterId count_parameter,
                       LinearPatternExtentMode extent_mode, ParameterId extent_parameter,
                       PatternDirectionSign direction_sign,
                       FeatureBodyResultContext body_result_context,
                       std::optional<PatternPlacementOptions> placements);

  FeatureId id_;
  std::string name_;
  std::vector<PatternSourceReference> sources_;
  AxisReference direction_;
  ParameterId count_parameter_;
  LinearPatternExtentMode extent_mode_;
  ParameterId extent_parameter_;
  PatternDirectionSign direction_sign_;
  FeatureBodyResultContext body_result_context_;
  std::optional<PatternPlacementOptions> placements_;
};

class CircularPatternFeature {
public:
  [[nodiscard]] static Result<CircularPatternFeature>
  create(FeatureId id, std::string name, std::vector<PatternSourceReference> sources,
         AxisReference axis, ParameterId count_parameter, double total_angle_deg,
         bool equal_spacing, FeatureBodyResultContext body_result_context,
         std::optional<PatternPlacementOptions> placements = {});

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<PatternSourceReference>& sources() const noexcept;
  [[nodiscard]] const AxisReference& axis() const noexcept;
  [[nodiscard]] const ParameterId& count_parameter() const noexcept;
  [[nodiscard]] double total_angle_deg() const noexcept;
  [[nodiscard]] bool equal_spacing() const noexcept;
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept;
  [[nodiscard]] const std::optional<PatternPlacementOptions>& placements() const noexcept;

private:
  CircularPatternFeature(FeatureId id, std::string name,
                         std::vector<PatternSourceReference> sources, AxisReference axis,
                         ParameterId count_parameter, double total_angle_deg, bool equal_spacing,
                         FeatureBodyResultContext body_result_context,
                         std::optional<PatternPlacementOptions> placements);

  FeatureId id_;
  std::string name_;
  std::vector<PatternSourceReference> sources_;
  AxisReference axis_;
  ParameterId count_parameter_;
  double total_angle_deg_;
  bool equal_spacing_;
  FeatureBodyResultContext body_result_context_;
  std::optional<PatternPlacementOptions> placements_;
};

// The shared placement authority; no OCCT geometry or copied Feature records.
// Legacy records retain their >=2 contract. Explicit placement intent permits
// one occurrence, two-direction linear layout and persistent exclusions.
[[nodiscard]] Result<std::vector<PatternPlacement>>
evaluate_pattern_placements(const PartDocument&, const LinearPatternFeature&);
[[nodiscard]] Result<std::vector<PatternPlacement>>
evaluate_pattern_placements(const PartDocument&, const CircularPatternFeature&);

} // namespace blcad
