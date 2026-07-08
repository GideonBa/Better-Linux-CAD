# MVP 1 Recompute-Plan-Datenmodell

Status: reines Datenmodell mit PartDocument-Integration, keine Ausfuehrung

Dieses Dokument beschreibt den aktuellen Stand von `RecomputePlan`.

Der Stand ist bewusst begrenzt:

- keine Geometrie
- keine OCCT-Operationen im Recompute-Plan
- kein ShapeCache-Update
- kein Feature-Recompute
- keine GUI

## Ziel

`RecomputePlan` beschreibt, welche `dirty`-Knoten spaeter neu berechnet werden
muessten und in welcher Reihenfolge.

Der Plan ist keine Ausfuehrung. Er ruft keine Feature-Logik auf, erzeugt keine
Shapes und schreibt keinen Cache. Er ist nur die geordnete Aufgabenliste fuer
einen spaeteren Recompute.

## Eingaben

Der Plan wird aus zwei bestehenden Datenmodellen abgeleitet:

- `DependencyGraph`
- `InvalidationState`

Der `DependencyGraph` liefert die topologische Ordnung. Der
`InvalidationState` sagt, welche Knoten `dirty` sind.

## Regeln

`RecomputePlan::from_graph_and_invalidation_state(graph, invalidation_state)`
arbeitet nach diesen Regeln:

- Der Dependency Graph muss azyklisch sein.
- Nur Knoten mit Status `dirty` werden in den Plan aufgenommen.
- Knoten mit Status `changed` werden nicht aufgenommen.
- Knoten mit Status `clean` werden nicht aufgenommen.
- Die Reihenfolge folgt der topologischen Ordnung des Graphen.

Damit wird ein geaenderter Parameter nicht selbst recomputed. Stattdessen
werden nur die davon abhaengigen Sketches und Features geplant.

## Beispiel

Wenn `part.width` geaendert wurde, markiert der Invalidierungsstatus:

```text
part.width               changed
sketch.base              dirty
feature.base_extrude     dirty
feature.center_hole_cut  dirty
```

Der Recompute-Plan enthaelt:

```text
1. sketch.base
2. feature.base_extrude
3. feature.center_hole_cut
```

`part.width` ist nicht Teil des Plans, weil der Parameter bereits die Quelle der
Aenderung ist.

## PartDocument-Integration

`PartDocument::create_recompute_plan()` leitet den Plan aus dem internen
Dependency Graph und dem internen Invalidierungsstatus ab.

Aktueller Ablauf:

```text
PartDocument::mark_parameter_changed(part.width)
  -> part.width changed
  -> abhaengige Knoten dirty

PartDocument::create_recompute_plan()
  -> dirty-Knoten in topologischer Reihenfolge
```

`PartDocument::mark_all_clean()` leert die Planung indirekt, weil danach keine
`dirty`-Knoten mehr vorhanden sind.

## Fehlerverhalten

Wenn der Dependency Graph einen Zyklus enthaelt, kann keine topologische
Reihenfolge erzeugt werden. In diesem Fall gibt `RecomputePlan` den
Dependency-Fehler des Graphen zurueck.

```text
dependency graph contains a cycle
```

## Testabdeckung

Aktuelle Tests pruefen:

- sauberer Invalidierungsstatus erzeugt einen leeren Plan
- `dirty`-Knoten werden in topologischer Reihenfolge aufgenommen
- `changed`-Knoten werden nicht in den Plan aufgenommen
- unabhaengige Graphzweige bleiben aus dem Plan heraus
- Graphzyklen werden als Dependency-Fehler gemeldet
- `PartDocument` erzeugt einen Plan aus seinem internen Zustand
- `PartDocument::mark_all_clean()` fuehrt zu einem leeren Plan

## Anschluss an Geometrie

Der erste Geometry-Adapter fuer eine Rechteckextrusion, ein kleiner
`ShapeCache` und eine schmale `AdditiveExtrude`-Ausfuehrung liegen inzwischen
im optionalen Target `blcad_geometry`. Der Recompute-Plan bleibt davon getrennt:
Er beschreibt weiterhin nur die Reihenfolge der neu zu berechnenden Knoten.

Naechste Schritte duerfen den Plan nutzen, sollten aber klein bleiben:

- `SubtractiveExtrude` aus einem Plan-Knoten ausfuehren
- Ergebnis im bestehenden `ShapeCache` aktualisieren
- OCCT weiter nur hinter `blcad_geometry` verwenden
- noch keine GUI bauen
