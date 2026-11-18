#pragma once
#include "blcad/core/project.hpp"
#include "blcad/core/project_design_reference.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"

namespace blcad::geometry {
class StepImportTopologyContext;
// Exact authored/effective occurrence chain. The mapping transforms
// source-local points/vectors into target-local coordinates without composing
// Euler angles.
[[nodiscard]] Result<GeometryAffineTransform>
project_occurrence_transform(const Project&, const ProjectDesignOccurrence&);
[[nodiscard]] Result<GeometryAffineTransform>
project_occurrence_mapping(const Project&, const ProjectDesignOccurrence& source,
                           const ProjectDesignOccurrence& target);

struct AdaptiveProjectEvaluation {
  Project project;
  std::vector<DocumentId> changed_parts;
};
class AdaptiveProjectEvaluator {
public:
  explicit AdaptiveProjectEvaluator(StepImportTopologyContext* imports = nullptr)
      : imports_(imports) {}
  // Every method owns a disposable candidate. Callers publish its returned
  // Project through their ordinary root transaction only after all checks pass.
  [[nodiscard]] Result<AdaptiveProjectEvaluation> evaluate(const Project&) const;
  [[nodiscard]] Result<AdaptiveProjectEvaluation> bind(const Project&,
                                                       ProjectDesignReference) const;
  [[nodiscard]] Result<Project> detach(const Project&, ProjectDesignReferenceId) const;

private:
  StepImportTopologyContext* imports_;
};
} // namespace blcad::geometry
