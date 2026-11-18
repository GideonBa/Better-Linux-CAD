#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/model_state.hpp"
#include "blcad/core/path_curve.hpp"
#include <utility>
#include <variant>

namespace blcad {
struct ProjectDesignReferenceIdTag;
using ProjectDesignReferenceId = TypedId<ProjectDesignReferenceIdTag>;

struct ProjectDesignOccurrence {
  std::vector<SubassemblyInstanceId> path;
  ComponentInstanceId component;
  DocumentId part;
  friend bool operator==(const ProjectDesignOccurrence&, const ProjectDesignOccurrence&) = default;
};
struct ProjectDesignSemanticSource {
  std::string reference;
  friend bool operator==(const ProjectDesignSemanticSource&,
                         const ProjectDesignSemanticSource&) = default;
};
struct ProjectDesignPathSource {
  PathCurveId path;
  friend bool operator==(const ProjectDesignPathSource&, const ProjectDesignPathSource&) = default;
};
struct ProjectDerivedPathTarget {
  PathCurveId path;
  Sketch3DId sketch;
  friend bool operator==(const ProjectDerivedPathTarget&,
                         const ProjectDerivedPathTarget&) = default;
};
using ProjectDesignSource = std::variant<ProjectDesignSemanticSource, ProjectDesignPathSource>;
using ProjectDesignTarget = std::variant<ConstructionPointId, ConstructionLineId,
                                         ConstructionPlaneId, ProjectDerivedPathTarget>;

// A directed link owns semantic source identity, state and one explicit driver
// occurrence. Target constructions contain local derived values, never world
// samples or native topology identity. Detach removes this link and keeps the
// last checked target construction as an independent snapshot.
class ProjectDesignReference {
public:
  [[nodiscard]] static Result<ProjectDesignReference>
  create(ProjectDesignReferenceId id, std::string name, ProjectDesignOccurrence source,
         ModelStateId source_state, ProjectDesignSource source_geometry,
         ProjectDesignOccurrence driver, ProjectDesignTarget target);
  [[nodiscard]] const ProjectDesignReferenceId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const ProjectDesignOccurrence& source() const noexcept {
    return source_;
  }
  [[nodiscard]] const ModelStateId& source_state() const noexcept {
    return source_state_;
  }
  [[nodiscard]] const ProjectDesignSource& source_geometry() const noexcept {
    return source_geometry_;
  }
  [[nodiscard]] const ProjectDesignOccurrence& driver() const noexcept {
    return driver_;
  }
  [[nodiscard]] const ProjectDesignTarget& target() const noexcept {
    return target_;
  }
  [[nodiscard]] std::string target_key() const;
  friend bool operator==(const ProjectDesignReference&, const ProjectDesignReference&) = default;

private:
  ProjectDesignReference(ProjectDesignReferenceId, std::string, ProjectDesignOccurrence,
                         ModelStateId, ProjectDesignSource, ProjectDesignOccurrence,
                         ProjectDesignTarget);
  ProjectDesignReferenceId id_;
  std::string name_;
  ProjectDesignOccurrence source_;
  ModelStateId source_state_;
  ProjectDesignSource source_geometry_;
  ProjectDesignOccurrence driver_;
  ProjectDesignTarget target_;
};

class Project;
// Core-only ordering and structural validation. Geometry/source freshness and
// shape/pose feedback validation happen before candidate publication.
[[nodiscard]] Result<std::vector<DocumentId>> project_design_dependency_order(
    const Project&,
    const std::vector<std::pair<DocumentId, DocumentId>>& additional_dependencies = {});
} // namespace blcad
