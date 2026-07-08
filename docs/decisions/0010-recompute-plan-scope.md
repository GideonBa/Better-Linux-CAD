# 0010: Umfang des MVP-1-Recompute-Plans

Status: akzeptiert fuer MVP 1

## Kontext

`DependencyGraph` beschreibt Abhaengigkeiten. `InvalidationState` markiert nach
einer Parameteraenderung betroffene Knoten als `dirty`.

Vor der eigentlichen Geometrieberechnung braucht MVP 1 eine geordnete Liste der
Knoten, die neu berechnet werden muessten. Diese Liste soll noch keine
Ausfuehrung enthalten.

## Entscheidung

MVP 1 bekommt einen kleinen `RecomputePlan`.

Der Plan:

- liest `DependencyGraph`
- liest `InvalidationState`
- uebernimmt nur `dirty`-Knoten
- sortiert diese Knoten topologisch
- meldet Graphzyklen als Dependency-Fehler

Der Plan fuehrt nicht aus:

- keine Feature-Berechnung
- keine OCCT-Operation
- kein Shape-Cache-Update
- kein STEP-Export
- keine GUI-Aktion

## Begruendung

Die Trennung macht den naechsten Schritt pruefbar. Das System kann bereits
zeigen, welche Sketches und Features in welcher Reihenfolge betroffen sind,
ohne dass Geometriefehler, Cachezustand oder Exportlogik eingemischt werden.

## Konsequenzen

- Recompute-Reihenfolge ist als Core-Datenmodell testbar.
- `PartDocument` kann einen Plan aus seinem internen Zustand erzeugen.
- OCCT kann spaeter hinter einem Adapter auf diesen Plan aufbauen.
- `changed`-Knoten bleiben Quelle der Aenderung und werden nicht selbst als
  Recompute-Schritt geplant.

## Verworfene Alternativen

### Recompute sofort im Plan ausfuehren

Das wuerde Plan, Feature-Ausfuehrung, Geometrieadapter und Fehlerbehandlung
vermischen. Fuer MVP 1 bleibt der Plan erst einmal ein Datenobjekt.

### Dirty-Knoten in Einfuegereihenfolge verwenden

Das waere einfacher, koennte aber bei verzweigten Abhaengigkeiten falsche
Reihenfolgen erzeugen. Die topologische Ordnung des Dependency Graph ist die
richtige Grundlage.
