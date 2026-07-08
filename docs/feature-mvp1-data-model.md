# MVP 1 Feature-Datenmodell

Status: reines Datenmodell mit PartDocument-Integration, keine Geometrieerzeugung

Dieses Dokument beschreibt den aktuellen Stand von `Feature`,
`AdditiveExtrude` und `SubtractiveExtrude`.

Der Stand ist bewusst begrenzt:

- keine OCCT-Operationen
- keine BRep-Geometrie
- kein Boolean-Cut
- kein Recompute
- kein ShapeCache in dieser Feature-Datenmodellschicht
- keine GUI

## Ziel

Das Feature-Datenmodell bildet die naechste Schicht der Konstruktionsabsicht ab:

- Ein `AdditiveExtrude` referenziert eine Skizze und einen Laengenparameter.
- Ein `SubtractiveExtrude` referenziert eine Skizze und ein vorhandenes
  Zielfeature.
- `PartDocument` validiert, dass diese Referenzen im Dokument existieren.

Damit ist noch keine Geometrie berechenbar. Es ist nur die Modellabsicht
speicherbar.

## `Feature`

`Feature` ist aktuell ein kompakter Datentyp mit einem Feature-Typ.

Aktuelle Feature-Typen:

- `additive_extrude`
- `subtractive_extrude`

Gemeinsame Felder:

```text
Feature
  id
  name
  type
  input_sketch
  direction
```

Die Richtung ist fuer MVP 1 nur:

```text
+Z
```

## `AdditiveExtrude`

Ein `AdditiveExtrude` erzeugt spaeter aus einer Skizze einen Koerper.

Aktuelle Felder:

```text
AdditiveExtrude
  id = "feature.base_extrude"
  name = "BaseExtrude"
  input_sketch = "sketch.base"
  length_parameter = "part.thickness"
  direction = "+Z"
```

Validierung im Feature:

- Feature-ID darf nicht leer sein.
- Name darf nicht leer sein.
- Eingabe-Sketch-ID darf nicht leer sein.
- Laengenparameter-ID darf nicht leer sein.

Validierung in `PartDocument`:

- Feature-ID muss eindeutig sein.
- Eingabe-Sketch muss im Dokument existieren.
- Laengenparameter muss im Dokument existieren.

## `SubtractiveExtrude`

Ein `SubtractiveExtrude` schneidet spaeter Material aus einem vorhandenen
Zielfeature.

Aktuelle Felder:

```text
SubtractiveExtrude
  id = "feature.center_hole_cut"
  name = "CenterHoleCut"
  input_sketch = "sketch.hole"
  target_feature = "feature.base_extrude"
  depth = "through_all"
  direction = "+Z"
```

Validierung im Feature:

- Feature-ID darf nicht leer sein.
- Name darf nicht leer sein.
- Eingabe-Sketch-ID darf nicht leer sein.
- Zielfeature-ID darf nicht leer sein.

Validierung in `PartDocument`:

- Feature-ID muss eindeutig sein.
- Eingabe-Sketch muss im Dokument existieren.
- Zielfeature muss bereits im Dokument existieren.

## Reihenfolge und Abhaengigkeiten

MVP 1 erzwingt im `PartDocument` aktuell implizit eine einfache Reihenfolge:

1. Parameter hinzufuegen.
2. Datum-Plane hinzufuegen.
3. Sketches hinzufuegen.
4. `AdditiveExtrude` hinzufuegen.
5. `SubtractiveExtrude` mit Ziel auf das vorhandene `AdditiveExtrude`
   hinzufuegen.

Ein `DependencyGraph` existiert inzwischen als reines Datenmodell und ist in
`PartDocument` integriert. Feature-Referenzen erzeugen Graphkanten:

- Eingabe-Sketch -> Feature
- Laengenparameter -> `AdditiveExtrude`
- Zielfeature -> `SubtractiveExtrude`

Die aktuelle Validierung verhindert weiterhin Referenzen auf fehlende Objekte.

## Testabdeckung

Aktuelle Tests pruefen:

- `AdditiveExtrude` speichert Sketch- und Laengenparameter-Referenz.
- `AdditiveExtrude` lehnt fehlende Pflichtfelder ab.
- `SubtractiveExtrude` speichert Sketch- und Zielfeature-Referenz.
- `SubtractiveExtrude` lehnt fehlende Pflichtfelder ab.
- `PartDocument` speichert Features.
- `PartDocument` lehnt doppelte Feature-IDs ab.
- `PartDocument` lehnt Features mit fehlenden Eingabe-Sketches ab.
- `PartDocument` lehnt `AdditiveExtrude` mit fehlendem Laengenparameter ab.
- `PartDocument` lehnt `SubtractiveExtrude` mit fehlendem Zielfeature ab.
- `DependencyGraph` prueft die feste MVP-1-Abhaengigkeitsreihenfolge separat.
- `PartDocument` erzeugt Feature-Knoten und Feature-Abhaengigkeitskanten.
- `InvalidationState` kann abhaengige Features als `dirty` markieren.
- `RecomputePlan` kann `dirty`-Features topologisch geordnet auflisten.

## Naechster sinnvoller Schritt

Der erste optionale Geometry-Adapter und die Additive-Ausfuehrung fuer ein
einzelnes Rechteckprofil sind vorhanden. Der naechste Schritt sollte den
subtraktiven Pfad klein anbinden:

- `SubtractiveExtrude` aus einem Recompute-Plan-Knoten ausfuehren
- Zielshape aus dem bestehenden Geometry-`ShapeCache` lesen
- Ergebnis als neuen finalen Shape speichern
- OCCT hinter der Geometry-Adaptergrenze halten
- noch keine GUI bauen
