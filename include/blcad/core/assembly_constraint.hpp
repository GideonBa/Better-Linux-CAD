#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace blcad {

enum class AssemblyConstraintType { Mate, Concentric, Distance };
[[nodiscard]] std::string_view to_string(AssemblyConstraintType type) noexcept;

enum class AssemblyConstraintState { Active, Inactive };
[[nodiscard]] std::string_view to_string(AssemblyConstraintState state) noexcept;

// Persistent semantic assembly target. The semantic reference is model intent
// such as feature.base_extrude.top or Bolt.main_axis, never a raw OCCT topology id.
class AssemblyConstraintTarget {
public:
  [[nodiscard]] static Result<AssemblyConstraintTarget>
  create(ComponentInstanceId component_instance, std::string semantic_reference);

  [[nodiscard]] const ComponentInstanceId& component_instance() const noexcept;
  [[nodiscard]] const std::string& semantic_reference() const noexcept;

  friend bool operator==(const AssemblyConstraintTarget&, const AssemblyConstraintTarget&) = default;

private:
  AssemblyConstraintTarget(ComponentInstanceId component_instance, std::string semantic_reference);

  ComponentInstanceId component_instance_;
  std::string semantic_reference_;
};

// Solver-independent assembly relationship intent. Distance constraints carry
// one length quantity; Mate and Concentric intentionally carry no distance.
class AssemblyConstraint {
public:
  [[nodiscard]] static Result<AssemblyConstraint>
  create(AssemblyConstraintId id,
         std::string name,
         AssemblyConstraintType type,
         AssemblyConstraintTarget target_a,
         AssemblyConstraintTarget target_b,
         AssemblyConstraintState state = AssemblyConstraintState::Active,
         std::optional<Quantity> distance = std::nullopt);

  [[nodiscard]] const AssemblyConstraintId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] AssemblyConstraintType type() const noexcept;
  [[nodiscard]] const AssemblyConstraintTarget& target_a() const noexcept;
  [[nodiscard]] const AssemblyConstraintTarget& target_b() const noexcept;
  [[nodiscard]] AssemblyConstraintState state() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& distance() const noexcept;

private:
  AssemblyConstraint(AssemblyConstraintId id,
                     std::string name,
                     AssemblyConstraintType type,
                     AssemblyConstraintTarget target_a,
                     AssemblyConstraintTarget target_b,
                     AssemblyConstraintState state,
                     std::optional<Quantity> distance);

  AssemblyConstraintId id_;
  std::string name_;
  AssemblyConstraintType type_;
  AssemblyConstraintTarget target_a_;
  AssemblyConstraintTarget target_b_;
  AssemblyConstraintState state_;
  std::optional<Quantity> distance_;
};

} // namespace blcad
