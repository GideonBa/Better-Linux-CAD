# 0007: Umfang des MVP-1-Dependency-Graph

Status: akzeptiert fuer MVP 1

## Kontext

MVP 1 braucht eine nachvollziehbare Reihenfolge zwischen Parametern, Sketches
und Features. Ohne Dependency Graph waere spaeter unklar, welche Objekte nach
einer Parameteraenderung betroffen sind.

Gleichzeitig waere es zu frueh, jetzt schon Recompute, Shape-Cache und OCCT in
diese Schicht zu ziehen. Der Graph soll zuerst als kleines Datenmodell
stabilisiert und getestet werden.

## Entscheidung

Der erste `DependencyGraph` ist ein reines Core-Datenmodell.

Eine Kante `A -> B` bedeutet:

```text
B haengt von A ab.
```

Der Graph speichert fuer MVP 1:

- stabile String-Knoten-IDs
- gerichtete Kanten
- direkte Abhaengige
- transitive Abhaengige
- topologische Ordnung
- Zyklenerkennung

Der Graph fuehrt nicht aus:

- keine Parameteraenderung
- keine Invalidierung
- kein Recompute
- keine OCCT-Operation
- kein Shape-Cache-Update

## Begruendung

Dieser Schnitt haelt den Core testbar und klein. Die naechste Schicht kann auf
einer geprueften Abhaengigkeitsstruktur aufbauen, ohne die Graphlogik mit
Geometriefehlern oder Cachezustand zu vermischen.

String-Knoten-IDs sind fuer diesen Schritt ausreichend, weil alle bestehenden
Objekte bereits stabile IDs besitzen. Die spaetere `PartDocument`-Integration
kann typisierte IDs kontrolliert in Graphknoten uebersetzen.

## Konsequenzen

- Der Dependency Graph bleibt frei von OCCT und Qt.
- Topologische Ordnung und Zyklenerkennung sind bereits testbar.
- Recompute kann spaeter als separate Schicht implementiert werden.
- `PartDocument` kann den Graphen in einem Folgeschritt integrieren.
- Fehler im Graphen verwenden `ErrorCategory::Dependency`, wenn die Struktur
  selbst ungueltig ist.

## Verworfene Alternativen

### Recompute direkt mit dem Graphen implementieren

Das wuerde schneller sichtbares Verhalten schaffen, aber zu viel auf einmal
vermischen: Abhaengigkeiten, Invalidierung, Geometrie und Fehlerzustand.

### Graphknoten sofort als Variant ueber alle ID-Typen modellieren

Das waere typsicherer, erhoeht aber die Kopplung in diesem fruehen Schritt. Fuer
MVP 1 reicht die stabile String-ID, solange die Integration zentral in
`PartDocument` erfolgt.

### Zyklen beim Hinzufuegen jeder Kante verhindern

Das ist spaeter moeglich, fuer das MVP-1-Datenmodell aber nicht noetig. Aktuell
duerfen Kanten gespeichert werden; `topological_order()` meldet einen
Dependency-Fehler, wenn ein Zyklus vorliegt.
