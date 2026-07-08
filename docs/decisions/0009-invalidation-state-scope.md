# 0009: Umfang des MVP-1-Invalidierungsstatus

Status: akzeptiert fuer MVP 1

## Kontext

`PartDocument` erzeugt einen `DependencyGraph` aus Parametern, Sketches und
Features. Damit ist bekannt, welche Knoten von welchem Parameter abhaengen.

Der naechste Schritt ist ein Datenzustand, der eine Aenderung sichtbar macht,
ohne bereits Geometrie neu zu berechnen.

## Entscheidung

MVP 1 bekommt einen kleinen `InvalidationState`.

Der Status kennt:

- `clean`
- `changed`
- `dirty`
- `error`

Eine Parameteraenderung markiert:

- den Parameter als `changed`
- alle transitiv abhaengigen Knoten als `dirty`

`InvalidationState` nutzt den bestehenden `DependencyGraph`, fuehrt aber keinen
Recompute aus.

## Begruendung

Dieser Schnitt haelt die Recompute-Vorbereitung testbar. Das System kann
bereits pruefen, welche Sketches und Features betroffen sind, ohne OCCT,
Shape-Cache oder Feature-Ausfuehrung einzubauen.

`PartDocument` ist der richtige Integrationspunkt, weil es den Graphen besitzt
und Parameter-IDs validieren kann.

## Konsequenzen

- Betroffene Knoten sind nach einer Parameteraenderung sichtbar.
- Der Zustand ist unabhaengig vom Geometriekernel.
- Recompute kann spaeter als separate Schicht auf `dirty`-Knoten aufbauen.
- `error` ist vorbereitet, wird in diesem Schritt aber noch nicht durch
  Geometriefehler gesetzt.

## Verworfene Alternativen

### Recompute sofort ausloesen

Das wuerde Invalidierung, Recompute-Plan, Feature-Ausfuehrung und
Geometriefehler vermischen. Fuer MVP 1 ist zuerst der reine Zustand wichtiger.

### Nur eine Liste betroffener Knoten zurueckgeben

Eine Liste waere fuer einzelne Operationen ausreichend, speichert aber keinen
persistenten Dokumentzustand. Ein eigener Status erlaubt spaeter UI-Anzeige,
Fehlerzustand und Recompute-Planung.
