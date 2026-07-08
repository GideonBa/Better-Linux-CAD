# MVP 1 Core Skeleton

Status: implementierter Core-Skeleton fuer die ersten MVP-1-Datenmodelle

Dieser Stand enthaelt nur die kleinsten Core-Bausteine fuer MVP 1. Der Core
selbst hat weiterhin keine Geometrie, keine OCCT-Anbindung, keine GUI, keinen
Recompute und keinen STEP-Export. OCCT wird nur im optionalen
`blcad_geometry`-Target verwendet.

## Ziel dieses Schritts

Der Skeleton macht die ersten ADRs ausfuehrbar:

- typisierte IDs aus `0002-object-identity-and-naming.md`
- `Quantity` fuer Laengen in Millimetern aus `0003-units-and-quantities.md`
- `Error` und `Result` aus `0004-error-handling.md`
- `Parameter` als erster Baustein des Part-Modells aus der MVP-1-Spezifikation
- `PartDocument` als erster Container fuer MVP-1-Parameter, Datum-Planes,
  Sketches, Features, den Dependency Graph, den Invalidierungsstatus und den
  Recompute-Plan
- `DatumPlane` und `Sketch` als reine Datenmodelle fuer die erste Platte
- `RectangleProfile` und `CircleProfile` als erste Skizzenprofile
- `Feature`, `AdditiveExtrude` und `SubtractiveExtrude` als erste
  Feature-Datenmodelle
- `DependencyGraph` als reine Abhaengigkeitsstruktur ohne Recompute
- `InvalidationState` als reiner Status fuer geaenderte und betroffene Knoten
- `RecomputePlan` als reine geordnete Liste von `dirty`-Knoten
- `ShapeCache` als optionales Geometry-Datenmodell fuer berechnete Shapes

## CMake-Targets

### `blcad_core`

Statische Core-Library.

Enthaelt aktuell:

- `blcad/core/id.hpp`
- `blcad/core/spatial.hpp`
- `blcad/core/error.hpp`
- `blcad/core/result.hpp`
- `blcad/core/quantity.hpp`
- `blcad/core/parameter.hpp`
- `blcad/core/part_document.hpp`
- `blcad/core/datum_plane.hpp`
- `blcad/core/sketch.hpp`
- `blcad/core/feature.hpp`
- `blcad/core/dependency_graph.hpp`
- `blcad/core/invalidation_state.hpp`
- `blcad/core/recompute_plan.hpp`

Das Target linkt nicht gegen OCCT und nicht gegen Qt.

### `blcad_core_tests`

Catch2-Testprogramm fuer den Core-Skeleton.

Aktuelle Testbereiche:

- `Quantity`
- `Parameter`
- `PartDocument`
- `DatumPlane`
- `Sketch`
- `RectangleProfile`
- `CircleProfile`
- `Feature`
- `DependencyGraph`
- `InvalidationState`
- `RecomputePlan`
- `Error`
- `Result`

### `blcad_geometry`

Optionale statische Library fuer erste OCCT-Adapter.

Aktuell enthalten:

- `blcad/geometry/rectangle_extrusion_adapter.hpp`
- `blcad/geometry/recompute_executor.hpp`
- `blcad/geometry/shape_cache.hpp`

Das Target wird nur gebaut, wenn `BLCAD_BUILD_GEOMETRY=ON` gesetzt ist. Es
linkt gegen OCCT, aber nicht gegen Qt.

### `blcad_geometry_tests`

Catch2-Testprogramm fuer den optionalen Geometry-Adapter.

Aktuelle Testbereiche:

- `RectangleExtrusionAdapter`
- `GeometryShape`
- `GeometryRecomputeExecutor`
- `ShapeCache`

## Core-Typen

### Typisierte IDs

IDs sind intern typisiert, damit zum Beispiel eine `ParameterId` nicht
versehentlich als `FeatureId` verwendet wird.

Aktuell vorbereitet:

- `DocumentId`
- `ParameterId`
- `DatumPlaneId`
- `SketchId`
- `FeatureId`
- `ShapeCacheId`
- `ProfileId`

### `Quantity`

`Quantity` repraesentiert fuer MVP 1 nur positive Laengen in Millimetern.

Regeln:

- interne Einheit: `mm`
- Werttyp: `double`
- `0`, negative Werte und nicht-endliche Werte sind ungueltig
- Erstellung laeuft ueber `Result<Quantity>`

### `Error`

`Error` beschreibt erwartbare Fehler im Core.

Mindestfelder:

- Kategorie
- Objekt-ID
- Nachricht

Aktuelle Kategorien:

- `validation`
- `dependency`
- `geometry`
- `export`
- `internal`

