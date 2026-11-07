#pragma once

#include <string>

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

struct PartFeatureHistoryEntry {
  // FeatureId and BodyTransformId already share the dependency-node namespace.
  std::string node_id;
  PartFeatureFamily family;

  bool operator==(const PartFeatureHistoryEntry&) const = default;
};

} // namespace blcad
