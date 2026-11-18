#pragma once

#include "blcad/core/hole_feature.hpp"
#include "blcad/core/project_design_reference.hpp"
#include <optional>
#include <string>
#include <vector>

namespace blcad {
enum class AssemblyHoleParticipantRole { Clearance, Threaded, Seat };
enum class AssemblyHoleDefinitionPolicy { SingleOccurrence, AffectSharedDefinition };
struct AssemblyHoleLayout {
  ProjectDesignOccurrence source;
  ModelStateId state{primary_model_state_id()};
  FeatureId hole;
  std::optional<FeatureId> pattern;
};
struct AssemblyHoleParticipant {
  std::string id;
  ProjectDesignOccurrence occurrence;
  ModelStateId state{primary_model_state_id()};
  AssemblyHoleParticipantRole role;
  AssemblyHoleDefinitionPolicy definition_policy;
  // The canonical Hole/Thread recipe retains participant-local depth/support,
  // pinned standard cells and shared HolePlacementId values. Execution derives
  // actual local placement coordinates from the shared layout, never a second
  // diameter, thread, pattern or machining interpreter.
  HoleFeature recipe;
};
class AssemblyHoleFeature {
public:
  [[nodiscard]] static Result<AssemblyHoleFeature>
      create(FeatureId, std::string, AssemblyHoleLayout, std::vector<AssemblyHoleParticipant>);
  [[nodiscard]] const FeatureId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const AssemblyHoleLayout& layout() const noexcept {
    return layout_;
  }
  [[nodiscard]] const std::vector<AssemblyHoleParticipant>& participants() const noexcept {
    return participants_;
  }

private:
  AssemblyHoleFeature(FeatureId, std::string, AssemblyHoleLayout,
                      std::vector<AssemblyHoleParticipant>);
  FeatureId id_;
  std::string name_;
  AssemblyHoleLayout layout_;
  std::vector<AssemblyHoleParticipant> participants_;
};
class Project;
[[nodiscard]] Result<std::size_t> validate_assembly_hole_features(const Project&);
} // namespace blcad
