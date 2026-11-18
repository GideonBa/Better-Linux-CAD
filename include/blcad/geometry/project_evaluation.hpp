#pragma once
#include "blcad/core/project.hpp"
namespace blcad::geometry {
class StepImportTopologyContext;
// Materialize directed references and coordinated machining in disposable local
// definitions, validate every downstream Part and the complete posed Assembly.
// No caller-owned document, cached result, resource or external asset changes.
[[nodiscard]] Result<Project> evaluate_project_geometry(const Project&,
                                                        StepImportTopologyContext* = nullptr);
} // namespace blcad::geometry
