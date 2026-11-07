#pragma once

#include "blcad/core/id.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace blcad {

// A projection of the existing typed executable records, not another feature tree.
// Body aliases and non-executable resources are deliberately excluded.
enum class PartFeatureFamily {
  ImportedBody,
  Extrude,
  Revolve,
  Sweep,
  Loft,
  Surface,
  LinearPattern,
  CircularPattern,
  Mirror,
  Fillet,
  Chamfer,
  Shell,
  Draft,
  BodyBoolean,
  BodyTransform,
};

enum class PartFeatureState { Active, Suppressed };

struct PartFeatureHistoryIntent {
  std::vector<std::string> order;
  // Sparse authored intent only. Effective blocked/bypassed states are derived.
  std::vector<std::string> suppressed;
  bool operator==(const PartFeatureHistoryIntent&) const = default;
};

enum class PartHistoryDiagnosticCode {
  ReferencePartReadOnly,
  UnknownExecutable,
  DuplicateExecutable,
  MissingExecutable,
  UnknownSuppressedExecutable,
  DuplicateSuppression,
  ImportedBaseRequired,
  DependencyAfterConsumer,
  RebuildRejected,
};

struct PartHistoryDiagnostic {
  PartHistoryDiagnosticCode code;
  std::string producer_id;
  std::string consumer_id;
  std::string message;
};

struct PartFeatureHistoryEntry {
  // FeatureId and BodyTransformId already share the dependency-node namespace.
  std::string node_id;
  PartFeatureFamily family;
  PartFeatureState state = PartFeatureState::Active;

  bool operator==(const PartFeatureHistoryEntry&) const = default;
};

enum class PartHistoryExecutionState { Active, Suppressed, Blocked, OutsidePrefix };
enum class PartHistoryInputIssueCode {
  SuppressedProducer,
  ProducerOutsidePrefix,
  BlockedProducer,
  MissingResource,
  MissingBodyCreator,
  ConsumedBody,
};
struct PartHistoryInputIssue {
  PartHistoryInputIssueCode code;
  std::string producer_id;
  std::string input_node_id;
  std::string consumer_id;
  std::string message;
};
struct PartHistoryBodyAvailability {
  BodyId body;
  bool available = false;
  std::optional<std::string> producer_id;
  std::optional<std::string> unavailable_due_to;
};
struct PartHistoryExecutionEntry {
  PartFeatureHistoryEntry feature;
  PartHistoryExecutionState state = PartHistoryExecutionState::Active;
  std::vector<PartHistoryInputIssue> issues;
  // Availability immediately before this row, not the terminal Body producer.
  std::vector<PartHistoryBodyAvailability> body_inputs;
};
struct PartHistoryEvaluationPlan {
  // A prefix is explicitly partial, even when it includes the final row.
  std::optional<std::size_t> prefix_count;
  std::vector<PartHistoryExecutionEntry> entries;
  std::vector<PartHistoryBodyAvailability> bodies;
  [[nodiscard]] bool has_blocked_entries() const noexcept;
  // Complete Core scope with available inputs; this is not Geometry freshness.
  [[nodiscard]] bool complete() const noexcept;
  [[nodiscard]] std::vector<std::string> active_ids() const;
};

} // namespace blcad
