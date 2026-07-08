# 0016: Dokument-Recompute und ShapeCache-Grenze

Status: akzeptiert fuer MVP 1

## Kontext

Der Geometry-Pfad kann bereits `AdditiveExtrude` und `SubtractiveExtrude`
ausfuehren und den finalen Shape als STEP-Datei exportieren. Fuer die erste
vertikale MVP-1-Linie fehlt der durchgaengige Ablauf: ein `PartDocument`
vollstaendig in einen `ShapeCache` rechnen und exportieren.

Die MVP-1-Spezifikation nennt `shape_cache` als Feld von `PartDocument`. Der
konkrete `ShapeCache` haelt aber OCCT-Geometrie. `PartDocument` liegt in
`blcad_core` und muss frei von OCCT bleiben.

## Entscheidung

Der `ShapeCache` wird nicht in `PartDocument` eingebettet. Stattdessen erhaelt
der `GeometryRecomputeExecutor` eine Operation `execute_document`, die das
Dokument liest und den Cache aus dem Modell erzeugt.

`execute_document` kann aktuell:

- die topologische Ordnung des Dependency Graph des Dokuments lesen
- jeden Feature-Knoten in dieser Reihenfolge ausfuehren
- Nicht-Feature-Knoten ueberspringen
- den gesamten `ShapeCache` unabhaengig vom Invalidierungsstatus neu bauen

Der Recompute-Lebenszyklus bleibt getrennt:

- `execute_document` und `execute_plan` arbeiten auf einem `const PartDocument`.
- Das Setzen von `mark_all_clean()` nach erfolgreichem Recompute bleibt beim
  Aufrufer.

## Regeln

- `blcad_core` und `PartDocument` bleiben frei von OCCT und `blcad_geometry`.
- Der `ShapeCache` bleibt in der Geometry-Schicht.
- Der `ShapeCache` ist ein berechnetes, aus dem Modell neu erzeugbares Ergebnis
  neben dem Dokument, nicht ein Feld darin.
- Der Executor veraendert das Dokument nicht.
- `execute_document` baut alles neu; `execute_plan` rechnet nur `dirty`-Knoten.

## Konsequenzen

- Die erste vertikale MVP-1-Linie ist als Ende-zu-Ende-Ablauf fuer das
  Referenzbauteil testbar.
- Die ShapeCache-Regel "aus Dokument, Parametern, Skizzen und Features neu
  erzeugbar" ist durch `execute_document` konkret erfuellt.
- Ein inkrementeller Recompute nutzt den bereits gecachten Basiskoerper.
- Ein echter numerischer inkrementeller Recompute fehlt noch, weil das Aendern
  eines Parameterwerts im `PartDocument` noch nicht moeglich ist.

## Verworfene Alternativen

### ShapeCache als Feld in PartDocument

Das wuerde OCCT in den Core ziehen und die Grenze aus ADR 0005 aufweichen. Der
Cache bleibt deshalb in der Geometry-Schicht.

### mark_all_clean im Executor setzen

Dann muesste der Executor das Dokument mutieren und koennte es nicht als `const`
lesen. Der Lebenszyklus bleibt darum beim Aufrufer.

### Nur ueber execute_plan rechnen

Ein frisch gebautes Dokument ist ueberall `clean`, sein Recompute-Plan also
leer. Fuer den ersten vollstaendigen Aufbau des Cache braucht es einen Recompute
ueber alle Features, unabhaengig vom Invalidierungsstatus.
