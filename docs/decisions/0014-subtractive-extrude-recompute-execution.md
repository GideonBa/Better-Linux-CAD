# 0014: Recompute-Ausfuehrung fuer SubtractiveExtrude

Status: akzeptiert fuer MVP 1

## Kontext

Der `GeometryRecomputeExecutor` kann bereits `AdditiveExtrude` fuer ein
Rechteckprofil ausfuehren und das Ergebnis im `ShapeCache` ablegen. Der naechste
kleine Schritt aus ADR 0013 ist der zentrische Cut fuer `SubtractiveExtrude`.

Das Referenzbauteil ist eine Platte mit zentraler Bohrung. Der Cut braucht einen
bereits berechneten Basiskoerper und darf keinen allgemeinen Feature-Solver
einfuehren.

## Entscheidung

Es wird ein `CircularCutAdapter` im Target `blcad_geometry` eingefuehrt, und der
`GeometryRecomputeExecutor` erhaelt `execute_subtractive_extrude`.

Der Adapter kann aktuell:

- aus einem `GeometryShape` und einem Durchmesser eine zentrische, durchgehende
  Bohrung schneiden
- die Z-Ausdehnung des Zielkoerpers aus dessen OCCT-Bounding-Box ableiten
- den `through_all`-Cut als OCCT-Boolean-Cut ausfuehren

Der Executor kann aktuell:

- `SubtractiveExtrude` fuer genau ein Kreisprofil ausfuehren
- den Durchmesser aus dem `PartDocument` aufloesen
- den Zielshape ueber `target_feature` aus dem `ShapeCache` lesen
- das Ergebnis im `ShapeCache` als Feature-Shape und finalen Shape speichern
- Feature-Knoten in einem `RecomputePlan` nach Typ verteilen

## Regeln

- `blcad_core` bleibt frei von OCCT und `blcad_geometry`.
- Adapter und Executor liegen in der Geometry-Schicht.
- `PartDocument` bleibt Quelle der Modellabsicht.
- Der `ShapeCache` bleibt berechnetes Ergebnis.
- Der Cut unterstuetzt fuer MVP 1 nur eine zentrische `through_all`-Bohrung.
- Das interne OCCT-Backing von `GeometryShape` wird nur ueber einen nicht
  oeffentlichen Header mit den Adaptern geteilt.

## Konsequenzen

- Die erste vertikale Linie deckt jetzt Parameter, Sketch, additives und
  subtraktives Feature bis zur OCCT-Shape ab.
- Die topologische Ordnung des `RecomputePlan` wird erstmals fuer eine echte
  Feature-Abhaengigkeit (Cut nach Basiskoerper) genutzt.
- Ein Teil-Plan ohne den Basiskoerper meldet einen klaren Fehler, statt einen
  unvollstaendigen Cut zu erzeugen.
- Der naechste Schritt kann auf dem finalen Shape aufsetzen und den STEP-Export
  bauen.

## Verworfene Alternativen

### Dickenwert an den Cut uebergeben

Der Executor koennte die Dicke des Zielfeatures an den Adapter reichen. Das
koppelt den Cut aber an den additiven Laengenparameter. Die Bounding-Box des
tatsaechlichen Zielkoerpers ist robuster und lokaler.

### Cut ohne vorhandenen Zielshape stillschweigend ueberspringen

Das wuerde einen scheinbar erfolgreichen, aber unvollstaendigen finalen Shape
erzeugen. Der fehlende Zielshape wird deshalb als Fehler gemeldet.

### GeometryShape oeffentlich fuer OCCT oeffnen

Das wuerde die Core/OCCT-Grenze aufweichen. Stattdessen teilen sich die Adapter
das Backing ueber einen internen Header und `friend`-Zugriff.
