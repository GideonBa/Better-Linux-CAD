# MVP 1 Invalidierungsstatus-Datenmodell

Status: reines Datenmodell mit PartDocument-Integration, kein Recompute

Dieses Dokument beschreibt den aktuellen Stand von `InvalidationState`.

Der Stand ist bewusst begrenzt:

- keine Geometrie
- keine OCCT-Operationen
- kein ShapeCache in dieser Invalidierungsschicht
- kein Recompute
- keine GUI

## Ziel

`InvalidationState` speichert, welche Knoten des `DependencyGraph` sauber,
direkt geaendert oder durch eine Aenderung betroffen sind.

Damit kann MVP 1 bereits nachvollziehen:

- welcher Parameter geaendert wurde
- welche Sketches davon abhaengen
- welche Features davon abhaengen
- welche Knoten vor einem spaeteren Recompute nicht mehr als sauber gelten

Das Modell fuehrt aber noch nichts aus. Es markiert nur Status.

## Statuswerte

Aktuelle Statuswerte:

```text
clean
changed
dirty
error
```

Bedeutung:

- `clean`: Knoten ist aktuell nicht als betroffen markiert.
- `changed`: Knoten wurde direkt geaendert, zum Beispiel ein Parameter.
- `dirty`: Knoten haengt transitiv von einem geaenderten Knoten ab.
- `error`: reserviert fuer spaetere Recompute- oder Validierungsfehler.

`dirty` bedeutet in diesem Schritt nur: Der Knoten waere vor einer
Geometrieberechnung neu zu betrachten. Es bedeutet noch nicht, dass ein
Recompute ausgefuehrt wurde oder fehlgeschlagen ist.

## Graphbasierte Markierung

`InvalidationState::mark_changed(graph, node_id)` verwendet den bestehenden
`DependencyGraph`.

Ablauf:

1. Der geaenderte Knoten muss im Graphen existieren.
2. Der geaenderte Knoten wird als `changed` markiert.
3. Alle transitiv abhaengigen Knoten werden als `dirty` markiert.
4. Die betroffenen Knoten werden in deterministischer Reihenfolge
   zurueckgegeben.

Beispiel fuer `part.width`:

```text
part.width
  -> sketch.base
  -> feature.base_extrude
  -> feature.center_hole_cut
```

Nach der Markierung:

```text
part.width               changed
sketch.base              dirty
feature.base_extrude     dirty
feature.center_hole_cut  dirty
```

Nicht betroffene Knoten bleiben `clean`.

## PartDocument-Integration

`PartDocument` besitzt einen `InvalidationState`.

Beim erfolgreichen Hinzufuegen von Parametern, Sketches und Features wird der
Invalidierungsstatus mit dem Dependency Graph synchronisiert. Neue Graphknoten
starten als `clean`.

Aktuelle API:

```text
PartDocument::invalidation_state()
PartDocument::mark_parameter_changed(ParameterId)
PartDocument::create_recompute_plan()
PartDocument::mark_all_clean()
```

`mark_parameter_changed()` aendert keinen Parameterwert. Die Methode beschreibt
nur den Zustand nach einer Parameteraenderung. Die eigentliche
Parameterwert-Aenderung und der Recompute folgen spaeter.

## Fehlerverhalten

Leere Knoten-IDs sind Validierungsfehler.

```text
invalidation node id must not be empty
changed node id must not be empty
```

Ein geaenderter Knoten muss im Dependency Graph existieren.

```text
changed node must exist in dependency graph
```

`PartDocument::mark_parameter_changed()` akzeptiert nur Parameter, die im
Dokument existieren.

```text
parameter must exist in part document
```

## Testabdeckung

Aktuelle Tests pruefen:

- stabile Textnamen der Statuswerte
- Graphknoten starten als `clean`
- leere Invalidierungsknoten werden abgelehnt
- geaenderte Knoten werden als `changed` markiert
- transitive Abhaengige werden als `dirty` markiert
- nicht betroffene Graphzweige bleiben `clean`
- alle Knoten koennen wieder auf `clean` gesetzt werden
- fehlende geaenderte Knoten werden abgelehnt
- `PartDocument` synchronisiert den Invalidierungsstatus mit seinem Graphen
- `PartDocument` markiert betroffene Knoten nach einer Parameteraenderung
- `PartDocument` kann daraus einen Recompute-Plan ableiten

## Naechster sinnvoller Schritt

Der erste optionale Geometry-Adapter und die Additive-Ausfuehrung sind
vorhanden. Die Invalidierung bleibt davon getrennt und markiert nur Knoten. Der
naechste Schritt sollte klein bleiben:

- `SubtractiveExtrude` aus einem Recompute-Plan-Knoten ausfuehren
- bestehenden Geometry-`ShapeCache` als Eingabe und Ergebnis behandeln
- OCCT nur hinter dieser Adaptergrenze verwenden
- noch keine GUI bauen
