# MVP 1 Dokument-Recompute und Referenzbauteil

Status: optionale Geometry-Ausfuehrung fuer einen vollstaendigen Dokument-Recompute

Dieses Dokument beschreibt den Schritt, der die einzelnen MVP-1-Bausteine zu
einem durchgaengigen Ablauf verbindet: ein `PartDocument` wird vollstaendig in
einen `ShapeCache` gerechnet und als STEP-Datei exportiert. Der Code liegt im
optionalen Target `blcad_geometry`.

## Ziel

Der Schritt schliesst die erste vertikale MVP-1-Linie zu einem Ende-zu-Ende-
Ablauf:

- `PartDocument` beschreibt die Konstruktionsabsicht.
- Der `GeometryRecomputeExecutor` rechnet alle Features des Dokuments in
  topologischer Reihenfolge in einen `ShapeCache`.
- Der `StepExporter` schreibt den finalen Shape als STEP-Datei.
- Der `ShapeCache` bleibt ein aus dem Modell neu erzeugbares Ergebnis.

## Grenze zwischen Core und ShapeCache

Die MVP-1-Spezifikation nennt einen `shape_cache` als Feld von `PartDocument`.
Der konkrete `ShapeCache` haelt aber OCCT-Geometrie und muss deshalb ausserhalb
des OCCT-freien Core-Targets bleiben.

Die gewaehlte Loesung haelt beide Anforderungen ein:

- `PartDocument` bleibt in `blcad_core` und frei von OCCT.
- Der `ShapeCache` bleibt in `blcad_geometry`.
- Der `GeometryRecomputeExecutor` verbindet beide, indem er das Dokument liest
  und den Cache aus dem Modell erzeugt.

Das Dokument besitzt den Cache also nicht direkt. Der Cache ist ein berechnetes
Ergebnis neben dem Dokument, genau wie es die ShapeCache-Regeln fordern.

## Oeffentliche Schnittstelle

Header:

```text
include/blcad/geometry/recompute_executor.hpp
```

Neue Operation:

```text
execute_document(document, shape_cache)
```

`execute_document` liest die topologische Ordnung des Dependency Graph des
Dokuments und fuehrt jeden Feature-Knoten in dieser Reihenfolge aus. Nicht-
Feature-Knoten wie Parameter und Sketches werden uebersprungen. Die topologische
Ordnung stellt sicher, dass ein `AdditiveExtrude` vor dem darauf aufbauenden
`SubtractiveExtrude` laeuft.

Im Unterschied dazu fuehrt `execute_plan` nur die `dirty`-Knoten eines
`RecomputePlan` aus. `execute_document` ignoriert den Invalidierungsstatus und
baut den gesamten Cache neu.

## Recompute-Lebenszyklus

Der Executor arbeitet auf einem `const PartDocument` und veraendert dessen
Invalidierungsstatus nicht. Das Setzen von `mark_all_clean()` nach einem
erfolgreichen Recompute bleibt beim Aufrufer. Dadurch bleibt die Ausfuehrung
frei von Dokumentmutation.

Ein typischer Ablauf:

1. Dokument vollstaendig rechnen: `execute_document(document, cache)`.
2. Dokument als sauber markieren: `document.mark_all_clean()`.
3. Bei einer Parameteraenderung `mark_parameter_changed(...)` aufrufen.
4. Aus dem `dirty`-Zustand einen `RecomputePlan` ableiten.
5. Nur die betroffenen Features mit `execute_plan(...)` neu rechnen.
6. Danach erneut `mark_all_clean()` setzen.

Ein inkrementeller Plan fuer eine Bohrungsaenderung enthaelt nur den Cut. Der
Basiskoerper bleibt sauber und muss aus einem frueheren Recompute im `ShapeCache`
liegen.

## Referenzbauteil

Der Ende-zu-Ende-Test erzeugt das Referenzbauteil aus der Spezifikation:

```text
width         = 120 mm
height        = 80 mm
thickness     = 8 mm
hole_diameter = 20 mm
```

Der Test beweist die MVP-1-Erfolgskriterien fuer die Geometrie:

- alle vier Parameter werden gesetzt
- die Platte entsteht als `AdditiveExtrude`
- die Bohrung entsteht als `SubtractiveExtrude`-Cut durch den Koerper
- der finale Shape hat genau einen Solid und ein kleineres Volumen als die volle
  Platte
- der finale Shape wird als gueltige STEP-Datei exportiert

## Testabdeckung

Aktuelle Tests pruefen:

- `execute_document` rechnet das Referenzbauteil vollstaendig und exportiert eine
  gueltige, nicht-leere STEP-Datei
- der Recompute folgt einem sauberen Clean/Dirty-Lebenszyklus: nach
  `mark_all_clean()` ist der Recompute-Plan leer, eine Bohrungsaenderung erzeugt
  einen inkrementellen Plan nur fuer den Cut

## Bewusste Begrenzung

Noch nicht enthalten:

- eine Methode zum Aendern eines Parameterwerts im `PartDocument`
- damit noch kein echter numerischer inkrementeller Recompute
- Serialisierung des Dokuments als JSON oder eigenes Dateiformat
- ShapeCache als direktes Feld von `PartDocument`
- GUI

## Naechster sinnvoller Schritt

Der naechste kleine Schritt sollte das inkrementelle Recompute inhaltlich
vervollstaendigen:

1. eine Operation zum Aendern eines Parameterwerts im `PartDocument` ergaenzen
2. damit einen echten numerischen inkrementellen Recompute testen
   (`hole_diameter` oder `thickness` aendern, Shape und Volumen pruefen)
3. anschliessend die JSON-Serialisierung der Modellabsicht vorbereiten
4. weiter keinen allgemeinen Solver und keine GUI bauen
