#pragma once

#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace blcad::geometry {

// Stable posed-leaf identity for interference relationships: assembly id,
// subassembly path, and component id. Never an OCCT topology id.
struct AssemblyLeafIdentity {
  DocumentId assembly_document;
  std::vector<SubassemblyInstanceId> subassembly_path;
  ComponentInstanceId component_instance;
  std::string occurrence_key;

  friend bool operator==(const AssemblyLeafIdentity&, const AssemblyLeafIdentity&) = default;
};

// One derived positive-volume interference between two distinct posed leaves.
// Identities are ordered lexicographically by occurrence key (a < b).
struct AssemblyLeafInterferenceRecord {
  AssemblyLeafIdentity leaf_a;
  AssemblyLeafIdentity leaf_b;
  double overlap_volume_mm3 = 0.0;

  friend bool operator==(const AssemblyLeafInterferenceRecord&,
                         const AssemblyLeafInterferenceRecord&) = default;
};

struct AssemblyInterferenceAnalysisOptions {
  // Overlap volumes at or below this tolerance count as contact, not
  // interference. Must be finite and positive.
  double minimum_overlap_volume_mm3 = 1.0e-6;
};

struct AssemblyInterferenceAnalysis {
  std::size_t leaf_count = 0U;
  std::size_t evaluated_pair_count = 0U;
  std::size_t recomputed_part_count = 0U;
  std::vector<AssemblyLeafInterferenceRecord> interferences;

  friend bool operator==(const AssemblyInterferenceAnalysis&,
                         const AssemblyInterferenceAnalysis&) = default;
};

// Read-only posed assembly interference analysis over the flattened
// visible-active leaf boundary. Every unordered leaf pair is evaluated exactly
// once in deterministic lexicographic occurrence-key order through OCCT
// boolean common geometry. Only finite positive common solid volume above the
// tolerance is interference; face/edge/point contact is not. All posed shapes
// and records are derived and unpersisted.
class AssemblyInterferenceAnalyzer {
public:
  [[nodiscard]] Result<AssemblyInterferenceAnalysis> analyze(
      const Project& project,
      AssemblyInterferenceAnalysisOptions options = AssemblyInterferenceAnalysisOptions{}) const;
};

// One derived clearance violation: two non-interfering posed leaves closer
// than the requested clearance threshold. Touching without positive common
// volume reports a minimum distance of zero here, never as interference.
struct AssemblyLeafClearanceRecord {
  AssemblyLeafIdentity leaf_a;
  AssemblyLeafIdentity leaf_b;
  double minimum_distance_mm = 0.0;

  friend bool operator==(const AssemblyLeafClearanceRecord&,
                         const AssemblyLeafClearanceRecord&) = default;
};

struct AssemblyClearanceAnalysisOptions {
  // Non-interfering pairs closer than this threshold are clearance
  // violations. Must be finite and positive.
  double clearance_threshold_mm = 1.0;
  // Same interference boundary as AssemblyInterferenceAnalysisOptions.
  double minimum_overlap_volume_mm3 = 1.0e-6;
};

struct AssemblyClearanceAnalysis {
  std::size_t leaf_count = 0U;
  std::size_t evaluated_pair_count = 0U;
  std::size_t recomputed_part_count = 0U;
  std::vector<AssemblyLeafClearanceRecord> clearance_violations;
  std::vector<AssemblyLeafInterferenceRecord> interferences;

  friend bool operator==(const AssemblyClearanceAnalysis&,
                         const AssemblyClearanceAnalysis&) = default;
};

// Read-only posed assembly clearance analysis over the same shared posed-leaf
// boundary, pair order, and identity contract as interference analysis.
// Interfering pairs are reported separately and never as clearance records;
// remaining pairs use the OCCT minimum distance between the posed shapes.
class AssemblyClearanceAnalyzer {
public:
  [[nodiscard]] Result<AssemblyClearanceAnalysis>
  analyze(const Project& project,
          AssemblyClearanceAnalysisOptions options = AssemblyClearanceAnalysisOptions{}) const;
};

} // namespace blcad::geometry
