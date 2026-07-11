#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/recompute_plan.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace blcad {

class ProjectPartUpdate {
public:
  ProjectPartUpdate(DocumentId part_document, BindingApplication binding_application,
                    RecomputePlan recompute_plan);

  [[nodiscard]] const DocumentId& part_document() const noexcept;
  [[nodiscard]] const BindingApplication& binding_application() const noexcept;
  [[nodiscard]] const RecomputePlan& recompute_plan() const noexcept;

private:
  DocumentId part_document_;
  BindingApplication binding_application_;
  RecomputePlan recompute_plan_;
};

class ProjectUpdateResult {
public:
  explicit ProjectUpdateResult(std::vector<ProjectPartUpdate> part_updates);

  [[nodiscard]] const std::vector<ProjectPartUpdate>& part_updates() const noexcept;
  [[nodiscard]] std::size_t updated_part_count() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

private:
  std::vector<ProjectPartUpdate> part_updates_;
};

// Project-level container for one assembly document and its owned part documents.
// It is an MVP container only: placements are stored as explicit component
// instance records, while assembly constraints and solver state remain later
// assembly-system MVP work.
class Project {
public:
  [[nodiscard]] static Result<Project> create(DocumentId id, std::string name,
                                              AssemblyDocument assembly);

  [[nodiscard]] Result<std::size_t> add_part_document(PartDocument part_document);
  [[nodiscard]] Result<std::size_t> validate_member_parts() const;
  [[nodiscard]] Result<std::size_t> validate_component_instances() const;
  [[nodiscard]] Result<std::size_t> validate_assembly_structure() const;

  // Changes one assembly parameter, applies all assembly bindings to owned
  // member parts, and returns per-part recompute plans for affected parts.
  [[nodiscard]] Result<ProjectUpdateResult> set_assembly_parameter_value(ParameterId id,
                                                                        Quantity value);

  [[nodiscard]] const DocumentId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const AssemblyDocument& assembly() const noexcept;
  [[nodiscard]] AssemblyDocument& assembly() noexcept;
  [[nodiscard]] const std::vector<PartDocument>& part_documents() const noexcept;
  [[nodiscard]] std::vector<PartDocument>& part_documents() noexcept;
  [[nodiscard]] std::size_t part_document_count() const noexcept;
  [[nodiscard]] const PartDocument* find_part_document(DocumentId id) const noexcept;
  [[nodiscard]] PartDocument* find_part_document(DocumentId id) noexcept;

private:
  Project(DocumentId id, std::string name, AssemblyDocument assembly);

  [[nodiscard]] bool has_part_document_id(const DocumentId& id) const noexcept;

  DocumentId id_;
  std::string name_;
  AssemblyDocument assembly_;
  std::vector<PartDocument> part_documents_;
};

} // namespace blcad
