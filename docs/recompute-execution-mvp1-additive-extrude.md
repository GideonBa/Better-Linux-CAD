# MVP 1 Recompute-Ausfuehrung: AdditiveExtrude

Status: optionale Geometry-Ausfuehrung fuer ein Rechteckprofil

Dieses Dokument beschreibt den ersten ausgefuehrten Recompute-Schritt fuer
MVP 1. Der Code liegt im optionalen Target `blcad_geometry` und bleibt damit
ausserhalb des reinen Core-Targets.

## Ziel

Der Schritt verbindet erstmals die vorhandenen Datenmodelle:

- `PartDocument`
- `RecomputePlan`
- `RectangleProfile`
- `AdditiveExtrude`
- `RectangleExtrusionAdapter`
- `ShapeCache`

Die Ausfuehrung soll beweisen, dass ein `dirty`-Feature-Knoten aus einem
Recompute-Plan in berechnete OCCT-Geometrie uebersetzt und im Cache abgelegt
werden kann.

## CMake-Target

Die Ausfuehrung wird nur mit dem Geometry-Preset gebaut:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Der Standard-Preset `dev` bleibt Core-only.

## Oeffentliche Schnittstelle

Header:

```text
include/blcad/geometry/recompute_executor.hpp
```

Aktuelle Typen:

- `GeometryRecomputeSummary`
- `GeometryRecomputeExecutor`

Aktuelle Operationen:

```text
execute_additive_extrude(document, feature_id, shape_cache)
execute_plan(document, recompute_plan, shape_cache)
```

## Ausfuehrungsmodell

`execute_additive_extrude` arbeitet in dieser Reihenfolge:

1. Feature-ID validieren.
2. Feature im `PartDocument` suchen.
3. Feature-Typ muss `AdditiveExtrude` sein.
4. Eingabe-Sketch suchen.
5. Sketch muss genau ein Rechteckprofil und keine weiteren Profile enthalten.
6. Breiten-, Hoehen- und Laengenparameter aufloesen.
7. `RectangleExtrusionAdapter` aufrufen.
8. Ergebnis im `ShapeCache` als Feature-Shape und finalen Shape speichern.

`execute_plan` laeuft ueber die Schritte eines `RecomputePlan`. Nicht-Feature-
Knoten, zum Beispiel Sketch-Knoten, werden uebersprungen. Feature-Knoten werden
derzeit nur ausgefuehrt, wenn sie `AdditiveExtrude` sind.

Der Plan wird vor der Ausfuehrung auf nicht unterstuetzte Feature-Typen
geprueft. Dadurch wird der `ShapeCache` nicht teilweise beschrieben, wenn spaeter
im selben Plan ein noch nicht implementierter `SubtractiveExtrude`-Knoten steht.

## Validierung

Aktuelle Fehlerfaelle:

- leere Feature-ID
- Feature existiert nicht im Dokument
- Feature ist kein `AdditiveExtrude`
- Eingabe-Sketch existiert nicht
- Sketch enthaelt nicht genau ein Rechteckprofil
- Breiten-, Hoehen- oder Laengenparameter fehlen
- Geometry-Adapter erzeugt einen Fehler
- ShapeCache lehnt das Ergebnis ab

Unsupported Feature-Typen in einem Plan liefern einen Geometry-Fehler. Dadurch
werden `SubtractiveExtrude`-Knoten nicht stillschweigend ignoriert.

## Testabdeckung

Aktuelle Tests pruefen:

- ein einzelnes `AdditiveExtrude` erzeugt ein nicht-leeres Shape im Cache
- `execute_plan` ueberspringt Sketch-Knoten und fuehrt den Additive-Feature-
  Knoten aus
- fehlende Feature-IDs werden gemeldet
- nicht-rechteckige Additive-Sketches werden abgelehnt
- nicht unterstuetzte Feature-Typen im Plan erzeugen einen Fehler, ohne den
  Cache teilweise zu beschreiben

## Bewusste Begrenzung

Noch nicht enthalten:

- `SubtractiveExtrude`
- Boolean Cut
- Kreisprofile als Bohrung
- mehrere Rechteckprofile in einem Sketch
- ShapeCache-Integration direkt in `PartDocument`
- automatisches `mark_all_clean()` nach erfolgreichem Recompute
- STEP-Export
- GUI

## Naechster sinnvoller Schritt

Der naechste technische Schritt sollte der zentrische Cut fuer
`SubtractiveExtrude` sein:

1. aus einem Kreisprofil Durchmesser und Position aufloesen
2. Zielshape aus dem `ShapeCache` lesen
3. OCCT-Boolean-Cut ausfuehren
4. neuen finalen Shape im `ShapeCache` speichern
5. weiter keinen allgemeinen Solver und keine GUI bauen
