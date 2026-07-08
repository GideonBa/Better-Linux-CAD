#pragma once

#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <cstddef>
#include <string>

namespace blcad::geometry {

// Schreibt ein berechnetes `GeometryShape` als STEP-Datei. Fuer MVP 1 wird nur
// der finale Shape aus dem `ShapeCache` exportiert. Der Exporter kapselt den
// OCCT-STEP-Writer und bleibt auf der Geometry-Seite der Core/OCCT-Grenze.
class StepExporter {
public:
  // Exportiert `shape` nach `path` und liefert die Groesse der geschriebenen
  // Datei in Bytes. Erwartbare Fehler werden als `ErrorCategory::Export`
  // gemeldet.
  [[nodiscard]] Result<std::size_t> write_step(const GeometryShape& shape,
                                               const std::string& path) const;
};

} // namespace blcad::geometry
