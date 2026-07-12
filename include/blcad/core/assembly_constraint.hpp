#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class AssemblyConstraintType {
  Mate,
  Concentric,
  Distance,
  Insert,
  Angle,
  Coincident,
  Parallel,
  Perpendicular
};
[[nodiscard]] std::string_view to_string(AssemblyConstraintType type) noexcept;

enum class AssemblyConstraintState { Active, Inactive };
[[nodiscard]] std::string_view to_string(AssemblyConstraintState state) noexcept;

// Persistent semantic assembly target. The semantic reference is model intent
// such as feature.base_extrude.top, feature.hole.axis, or feature.hole.seat,
// never a raw OCCT topology id.
class AssemblyConstraintTarget {
public:
  [[nodiscard]] static Result<AssemblyConstraintTarget>
  create(ComponentInstanceId component_instance, std::string semantic_reference);

  [[nodiscard]] const ComponentInstanceId& component_instance() const noexcept;
  [[nodiscard]] const std::string& semantic_reference() const noexcept;

  friend bool operator==(const AssemblyConstraintTarget&,
                         const AssemblyConstraintTarget&) = default;

private:
  AssemblyConstraintTarget(ComponentInstanceId component_instance, std::string semantic_reference);

  ComponentInstanceId component_instance_;
  std::string semantic_reference_;
};

// Core-owned occurrence-qualified semantic endpoint intent. An empty occurrence
// path addresses the explicit root assembly occurrence; non-empty paths are
// exact root-to-current SubassemblyInstanceId sequences. This record stores
// identity only and never resolves hierarchy or geometry during construction.
class AssemblyHierarchyConstraintEndpoint {
public:
  [[nodiscard]] static Result<AssemblyHierarchyConstraintEndpoint>
  create(std::vector<SubassemblyInstanceId> occurrence_path, ComponentInstanceId component_instance,
         std::string semantic_reference);

  [[nodiscard]] const std::vector<SubassemblyInstanceId>& occurrence_path() const noexcept;
  [[nodiscard]] const ComponentInstanceId& component_instance() const noexcept;
  [[nodiscard]] const std::string& semantic_reference() const noexcept;

  friend bool operator==(const AssemblyHierarchyConstraintEndpoint&,
                         const AssemblyHierarchyConstraintEndpoint&) = default;

private:
  AssemblyHierarchyConstraintEndpoint(std::vector<SubassemblyInstanceId> occurrence_path,
                                      ComponentInstanceId component_instance,
                                      std::string semantic_reference);

  std::vector<SubassemblyInstanceId> occurrence_path_;
  ComponentInstanceId component_instance_;
  std::string semantic_reference_;
};

// Solver-independent local assembly relationship intent. Distance constraints
// carry one length quantity and Angle constraints carry one angle quantity;
// Mate, Concentric, Insert, Coincident, Parallel, and Perpendicular intentionally
// carry no scalar value. Generic Block-38 families remain persistent-only until
// Block 39 adds equation and graph participation semantics.
class AssemblyConstraint {
public:
  [[nodiscard]] static Result<AssemblyConstraint>
  create(AssemblyConstraintId id, std::string name, AssemblyConstraintType type,
         AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
         AssemblyConstraintState state = AssemblyConstraintState::Active,
         std::optional<Quantity> distance = std::nullopt,
         std::optional<Quantity> angle = std::nullopt);

  [[nodiscard]] const AssemblyConstraintId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] AssemblyConstraintType type() const noexcept;
  [[nodiscard]] const AssemblyConstraintTarget& target_a() const noexcept;
  [[nodiscard]] const AssemblyConstraintTarget& target_b() const noexcept;
  [[nodiscard]] AssemblyConstraintState state() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& distance() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& angle() const noexcept;

private:
  AssemblyConstraint(AssemblyConstraintId id, std::string name, AssemblyConstraintType type,
                     AssemblyConstraintTarget target_a, AssemblyConstraintTarget target_b,
                     AssemblyConstraintState state, std::optional<Quantity> distance,
                     std::optional<Quantity> angle);

  AssemblyConstraintId id_;
  std::string name_;
  AssemblyConstraintType type_;
  AssemblyConstraintTarget target_a_;
  AssemblyConstraintTarget target_b_;
  AssemblyConstraintState state_;
  std::optional<Quantity> distance_;
  std::optional<Quantity> angle_;
};

// Persistent project-level geometric relationship intent between exact rooted
// assembly occurrences. Endpoint paths are stored verbatim; hierarchy/component
// structure and semantic geometry resolution are intentionally later layers.
class AssemblyHierarchyConstraint {
public:
  [[nodiscard]] static Result<AssemblyHierarchyConstraint>
  create(AssemblyConstraintId id, std::string name, AssemblyConstraintType type,
         AssemblyHierarchyConstraintEndpoint target_a, AssemblyHierarchyConstraintEndpoint target_b,
         AssemblyConstraintState state = AssemblyConstraintState::Active,
         std::optional<Quantity> distance = std::nullopt,
         std::optional<Quantity> angle = std::nullopt);

  [[nodiscard]] const AssemblyConstraintId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] AssemblyConstraintType type() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintEndpoint& target_a() const noexcept;
  [[nodiscard]] const AssemblyHierarchyConstraintEndpoint& target_b() const noexcept;
  [[nodiscard]] AssemblyConstraintState state() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& distance() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& angle() const noexcept;

private:
  AssemblyHierarchyConstraint(AssemblyConstraintId id, std::string name,
                              AssemblyConstraintType type,
                              AssemblyHierarchyConstraintEndpoint target_a,
                              AssemblyHierarchyConstraintEndpoint target_b,
                              AssemblyConstraintState state, std::optional<Quantity> distance,
                              std::optional<Quantity> angle);

  AssemblyConstraintId id_;
  std::string name_;
  AssemblyConstraintType type_;
  AssemblyHierarchyConstraintEndpoint target_a_;
  AssemblyHierarchyConstraintEndpoint target_b_;
  AssemblyConstraintState state_;
  std::optional<Quantity> distance_;
  std::optional<Quantity> angle_;
};

} // namespace blcad
