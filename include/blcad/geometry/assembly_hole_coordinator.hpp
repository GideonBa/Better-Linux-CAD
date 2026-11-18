#pragma once
#include "blcad/core/assembly_hole_feature.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/core/project.hpp"
namespace blcad::geometry {
class StepImportTopologyContext;
struct CoordinatedAssemblyHoleEvaluation {
  Project project;
  std::vector<DocumentId> changed_parts;
};
[[nodiscard]] HolePlacementId
    assembly_hole_placement_id(HolePlacementId, std::optional<PatternOccurrenceIndex> = {});
class AssemblyHoleCoordinator {
public:
  explicit AssemblyHoleCoordinator(StepImportTopologyContext* imports = nullptr)
      : imports_(imports) {}
  [[nodiscard]] Result<CoordinatedAssemblyHoleEvaluation> evaluate(const Project&) const;
  [[nodiscard]] Result<CoordinatedAssemblyHoleEvaluation> bind(const Project&,
                                                               AssemblyHoleFeature) const;
  // Explicitly retain the last local Hole snapshots and release coordination.
  [[nodiscard]] Result<Project> detach(const Project&, FeatureId) const;

private:
  StepImportTopologyContext* imports_;
};
} // namespace blcad::geometry
