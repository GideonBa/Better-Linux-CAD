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

// Schreibt einen berechneten Einzel-Shape oder die sichtbaren Body-Ergebnisse
// eines Parts als STEP-Datei. Der Exporter kapselt die OCCT-STEP/XDE-Writer und
// bleibt auf der Geometry-Seite der Core/OCCT-Grenze.
class StepExporter {
public:
  // Exportiert `shape` nach `path` und liefert die Groesse der geschriebenen
  // Datei in Bytes. Erwartbare Fehler werden als `ErrorCategory::Export`
  // gemeldet.
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
