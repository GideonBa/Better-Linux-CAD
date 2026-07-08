# MVP 1 Geometry-Adapter: Zentrischer Cut

Status: optionaler OCCT-Adapter fuer den zentrischen Bohrungs-Cut

Dieses Dokument beschreibt den zweiten Geometriepfad fuer MVP 1. Der Adapter
schneidet aus einem bereits berechneten Volumenkoerper eine zentrische,
durchgehende Bohrung. Er entspricht dem MVP-1-`SubtractiveExtrude` mit
`through_all`.

## Ziel

Der Schritt setzt den Boolean-Cut hinter dieselbe Core/OCCT-Grenze wie die
Rechteckextrusion:

- `blcad_core` bleibt frei von OCCT-Headern und OCCT-Linking.
- `blcad_geometry` kapselt den konkreten OCCT-Boolean-Cut.
- Der Adapter arbeitet auf einem `GeometryShape` und liefert ein `GeometryShape`.
- Der Adapter liest kein `PartDocument` und keinen `ShapeCache`; das uebernimmt
  der `GeometryRecomputeExecutor`.

## CMake-Target

Der Adapter liegt im optionalen Target `blcad_geometry` und wird nur mit dem
Geometry-Preset gebaut:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Der Boolean-Cut braucht zusaetzlich die OCCT-Toolkits `TKBO` und `TKBool`.

## Oeffentliche Schnittstelle

Der oeffentliche Header ist:

```text
include/blcad/geometry/circular_cut_adapter.hpp
```

Aktueller Typ:

- `CircularCutAdapter`: OCCT-Adapter fuer den zentrischen Cut

Aktuelle Operation:

```text
cut_circular_hole(target, diameter, center)
```

`GeometryShape` bleibt ein opaker Handle. Sein internes OCCT-Backing wird nur
ueber den nicht oeffentlichen Header `src/geometry/geometry_shape_internal.hpp`
mit den Adaptern geteilt. Der `CircularCutAdapter` ist wie der
`RectangleExtrusionAdapter` als `friend` von `GeometryShape` deklariert.

## Operation

Aktuell implementiert:

```text
cut_circular_hole(target, diameter, center)
```

Regeln:

- Der Zielkoerper `target` muss ein nicht-leeres `GeometryShape` sein.
- Der Durchmesser kommt als validierte `Quantity` und muss groesser als `0` sein.
- Der Kreis liegt standardmaessig zentriert um `(0, 0)` in der XY-Ebene.
- Der Cut geht durch den gesamten Koerper (`through_all`).
- Die Schneidgeometrie ist ein Zylinder in `+Z`-Richtung.
- Das Ergebnis muss ein nicht-leerer OCCT-Shape sein.
- Erwartbare OCCT-Fehler werden als `ErrorCategory::Geometry` gemeldet.

## Through-All-Modell

Der Cut kennt die Dicke des Zielkoerpers nicht direkt. Statt einen Dickenwert zu
uebergeben, liest der Adapter die Z-Ausdehnung des Zielkoerpers aus dessen
OCCT-Bounding-Box (`BRepBndLib`). Der Schneidzylinder wird um einen festen
Ueberstand ueber diese Grenzen hinaus verlaengert. Dadurch bleibt der Cut auch
bei Rundungsfehlern an den Deckflaechen sauber `through_all`.

## Shape-Diagnose

`ShapeSummary` und `RectangleExtrusionAdapter::summarize` wurden um `volume_mm3`
erweitert. Das Volumen wird ueber `BRepGProp::VolumeProperties` berechnet.
Dadurch koennen Tests ohne direkte OCCT-Abhaengigkeit pruefen, dass der Cut das
Volumen gegenueber dem Vollkoerper verkleinert.

## Testabdeckung

Aktuelle Geometry-Tests pruefen:

- ein zentrischer Cut erzeugt ein nicht-leeres Shape mit genau einem Solid
- das Volumen nach dem Cut ist kleiner als das Volumen des Vollkoerpers
- ein leerer Zielkoerper wird als Geometry-Fehler abgelehnt

## Bewusste Begrenzung

Noch nicht enthalten:

- nicht zentrierte oder mehrere Bohrungen
- begrenzte Cut-Tiefe statt `through_all`
- Cut-Richtung ausser `+Z`
- Lochkreise oder Muster
- STEP-Export
- GUI

## Naechster sinnvoller Schritt

Nach der additiven und der subtraktiven Ausfuehrung fehlt fuer die erste
vertikale MVP-1-Linie nur noch der STEP-Export des finalen Shapes aus dem
`ShapeCache`. Das `PartDocument` bleibt Quelle der Modellabsicht, nicht die
OCCT-Shape.