### `Result<T>`

`Result<T>` kapselt entweder einen erfolgreichen Wert oder einen erwartbaren
Fehler. Es ist der normale Rueckgabetyp fuer Validierung und spaetere
Core-Operationen.

### `Parameter`

`Parameter` repraesentiert im ersten Schritt nur Laengenparameter im Part-Scope.

Pflicht:

- nicht-leere `ParameterId`
- nicht-leerer Name
- positive `Quantity`
- Typ `length`
- Scope `part`

### `PartDocument`

`PartDocument` ist der erste Dokumentcontainer fuer MVP 1. Es enthaelt aktuell:

- `DocumentId`
- Name
- Parameterliste
- Datum-Planes
- Sketches
- Features

Regeln:

- Dokument-ID darf nicht leer sein.
- Dokumentname darf nicht leer sein.
- Parameter-IDs muessen innerhalb des Dokuments eindeutig sein.
- Parameternamen muessen innerhalb des Dokuments eindeutig sein.
- Parameter koennen per ID und per Name gesucht werden.
- Die Einfuegereihenfolge der Parameter bleibt erhalten.
- Datum-Plane-IDs muessen innerhalb des Dokuments eindeutig sein.
- Sketch-IDs muessen innerhalb des Dokuments eindeutig sein.
- Sketch-Workplanes muessen auf vorhandene Datum-Planes zeigen.
- Profil-Parameterreferenzen muessen auf vorhandene Parameter zeigen.
- Feature-IDs muessen innerhalb des Dokuments eindeutig sein.
- Feature-Eingabe-Sketches muessen auf vorhandene Sketches zeigen.
- Additive-Extrude-Laengenparameter muessen auf vorhandene Parameter zeigen.
- Subtractive-Extrude-Zielfeatures muessen auf vorhandene Features zeigen.
- Parameter erzeugen Dependency-Graph-Knoten.
- Sketches erzeugen Dependency-Graph-Knoten.
- Profil-Parameterreferenzen erzeugen Graphkanten vom Parameter zum Sketch.
- Features erzeugen Dependency-Graph-Knoten.
- Feature-Referenzen erzeugen Graphkanten von Sketch, Parameter oder
  Zielfeature zum Feature.
- `PartDocument` synchronisiert seinen Invalidierungsstatus mit dem
  Dependency Graph.
- Parameteraenderungen koennen betroffene Knoten als `changed` und `dirty`
  markieren.
- `PartDocument` kann aus `dirty`-Knoten einen Recompute-Plan ableiten.

`PartDocument` enthaelt noch keinen Shape-Cache.

### `DatumPlane`

`DatumPlane` beschreibt eine Arbeitsebene. Aktuell gibt es nur die Standardebene
`XY`:

- Ursprung `(0, 0, 0)`
- X-Achse `(1, 0, 0)`
- Y-Achse `(0, 1, 0)`
- Normalenrichtung `(0, 0, 1)`

Die ID und der Name muessen nicht leer sein.

### `RectangleProfile`

`RectangleProfile` beschreibt ein zentriertes Rechteck in einer Skizze.

Aktuell gespeichert:

- `ProfileId`
- Mittelpunkt als `Point2`
- Parameterreferenz fuer Breite
- Parameterreferenz fuer Hoehe

Es werden nur Referenzen gespeichert. Die Parameterwerte werden noch nicht
aufgeloest.

### `CircleProfile`

`CircleProfile` beschreibt einen zentrierten Kreis in einer Skizze.

Aktuell gespeichert:

- `ProfileId`
- Mittelpunkt als `Point2`
- Parameterreferenz fuer Durchmesser

Es wird noch nicht geprueft, ob der Kreis in das Rechteck passt.

### `Sketch`

`Sketch` speichert:

- `SketchId`
- Name
- Workplane-Referenz auf eine `DatumPlaneId`
- Rechteckprofile
- Kreisprofile

Profil-IDs muessen innerhalb einer Skizze eindeutig sein, auch ueber
Profiltypen hinweg.

### `Feature`

`Feature` beschreibt aktuell nur Feature-Absicht, keine Geometrie.

Aktuelle Typen:

- `AdditiveExtrude`
- `SubtractiveExtrude`

`AdditiveExtrude` speichert:

- `FeatureId`
- Name
- Eingabe-Sketch
- Laengenparameter
- Richtung `+Z`

`SubtractiveExtrude` speichert:

- `FeatureId`
- Name
- Eingabe-Sketch
- Zielfeature
- Tiefe `through_all`
- Richtung `+Z`

### `DependencyGraph`

`DependencyGraph` beschreibt aktuell nur Abhaengigkeiten zwischen stabilen
String-Knoten-IDs.

