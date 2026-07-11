#pragma once

#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_concentric_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace blcad::geometry {

// Stable read-only endpoint identity for a relationship target anywhere in the
// rooted assembly occurrence tree. An empty path addresses the root assembly;
// non-empty paths are exact root-to-occurrence SubassemblyInstanceId sequences.
class AssemblyHierarchyConstraintTarget {
public:
  [[nodiscard]] static Result<AssemblyHierarchyConstraintTarget>
  create(std::vector<SubassemblyInstanceId> occurrence_path,
         ComponentInstanceId component_instance,
         std::string semantic_reference);

  [[nodiscard]] const std::vector<SubassemblyInstanceId>& occurrence_path() const noexcept;
  [[nodiscard]] const ComponentInstanceId& component_instance() const noexcept;
  [[nodiscard]] const std::string& semantic_reference() const noexcept;

  friend bool operator==(const AssemblyHierarchyConstraintTarget&,
                         const AssemblyHierarchyConstraintTarget&) = default;

private:
  AssemblyHierarchyConstraintTarget(std::vector<SubassemblyInstanceId> occurrence_path,
                                    ComponentInstanceId component_instance,
                                    std::string semantic_reference);

  std::vector<SubassemblyInstanceId> occurrence_path_;
  ComponentInstanceId component_instance_;
  std::string semantic_reference_;
};

// Derived relationship query. It deliberately reuses AssemblyConstraintType and
// the existing Distance/Angle value contract without becoming persistent model
// intent or entering an AssemblyDocument constraint collection.
class AssemblyHierarchyConstraintQuery {
public:
  [[nodiscard]] static Result<AssemblyHierarchyConstraintQuery>
  create(AssemblyConstraintId id,
         AssemblyConstraintType type,
         AssemblyHierarchyConstraintTarget target_a,
         AssemblyHierarchyConstraintTarget target_b,
         std::optional<Quantity> distance = std::nullopt,
         std::optional<Quantity> angle = std::nullopt);

  [[nodiscard]] const AssemblyConstraintId& id() const noexcept;
  [[nodiscard]] AssemblyConstraintType type() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintTarget& target_a() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintTarget& target_b() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& distance() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& angle() const noexcept;

  friend bool operator==(const AssemblyHierarchyConstraintQuery&,
                         const AssemblyHierarchyConstraintQuery&) = default;

private:
  AssemblyHierarchyConstraintQuery(AssemblyConstraintId id,
                                   AssemblyConstraintType type,
                                   AssemblyHierarchyConstraintTarget target_a,
                                   AssemblyHierarchyConstraintTarget target_b,
                                   std::optional<Quantity> distance,
                                   std::optional<Quantity> angle);

  AssemblyConstraintId id_;
  AssemblyConstraintType type_;
  AssemblyHierarchyConstraintTarget target_a_;
  AssemblyHierarchyConstraintTarget target_b_;
  std::optional<Quantity> distance_;
  std::optional<Quantity> angle_;
};

struct AssemblyHierarchyPlanarConstraintTargetDescriptor {
  std::vector<SubassemblyInstanceId> occurrence_path;
  DocumentId assembly_document;
  ComponentInstanceId component_instance;
  DocumentId referenced_part_document;
  FeatureId source_feature;
  SemanticFace face = SemanticFace::Top;
  std::string semantic_reference;
  AssemblySpacePlanarDescriptor plane;

  friend bool operator==(const AssemblyHierarchyPlanarConstraintTargetDescriptor&,
                         const AssemblyHierarchyPlanarConstraintTargetDescriptor&) = default;
};

struct AssemblyHierarchyAxisConstraintTargetDescriptor {
  std::vector<SubassemblyInstanceId> occurrence_path;
  DocumentId assembly_document;
  ComponentInstanceId component_instance;
  DocumentId referenced_part_document;
  FeatureId source_feature;
  ProfileId source_profile;
  SemanticAxis semantic_axis = SemanticAxis::Primary;
  std::string semantic_reference;
  AssemblySpaceAxisDescriptor axis;

  friend bool operator==(const AssemblyHierarchyAxisConstraintTargetDescriptor&,
                         const AssemblyHierarchyAxisConstraintTargetDescriptor&) = default;
};

struct AssemblyHierarchyInsertConstraintTargetDescriptor {
  std::vector<SubassemblyInstanceId> occurrence_path;
  DocumentId assembly_document;
  ComponentInstanceId component_instance;
  DocumentId referenced_part_document;
  FeatureId source_feature;
  ProfileId source_profile;
  SemanticAxis semantic_axis = SemanticAxis::Primary;
  SemanticSeatingPlane semantic_seating_plane = SemanticSeatingPlane::Primary;
  std::string semantic_reference;
  AssemblySpaceAxisDescriptor axis;
  AssemblySpacePlanarDescriptor seating_plane;

  friend bool operator==(const AssemblyHierarchyInsertConstraintTargetDescriptor&,
                         const AssemblyHierarchyInsertConstraintTargetDescriptor&) = default;
};

using AssemblyHierarchyConstraintTargetDescriptor =
    std::variant<AssemblyHierarchyPlanarConstraintTargetDescriptor,
                 AssemblyHierarchyAxisConstraintTargetDescriptor,
                 AssemblyHierarchyInsertConstraintTargetDescriptor>;

using AssemblyHierarchyConstraintResidualDescriptor =
    std::variant<PlanarMateResidualDescriptor,
                 PlanarDistanceResidualDescriptor,
                 PlanarAngleResidualDescriptor,
                 ConcentricResidualDescriptor,
                 InsertResidualDescriptor>;

struct AssemblyHierarchyConstraintEquationDescriptor {
  AssemblyConstraintId relationship;
  AssemblyConstraintType type = AssemblyConstraintType::Mate;
  AssemblyHierarchyConstraintTargetDescriptor target_a;
  AssemblyHierarchyConstraintTargetDescriptor target_b;
  AssemblyHierarchyConstraintResidualDescriptor residual;

  friend bool operator==(const AssemblyHierarchyConstraintEquationDescriptor&,
                         const AssemblyHierarchyConstraintEquationDescriptor&) = default;
};

// Resolves occurrence-qualified local semantic targets to exact root-assembly
// space. Visibility and suppression do not alter geometric resolution; later
// graph/solver integration owns relationship participation filtering.
class AssemblyHierarchyConstraintTargetResolver {
public:
  [[nodiscard]] Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>
  resolve_planar(const Project& project,
                 const AssemblyHierarchyConstraintTarget& target) const;

  [[nodiscard]] Result<AssemblyHierarchyAxisConstraintTargetDescriptor>
  resolve_axis(const Project& project,
               const AssemblyHierarchyConstraintTarget& target) const;

  [[nodiscard]] Result<AssemblyHierarchyInsertConstraintTargetDescriptor>
  resolve_insert(const Project& project,
                 const AssemblyHierarchyConstraintTarget& target) const;
};

// Builds read-only Mate/Distance/Angle/Concentric/Insert residual semantics for
// endpoints that may live in different AssemblyDocument occurrences. The query
// is not solved, persisted, or inserted into a local constraint graph.
class AssemblyHierarchyConstraintEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyHierarchyConstraintEquationDescriptor>
  build(const Project& project, const AssemblyHierarchyConstraintQuery& query) const;
};

} // namespace blcad::geometry
