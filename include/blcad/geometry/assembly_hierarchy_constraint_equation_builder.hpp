#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_concentric_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"
#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::geometry {

// Read-only geometry-query endpoint retained by the target-resolution layer.
// Persistent project intent uses the Core-owned
// AssemblyHierarchyConstraintEndpoint; query construction accepts that Core
// value directly and converts it into this derived geometry-query seed.
class AssemblyHierarchyConstraintTarget {
public:
  [[nodiscard]] static Result<AssemblyHierarchyConstraintTarget>
  create(std::vector<SubassemblyInstanceId> occurrence_path, ComponentInstanceId component_instance,
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
  create(AssemblyConstraintId id, AssemblyConstraintType type,
         AssemblyHierarchyConstraintTarget target_a, AssemblyHierarchyConstraintTarget target_b,
         std::optional<Quantity> distance = std::nullopt,
         std::optional<Quantity> angle = std::nullopt);

  [[nodiscard]] static Result<AssemblyHierarchyConstraintQuery>
  create(AssemblyConstraintId id, AssemblyConstraintType type,
         AssemblyHierarchyConstraintEndpoint target_a, AssemblyHierarchyConstraintEndpoint target_b,
         std::optional<Quantity> distance = std::nullopt,
         std::optional<Quantity> angle = std::nullopt) {
    auto query_target_a = AssemblyHierarchyConstraintTarget::create(
        target_a.occurrence_path(), target_a.component_instance(), target_a.semantic_reference());
    if (query_target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintQuery>::failure(query_target_a.error());
    }
    auto query_target_b = AssemblyHierarchyConstraintTarget::create(
        target_b.occurrence_path(), target_b.component_instance(), target_b.semantic_reference());
    if (query_target_b.has_error()) {
      return Result<AssemblyHierarchyConstraintQuery>::failure(query_target_b.error());
    }
    return create(std::move(id), type, std::move(query_target_a.value()),
                  std::move(query_target_b.value()), std::move(distance), std::move(angle));
  }

  [[nodiscard]] static Result<AssemblyHierarchyConstraintQuery>
  create(const AssemblyHierarchyConstraint& constraint) {
    return create(constraint.id(), constraint.type(), constraint.target_a(), constraint.target_b(),
                  constraint.distance(), constraint.angle());
  }

  [[nodiscard]] const AssemblyConstraintId& id() const noexcept;
  [[nodiscard]] AssemblyConstraintType type() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintTarget& target_a() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintTarget& target_b() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& distance() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& angle() const noexcept;

  friend bool operator==(const AssemblyHierarchyConstraintQuery&,
                         const AssemblyHierarchyConstraintQuery&) = default;

private:
  AssemblyHierarchyConstraintQuery(AssemblyConstraintId id, AssemblyConstraintType type,
                                   AssemblyHierarchyConstraintTarget target_a,
                                   AssemblyHierarchyConstraintTarget target_b,
                                   std::optional<Quantity> distance, std::optional<Quantity> angle);

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

struct AssemblyHierarchyGenericConstraintTargetDescriptor {
  std::vector<SubassemblyInstanceId> occurrence_path;
  DocumentId assembly_document;
  ComponentInstanceId component_instance;
  AssemblyGeometricTargetSourceMetadata source_metadata;
  std::string semantic_reference;
  AssemblyGeometricTargetCapability selected_capability = AssemblyGeometricTargetCapability::Point;
  AssemblyGeometricTargetDescriptor descriptor;

  friend bool operator==(const AssemblyHierarchyGenericConstraintTargetDescriptor&,
                         const AssemblyHierarchyGenericConstraintTargetDescriptor&) = default;
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
                 AssemblyHierarchyGenericConstraintTargetDescriptor,
                 AssemblyHierarchyInsertConstraintTargetDescriptor>;

using AssemblyHierarchyConstraintResidualDescriptor =
    std::variant<PlanarMateResidualDescriptor, PlanarDistanceResidualDescriptor,
                 PlanarAngleResidualDescriptor, ConcentricResidualDescriptor,
                 InsertResidualDescriptor, AssemblyGenericRelationshipResidualDescriptor>;

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
  [[nodiscard]] Result<AssemblyResolvedGeometricTarget>
  resolve_geometric(const Project& project, const AssemblyHierarchyConstraintTarget& target) const;

  [[nodiscard]] Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>
  resolve_planar(const Project& project, const AssemblyHierarchyConstraintTarget& target) const;

  [[nodiscard]] Result<AssemblyHierarchyAxisConstraintTargetDescriptor>
  resolve_axis(const Project& project, const AssemblyHierarchyConstraintTarget& target) const;

  [[nodiscard]] Result<AssemblyHierarchyInsertConstraintTargetDescriptor>
  resolve_insert(const Project& project, const AssemblyHierarchyConstraintTarget& target) const;
};

// Builds read-only Mate/Distance/Angle/Concentric/Insert and generic
// Coincident/Parallel/Perpendicular residual semantics for endpoints that may
// live in different AssemblyDocument occurrences. The query is not solved,
// persisted, or inserted into a local constraint graph.
class AssemblyHierarchyConstraintEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyHierarchyConstraintEquationDescriptor>
  build(const Project& project, const AssemblyHierarchyConstraintQuery& query) const;
};

} // namespace blcad::geometry
