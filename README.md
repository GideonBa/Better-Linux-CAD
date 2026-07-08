# BLCAD

BLCAD ist als eigenes parametrisches CAD-System fuer Linux geplant. Die
Zielarchitektur steht in
`zielarchitektur-parametrisches-cad-system.tex`.

Der aktuelle Stand ist ein bewusst kleiner MVP-1-Core-Skeleton. Er enthaelt
Basistypen fuer Parameter, Laengenwerte, Fehlerbehandlung, ein reines
`PartDocument`-Datenmodell, integrierte Datum-Plane- und Sketch-Datenmodelle,
erste Feature-Datenmodelle, einen in `PartDocument` integrierten Dependency
Graph, einen Invalidierungsstatus sowie einen Recompute-Plan als Datenmodell.
Zusaetzlich gibt es ein optionales `blcad_geometry`-Target mit OCCT-Adaptern
fuer die Rechteckextrusion und den zentrischen Bohrungs-Cut, einem kleinen
`ShapeCache`, einer Recompute-Ausfuehrung fuer `AdditiveExtrude` und
`SubtractiveExtrude`, einem vollstaendigen Dokument-Recompute sowie einem
STEP-Export fuer den finalen Shape. Parameterwerte lassen sich im `PartDocument`
aendern, sodass eine Aenderung die abhaengige Geometrie inkrementell neu
berechnet. Damit laeuft das MVP-1-Referenzbauteil als Ende-zu-Ende-Ablauf. Es
gibt noch keine GUI.

## Technische Basis

- Sprache: C++20
- Buildsystem: CMake mit Ninja
- Geometriekernel: OCCT / Open CASCADE Technology
- GUI-Basis fuer spaeter: Qt 6
- Mathematische Hilfsbibliothek: Eigen
- Dateiformat-Prototyping: nlohmann-json
- Logging und Formatierung: spdlog, fmt
- Tests: Catch2

## Projektstruktur

- `docs/` enthaelt Architektur-, MVP- und Setup-Dokumente.
- `include/blcad/` enthaelt oeffentliche Header des Core-Skeletons.
- `src/` enthaelt die erste Core-Implementierung.
- `tests/` enthaelt Catch2-Tests fuer den Core.
- `cmake/` ist fuer spaetere CMake-Hilfsmodule vorgesehen.
- `assets/` ist fuer spaetere UI- und Beispielressourcen vorgesehen.
- `examples/` ist fuer spaetere Beispielmodelle vorgesehen.

## Dokumentation

- `docs/development-setup.md`: lokales Setup und Standardbefehle
- `docs/architecture-summary.md`: Architekturuebersicht
- `docs/core-mvp1-skeleton.md`: aktueller Core-Skeleton
- `docs/sketch-mvp1-data-model.md`: Datum-Plane- und Sketch-Datenmodell
- `docs/feature-mvp1-data-model.md`: Feature-Datenmodell
- `docs/dependency-graph-mvp1-data-model.md`: Dependency-Graph-Datenmodell
- `docs/invalidation-mvp1-data-model.md`: Invalidierungsstatus-Datenmodell
- `docs/recompute-plan-mvp1-data-model.md`: Recompute-Plan-Datenmodell
- `docs/geometry-adapter-mvp1-rectangle-extrusion.md`: erster OCCT-Adapter
- `docs/geometry-adapter-mvp1-circular-cut.md`: OCCT-Adapter fuer den
  zentrischen Cut
- `docs/shape-cache-mvp1-data-model.md`: ShapeCache-Datenmodell
- `docs/recompute-execution-mvp1-additive-extrude.md`: erste
  Additive-Recompute-Ausfuehrung
- `docs/recompute-execution-mvp1-subtractive-cut.md`: Subtractive-Recompute-
  Ausfuehrung
- `docs/step-export-mvp1.md`: STEP-Export fuer den finalen Shape
- `docs/document-recompute-mvp1.md`: vollstaendiger Dokument-Recompute und
  Referenzbauteil-Pipeline
- `docs/parameter-update-mvp1.md`: Parameterwert-Update und numerischer
  inkrementeller Recompute
- `docs/mvp-plan.md`: MVP-Reihenfolge
- `docs/mvp-1-specification.md`: genaue MVP-1-Spezifikation
- `docs/decisions/`: Architecture Decision Records

## Naechster technischer Schritt

Der aktuelle Core-Skeleton deckt `Quantity`, typisierte IDs, `Error`, `Result`,
`Parameter`, `PartDocument`, `DatumPlane`, `Sketch`, `RectangleProfile` und
`CircleProfile` sowie `Feature`, `AdditiveExtrude`, `SubtractiveExtrude` und
`DependencyGraph`, `InvalidationState` und `RecomputePlan` ab. `PartDocument`
validiert inzwischen Workplane-, Profil- und Feature-Referenzen, erzeugt daraus
Graphknoten und Graphkanten, markiert betroffene Knoten nach einer
Parameteraenderung und kann daraus einen geordneten Recompute-Plan ableiten.
Der optionale Geometry-Build erzeugt aus drei Laengenwerten ein OCCT-Solid fuer
eine zentrierte Rechteckextrusion und kann berechnete Feature-Shapes in einem
`ShapeCache` ablegen. Der `GeometryRecomputeExecutor` kann einen
`AdditiveExtrude`-Knoten aus einem `RecomputePlan` ausfuehren, sofern dessen
Sketch genau ein Rechteckprofil enthaelt. Zusaetzlich schneidet der
`CircularCutAdapter` aus einem gecachten Basiskoerper eine zentrische,
durchgehende Bohrung, und der Executor fuehrt daraus einen
`SubtractiveExtrude`-Knoten aus, sofern dessen Sketch genau ein Kreisprofil
enthaelt und der Zielkoerper bereits im `ShapeCache` liegt. `execute_document`
rechnet ein ganzes `PartDocument` in topologischer Reihenfolge in den
`ShapeCache`, und der `StepExporter` schreibt den finalen Shape als STEP-Datei.
Ein Ende-zu-Ende-Test erzeugt daraus das MVP-1-Referenzbauteil (Platte 120 x 80
x 8 mm mit zentraler Bohrung von 20 mm) und exportiert es als gueltige
STEP-Datei. Der `ShapeCache` bleibt dabei in der Geometry-Schicht; `PartDocument`
bleibt OCCT-frei, und `mark_all_clean()` setzt der Aufrufer nach dem Recompute.
`PartDocument::set_parameter_value` aendert einen Parameterwert, validiert ihn
und markiert die Abhaengigen als geaendert. Dadurch rechnet ein inkrementeller
Plan nach einer echten Durchmesseraenderung nur den Cut neu, und ein groesseres
Loch verkleinert das finale Volumen.

Der naechste technische Schritt sollte klein bleiben:

1. Eine JSON-Serialisierung der Modellabsicht fuer `PartDocument` vorbereiten.
2. Das Dokument aus JSON wieder aufbauen und erneut rechnen.
3. Den `ShapeCache` weiter nur als berechnetes Ergebnis behandeln.
4. Weiter keinen allgemeinen Solver bauen.
5. Weiter keine GUI bauen.

Die genaue MVP-1-Spezifikation steht in `docs/mvp-1-specification.md`.
