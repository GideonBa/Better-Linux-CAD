#pragma once

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

} // namespace blcad
