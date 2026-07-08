# 0013: Erste Recompute-Ausfuehrung fuer AdditiveExtrude

Status: akzeptiert fuer MVP 1

## Kontext

Der Core kann bereits Parameter, Sketches, Features, Dependency Graph,
Invalidierungsstatus und Recompute-Plan modellieren. Das optionale
Geometry-Target kann eine Rechteckextrusion erzeugen und berechnete Shapes in
einem `ShapeCache` halten.

Der naechste kleine Schritt ist, diese Bausteine erstmals kontrolliert zu
verbinden, ohne einen allgemeinen Feature-Solver zu bauen.

## Entscheidung

Es wird ein `GeometryRecomputeExecutor` im Target `blcad_geometry` eingefuehrt.

Der Executor kann aktuell:

- `AdditiveExtrude` fuer genau ein Rechteckprofil ausfuehren
- Breite, Hoehe und Extrusionslaenge aus dem `PartDocument` aufloesen
- `RectangleExtrusionAdapter` verwenden
- das Ergebnis im `ShapeCache` als Feature-Shape und finalen Shape speichern
- einen `RecomputePlan` durchlaufen und Nicht-Feature-Knoten ueberspringen

Nicht unterstuetzte Feature-Typen im Plan werden als Fehler gemeldet. Sie werden
nicht still ignoriert.

## Regeln

- `blcad_core` bleibt frei von OCCT und `blcad_geometry`.
- Der Executor liegt in der Geometry-Schicht.
- `PartDocument` bleibt Quelle der Modellabsicht.
- Der `ShapeCache` bleibt berechnetes Ergebnis.
- Der Executor unterstuetzt fuer MVP 1 nur ein einzelnes Rechteckprofil.
- `SubtractiveExtrude`, Boolean Cut und STEP-Export bleiben separate Schritte.

## Konsequenzen

- Der erste vertikale Pfad von Parameter/Sketch/Feature zu OCCT-Shape ist
  testbar.
- Der `RecomputePlan` wird erstmals fuer eine Ausfuehrung genutzt.
- Der naechste Schritt kann auf einem vorhandenen finalen Shape aufbauen und
  den zentrischen Cut implementieren.
- Unsupported Features erzeugen frueh sichtbare Fehler statt unvollstaendige
  Modelle.

## Verworfene Alternativen

### Allgemeinen Recompute-Solver bauen

Das waere zu frueh. MVP 1 braucht zuerst einen kleinen, nachvollziehbaren Pfad.

### SubtractiveExtrude sofort mitbauen

Das wuerde den Schritt groesser machen und Fehlerquellen mischen. Der additive
Pfad soll erst fuer sich stabil sein.

### Plan-Knoten fuer unbekannte Features ignorieren

Das waere gefaehrlich, weil der Cache dann scheinbar erfolgreich, aber
unvollstaendig waere.
