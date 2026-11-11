#pragma once

#include "blcad/core/part_pattern_feature.hpp"

namespace blcad {
enum class HoleReferenceRole { Axis, Entry, Seat, Step };
[[nodiscard]] std::string_view to_string(HoleReferenceRole role) noexcept;
struct HolePatternOccurrence {
  FeatureId pattern;
  PatternOccurrenceIndex index;
  friend bool operator==(const HolePatternOccurrence&, const HolePatternOccurrence&) = default;
};

// A Hole placement and a Pattern occurrence are independent semantic identities.
// No evaluated coordinates, radius, topology index or face witness is persisted.
class HoleInstanceReference {
public:
  [[nodiscard]] static Result<HoleInstanceReference>
  create(FeatureId hole, HolePlacementId placement, HoleReferenceRole role,
         std::optional<HolePatternOccurrence> occurrence = {}, std::string step = {});
  [[nodiscard]] const FeatureId& hole() const noexcept {
    return hole_;
  }
  [[nodiscard]] const HolePlacementId& placement() const noexcept {
    return placement_;
  }
  [[nodiscard]] HoleReferenceRole role() const noexcept {
    return role_;
  }
  [[nodiscard]] const std::optional<HolePatternOccurrence>& occurrence() const noexcept {
    return occurrence_;
  }
  [[nodiscard]] const std::string& step() const noexcept {
    return step_;
  }
  [[nodiscard]] FeatureId producer() const {
    return occurrence_ ? occurrence_->pattern : hole_;
  }
  [[nodiscard]] ResultFaceOrigin origin(std::string suffix) const;
  friend bool operator==(const HoleInstanceReference&, const HoleInstanceReference&) = default;

private:
  HoleInstanceReference(FeatureId hole, HolePlacementId placement, HoleReferenceRole role,
                        std::optional<HolePatternOccurrence> occurrence, std::string step)
      : hole_(std::move(hole)), placement_(std::move(placement)), role_(role),
        occurrence_(std::move(occurrence)), step_(std::move(step)) {}
  FeatureId hole_;
  HolePlacementId placement_;
  HoleReferenceRole role_;
  std::optional<HolePatternOccurrence> occurrence_;
  std::string step_;
};

// Canonical, dot-free reserved Assembly endpoint grammar. IDs use the existing
// semantic-target ASCII-safe / uppercase %HH byte escaping convention.
[[nodiscard]] Result<std::string> make_hole_target_spelling(const HoleInstanceReference&);
[[nodiscard]] Result<HoleInstanceReference> parse_hole_target_spelling(std::string_view);
[[nodiscard]] bool is_hole_target_spelling(std::string_view) noexcept;
// This is a resolution check, not an Assembly-loading gate. Removed/excluded
// occurrences retain their endpoint strings and diagnose their own relationship.
[[nodiscard]] Result<BodyId> validate_hole_instance_reference(const PartDocument&,
                                                              const HoleInstanceReference&);
} // namespace blcad
