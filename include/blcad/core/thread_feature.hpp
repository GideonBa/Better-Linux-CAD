#pragma once

#include "blcad/core/engineering_resource.hpp"
#include "blcad/core/part_feature_input_reference.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad {

class PartDocument;

enum class ThreadHandedness { Right, Left };
enum class ThreadRepresentation { Cosmetic, Modeled };
enum class ThreadDirection { AlongSupportAxis, OppositeSupportAxis };
[[nodiscard]] std::string_view to_string(ThreadHandedness) noexcept;
[[nodiscard]] std::string_view to_string(ThreadRepresentation) noexcept;
[[nodiscard]] std::string_view to_string(ThreadDirection) noexcept;

// Authored labels do not imply that a standard or tolerance has been inferred.
// Diameters are specified at the start of the threaded interval. Optional signed
// taper is the cone half-angle; omission means a straight thread.
struct ThreadParameterProfile {
  std::string system;
  std::string designation;
  std::string tolerance;
  ParameterId major_diameter;
  ParameterId minor_diameter;
  ParameterId pitch;
  ParameterId included_angle;
  std::optional<ParameterId> taper_angle;
};

struct ThreadStandardColumns {
  std::string system;
  std::string designation;
  std::string tolerance;
  std::string major_diameter;
  std::string minor_diameter;
  std::string pitch;
  std::string included_angle;
  std::optional<std::string> taper_angle;
};
struct ThreadStandardProfile {
  EngineeringResourceKey table;
  std::string row;
  ThreadStandardColumns columns;
};
using ThreadProfile = std::variant<ThreadParameterProfile, ThreadStandardProfile>;

// Shared by internal Hole threads and standalone external ThreadFeature intent.
// No kernel geometry, resolved numeric cache or drill-depth value is persisted here.
class ThreadDefinition {
public:
  [[nodiscard]] static Result<ThreadDefinition>
  create(ThreadProfile profile, ParameterId length, std::optional<ParameterId> start = {},
         ThreadHandedness handedness = ThreadHandedness::Right,
         ThreadRepresentation representation = ThreadRepresentation::Cosmetic);
  [[nodiscard]] const ThreadProfile& profile() const noexcept {
    return profile_;
  }
  [[nodiscard]] const ParameterId& length() const noexcept {
    return length_;
  }
  // Omission means zero offset. Existing Length parameters are strictly positive.
  [[nodiscard]] const std::optional<ParameterId>& start() const noexcept {
    return start_;
  }
  [[nodiscard]] ThreadHandedness handedness() const noexcept {
    return handedness_;
  }
  [[nodiscard]] ThreadRepresentation representation() const noexcept {
    return representation_;
  }
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;
  [[nodiscard]] std::vector<EngineeringResourceKey> standard_dependencies() const;
  [[nodiscard]] Result<ThreadDefinition>
  remap_standard_revision(const EngineeringResourceKey& source,
                          const EngineeringResourceKey& next) const;

private:
  ThreadDefinition(ThreadProfile profile, ParameterId length, std::optional<ParameterId> start,
                   ThreadHandedness handedness, ThreadRepresentation representation);
  ThreadProfile profile_;
  ParameterId length_;
  std::optional<ParameterId> start_;
  ThreadHandedness handedness_;
  ThreadRepresentation representation_;
};

struct ThreadFieldProvenance {
  std::string field;
  EngineeringProvenance provenance;
};
struct ResolvedThreadDefinition {
  std::string system, designation, tolerance;
  double major_diameter_mm = 0, minor_diameter_mm = 0, pitch_mm = 0;
  double included_angle_deg = 0, taper_angle_deg = 0;
  double start_mm = 0, length_mm = 0;
  ThreadHandedness handedness = ThreadHandedness::Right;
  ThreadRepresentation representation = ThreadRepresentation::Cosmetic;
  std::optional<EngineeringResourceKey> standard;
  std::vector<ThreadFieldProvenance> provenance;
};
[[nodiscard]] Result<ResolvedThreadDefinition>
resolve_thread_definition(const PartDocument& part, const ThreadDefinition& definition,
                          std::string_view context = "thread");

// External thread modifier. Constructing a record does not modify a Part.
class ThreadFeature {
public:
  [[nodiscard]] static Result<ThreadFeature>
  create(FeatureId id, std::string name, BodyId target_body, FaceReference support,
         ThreadDefinition thread, ThreadDirection direction = ThreadDirection::AlongSupportAxis);
  [[nodiscard]] const FeatureId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const BodyId& target_body() const noexcept {
    return target_body_;
  }
  [[nodiscard]] const FaceReference& support() const noexcept {
    return support_;
  }
  [[nodiscard]] const ThreadDefinition& thread() const noexcept {
    return thread_;
  }
  [[nodiscard]] ThreadDirection direction() const noexcept {
    return direction_;
  }
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const {
    return thread_.parameter_dependencies();
  }
  [[nodiscard]] std::vector<EngineeringResourceKey> standard_dependencies() const {
    return thread_.standard_dependencies();
  }
  [[nodiscard]] Result<ThreadFeature>
  remap_standard_revision(const EngineeringResourceKey& source,
                          const EngineeringResourceKey& next) const;

private:
  ThreadFeature(FeatureId id, std::string name, BodyId target_body, FaceReference support,
                ThreadDefinition thread, ThreadDirection direction);
  FeatureId id_;
  std::string name_;
  BodyId target_body_;
  FaceReference support_;
  ThreadDefinition thread_;
  ThreadDirection direction_;
};
[[nodiscard]] Result<std::size_t> validate_thread_feature_inputs(const PartDocument& part,
                                                                 const ThreadFeature& feature);

} // namespace blcad
