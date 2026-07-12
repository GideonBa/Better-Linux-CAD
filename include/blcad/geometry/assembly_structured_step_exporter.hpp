#pragma once

#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>

namespace blcad::geometry {

struct AssemblyStructuredStepExportSummary {
  std::size_t recomputed_part_count = 0U;
  std::size_t exported_assembly_occurrence_count = 0U;
  std::size_t exported_component_occurrence_count = 0U;
  std::size_t part_product_definition_count = 0U;
  std::size_t written_bytes = 0U;

  friend bool operator==(const AssemblyStructuredStepExportSummary&,
                         const AssemblyStructuredStepExportSummary&) = default;
};

// Writes one deterministic XDE/STEP assembly product graph derived from
// AssemblyExchangeGraph. The source Project is read-only; graph identities,
// OCCT labels, part definitions, and STEP entities remain derived products.
class AssemblyStructuredStepExporter {
public:
  [[nodiscard]] Result<AssemblyStructuredStepExportSummary>
  write_step(const Project& project, const std::string& path) const;
};

} // namespace blcad::geometry
