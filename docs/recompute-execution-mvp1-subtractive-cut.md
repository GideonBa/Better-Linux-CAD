# MVP 1 Recompute-Ausfuehrung: SubtractiveExtrude

Status: optionale Geometry-Ausfuehrung fuer einen zentrischen Cut

Dieses Dokument beschreibt den zweiten ausgefuehrten Recompute-Schritt fuer
MVP 1. Der Code liegt im optionalen Target `blcad_geometry` und bleibt damit
ausserhalb des reinen Core-Targets.

## Ziel

Der Schritt baut auf der Additive-Ausfuehrung auf und schliesst den Boolean-Cut
an denselben Pfad an:

- `PartDocument`
- `RecomputePlan`
- `CircleProfile`
- `SubtractiveExtrude`
- `CircularCutAdapter`
- `ShapeCache`

Die Ausfuehrung soll beweisen, dass ein zuvor berechneter Basiskoerper aus dem
`ShapeCache` gelesen, mit einem zentrischen Cut versehen und als neuer finaler
Shape zurueckgeschrieben werden kann.

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

Aktuelle Operationen:

```text
execute_additive_extrude(document, feature_id, shape_cache)
execute_subtractive_extrude(document, feature_id, shape_cache)
execute_plan(document, recompute_plan, shape_cache)
```

## Ausfuehrungsmodell

`execute_subtractive_extrude` arbeitet in dieser Reihenfolge:

1. Feature-ID validieren.
2. Feature im `PartDocument` suchen.
3. Feature-Typ muss `SubtractiveExtrude` sein.
4. Eingabe-Sketch suchen.
5. Sketch muss genau ein Kreisprofil und keine weiteren Profile enthalten.
6. Durchmesserparameter aufloesen.
7. Zielshape ueber `target_feature` aus dem `ShapeCache` lesen.
8. `CircularCutAdapter` aufrufen.
9. Ergebnis im `ShapeCache` als Feature-Shape und finalen Shape speichern.

`execute_plan` laeuft ueber die Schritte eines `RecomputePlan`. Nicht-Feature-
Knoten, zum Beispiel Sketch-Knoten, werden uebersprungen. Feature-Knoten werden
nach ihrem Typ verteilt: `AdditiveExtrude` geht an `execute_additive_extrude`,
`SubtractiveExtrude` an `execute_subtractive_extrude`.

Die topologische Ordnung des `RecomputePlan` stellt sicher, dass ein
`AdditiveExtrude` vor dem darauf aufbauenden `SubtractiveExtrude` laeuft. Der Cut
findet seinen Zielkoerper dadurch bereits im `ShapeCache`.

## Abhaengigkeit vom Zielkoerper

Der Cut benoetigt den Shape seines `target_feature` im `ShapeCache`. In einem
Teil-Plan, der nur `hole_diameter` betrifft, wird der Basiskoerper nicht neu
berechnet. Er muss dann noch aus einem frueheren vollstaendigen Recompute im
Cache liegen. Fehlt der Zielshape, meldet der Executor einen Geometry-Fehler und
schreibt keinen finalen Shape.

## Validierung

Aktuelle Fehlerfaelle:

- leere Feature-ID
- Feature existiert nicht im Dokument
- Feature ist kein `SubtractiveExtrude`
- Eingabe-Sketch existiert nicht
- Sketch enthaelt nicht genau ein Kreisprofil
- Durchmesserparameter fehlt
- Zielshape fehlt im `ShapeCache`
- Geometry-Adapter erzeugt einen Fehler
- ShapeCache lehnt das Ergebnis ab

## Testabdeckung

Aktuelle Tests pruefen:

- ein `SubtractiveExtrude` schneidet ein Loch in einen zuvor gecachten
  Basiskoerper und verkleinert das Volumen
- `execute_plan` fuehrt aus einem `width`-Plan zuerst den additiven, dann den
  subtraktiven Feature-Knoten aus
- ein fehlender Zielshape im Cache wird gemeldet
- nicht-kreisfoermige Subtractive-Sketches werden abgelehnt

## Bewusste Begrenzung

Noch nicht enthalten:

- mehrere Bohrungen oder Muster
- begrenzte Cut-Tiefe statt `through_all`
- ShapeCache-Integration direkt in `PartDocument`
- automatisches `mark_all_clean()` nach erfolgreichem Recompute
- STEP-Export
- GUI

## Naechster sinnvoller Schritt

Der naechste technische Schritt sollte der STEP-Export des finalen Shapes aus
dem `ShapeCache` sein:

1. finalen Shape aus dem `ShapeCache` lesen
2. ueber einen kleinen OCCT-STEP-Adapter exportieren
3. Exportpfad vom Aufrufer uebernehmen
4. Exportfehler als Fehlerobjekt melden
5. weiter keinen allgemeinen Solver und keine GUI bauen
