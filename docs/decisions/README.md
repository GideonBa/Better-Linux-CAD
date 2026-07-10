# Architecture Decision Records

Dieser Ordner enthaelt Architekturentscheidungen fuer BLCAD.

## Entscheidungen

- `0001-technology-baseline.md`: technische Startbasis
- `0002-object-identity-and-naming.md`: Objekt-IDs und Namen
- `0003-units-and-quantities.md`: Einheiten und Laengenwerte
- `0004-error-handling.md`: Fehlerbehandlung im Core
- `0005-geometry-adapter-boundary.md`: Grenze zwischen Core und OCCT
- `0006-mvp1-serialization-timing.md`: Zeitpunkt und Umfang der Serialisierung
- `0007-dependency-graph-scope.md`: Umfang des MVP-1-Dependency-Graph
- `0008-part-document-dependency-graph-integration.md`: Dependency-Graph im
  PartDocument
- `0009-invalidation-state-scope.md`: Umfang des MVP-1-Invalidierungsstatus
- `0010-recompute-plan-scope.md`: Umfang des MVP-1-Recompute-Plans
- `0011-geometry-adapter-rectangle-extrusion.md`: erster Geometry-Adapter fuer
  die Rechteckextrusion
- `0012-shape-cache-scope.md`: Umfang des ersten ShapeCache
- `0013-additive-extrude-recompute-execution.md`: erste Recompute-Ausfuehrung
  fuer AdditiveExtrude
- `0014-subtractive-extrude-recompute-execution.md`: Recompute-Ausfuehrung fuer
  SubtractiveExtrude
- `0015-step-export.md`: STEP-Export fuer den finalen Shape
- `0016-document-recompute-and-shape-cache-boundary.md`: Dokument-Recompute und
  ShapeCache-Grenze
- `0017-parameter-value-update.md`: Parameterwert-Update im PartDocument
- `0018-count-parameters-and-bolt-circle-pattern.md`: Count-Parameter und
  parametrischer Lochkreis
- `0019-assembly-parameter-binding.md`: Assembly-Parameter-Binding fuer
  bauteiluebergreifende Parametrik

## Statuswerte

- `akzeptiert`: gilt fuer die naechste Implementierung
- `ersetzt`: durch eine spaetere ADR abgeloest
- `verworfen`: dokumentierte Alternative, aber nicht gewaehlt

Neue Entscheidungen sollen klein bleiben und eine konkrete Frage beantworten.
