#pragma once
#include "blcad/core/project.hpp"
#include <memory>
#include <optional>

namespace blcad::geometry {
class StepImportTopologyContext;
namespace detail {
inline thread_local std::size_t project_evaluation_depth = 0;
class ProjectEvaluationGuard {
public:
  ProjectEvaluationGuard() {
    ++project_evaluation_depth;
  }
  ~ProjectEvaluationGuard() {
    --project_evaluation_depth;
  }
  ProjectEvaluationGuard(const ProjectEvaluationGuard&) = delete;
  ProjectEvaluationGuard& operator=(const ProjectEvaluationGuard&) = delete;
};
[[nodiscard]] Result<std::size_t> validate_project_feedback(const Project&);
[[nodiscard]] Result<std::size_t> settle_project_pose(Project&, StepImportTopologyContext*);
[[nodiscard]] Result<Project> stage_adaptive_part(const Project&, DocumentId,
                                                  StepImportTopologyContext*);
[[nodiscard]] Result<Project> stage_assembly_holes(const Project&, DocumentId,
                                                   StepImportTopologyContext*);
[[nodiscard]] Result<std::size_t> verify_assembly_holes(const Project&, StepImportTopologyContext*);

// Outer public consumers normalize their candidate once. Evaluator-internal
// checks consume the current disposable candidate and cannot re-enter rebuild.
class ProjectGeometryScope {
public:
  [[nodiscard]] static Result<std::unique_ptr<ProjectGeometryScope>>
  create(const Project&, StepImportTopologyContext*);
  [[nodiscard]] bool outer() const noexcept {
    return outer_;
  }
  [[nodiscard]] bool changed() const noexcept {
    return evaluated_.has_value();
  }
  [[nodiscard]] const Project& project() const {
    return *evaluated_;
  }

private:
  ProjectEvaluationGuard guard_;
  bool outer_ = false;
  std::optional<Project> evaluated_;
};
} // namespace detail
} // namespace blcad::geometry
