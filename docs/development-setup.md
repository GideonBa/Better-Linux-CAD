# Development Setup

Dieses Dokument beschreibt den lokalen Entwicklungsstart fuer BLCAD.

## System

Aktueller Zielstand:

- Ubuntu 24.04
- C++20
- CMake 3.28 oder neuer
- Ninja
- OCCT 7.6 aus den Ubuntu-Paketen
- Qt 6 aus den Ubuntu-Paketen
- Catch2 fuer spaetere Tests

## Abhaengigkeiten installieren

Die Paketliste steht ausfuehrlicher in `docs/dependencies-ubuntu-24.04.md`.

Standardinstallation aus den Ubuntu-Repositories:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git clang-format clang-tidy \
  libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev libocct-visualization-dev libocct-ocaf-dev libocct-draw-dev \
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2 \
  doxygen graphviz
```

Wenn die lokalen `.deb`-Pakete bereits im Projektordner liegen:

```bash
sudo apt-get install ./downloads/apt/*.deb
```

## Installation pruefen

```bash
cmake --version
ninja --version
pkg-config --version
g++ --version
```

Optional:

```bash
dpkg-query -W cmake ninja-build pkg-config qt6-base-dev libeigen3-dev catch2
dpkg-query -W libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev libocct-data-exchange-dev
```

## Projekt konfigurieren

Das Projekt besitzt CMake-Presets. Der Standard-Preset baut nur den MVP-1-Core
und die Tests. OCCT- und GUI-Targets sind standardmaessig deaktiviert.

Debug-Konfiguration:

```bash
cmake --preset dev
```

Release-Konfiguration:

```bash
cmake --preset release
```

Erwartung fuer den aktuellen Stand:

```text
BLCAD MVP-1 core skeleton configured.
Geometry/OCCT targets enabled: OFF
GUI/Qt targets enabled: OFF
```

Optionaler Geometry-Build mit OCCT:

```bash
cmake --preset dev-geometry
```

Erwartung:

```text
BLCAD MVP-1 core skeleton configured.
Geometry/OCCT targets enabled: ON
GUI/Qt targets enabled: OFF
```

## Build ausfuehren

Debug-Build:

```bash
cmake --build --preset dev
```

Das baut aktuell:

- `blcad_core`
- `blcad_core_tests`

Der Geometry-Build baut zusaetzlich:

- `blcad_geometry`
- `blcad_geometry_tests`

## Tests ausfuehren

Standardbefehl:

```bash
ctest --preset dev
```

Geometry-Tests:

```bash
ctest --preset dev-geometry
```

Aktuelle Tests:

- `Quantity` akzeptiert positive Millimeter-Laengen.
- `Quantity` lehnt `0`, negative und nicht-endliche Werte ab.
- `Parameter` speichert ID, Name, Scope und Wert.
- `Parameter` lehnt leere IDs und Namen ab.
- `PartDocument` speichert ID und Name.
- `PartDocument` verwaltet Parameter mit eindeutigen IDs und Namen.
- `PartDocument` erlaubt Parameterzugriff per ID und Name.
- `PartDocument` verwaltet Datum-Planes mit eindeutigen IDs.
- `PartDocument` verwaltet Sketches mit eindeutigen IDs.
- `PartDocument` validiert Sketch-Workplanes gegen vorhandene Datum-Planes.
- `PartDocument` validiert Profil-Parameterreferenzen gegen vorhandene
  Parameter.
- `PartDocument` verwaltet Features mit eindeutigen IDs.
- `PartDocument` validiert Feature-Sketch-, Feature-Parameter- und
  Feature-Zielfeature-Referenzen.
- `DatumPlane` erzeugt die Standardebene `XY`.
- `Sketch` speichert eine Workplane-Referenz.
- `RectangleProfile` und `CircleProfile` speichern Parameterreferenzen.
- Profil-IDs sind innerhalb einer Skizze eindeutig.
- `AdditiveExtrude` und `SubtractiveExtrude` speichern Feature-Absicht ohne
  Geometrie.
- `DependencyGraph` speichert Knoten und Abhaengigkeitskanten.
- `DependencyGraph` findet direkte und transitive Abhaengige.
- `DependencyGraph` liefert eine topologische Ordnung fuer die Referenzplatte.
- `DependencyGraph` erkennt Zyklen als Dependency-Fehler.
- `PartDocument` erzeugt Dependency-Graph-Knoten fuer Parameter, Sketches und
  Features.
- `PartDocument` erzeugt Dependency-Graph-Kanten aus Profil- und
  Feature-Referenzen.
- `InvalidationState` speichert `clean`, `changed`, `dirty` und `error`.
- `InvalidationState` markiert transitive Abhaengige ueber den
  `DependencyGraph`.
- `PartDocument` markiert betroffene Knoten nach einer Parameteraenderung.
- `RecomputePlan` listet `dirty`-Knoten in topologischer Reihenfolge.
- `PartDocument` kann aus seinem Zustand einen Recompute-Plan ableiten.
- `RectangleExtrusionAdapter` erzeugt im optionalen Geometry-Build ein
  nicht-leeres OCCT-Solid aus positiven Laengen.
- `GeometryShape` kapselt eine berechnete Shape als opaken Handle.
- `ShapeCache` speichert Feature-Shapes und einen finalen Shape im optionalen
  Geometry-Build.
- `GeometryRecomputeExecutor` fuehrt im optionalen Geometry-Build einen
  `AdditiveExtrude` aus einem Rechteckprofil aus und schreibt in den
  `ShapeCache`.
- `Error` und `Result` speichern erwartbare Fehler und Erfolgswerte.

Spaetere MVP-1-Tests sollen zentrischen Cut und STEP-Export ergaenzen.

## Dokumentation

Wichtige Dokumente:

- `zielarchitektur-parametrisches-cad-system.tex`: urspruengliche Zielarchitektur
- `docs/architecture-summary.md`: verdichtete Architekturuebersicht
- `docs/core-mvp1-skeleton.md`: aktueller Core-Skeleton
- `docs/sketch-mvp1-data-model.md`: Sketch- und Profil-Datenmodell
- `docs/feature-mvp1-data-model.md`: Feature-Datenmodell
- `docs/dependency-graph-mvp1-data-model.md`: Dependency-Graph-Datenmodell
- `docs/invalidation-mvp1-data-model.md`: Invalidierungsstatus-Datenmodell
- `docs/recompute-plan-mvp1-data-model.md`: Recompute-Plan-Datenmodell
- `docs/geometry-adapter-mvp1-rectangle-extrusion.md`: erster OCCT-Adapter
- `docs/shape-cache-mvp1-data-model.md`: ShapeCache-Datenmodell
- `docs/recompute-execution-mvp1-additive-extrude.md`: erste
  Additive-Recompute-Ausfuehrung
- `docs/mvp-plan.md`: Reihenfolge der MVPs
- `docs/mvp-1-specification.md`: genaue MVP-1-Spezifikation
- `docs/decisions/`: Architecture Decision Records
- `docs/dependencies-ubuntu-24.04.md`: Paketliste fuer Ubuntu 24.04

## Formatierung

Die Projektformatierung ist vorbereitet:

- `.editorconfig`
- `.clang-format`

Vor Commits soll `clang-format` fuer C++-Dateien verwendet werden.

## Generierte Dateien aufraeumen

CMake-Buildordner liegen unter `build/`.

```bash
rm -rf build/
```

LaTeX-Hilfsdateien sind in `.gitignore` eingetragen. Das PDF der Zielarchitektur
darf als Arbeitsartefakt im Ordner bleiben.

## Entwicklungsregel fuer MVP 1

Der erste Code soll nur den MVP-1-Pfad betreffen:

- kein GUI-Code
- keine Assembly
- keine Engineering-Assistenten
- kein Lochkreis
- kein allgemeiner Constraint Solver

Erst wenn Parameter, Recompute, OCCT-Geometrie und STEP-Export fuer die
Referenzplatte funktionieren, wird der Umfang erweitert.
