# MVP 1 Geometry-Adapter: Rechteck-Extrusion

Status: optionaler OCCT-Adapter, noch nicht in Recompute integriert

Dieses Dokument beschreibt den ersten kleinen Geometriepfad fuer MVP 1. Der
Adapter erzeugt aus drei bereits validierten `Quantity`-Werten ein OCCT-Solid
fuer eine zentrierte rechteckige Platte.

## Ziel

Der Schritt prueft die technische Grenze zwischen Core und OCCT:

- `blcad_core` bleibt frei von OCCT-Headern und OCCT-Linking.
- `blcad_geometry` kapselt die konkrete OCCT-Erzeugung.
- Tests fuer den Core laufen weiterhin ohne Geometrie-Target.
- Geometry-Tests werden nur mit `BLCAD_BUILD_GEOMETRY=ON` gebaut.

Der Adapter ist noch kein Feature-Recompute. Er liest kein `PartDocument` und
fuehrt keinen `RecomputePlan` aus. Ein separater `ShapeCache` im selben
Geometry-Target kann berechnete Shapes speichern.

## CMake-Target

Das Target heisst:

```text
blcad_geometry
```

Es wird nur gebaut, wenn diese CMake-Option aktiv ist:

```text
BLCAD_BUILD_GEOMETRY=ON
```

Der Standard-Preset `dev` laesst diese Option aus. Fuer den Geometry-Pfad gibt
es einen eigenen Preset:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

## Oeffentliche Schnittstelle

Der oeffentliche Header ist:

```text
include/blcad/geometry/rectangle_extrusion_adapter.hpp
```

Aktuelle Typen:

- `GeometryShape`: opaker Handle fuer berechnete Geometrie
- `ShapeSummary`: kleine Test- und Diagnosezusammenfassung
- `RectangleExtrusionAdapter`: erster OCCT-Adapter

`GeometryShape` enthaelt keine oeffentlichen OCCT-Typen. Dadurch kann
`ShapeCache` eine Geometrie halten, ohne den Core mit OCCT zu vermischen.

## Operation

Aktuell implementiert:

```text
make_extruded_rectangle(width, height, thickness)
```

Regeln:

- Breite, Hoehe und Dicke kommen als validierte `Quantity`.
- Das Rechteck liegt zentriert um `(0, 0)` in der XY-Ebene.
- Extrusionsrichtung ist fest `+Z`.
- Das Ergebnis muss ein nicht-leerer OCCT-Shape sein.
- Erwartbare OCCT-Fehler werden als `ErrorCategory::Geometry` gemeldet.

## Testabdeckung

Aktuelle Geometry-Tests pruefen:

- positive Laengen erzeugen ein nicht-leeres Shape
- die Rechteckextrusion erzeugt genau einen Solid
- ein default-konstruierter `GeometryShape` ist leer

## Bewusste Begrenzung

Noch nicht enthalten:

- Ausfuehrung von `AdditiveExtrude`
- Ausfuehrung von `SubtractiveExtrude`
- zentrischer Bohrungs-Cut
- `SubtractiveExtrude`-Cut
- STEP-Export
- GUI

## Naechster sinnvoller Schritt

Nach diesem Adapter, dem ersten `ShapeCache` und der Additive-Ausfuehrung
sollte als naechstes der zentrische Cut fuer `SubtractiveExtrude` vorbereitet
werden. Das `PartDocument` bleibt Quelle der Modellabsicht, nicht die
OCCT-Shape.