Eine Kante:

```text
A -> B
```

bedeutet:

```text
B haengt von A ab.
```

Aktuell gespeichert:

- Knoten-IDs
- gerichtete Kanten

Aktuelle Abfragen:

- direkte Abhaengige
- transitive Abhaengige
- topologische Ordnung
- Zyklenerkennung

Der Graph fuehrt keine Invalidierung aus und startet keinen Recompute.

`PartDocument` besitzt einen `DependencyGraph` und leitet Knoten und Kanten aus
Parameter-, Sketch- und Feature-Referenzen ab.

### `InvalidationState`

`InvalidationState` beschreibt den aktuellen Invalidierungszustand von
Dependency-Graph-Knoten.

Aktuelle Statuswerte:

- `clean`
- `changed`
- `dirty`
- `error`

Eine Parameteraenderung markiert den Parameter als `changed` und alle transitiv
abhaengigen Knoten als `dirty`. Der Status startet keinen Recompute.

### `RecomputePlan`

`RecomputePlan` beschreibt, welche `dirty`-Knoten spaeter neu berechnet werden
muessten.

Regeln:

- nur `dirty`-Knoten werden aufgenommen
- `changed`-Knoten werden nicht aufgenommen
- die Reihenfolge folgt der topologischen Ordnung des Dependency Graph
- Graphzyklen werden als Dependency-Fehler gemeldet

Der Plan ist eine Aufgabenliste. Er fuehrt keine Features aus und erzeugt keine
Geometrie.

## Optionale Geometry-Schicht

`RectangleExtrusionAdapter` ist der erste konkrete OCCT-Pfad fuer MVP 1. Er
liegt ausserhalb des Core-Skeletons im Target `blcad_geometry`.

Aktuelle Operation:

```text
make_extruded_rectangle(width, height, thickness)
```

Eigenschaften:

- Eingaben sind validierte `Quantity`-Werte.
- Das Rechteck liegt zentriert auf der XY-Ebene.
- Die Extrusion laeuft fest in `+Z`.
- Das Ergebnis ist ein opaker `GeometryShape`.
- OCCT-Fehler werden als `ErrorCategory::Geometry` gemeldet.

Der Adapter liest kein `PartDocument` und fuehrt keinen `RecomputePlan` aus.

`ShapeCache` ist ein kleines Geometry-Datenmodell fuer berechnete Shapes.
Aktuell speichert es Feature-Shapes nach `FeatureId`, einen finalen Shape und
das Quellfeature dieses finalen Shapes.

`GeometryRecomputeExecutor` fuehrt derzeit nur `AdditiveExtrude` fuer einen
Sketch mit genau einem Rechteckprofil aus. Dabei werden die Parameter aus dem
`PartDocument` gelesen, der `RectangleExtrusionAdapter` aufgerufen und das
Ergebnis im `ShapeCache` abgelegt.

Der Cache ist noch nicht in `PartDocument` integriert. `SubtractiveExtrude`,
Boolean Cut und STEP-Export fehlen noch.

## Bewusste Begrenzung

Dieser Skeleton implementiert noch nicht:

- vollstaendigen Feature-Recompute
- `SubtractiveExtrude`
- Boolean Cut
- `ShapeCache` im `PartDocument`
- OCCT-Geometrie im Core
- STEP-Export
- JSON-Serialisierung
- GUI

Diese Reihenfolge ist Absicht. Der Core soll erst sichere Grundtypen und Tests
haben, bevor die Geometrie- und Recompute-Schicht hinzukommt.

## Standardbefehle

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Die Tests muessen erfolgreich sein, bevor neue Core-Bausteine hinzukommen.

Optionaler Geometry-Build:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Weitere Details zum Sketch-Datenmodell stehen in
`docs/sketch-mvp1-data-model.md`.

Weitere Details zum Feature-Datenmodell stehen in
`docs/feature-mvp1-data-model.md`.

Weitere Details zum Dependency-Graph-Datenmodell stehen in
`docs/dependency-graph-mvp1-data-model.md`.

Weitere Details zum Invalidierungsstatus stehen in
`docs/invalidation-mvp1-data-model.md`.

Weitere Details zum Recompute-Plan stehen in
`docs/recompute-plan-mvp1-data-model.md`.

Weitere Details zum Geometry-Adapter stehen in
`docs/geometry-adapter-mvp1-rectangle-extrusion.md`.

Weitere Details zum ShapeCache stehen in
`docs/shape-cache-mvp1-data-model.md`.

Weitere Details zur ersten Recompute-Ausfuehrung stehen in
`docs/recompute-execution-mvp1-additive-extrude.md`.
