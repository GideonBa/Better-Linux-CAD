#pragma once

#include "blcad/core/engineering_resource.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/thread_feature.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad {

class PartDocument;

enum class HoleKind { Plain, Clearance };
enum class HoleDirection { SupportNormal, OppositeSupportNormal };
[[nodiscard]] std::string_view to_string(HoleKind kind) noexcept;
[[nodiscard]] std::string_view to_string(HoleDirection direction) noexcept;

struct HoleSketchPointReference {
  SketchId sketch;
  SketchEntityId point;
};
struct HolePlacement {
  HolePlacementId id;
  // Owned coordinates are millimeters in the support frame. Sketch points keep
  // their existing identity/frame; geometric coplanarity is checked at execution.
  std::variant<Point2, HoleSketchPointReference> source;
};
struct HoleStandardSelection {
  EngineeringResourceKey table;
  std::string row;
  std::string diameter_column;
  friend bool operator==(const HoleStandardSelection&, const HoleStandardSelection&) = default;
};
using HoleDiameter = std::variant<ParameterId, HoleStandardSelection>;

struct HoleBlindExtent {
  ParameterId depth;
};
struct HoleThroughAllExtent {};
struct HoleToNextExtent {};
struct HoleToFaceExtent {
  FaceReference face;
};
struct HoleUpToBodyExtent {
  BodyId body;
};
struct HoleSymmetricExtent {
  ParameterId total_depth;
};
using HoleExtent = std::variant<HoleBlindExtent, HoleThroughAllExtent, HoleToNextExtent,
                                HoleToFaceExtent, HoleUpToBodyExtent, HoleSymmetricExtent>;
struct HoleFlatTip {};
struct HoleConicalTip {
  ParameterId included_angle;
};
using HoleTip = std::variant<HoleFlatTip, HoleConicalTip>;

// Reuse the existing pinned table-cell selection. Its legacy diameter_column
// member names the selected column also for seat lengths and included angles.
using HoleDimension = HoleDiameter;
struct HoleCounterbore {
  HoleDimension diameter;
  HoleDimension depth;
};
struct HoleCountersink {
  HoleDimension diameter;
  HoleDimension included_angle;
};
struct HoleSpotface {
  HoleDimension diameter;
  HoleDimension depth;
};
struct HoleStep {
  std::string id;
  HoleDimension diameter;
  // Absolute end depth from the authored support, never an incremental length.
  HoleDimension depth;
};
struct HoleStepped {
  std::vector<HoleStep> steps;
};
enum class HoleFitProcess { Fitted, Reamed };
struct HoleFitted {
  std::string designation;
  HoleDimension minimum_diameter;
  HoleDimension maximum_diameter;
  HoleFitProcess process = HoleFitProcess::Fitted;
};
struct HoleCenter {
  HoleDimension diameter;
  HoleDimension included_angle;
};
struct HoleSlot {
  ProfileRegionReference profile;
};
using HoleManufacturing = std::variant<std::monostate, HoleCounterbore, HoleCountersink,
                                       HoleSpotface, HoleStepped, HoleFitted, HoleCenter, HoleSlot>;

// Semantic intent only. This record does not create a sketch cut or an
// independent placement-pattern interpreter.
class HoleFeature {
public:
  [[nodiscard]] static Result<HoleFeature>
  create(FeatureId id, std::string name, HoleKind kind, BodyId target_body, PlaneReference support,
         std::vector<HolePlacement> placements, HoleDiameter diameter,
         HoleExtent extent = HoleThroughAllExtent{},
         HoleDirection direction = HoleDirection::OppositeSupportNormal,
         HoleTip tip = HoleFlatTip{}, HoleManufacturing manufacturing = {},
         std::optional<ThreadDefinition> thread = {});

  [[nodiscard]] const FeatureId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] HoleKind kind() const noexcept {
    return kind_;
  }
  [[nodiscard]] const BodyId& target_body() const noexcept {
    return target_body_;
  }
  [[nodiscard]] const PlaneReference& support() const noexcept {
    return support_;
  }
  [[nodiscard]] const std::vector<HolePlacement>& placements() const noexcept {
    return placements_;
  }
  [[nodiscard]] const HoleDiameter& diameter() const noexcept {
    return diameter_;
  }
  [[nodiscard]] const HoleExtent& extent() const noexcept {
    return extent_;
  }
  [[nodiscard]] HoleDirection direction() const noexcept {
    return direction_;
  }
  [[nodiscard]] const HoleTip& tip() const noexcept {
    return tip_;
  }
  [[nodiscard]] const HoleManufacturing& manufacturing() const noexcept {
    return manufacturing_;
  }
  [[nodiscard]] const std::optional<ThreadDefinition>& thread() const noexcept {
    return thread_;
  }
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;
  [[nodiscard]] std::vector<EngineeringResourceKey> standard_dependencies() const;
  [[nodiscard]] Result<HoleFeature>
  remap_standard_revision(const EngineeringResourceKey& source,
                          const EngineeringResourceKey& next) const;

private:
  HoleFeature(FeatureId id, std::string name, HoleKind kind, BodyId target_body,
              PlaneReference support, std::vector<HolePlacement> placements, HoleDiameter diameter,
              HoleExtent extent, HoleDirection direction, HoleTip tip,
              HoleManufacturing manufacturing, std::optional<ThreadDefinition> thread);
  FeatureId id_;
  std::string name_;
  HoleKind kind_;
  BodyId target_body_;
  PlaneReference support_;
  std::vector<HolePlacement> placements_;
  HoleDiameter diameter_;
  HoleExtent extent_;
  HoleDirection direction_;
  HoleTip tip_;
  HoleManufacturing manufacturing_;
  std::optional<ThreadDefinition> thread_;
};

struct ResolvedHoleDiameter {
  Quantity diameter;
  // Standard-derived dimensions disclose the exact cell's provenance. Custom
  // dimensions remain parameters, without fabricated standards attribution.
  std::optional<EngineeringProvenance> provenance;
};
[[nodiscard]] Result<ResolvedHoleDiameter> resolve_hole_diameter(const PartDocument& part,
                                                                 const HoleDiameter& diameter);
// Angles select Angle cells/parameters; all other dimensions select positive Length.
[[nodiscard]] Result<ResolvedHoleDiameter> resolve_hole_dimension(const PartDocument& part,
                                                                  const HoleDimension& dimension,
                                                                  bool angle = false);
[[nodiscard]] Result<std::size_t> validate_hole_feature_inputs(const PartDocument& part,
                                                               const HoleFeature& feature);

} // namespace blcad
