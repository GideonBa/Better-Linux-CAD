#pragma once

#include "blcad/core/assembly_exchange_graph.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace blcad::geometry {

enum class AssemblyContactClassification {
  Separated,
  Touching,
  Interfering,
};

struct AssemblyComponentOccurrencePairIdentity {
  AssemblyExchangeComponentOccurrenceIdentity occurrence_a;
  AssemblyExchangeComponentOccurrenceIdentity occurrence_b;

  friend bool operator==(const AssemblyComponentOccurrencePairIdentity&,
                         const AssemblyComponentOccurrencePairIdentity&) = default;
};

struct AssemblyContactRecord {
  AssemblyComponentOccurrencePairIdentity pair;
  AssemblyContactClassification classification = AssemblyContactClassification::Separated;
  double overlap_volume_mm3 = 0.0;
  std::optional<double> minimum_distance_mm;

  friend bool operator==(const AssemblyContactRecord&, const AssemblyContactRecord&) = default;
};

struct AssemblyContactAnalysisOptions {
  // Non-interfering pairs at or below this minimum distance are Touching.
  // Must be finite and non-negative.
  double touching_tolerance_mm = 1.0e-6;
  // Common solid volume strictly above this value is Interfering.
  // Must be finite and positive.
  double minimum_overlap_volume_mm3 = 1.0e-6;
};

struct AssemblyContactAnalysis {
  std::size_t component_occurrence_count = 0U;
  std::size_t evaluated_pair_count = 0U;
  std::size_t recomputed_part_count = 0U;
  std::vector<AssemblyContactRecord> records;

  friend bool operator==(const AssemblyContactAnalysis&, const AssemblyContactAnalysis&) = default;
};

// Read-only static posed-contact classification over every unordered pair of
// canonical visible-active component occurrences. Pair identity/order uses the
// exact rooted AssemblyExchangeComponentOccurrenceIdentity, never a joined text
// key or OCCT topology id. All records are derived and unpersisted.
class AssemblyContactAnalyzer {
public:
  [[nodiscard]] Result<AssemblyContactAnalysis>
  analyze(const Project& project,
          AssemblyContactAnalysisOptions options = AssemblyContactAnalysisOptions{}) const;
};

} // namespace blcad::geometry
