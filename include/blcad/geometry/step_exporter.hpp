#pragma once

#include "blcad/core/body.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace blcad::geometry {

struct PartStepExportBodySummary {
  BodyId body_id;
  std::string exchange_name;
  BodyKind kind = BodyKind::Solid;
};

struct PartStepExportSummary {
  std::size_t exported_body_count = 0U;
  std::size_t exported_solid_body_count = 0U;
  std::size_t exported_surface_body_count = 0U;
  std::size_t written_bytes = 0U;
  std::vector<PartStepExportBodySummary> bodies;
};

// Writes one computed shape, or a Part's visible Body results, as a STEP file.
// The exporter encapsulates the OCCT STEP/XDE writers and stays on the Geometry
// side of the Core/OCCT boundary.
class StepExporter {
public:
  // Exports `shape` to `path` and returns the written file size in bytes.
  // Expected failures are reported as `ErrorCategory::Export`.
  [[nodiscard]] Result<std::size_t> write_step(const GeometryShape& shape,
                                               const std::string& path) const;

  // Exports every visible Body using its already-computed ShapeCache result.
  // BodyId defines both deterministic ordering and the collision-free XDE name;
  // authored display names and transient STEP/XDE ids are deliberately ignored.
  [[nodiscard]] Result<PartStepExportSummary>
  write_part_step(const PartDocument& document, const ShapeCache& shape_cache,
                  const std::string& path) const;
};

} // namespace blcad::geometry
