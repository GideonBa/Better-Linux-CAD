# MVP 1 Dependency-Graph-Datenmodell

Status: reines Datenmodell mit PartDocument-Integration, kein Recompute

Dieses Dokument beschreibt den aktuellen Stand von `DependencyGraph`.

Der Stand ist bewusst begrenzt:

- keine Geometrie
- keine OCCT-Operationen
- kein ShapeCache in dieser Graphschicht
- kein Recompute
- keine GUI

## Ziel

Der Dependency Graph speichert, welche MVP-1-Objekte voneinander abhaengen.
Damit wird die Konstruktionsabsicht sichtbar und testbar, ohne dass bereits
Geometrie berechnet wird.

Eine Kante:

```text
A -> B
```

bedeutet:

```text
B haengt von A ab.
```

Wenn `A` spaeter geaendert wird, ist `B` ein Kandidat fuer Invalidierung und
Recompute. Diese Invalidierung wird in diesem Schritt aber noch nicht
ausgefuehrt.

## Knoten

Knoten sind fuer MVP 1 einfache stabile String-IDs.

Beispiele:

```text
part.width
part.height
part.thickness
part.hole_diameter
sketch.base
sketch.hole
feature.base_extrude
feature.center_hole_cut
```

Der Graph verwendet bewusst keine OCCT- oder GUI-Typen. Er kennt intern auch
keine konkreten `ParameterId`, `SketchId` oder `FeatureId`-Klassen.
`PartDocument` uebersetzt die typisierten IDs in diese stabilen Knoten-IDs.

## Kanten

`add_dependency(dependency, dependent)` speichert eine gerichtete Kante.

Regeln:

- leere Quellknoten werden abgelehnt
- leere Zielknoten werden abgelehnt
- Selbstabhaengigkeiten werden abgelehnt
- doppelte Kanten werden abgelehnt
- fehlende Knoten werden beim Hinzufuegen einer Kante automatisch angelegt
- die Einfuegereihenfolge von Knoten und Kanten bleibt erhalten

Explizites `add_node(node_id)` ist idempotent. Wenn ein Knoten bereits existiert,
wird sein vorhandener Index zurueckgegeben.

## Abfragen

Aktuelle Abfragen:

- `has_node(node_id)`
- `has_dependency(dependency, dependent)`
- `direct_dependents(node_id)`
- `transitive_dependents(node_id)`
- `topological_order()`
- `has_cycle()`

`direct_dependents()` liefert nur direkte Nachfolger.

`transitive_dependents()` liefert alle indirekt betroffenen Knoten in
deterministischer Suchreihenfolge. Der Startknoten selbst wird nicht
zurueckgegeben.

`topological_order()` liefert eine Reihenfolge, in der Abhaengigkeiten vor ihren
abhaengigen Knoten stehen. Enthalt der Graph einen Zyklus, wird ein
`dependency`-Fehler zurueckgegeben.

## Referenzplatte

Der aktuelle Testgraph fuer MVP 1 beschreibt die Referenzplatte:

```text
part.width          -> sketch.base
part.height         -> sketch.base
part.hole_diameter  -> sketch.hole
sketch.base         -> feature.base_extrude
part.thickness      -> feature.base_extrude
sketch.hole         -> feature.center_hole_cut
feature.base_extrude -> feature.center_hole_cut
```

Damit ist pruefbar:

- `sketch.base` kommt nach `part.width` und `part.height`
- `feature.base_extrude` kommt nach `sketch.base` und `part.thickness`
- `sketch.hole` kommt nach `part.hole_diameter`
- `feature.center_hole_cut` kommt nach `sketch.hole` und
  `feature.base_extrude`

Das ist noch keine Recompute-Ausfuehrung. Es ist nur die geordnete
Abhaengigkeitsstruktur.

## PartDocument-Integration

`PartDocument` besitzt einen internen `DependencyGraph`.

Beim Hinzufuegen von Objekten entstehen automatisch Knoten und Kanten:

- `add_parameter()` erzeugt einen Parameterknoten.
- `add_sketch()` erzeugt einen Sketchknoten.
- Rechteckprofile erzeugen Kanten von Breiten- und Hoehenparameter zum Sketch.
- Kreisprofile erzeugen Kanten vom Durchmesserparameter zum Sketch.
- `add_feature()` erzeugt einen Featureknoten.
- Jedes Feature erzeugt eine Kante vom Eingabe-Sketch zum Feature.
- `AdditiveExtrude` erzeugt zusaetzlich eine Kante vom Laengenparameter zum
  Feature.
- `SubtractiveExtrude` erzeugt zusaetzlich eine Kante vom Zielfeature zum
  Feature.

Der Graph ist ueber `PartDocument::dependency_graph()` lesbar. Der Accessor ist
absichtlich `const`, damit externe Aufrufer die Dokumentinvarianten nicht
umgehen.

## Fehlerverhalten

Validierungsfehler verwenden `ErrorCategory::Validation`.

Beispiele:

```text
dependency node id must not be empty
dependency source node id must not be empty
dependency dependent node id must not be empty
```

Strukturelle Graphfehler verwenden `ErrorCategory::Dependency`.

Beispiele:

```text
dependency edge must not point to itself
dependency edge must be unique
dependency graph contains a cycle
```

## Testabdeckung

Aktuelle Tests pruefen:

- leerer Graph startet ohne Knoten und Kanten
- explizite Knoten werden gespeichert
- doppelte Knoten werden nicht erneut angelegt
- leere Knoten-IDs werden abgelehnt
- Kanten legen fehlende Knoten automatisch an
- leere Quell- und Zielknoten werden abgelehnt
- Selbstabhaengigkeiten werden abgelehnt
- doppelte Kanten werden abgelehnt
- direkte Abhaengige werden gefunden
- transitive Abhaengige werden gefunden
- unbekannte Knoten liefern leere Abfragen
- die Referenzplatte hat eine gueltige topologische Ordnung
- Zyklen werden erkannt
- `PartDocument` erzeugt Graphknoten fuer Parameter, Sketches und Features
- `PartDocument` erzeugt Graphkanten aus Profil- und Feature-Referenzen
- die MVP-1-Referenzplatte ist ueber den Dokumentgraphen topologisch geordnet
- `InvalidationState` nutzt den Graphen, um abhaengige Knoten als `dirty` zu
  markieren
- `RecomputePlan` nutzt die topologische Ordnung des Graphen, um `dirty`-Knoten
  zu sortieren

## Naechster sinnvoller Schritt

Der erste optionale Geometry-Adapter und die Additive-Ausfuehrung sind
vorhanden. Der Graph bleibt davon getrennt und beschreibt nur Abhaengigkeiten.
Der naechste Schritt sollte klein bleiben:

- `SubtractiveExtrude` aus einem Recompute-Plan-Knoten ausfuehren
- bestehenden Geometry-`ShapeCache` als Eingabe und Ergebnis behandeln
- OCCT nur hinter dieser Adaptergrenze verwenden
